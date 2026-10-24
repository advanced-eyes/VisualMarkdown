#include "VisualMarkdownViewer/VisualMarkdownViewerOptions.h"

#include <QTest>
#include <QTextCodec>
#include <QVector>

#include <memory>
#include <vector>

namespace
{
const QString kDefaultWiki = QStringLiteral("C:/default/wiki");
constexpr const char* kConsoleCodec = "CP949";   // 한국어 Windows 의 콘솔 글자표(시스템 코드 페이지에 기대지 않게 이름으로 고른다)
}

// 명령 줄(VisualMarkdownViewer [wiki] [--snapshot <폴더>] [--theme light|dark] [--size 가로x세로], design/Views.md §8)
class VisualMarkdownViewerOptionsTest : public QObject
{
    Q_OBJECT

private slots:
    // 인자가 없으면 기본 wiki 를 창으로 연다: 밝음 · 1600×1000
    void defaultsOpenShowcaseWindow()
    {
        const VisualMarkdownViewerOptions options = VisualMarkdownViewerOptions::Parse({}, kDefaultWiki);
        QCOMPARE(options.strError, QString());
        QCOMPARE(options.strWikiRoot, kDefaultWiki);
        QVERIFY(!options.bSnapshot);
        QVERIFY(!options.bDark);
        QCOMPARE(options.size, QSize(1600, 1000));
        QVERIFY(!options.bSizeGiven);
    }

    // wiki 폴더 · 스냅샷 폴더 · 테마 · 크기를 읽는다(순서는 상관없다)
    void parsesSnapshotOptions()
    {
        const VisualMarkdownViewerOptions options = VisualMarkdownViewerOptions::Parse(
            { QStringLiteral("--size"), QStringLiteral("800x600"), QStringLiteral("D:/위키"), QStringLiteral("--snapshot"), QStringLiteral("D:/out"),
              QStringLiteral("--theme"), QStringLiteral("dark") },
            kDefaultWiki);
        QCOMPARE(options.strError, QString());
        QCOMPARE(options.strWikiRoot, QStringLiteral("D:/위키"));
        QVERIFY(options.bSnapshot);
        QCOMPARE(options.strSnapshotDir, QStringLiteral("D:/out"));
        QVERIFY(options.bDark);
        QCOMPARE(options.size, QSize(800, 600));
        QVERIFY(options.bSizeGiven);

        const VisualMarkdownViewerOptions light = VisualMarkdownViewerOptions::Parse(
            { QStringLiteral("--snapshot"), QStringLiteral("out"), QStringLiteral("--theme"), QStringLiteral("light") }, kDefaultWiki);
        QCOMPARE(light.strError, QString());
        QCOMPARE(light.strWikiRoot, kDefaultWiki);
        QVERIFY(!light.bDark);
    }

    // 잘못된 인자는 까닭(그 인자를 담은 글)을 남긴다
    void rejectsBadArguments_data()
    {
        QTest::addColumn<QStringList>("arguments");
        QTest::addColumn<QString>("mention");
        QTest::newRow("unknown option") << QStringList { QStringLiteral("--bogus") } << QStringLiteral("--bogus");
        QTest::newRow("second wiki") << QStringList { QStringLiteral("a"), QStringLiteral("b") } << QStringLiteral("b");
        QTest::newRow("snapshot without folder") << QStringList { QStringLiteral("--snapshot") } << QStringLiteral("--snapshot");
        QTest::newRow("snapshot followed by option") << QStringList { QStringLiteral("--snapshot"), QStringLiteral("--theme"), QStringLiteral("dark") }
                                                     << QStringLiteral("--snapshot");
        QTest::newRow("snapshot twice") << QStringList { QStringLiteral("--snapshot"), QStringLiteral("a"), QStringLiteral("--snapshot"), QStringLiteral("b") }
                                        << QStringLiteral("--snapshot");
        QTest::newRow("theme without value") << QStringList { QStringLiteral("--theme") } << QStringLiteral("--theme");
        QTest::newRow("unknown theme") << QStringList { QStringLiteral("--theme"), QStringLiteral("blue") } << QStringLiteral("blue");
        QTest::newRow("size without value") << QStringList { QStringLiteral("--size") } << QStringLiteral("--size");
        QTest::newRow("size without x") << QStringList { QStringLiteral("--size"), QStringLiteral("1600") } << QStringLiteral("1600");
        QTest::newRow("size not a number") << QStringList { QStringLiteral("--size"), QStringLiteral("wide x tall") } << QStringLiteral("wide x tall");
        QTest::newRow("size zero") << QStringList { QStringLiteral("--size"), QStringLiteral("0x600") } << QStringLiteral("0x600");
        QTest::newRow("size negative") << QStringList { QStringLiteral("--size"), QStringLiteral("-800x600") } << QStringLiteral("-800x600");
        QTest::newRow("size too big") << QStringList { QStringLiteral("--size"), QStringLiteral("100000x600") } << QStringLiteral("100000x600");
        QTest::newRow("qt option") << QStringList { QStringLiteral("-platform"), QStringLiteral("minimal") } << QStringLiteral("-platform");
    }

