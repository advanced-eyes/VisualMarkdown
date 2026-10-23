#include "VisualMarkdown/KnowledgeMarkdownLayout.h"
#include "VisualMarkdown/KnowledgeMarkdownReader.h"
#include "VisualMarkdown/KnowledgeMarkdownWriter.h"
#include "VisualMarkdown/KnowledgeText.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPair>
#include <QSet>
#include <QTest>

#include <memory>

namespace
{
QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

const QString kDocumentsFolder = QStringLiteral("documents");
const QString kConceptsFolder = QStringLiteral("concepts");

// examples/wiki 안의 경로(CMake 가 VISUALMARKDOWN_EXAMPLES_DIR 로 절대 경로를 넣는다)
QString MakeExamplePath(const QString& relativePath)
{
    return QStringLiteral(VISUALMARKDOWN_EXAMPLES_DIR) + QLatin1Char('/') + relativePath;
}

// 한 폴더(documents · concepts) 바로 아래 *.md 의 절대 경로(이름 순). 돌 때마다 훑으므로 나중에 더한 예시도 시험한다.
QStringList ListExampleFiles(const QString& folder)
{
    const QDir dir(MakeExamplePath(folder));
    QStringList vecPaths;
    for (const QFileInfo& info : dir.entryInfoList(QStringList { QStringLiteral("*.md") }, QDir::Files, QDir::Name))
        vecPaths << info.absoluteFilePath();
    return vecPaths;
}

// 파일마다 한 행. 파일이 하나도 없으면 빈 경로 한 행을 넣는다(행이 없으면 QFETCH 가 멈추므로, 시험이 실패로 끝나게).
void AddFileRows(const QString& folder)
{
    QTest::addColumn<QString>("path");
    const QStringList vecPaths = ListExampleFiles(folder);
    for (const QString& strPath : vecPaths)
        QTest::newRow(QFileInfo(strPath).fileName().toUtf8().constData()) << strPath;
    if (vecPaths.isEmpty())
        QTest::newRow("no files") << QString();
}

// 파일을 바이트 그대로 읽는다
bool ReadBytes(const QString& path, QByteArray* bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    *bytes = file.readAll();
    return true;
}

bool HasSection(const QString& text, const QString& name)
{
    for (const KnowledgeSectionRange& section : KnowledgeMarkdownLayout::Scan(text).vecSections)
        if (section.strName == name)
            return true;
    return false;
}

// 실패 메시지에 보일 줄. 보이지 않는 글자(\r · \t)는 글자로 적는다.
QString ShowLine(const QList<QByteArray>& lines, int index)
{
    if (index >= lines.size())
        return DecodeUtf8("(줄 없음)");
    QString strLine = QString::fromUtf8(lines[index]);
    strLine.replace(QLatin1Char('\r'), QStringLiteral("\\r"));
    strLine.replace(QLatin1Char('\t'), QStringLiteral("\\t"));
    return QLatin1Char('[') + strLine + QLatin1Char(']');
}

// 바이트가 다를 때 처음 달라지는 줄(1부터)과 그 두 줄. QCOMPARE 는 앞 몇 바이트만 보여 줘 어디가 다른지 모른다.
QString DescribeDifference(const QByteArray& written, const QByteArray& file)
{
    if (written == file)
        return QString();
    const QList<QByteArray> vecWritten = written.split('\n');
    const QList<QByteArray> vecFile = file.split('\n');
    int iLine = 0;
    while (iLine < vecWritten.size() && iLine < vecFile.size() && vecWritten[iLine] == vecFile[iLine])
        ++iLine;
    return DecodeUtf8("쓴 글 %1 바이트 · 파일 %2 바이트, %3번째 줄부터 다르다\n   쓴 글: %4\n   파일 : %5")
        .arg(written.size())
        .arg(file.size())
        .arg(iLine + 1)
        .arg(ShowLine(vecWritten, iLine), ShowLine(vecFile, iLine));
}

QString DescribeUnread(const QHash<QString, QStringList>& unread)
{
    QStringList vecParts;
    for (auto it = unread.cbegin(); it != unread.cend(); ++it)
        vecParts << DecodeUtf8("읽지 못한 줄(") + it.key() + DecodeUtf8("): ") + it.value().join(QStringLiteral(" / "));
    return vecParts.join(QLatin1Char('\n'));
}
}

