#pragma once

#include "VisualMarkdown/KnowledgeMarkdownLayout.h"
#include "VisualMarkdown/KnowledgeMarkdownReader.h"
#include "VisualMarkdown/KnowledgeMarkdownWriter.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>
#include <QTest>

// 예시 wiki(examples/wiki · examples/showcase/wiki)를 시험하는 클래스가 함께 쓴다.
// 파일을 바이트 그대로 읽고, 라이브러리가 다시 쓴 글과 바이트까지 견준다.
namespace WikiTestFiles
{
inline const QString kDocumentsFolder = QStringLiteral("documents");
inline const QString kConceptsFolder = QStringLiteral("concepts");

// 폴더 바로 아래 *.md 의 절대 경로(이름 순). 돌 때마다 훑으므로 나중에 더한 파일도 시험한다.
inline QStringList ListMarkdownFiles(const QString& folderPath)
{
    const QDir dir(folderPath);
    QStringList vecPaths;
    for (const QFileInfo& info : dir.entryInfoList(QStringList { QStringLiteral("*.md") }, QDir::Files, QDir::Name))
        vecPaths << info.absoluteFilePath();
    return vecPaths;
}

// 파일마다 한 행. 파일이 하나도 없으면 빈 경로 한 행을 넣는다(행이 없으면 QFETCH 가 멈추므로, 시험이 실패로 끝나게).
inline void AddFileRows(const QString& folderPath)
{
    QTest::addColumn<QString>("path");
    const QStringList vecPaths = ListMarkdownFiles(folderPath);
    for (const QString& strPath : vecPaths)
        QTest::newRow(QFileInfo(strPath).fileName().toUtf8().constData()) << strPath;
    if (vecPaths.isEmpty())
        QTest::newRow("no files") << QString();
}

// 파일을 바이트 그대로 읽는다
inline bool ReadBytes(const QString& path, QByteArray* bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    *bytes = file.readAll();
    return true;
}

inline bool HasSection(const QString& text, const QString& name)
{
    for (const KnowledgeSectionRange& section : KnowledgeMarkdownLayout::Scan(text).vecSections)
        if (section.strName == name)
            return true;
    return false;
}

// 실패 메시지에 보일 줄. 보이지 않는 글자(\r · \t)는 글자로 적는다.
inline QString ShowLine(const QList<QByteArray>& lines, int index)
{
    if (index >= lines.size())
        return QString::fromUtf8("(줄 없음)");
    QString strLine = QString::fromUtf8(lines[index]);
    strLine.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    strLine.replace(QLatin1Char('\t'), QStringLiteral("\\t"));
    return QLatin1Char('[') + strLine + QLatin1Char(']');
}

// 바이트가 다를 때 처음 달라지는 줄(1부터)과 그 두 줄. QCOMPARE 는 앞 몇 바이트만 보여 줘 어디가 다른지 모른다.
inline QString DescribeDifference(const QByteArray& written, const QByteArray& file)
{
    if (written == file)
        return QString();
    const QList<QByteArray> vecWritten = written.split('\n');
    const QList<QByteArray> vecFile = file.split('\n');
    int iLine = 0;
    while (iLine < vecWritten.size() && iLine < vecFile.size() && vecWritten[iLine] == vecFile[iLine])
        ++iLine;
    return QString::fromUtf8("쓴 글 %1 바이트 · 파일 %2 바이트, %3번째 줄부터 다르다\n   쓴 글: %4\n   파일 : %5")
        .arg(written.size())
        .arg(file.size())
        .arg(iLine + 1)
        .arg(ShowLine(vecWritten, iLine), ShowLine(vecFile, iLine));
}

inline QString DescribeUnread(const QHash<QString, QStringList>& unread)
{
    QStringList vecParts;
    for (auto it = unread.cbegin(); it != unread.cend(); ++it)
        vecParts << QString::fromUtf8("읽지 못한 줄(") + it.key() + QString::fromUtf8("): ") + it.value().join(QStringLiteral(" / "));
    return vecParts.join(QLatin1Char('\n'));
}

// 문서 파일: 다 읽히고, digest 가 맞고, 새로 써도 · 있는 글에 갈아 끼워도 파일과 바이트가 같다
inline void VerifyDocumentRewrite(const QString& path)
{
    QByteArray baRaw;
    QVERIFY2(ReadBytes(path, &baRaw), qUtf8Printable(path));
    const QString strText = QString::fromUtf8(baRaw);

    const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
    QVERIFY2(result.bOk, qUtf8Printable(result.strError));
    QVERIFY2(result.hashUnreadLines.isEmpty(), qUtf8Printable(DescribeUnread(result.hashUnreadLines)));
    QCOMPARE(result.document.strDigest, KnowledgeMarkdownWriter::ComputeDigest(result.document));

    const QByteArray baFresh = KnowledgeMarkdownWriter::WriteDocument(result.document).toUtf8();
    QVERIFY2(baFresh == baRaw, qUtf8Printable(QString::fromUtf8("새로 쓰기: ") + DescribeDifference(baFresh, baRaw)));
    const QByteArray baRewritten = KnowledgeMarkdownWriter::WriteDocument(result.document, strText).toUtf8();
    QVERIFY2(baRewritten == baRaw, qUtf8Printable(QString::fromUtf8("다시 쓰기: ") + DescribeDifference(baRewritten, baRaw)));
}

// 개념 파일: 다 읽히고, id 가 파일 이름이고, 있는 글에 다시 쓰면 바이트가 같다.
// WriteConcept 는 `## 관계` 를 만들지 않으므로 그 절이 없는 파일은 새로 써도 같다.
inline void VerifyConceptRewrite(const QString& path)
{
    QByteArray baRaw;
    QVERIFY2(ReadBytes(path, &baRaw), qUtf8Printable(path));
    const QString strText = QString::fromUtf8(baRaw);

    const ConceptReadResult result = KnowledgeMarkdownReader::ReadConcept(strText);
    QVERIFY2(result.bOk, qUtf8Printable(result.strError));
    QVERIFY2(result.hashUnreadLines.isEmpty(), qUtf8Printable(DescribeUnread(result.hashUnreadLines)));
    QCOMPARE(result.concept.strId, QFileInfo(path).completeBaseName());

    const QByteArray baRewritten = KnowledgeMarkdownWriter::WriteConcept(result.concept, strText).toUtf8();
    QVERIFY2(baRewritten == baRaw, qUtf8Printable(QString::fromUtf8("다시 쓰기: ") + DescribeDifference(baRewritten, baRaw)));
    if (!HasSection(strText, QString::fromUtf8("관계")))
    {
        const QByteArray baFresh = KnowledgeMarkdownWriter::WriteConcept(result.concept).toUtf8();
        QVERIFY2(baFresh == baRaw, qUtf8Printable(QString::fromUtf8("새로 쓰기: ") + DescribeDifference(baFresh, baRaw)));
    }
}
}
