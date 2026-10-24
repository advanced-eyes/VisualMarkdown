#include "VisualMarkdownViewer/VisualMarkdownViewerOptions.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerSnapshot.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerWindow.h"

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFont>
#include <QGuiApplication>
#include <QScreen>
#include <QStringList>
#include <QStyleFactory>

#include <cstdio>

namespace
{
#ifdef Q_OS_WIN
constexpr int kWindowsUiFontPointSize = 9;   // Windows 메시지 글꼴 크기(맑은 고딕 9pt)
#endif
constexpr double kScreenShare = 0.85;        // 크기를 주지 않은 창: 화면의 이만큼(기본 크기를 넘지 않게)

// 까닭은 표준 오류에 바로 쓴다(콘솔 실행 파일이다). 콘솔 글자표(로컬 8비트)로 쓴다
void PrintError(const QString& text)
{
    std::fprintf(stderr, "%s\n", text.toLocal8Bit().constData());
    std::fflush(stderr);
}

QStringList ReadArguments(int argc, char* argv[])
{
    QStringList vecArguments;
    for (int iArgument = 1; iArgument < argc; ++iArgument)
        vecArguments << QString::fromLocal8Bit(argv[iArgument]);
    return vecArguments;
}

int ReportBadArguments(const VisualMarkdownViewerOptions& options)
{
    PrintError(options.strError);
    PrintError(VisualMarkdownViewerOptions::GetUsage());
    return VisualMarkdownViewerExit::kBadArguments;
}
}

// VisualMarkdownViewer [wiki 폴더] [--snapshot <폴더>] [--theme light|dark] [--size 가로x세로] (design/Views.md §8)
int main(int argc, char* argv[])
{
    const QString strDefaultWiki = QStringLiteral(VISUALMARKDOWN_SHOWCASE_DIR);
    // 창 · 화면을 만들기 전에 인자를 본다. 잘못되면 화면 없이 끝난다
    const VisualMarkdownViewerOptions early = VisualMarkdownViewerOptions::Parse(ReadArguments(argc, argv), strDefaultWiki);
    if (!early.strError.isEmpty())
        return ReportBadArguments(early);

    // 스냅샷은 화면 없이 돈다. QApplication 을 만들기 전에 정해야 한다
    if (VisualMarkdownViewerOptions::HasSnapshotArgument(argc, argv))
    {
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
#ifdef Q_OS_WIN
        // Windows 의 offscreen 은 글꼴 폴더를 모른다. 정하지 않았으면 Windows 글꼴 폴더를 알려 준다(한글이 그려지게, tests/view/TestMain.cpp 와 같다)
        if (!qEnvironmentVariableIsSet("QT_QPA_FONTDIR") && qEnvironmentVariableIsSet("WINDIR"))
            qputenv("QT_QPA_FONTDIR", QDir::fromNativeSeparators(qEnvironmentVariable("WINDIR") + QStringLiteral("/Fonts")).toUtf8());
#endif
    }

    QApplication app(argc, argv);
    // 밝음 · 어둠 팔레트를 그대로 따르는 스타일(창과 스냅샷이 같다)
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
#ifdef Q_OS_WIN
    // Windows 의 offscreen 은 시스템 글꼴을 몰라 Qt 의 대체 기본값을 쓴다. 실제 windows 플랫폼처럼 맑은 고딕을 기본 글꼴로 둔다
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QApplication::setFont(QFont(QStringLiteral("Malgun Gothic"), kWindowsUiFontPointSize));
#endif

    // 같은 인자를 QApplication 이 준 글(Windows 에서 유니코드 경로가 맞다)로 다시 읽는다
    const VisualMarkdownViewerOptions options = VisualMarkdownViewerOptions::Parse(QCoreApplication::arguments().mid(1), strDefaultWiki);
    if (!options.strError.isEmpty())
        return ReportBadArguments(options);

    if (options.bSnapshot)
    {
        QStringList vecErrors;
        const int iExit = VisualMarkdownViewerSnapshot::Run(options, &vecErrors);
        for (const QString& strError : vecErrors)
            PrintError(strError);
        return iExit;
    }

    VisualMarkdownViewerWindow window;
    window.SetDarkTheme(options.bDark);
    QSize sizeWindow = options.size;
    const QScreen* pScreen = QGuiApplication::primaryScreen();
    if (!options.bSizeGiven && pScreen != nullptr)
        sizeWindow = sizeWindow.boundedTo(pScreen->availableGeometry().size() * kScreenShare);
    window.resize(sizeWindow);
    window.show();
    window.OpenWiki(options.strWikiRoot);   // 읽지 못하면 창에 알림을 띄운다
    return app.exec();
}