// examples/wiki 의 정준 예시가 라이브러리가 쓰는 글과 바이트까지 같은지 본다(README §5.4 의 2).
// 실패하면 예시나 라이브러리를 고쳐 맞추지 않는다. 예시는 스펙의 픽스처라 다른 곳이 찾을 거리다.
class ExampleWikiTest : public QObject
{
    Q_OBJECT

private slots:
    void examplesAreFound()
    {
        QVERIFY2(!ListExampleFiles(kDocumentsFolder).isEmpty(), qUtf8Printable(MakeExamplePath(kDocumentsFolder)));
        QVERIFY2(!ListExampleFiles(kConceptsFolder).isEmpty(), qUtf8Printable(MakeExamplePath(kConceptsFolder)));
    }

    void documentRewriteIsByteExact_data()
    {
        AddFileRows(kDocumentsFolder);
    }

    // 문서 파일: 다 읽히고, digest 가 맞고, 새로 써도 · 있는 글에 갈아 끼워도 파일과 바이트가 같다
    void documentRewriteIsByteExact()
    {
        QFETCH(QString, path);
        QVERIFY2(!path.isEmpty(), "examples/wiki/documents has no *.md file");
        QByteArray baRaw;
        QVERIFY2(ReadBytes(path, &baRaw), qUtf8Printable(path));
        const QString strText = QString::fromUtf8(baRaw);

        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY2(result.bOk, qUtf8Printable(result.strError));
        QVERIFY2(result.hashUnreadLines.isEmpty(), qUtf8Printable(DescribeUnread(result.hashUnreadLines)));
        QCOMPARE(result.document.strDigest, KnowledgeMarkdownWriter::ComputeDigest(result.document));

        const QByteArray baFresh = KnowledgeMarkdownWriter::WriteDocument(result.document).toUtf8();
        QVERIFY2(baFresh == baRaw, qUtf8Printable(DecodeUtf8("새로 쓰기: ") + DescribeDifference(baFresh, baRaw)));
        const QByteArray baRewritten = KnowledgeMarkdownWriter::WriteDocument(result.document, strText).toUtf8();
        QVERIFY2(baRewritten == baRaw, qUtf8Printable(DecodeUtf8("다시 쓰기: ") + DescribeDifference(baRewritten, baRaw)));
    }

    void conceptRewriteIsByteExact_data()
    {
        AddFileRows(kConceptsFolder);
    }

    // 개념 파일: 다 읽히고, id 가 파일 이름이고, 있는 글에 다시 쓰면 바이트가 같다.
    // WriteConcept 는 `## 관계` 를 만들지 않으므로 그 절이 없는 파일은 새로 써도 같다.
    void conceptRewriteIsByteExact()
    {
        QFETCH(QString, path);
        QVERIFY2(!path.isEmpty(), "examples/wiki/concepts has no *.md file");
        QByteArray baRaw;
        QVERIFY2(ReadBytes(path, &baRaw), qUtf8Printable(path));
        const QString strText = QString::fromUtf8(baRaw);

        const ConceptReadResult result = KnowledgeMarkdownReader::ReadConcept(strText);
        QVERIFY2(result.bOk, qUtf8Printable(result.strError));
        QVERIFY2(result.hashUnreadLines.isEmpty(), qUtf8Printable(DescribeUnread(result.hashUnreadLines)));
        QCOMPARE(result.concept.strId, QFileInfo(path).completeBaseName());

        const QByteArray baRewritten = KnowledgeMarkdownWriter::WriteConcept(result.concept, strText).toUtf8();
        QVERIFY2(baRewritten == baRaw, qUtf8Printable(DecodeUtf8("다시 쓰기: ") + DescribeDifference(baRewritten, baRaw)));
        if (!HasSection(strText, DecodeUtf8("관계")))
        {
            const QByteArray baFresh = KnowledgeMarkdownWriter::WriteConcept(result.concept).toUtf8();
            QVERIFY2(baFresh == baRaw, qUtf8Printable(DecodeUtf8("새로 쓰기: ") + DescribeDifference(baFresh, baRaw)));
        }
    }

