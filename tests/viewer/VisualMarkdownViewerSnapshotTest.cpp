#include "VisualMarkdown/UI/VisualMarkdownTheme.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerModel.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerSnapshot.h"
#include "ViewerTestSupport.h"

#include <QDir>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCodec>

#include <algorithm>
#include <memory>

namespace
{
constexpr int kGraphFiles = 6;
constexpr int kChartFiles = 12;
const QSize kSmallSize(800, 500);
constexpr const char* kConsoleCodec = "CP949";   // 한국어 Windows 의 콘솔 글자표(시스템 코드 페이지에 기대지 않게 이름으로 고른다)

VisualMarkdownViewerOptions MakeOptions(const QString& wiki, const QString& folder)
{
    VisualMarkdownViewerOptions options;
    options.strWikiRoot = wiki;
    options.bSnapshot = true;
    options.strSnapshotDir = folder;
    return options;
}

// 기대 목록(tests/viewer/showcase-snapshot.tsv)의 파일 이름 열(머리 줄 다음부터)
QStringList ListExpectedFiles()
{
    QByteArray baExpected;
    if (!ViewerTestSupport::ReadBytes(ViewerTestSupport::GetExpectedSnapshotList(), &baExpected))
        return {};
    QStringList vecFiles;
    const QStringList vecLines = QString::fromUtf8(baExpected).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (int iLine = 1; iLine < vecLines.size(); ++iLine)
        vecFiles << vecLines[iLine].section(QLatin1Char('\t'), 0, 0);
    return vecFiles;
}

QStringList ListPngFiles(const QString& folder)
{
    return QDir(folder).entryList({ QStringLiteral("*.png") }, QDir::Files, QDir::Name);
}
}

// 스냅샷 모드(VisualMarkdownViewerSnapshot)를 showcase 로 시험한다(offscreen). 실행 파일 그대로의 시험은 ctest VisualMarkdownViewerSnapshotTest(cmake -P)다
class VisualMarkdownViewerSnapshotTest : public QObject
{
    Q_OBJECT

private slots:
    // 파일 이름: graph-<보기>.png, chart-DD-TT.png(1부터 두 자리), snapshot.tsv
    void namesFiles()
    {
        const QStringList vecGraphs { "graph-network.png", "graph-lineage.png", "graph-tree.png", "graph-timeline.png", "graph-matrix.png", "graph-bipartite.png" };
        QStringList vecNames;
        for (const EGraphLayoutType eType : VisualMarkdownViewerModel::ListViews())
            vecNames << VisualMarkdownViewerSnapshot::MakeGraphFileName(eType);
        QCOMPARE(vecNames, vecGraphs);
        QCOMPARE(VisualMarkdownViewerSnapshot::MakeChartFileName(0, 0), QStringLiteral("chart-01-01.png"));
        QCOMPARE(VisualMarkdownViewerSnapshot::MakeChartFileName(4, 2), QStringLiteral("chart-05-03.png"));
        QCOMPARE(VisualMarkdownViewerSnapshot::MakeChartFileName(11, 9), QStringLiteral("chart-12-10.png"));
        QCOMPARE(VisualMarkdownViewerSnapshot::GetListFileName(), QStringLiteral("snapshot.tsv"));
    }

    // showcase 를 찍으면 0 으로 끝나고 PNG 18개(그래프 6 + 차트 12)와 snapshot.tsv(기대 파일과 바이트로 같다)를 남긴다. 그림은 1600 × 1000(기본)이다
    void writesEveryViewAndChart()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString strFolder = dir.filePath(QStringLiteral("light"));
        QStringList vecErrors;
        QCOMPARE(VisualMarkdownViewerSnapshot::Run(MakeOptions(ViewerTestSupport::GetShowcaseDir(), strFolder), &vecErrors), VisualMarkdownViewerExit::kOk);
        QVERIFY2(vecErrors.isEmpty(), qUtf8Printable(vecErrors.join(QLatin1Char('\n'))));

        QStringList vecExpected = ListExpectedFiles();
        QCOMPARE(vecExpected.size(), kGraphFiles + kChartFiles);
        const QStringList vecPngs = ListPngFiles(strFolder);
        QCOMPARE(vecPngs.size(), kGraphFiles + kChartFiles);
        std::sort(vecExpected.begin(), vecExpected.end());
        QCOMPARE(vecPngs, vecExpected);
        const VisualMarkdownTheme light = VisualMarkdownTheme::MakeLight();
        for (const QString& strPng : vecPngs)
        {
            const QImage image(QDir(strFolder).filePath(strPng));
            QVERIFY2(!image.isNull(), qUtf8Printable(strPng));
            QCOMPARE(image.size(), QSize(VisualMarkdownViewerOptions::kDefaultWidth, VisualMarkdownViewerOptions::kDefaultHeight));
            QVERIFY2(image.pixelColor(1, 1) == light.background, qUtf8Printable(strPng + QLatin1Char(' ') + image.pixelColor(1, 1).name()));
        }