    void rejectsBadArguments()
    {
        QFETCH(QStringList, arguments);
        QFETCH(QString, mention);
        const VisualMarkdownViewerOptions options = VisualMarkdownViewerOptions::Parse(arguments, kDefaultWiki);
        QVERIFY2(!options.strError.isEmpty(), qUtf8Printable(arguments.join(QLatin1Char(' '))));
        QVERIFY2(options.strError.contains(mention), qUtf8Printable(options.strError));
    }

    // 보기 앱은 인자 오류 글과 사용법을 콘솔 글자표(toLocal8Bit)로 표준 오류에 쓴다. 한국어 Windows(CP949)에서 글자를 잃지 않게
    // 인자 오류의 모든 갈래와 사용법이 CP949 로 바뀌어야 한다('?' 로 찍히는 글자가 없다)
    void errorTextsKeepInConsoleCodePage()
    {
        const QTextCodec* pCodec = QTextCodec::codecForName(kConsoleCodec);
        QVERIFY(pCodec != nullptr);
        const QVector<QStringList> vecBadArguments {
            { QStringLiteral("--bogus") },                                                                       // 모르는 인자
            { QStringLiteral("a"), QStringLiteral("b") },                                                        // wiki 폴더 둘
            { QStringLiteral("--snapshot") },                                                                    // 값이 없다
            { QStringLiteral("--snapshot"), QStringLiteral("a"), QStringLiteral("--snapshot"), QStringLiteral("b") },   // 두 번
            { QStringLiteral("--theme"), QStringLiteral("dark"), QStringLiteral("--theme"), QStringLiteral("light") },
            { QStringLiteral("--size"), QStringLiteral("800x600"), QStringLiteral("--size"), QStringLiteral("800x600") },
            { QStringLiteral("--theme"), QStringLiteral("blue") },                                               // 모르는 테마
            { QStringLiteral("--size"), QStringLiteral("0x600") },                                               // 한 변이 범위 밖
            { QStringLiteral("--size"), QStringLiteral("wide") },                                                // 가로x세로 꼴이 아니다
        };
        QStringList vecTexts { VisualMarkdownViewerOptions::GetUsage() };
        for (const QStringList& vecArguments : vecBadArguments)
        {
            const VisualMarkdownViewerOptions options = VisualMarkdownViewerOptions::Parse(vecArguments, kDefaultWiki);
            QVERIFY2(!options.strError.isEmpty(), qUtf8Printable(vecArguments.join(QLatin1Char(' '))));
            vecTexts << options.strError;
        }
        for (const QString& strText : vecTexts)
            QVERIFY2(pCodec->canEncode(strText), qUtf8Printable(strText));
    }

    // QApplication 전에 스냅샷 모드를 안다(offscreen 을 정하려고)
    void detectsSnapshotBeforeApplication()
    {
        QByteArray baProgram("VisualMarkdownViewer");
        QByteArray baWiki("wiki");
        QByteArray baSnapshot("--snapshot");
        QByteArray baOut("out");
        std::vector<char*> vecWith { baProgram.data(), baWiki.data(), baSnapshot.data(), baOut.data() };
        std::vector<char*> vecWithout { baProgram.data(), baWiki.data() };
        QVERIFY(VisualMarkdownViewerOptions::HasSnapshotArgument(static_cast<int>(vecWith.size()), vecWith.data()));
        QVERIFY(!VisualMarkdownViewerOptions::HasSnapshotArgument(static_cast<int>(vecWithout.size()), vecWithout.data()));
        QVERIFY(VisualMarkdownViewerOptions::GetUsage().contains(QStringLiteral("--snapshot")));
    }
};

std::unique_ptr<QObject> CreateVisualMarkdownViewerOptionsTest() { return std::make_unique<VisualMarkdownViewerOptionsTest>(); }

#include "VisualMarkdownViewerOptionsTest.moc"