    // 가리키는 개념 id(카드 `개념:` · 문서와 개념 파일의 관계 from/to · 표 concepts)마다 concepts/ 에 파일이 있다
    void referencedConceptsHaveFiles()
    {
        QSet<QString> setConceptFiles;
        for (const QString& strPath : ListExampleFiles(kConceptsFolder))
            setConceptFiles.insert(QFileInfo(strPath).completeBaseName());

        QVector<QPair<QString, QString>> vecReferences;   // (개념 id, 가리킨 파일 이름)
        for (const QString& strPath : ListExampleFiles(kDocumentsFolder))
        {
            QByteArray baRaw;
            QVERIFY2(ReadBytes(strPath, &baRaw), qUtf8Printable(strPath));
            const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(QString::fromUtf8(baRaw));
            QVERIFY2(result.bOk, qUtf8Printable(strPath + DecodeUtf8(": ") + result.strError));
            const QString strFile = QFileInfo(strPath).fileName();
            for (const KnowledgeCard& card : result.document.vecCards)
                for (const ConceptLink& link : card.vecConcepts)
                    vecReferences.push_back({ link.strConceptId, strFile });
            for (const KnowledgeRelation& relation : result.document.vecRelations)
            {
                vecReferences.push_back({ relation.fromConcept.strConceptId, strFile });
                vecReferences.push_back({ relation.toConcept.strConceptId, strFile });
            }
            for (const KnowledgeTable& table : result.document.vecTables)
                for (const QString& strConceptId : table.vecConceptIds)
                    vecReferences.push_back({ strConceptId, strFile });
        }
        for (const QString& strPath : ListExampleFiles(kConceptsFolder))
        {
            QByteArray baRaw;
            QVERIFY2(ReadBytes(strPath, &baRaw), qUtf8Printable(strPath));
            const ConceptReadResult result = KnowledgeMarkdownReader::ReadConcept(QString::fromUtf8(baRaw));
            QVERIFY2(result.bOk, qUtf8Printable(strPath + DecodeUtf8(": ") + result.strError));
            const QString strFile = QFileInfo(strPath).fileName();
            for (const KnowledgeRelation& relation : result.concept.vecRelations)
            {
                vecReferences.push_back({ relation.fromConcept.strConceptId, strFile });
                vecReferences.push_back({ relation.toConcept.strConceptId, strFile });
            }
        }

        QVERIFY(!vecReferences.isEmpty());
        for (const QPair<QString, QString>& pairReference : vecReferences)
            QVERIFY2(setConceptFiles.contains(pairReference.first),
                     qUtf8Printable(pairReference.second + DecodeUtf8(" 이 가리키는 개념 '") + pairReference.first + DecodeUtf8("' 의 파일이 concepts/ 에 없다")));
    }

    // 정준 예시 문서의 기록을 하나씩 확인한다
    void exampleDocumentRecords()
    {
        QByteArray baRaw;
        QVERIFY(ReadBytes(MakeExamplePath(QStringLiteral("documents/yolo9000-better-faster-stronger.md")), &baRaw));
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(QString::fromUtf8(baRaw));
        QVERIFY2(result.bOk, qUtf8Printable(result.strError));
        const KnowledgeDocument& doc = result.document;

        QCOMPARE(doc.vecOutline.size(), 3);
        QCOMPARE(doc.vecOutline[1].vecChildren.size(), 1);
        QCOMPARE(doc.vecOutline[1].vecChildren[0].strNumber, QString("2.1"));

        QCOMPARE(doc.vecKeyNumbers.size(), 1);
        QCOMPARE(doc.vecKeyNumbers[0].strValue, QString("76.8"));
        QCOMPARE(doc.vecKeyNumbers[0].strUnit, QString("mAP"));
        QCOMPARE(doc.vecKeyNumbers[0].strLabel, QString("VOC 2007, 67 fps"));

        QCOMPARE(doc.vecCards.size(), 2);
        QCOMPARE(doc.vecCards[0].strId, QString("7f3a2c1e"));
        QCOMPARE(doc.vecCards[1].strId, QString("2b9d04e6"));

        QCOMPARE(doc.vecRelations.size(), 3);
        const KnowledgeRelation& improves = doc.vecRelations[0];
        QCOMPARE(improves.strKind, QString("improves"));
        QCOMPARE(improves.fromConcept.strConceptId, QString("yolov2"));
        QCOMPARE(improves.toConcept.strConceptId, QString("yolo"));
        QCOMPARE(improves.eStatus, ERelationStatus::PROPOSED);
        const KnowledgeRelation& alternative = doc.vecRelations[1];
        QCOMPARE(alternative.strKind, QString("alternative"));
        QCOMPARE(alternative.fromConcept.strConceptId, QString("yolov2"));
        QCOMPARE(alternative.toConcept.strConceptId, QString("faster-r-cnn"));
        QCOMPARE(alternative.eStatus, ERelationStatus::PROPOSED);
        // 앞뒤를 감싼 " 는 벗겨 읽는다
        QCOMPARE(alternative.strQuote, QString("At 40 FPS, YOLOv2 gets 78.6 mAP, outperforming state-of-the-art methods like "
                                               "Faster RCNN with ResNet and SSD while still running significantly faster."));
        const KnowledgeRelation& solves = doc.vecRelations[2];
        QCOMPARE(solves.strKind, QString("solves"));
        QCOMPARE(solves.fromConcept.strConceptId, QString("yolov2"));
        QCOMPARE(solves.toConcept.strConceptId, DecodeUtf8("객체-검출"));
        QCOMPARE(solves.eStatus, ERelationStatus::CONFIRMED);   // 상태 줄이 없으면 확인됨
        QCOMPARE(solves.strWhere, DecodeUtf8("1절"));

        QCOMPARE(doc.vecTables.size(), 2);
        const KnowledgeTable& steps = doc.vecTables[0];
        QCOMPARE(steps.strX, DecodeUtf8("단계"));
        QCOMPARE(steps.vecY, QStringList({ "mAP" }));
        QCOMPARE(steps.vecOrdered, QStringList({ DecodeUtf8("단계") }));
        QCOMPARE(steps.bCumulative, true);
        QCOMPARE(steps.vecConceptIds, QStringList({ "yolov2", "yolo" }));
        QCOMPARE(steps.vecColumns.size(), 2);
        QCOMPARE(steps.vecColumns[0].strName, DecodeUtf8("단계"));
        QCOMPARE(steps.vecColumns[0].strUnit, QString());
        QCOMPARE(steps.vecColumns[1].strName, QString("mAP"));
        QCOMPARE(steps.vecColumns[1].strUnit, QString("%"));
        QCOMPARE(KnowledgeText::ClassifyColumn(steps, 0), EColumnKind::ORDINAL);
        QCOMPARE(KnowledgeText::ClassifyColumn(steps, 1), EColumnKind::QUANTITY);

        const KnowledgeTable& speed = doc.vecTables[1];
        QCOMPARE(speed.strX, QString("FPS"));
        QCOMPARE(speed.vecY, QStringList({ "mAP" }));
        QCOMPARE(speed.strSeries, DecodeUtf8("검출기"));
        QCOMPARE(speed.vecConceptIds, QStringList({ "yolov2" }));
        QCOMPARE(speed.vecColumns.size(), 4);
        QCOMPARE(speed.vecColumns[0].strName, DecodeUtf8("검출기"));
        QCOMPARE(speed.vecColumns[0].strUnit, QString());
        QCOMPARE(speed.vecColumns[1].strName, DecodeUtf8("입력 크기"));
        QCOMPARE(speed.vecColumns[1].strUnit, QString("px"));
        QCOMPARE(speed.vecColumns[2].strName, QString("mAP"));
        QCOMPARE(speed.vecColumns[2].strUnit, QString("%"));
        QCOMPARE(speed.vecColumns[3].strName, QString("FPS"));
        QCOMPARE(speed.vecColumns[3].strUnit, QString());
        QCOMPARE(speed.vecRows.size(), 7);
        QCOMPARE(KnowledgeText::ClassifyColumn(speed, 0), EColumnKind::NOMINAL);
        QCOMPARE(KnowledgeText::ClassifyColumn(speed, 1), EColumnKind::QUANTITY);
        QCOMPARE(KnowledgeText::ClassifyColumn(speed, 2), EColumnKind::QUANTITY);
        QCOMPARE(KnowledgeText::ClassifyColumn(speed, 3), EColumnKind::QUANTITY);
    }