        QByteArray baList;
        QByteArray baExpectedList;
        QVERIFY(ViewerTestSupport::ReadBytes(QDir(strFolder).filePath(VisualMarkdownViewerSnapshot::GetListFileName()), &baList));
        QVERIFY(ViewerTestSupport::ReadBytes(ViewerTestSupport::GetExpectedSnapshotList(), &baExpectedList));
        QVERIFY2(baList == baExpectedList, qUtf8Printable(QString::fromUtf8(baList)));
    }

    // 어둠 · 다른 크기: 그림이 그 크기이고 바탕이 어둠 테마 바탕이다. 목록은 같다. 같은 입력이면 같은 그림이다
    void darkThemeAndSizeAreDeterministic()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QStringList vecFolders { dir.filePath(QStringLiteral("dark-1")), dir.filePath(QStringLiteral("dark-2")) };
        for (const QString& strFolder : vecFolders)
        {
            VisualMarkdownViewerOptions options = MakeOptions(ViewerTestSupport::GetShowcaseDir(), strFolder);
            options.bDark = true;
            options.size = kSmallSize;
            QStringList vecErrors;
            QCOMPARE(VisualMarkdownViewerSnapshot::Run(options, &vecErrors), VisualMarkdownViewerExit::kOk);
            QVERIFY2(vecErrors.isEmpty(), qUtf8Printable(vecErrors.join(QLatin1Char('\n'))));
        }
        const VisualMarkdownTheme dark = VisualMarkdownTheme::MakeDark();
        const QStringList vecPngs = ListPngFiles(vecFolders[0]);
        QCOMPARE(vecPngs.size(), kGraphFiles + kChartFiles);
        QCOMPARE(ListPngFiles(vecFolders[1]), vecPngs);
        for (const QString& strPng : vecPngs)
        {
            const QImage image(QDir(vecFolders[0]).filePath(strPng));
            QCOMPARE(image.size(), kSmallSize);
            QVERIFY2(image.pixelColor(1, 1) == dark.background, qUtf8Printable(strPng + QLatin1Char(' ') + image.pixelColor(1, 1).name()));
            const QImage again(QDir(vecFolders[1]).filePath(strPng));
            QVERIFY2(again.convertToFormat(QImage::Format_ARGB32) == image.convertToFormat(QImage::Format_ARGB32), qUtf8Printable(strPng));
        }
        QByteArray baList;
        QByteArray baExpectedList;
        QVERIFY(ViewerTestSupport::ReadBytes(QDir(vecFolders[0]).filePath(VisualMarkdownViewerSnapshot::GetListFileName()), &baList));
        QVERIFY(ViewerTestSupport::ReadBytes(ViewerTestSupport::GetExpectedSnapshotList(), &baExpectedList));
        QCOMPARE(baList, baExpectedList);
    }

    // wiki 를 읽지 못하면 0 이 아닌 코드와 까닭(그 경로)을 남기고 아무것도 쓰지 않는다
    void failsWhenWikiCannotBeRead()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString strMissing = dir.filePath(QStringLiteral("no-such-wiki"));
        const QString strFolder = dir.filePath(QStringLiteral("out"));
        QStringList vecErrors;
        QCOMPARE(VisualMarkdownViewerSnapshot::Run(MakeOptions(strMissing, strFolder), &vecErrors), VisualMarkdownViewerExit::kWikiNotRead);
        QVERIFY(!vecErrors.isEmpty());
        QVERIFY2(vecErrors.join(QLatin1Char('\n')).contains(strMissing), qUtf8Printable(vecErrors.join(QLatin1Char('\n'))));
        QVERIFY(!QFileInfo::exists(strFolder));
    }

    // 파일 하나라도 못 쓰면 0 이 아니다(그 파일 이름을 까닭에 남긴다). 나머지 파일은 쓴다
    void failsWhenAFileCannotBeWritten()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString strFolder = dir.filePath(QStringLiteral("blocked"));
        QVERIFY(QDir().mkpath(QDir(strFolder).filePath(QStringLiteral("graph-network.png"))));   // PNG 자리에 폴더가 있다
        QStringList vecErrors;
        QCOMPARE(VisualMarkdownViewerSnapshot::Run(MakeOptions(ViewerTestSupport::GetShowcaseDir(), strFolder), &vecErrors),
                 VisualMarkdownViewerExit::kWriteFailed);
        QCOMPARE(vecErrors.size(), 1);
        QVERIFY2(vecErrors[0].contains(QStringLiteral("graph-network.png")), qUtf8Printable(vecErrors[0]));
        QVERIFY(QFileInfo(QDir(strFolder).filePath(QStringLiteral("graph-lineage.png"))).isFile());
        QVERIFY(QFileInfo(QDir(strFolder).filePath(QStringLiteral("chart-06-01.png"))).isFile());
        QVERIFY(QFileInfo(QDir(strFolder).filePath(VisualMarkdownViewerSnapshot::GetListFileName())).isFile());
    }

    // 스냅샷 폴더를 만들지 못하면(그 자리에 파일이 있다) 0 이 아니고 그 경로를 까닭에 남긴다
    void failsWhenFolderCannotBeMade()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString strFile = dir.filePath(QStringLiteral("file"));
        QVERIFY(ViewerTestSupport::WriteText(strFile, QStringLiteral("not a folder")));
        QStringList vecErrors;
        QCOMPARE(VisualMarkdownViewerSnapshot::Run(MakeOptions(ViewerTestSupport::GetShowcaseDir(), strFile), &vecErrors),
                 VisualMarkdownViewerExit::kWriteFailed);
        QVERIFY2(vecErrors.join(QLatin1Char('\n')).contains(strFile), qUtf8Printable(vecErrors.join(QLatin1Char('\n'))));
    }

    // 보기 앱은 스냅샷 실패 까닭을 콘솔 글자표(toLocal8Bit)로 표준 오류에 쓴다. 한국어 Windows(CP949)에서 글자를 잃지 않게
    // 실패 까닭의 모든 갈래(wiki 폴더 없음 · documents/concepts 없음 · 폴더를 못 만듦 · 파일을 못 씀)가 CP949 로 바뀌어야 한다
    void errorTextsKeepInConsoleCodePage()
    {
        const QTextCodec* pCodec = QTextCodec::codecForName(kConsoleCodec);
        QVERIFY(pCodec != nullptr);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QStringList vecErrors;
        QCOMPARE(VisualMarkdownViewerSnapshot::Run(MakeOptions(dir.filePath(QStringLiteral("no-such-wiki")), dir.filePath(QStringLiteral("out-1"))), &vecErrors),
                 VisualMarkdownViewerExit::kWikiNotRead);
        const QString strEmptyWiki = dir.filePath(QStringLiteral("empty-wiki"));
        QVERIFY(QDir().mkpath(strEmptyWiki));
        QCOMPARE(VisualMarkdownViewerSnapshot::Run(MakeOptions(strEmptyWiki, dir.filePath(QStringLiteral("out-2"))), &vecErrors),
                 VisualMarkdownViewerExit::kWikiNotRead);
        const QString strFile = dir.filePath(QStringLiteral("file"));
        QVERIFY(ViewerTestSupport::WriteText(strFile, QStringLiteral("not a folder")));
        QCOMPARE(VisualMarkdownViewerSnapshot::Run(MakeOptions(ViewerTestSupport::GetShowcaseDir(), strFile), &vecErrors),
                 VisualMarkdownViewerExit::kWriteFailed);
        const QString strBlocked = dir.filePath(QStringLiteral("blocked"));
        QVERIFY(QDir().mkpath(QDir(strBlocked).filePath(QStringLiteral("graph-network.png"))));   // PNG 자리에 폴더가 있다
        QCOMPARE(VisualMarkdownViewerSnapshot::Run(MakeOptions(ViewerTestSupport::GetShowcaseDir(), strBlocked), &vecErrors),
                 VisualMarkdownViewerExit::kWriteFailed);
        QCOMPARE(vecErrors.size(), 4);
        for (const QString& strError : vecErrors)
            QVERIFY2(pCodec->canEncode(strError), qUtf8Printable(strError));
    }
};

std::unique_ptr<QObject> CreateVisualMarkdownViewerSnapshotTest() { return std::make_unique<VisualMarkdownViewerSnapshotTest>(); }

#include "VisualMarkdownViewerSnapshotTest.moc"
