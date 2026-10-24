#pragma once

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QStringList>

// 보기 앱 시험이 함께 쓰는 것: showcase 경로, wiki 사본, 파일 쓰기 · 읽기
namespace ViewerTestSupport
{
inline QString GetShowcaseDir()
{
    return QStringLiteral(VISUALMARKDOWN_SHOWCASE_DIR);
}

// 저장소에 둔 스냅샷 기대 파일(tests/viewer/showcase-snapshot.tsv)
inline QString GetExpectedSnapshotList()
{
    return QStringLiteral(VISUALMARKDOWN_VIEWER_EXPECTED_TSV);
}

// wiki 의 documents/ · concepts/ 바로 아래 파일을 target 으로 복사한다
inline bool CopyWiki(const QString& source, const QString& target)
{
    for (const QString& strFolder : { QStringLiteral("documents"), QStringLiteral("concepts") })
    {
        const QDir dirSource(source + QLatin1Char('/') + strFolder);
        const QString strTarget = target + QLatin1Char('/') + strFolder;
        if (!QDir().mkpath(strTarget))
            return false;
        for (const QFileInfo& info : dirSource.entryInfoList(QDir::Files, QDir::Name))
            if (!QFile::copy(info.absoluteFilePath(), strTarget + QLatin1Char('/') + info.fileName()))
                return false;
    }
    return true;
}

// path 에 글을 UTF-8 로 쓴다(폴더가 없으면 만든다)
inline bool WriteText(const QString& path, const QString& text)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    return file.write(text.toUtf8()) >= 0;
}

inline bool ReadBytes(const QString& path, QByteArray* bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    *bytes = file.readAll();
    return true;
}
}