    // 정준 예시 개념의 기록을 확인한다
    void exampleConceptRecords()
    {
        QByteArray baRaw;
        QVERIFY(ReadBytes(MakeExamplePath(QStringLiteral("concepts/yolov2.md")), &baRaw));
        const ConceptReadResult yolov2 = KnowledgeMarkdownReader::ReadConcept(QString::fromUtf8(baRaw));
        QVERIFY2(yolov2.bOk, qUtf8Printable(yolov2.strError));
        QCOMPARE(yolov2.concept.strKind, QString("method"));
        QCOMPARE(yolov2.concept.vecAliases, QStringList({ "YOLO9000" }));
        QCOMPARE(yolov2.concept.strYear, QString("2017"));
        QCOMPARE(yolov2.concept.vecRelations.size(), 1);
        const KnowledgeRelation& isA = yolov2.concept.vecRelations[0];
        QCOMPARE(isA.strKind, QString("isA"));
        QCOMPARE(isA.fromConcept.strConceptId, QString("yolov2"));
        QCOMPARE(isA.toConcept.strConceptId, DecodeUtf8("1단계-검출기"));
        QCOMPARE(isA.eStatus, ERelationStatus::CONFIRMED);

        QVERIFY(ReadBytes(MakeExamplePath(DecodeUtf8("concepts/객체-검출.md")), &baRaw));
        const ConceptReadResult task = KnowledgeMarkdownReader::ReadConcept(QString::fromUtf8(baRaw));
        QVERIFY2(task.bOk, qUtf8Printable(task.strError));
        QCOMPARE(task.concept.strKind, QString("task"));
        QVERIFY(task.concept.strYear.isEmpty());
    }
};

std::unique_ptr<QObject> CreateExampleWikiTest() { return std::make_unique<ExampleWikiTest>(); }

#include "ExampleWikiTest.moc"
