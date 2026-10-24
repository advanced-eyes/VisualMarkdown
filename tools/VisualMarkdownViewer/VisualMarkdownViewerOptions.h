#pragma once

#include <QSize>
#include <QString>
#include <QStringList>

// 보기 앱의 끝 코드. 0 이 아니면 까닭을 표준 오류에 쓴다
namespace VisualMarkdownViewerExit
{
constexpr int kOk = 0;
constexpr int kBadArguments = 2;   // 모르는 인자, 잘못된 --size · --theme
constexpr int kWikiNotRead = 3;    // wiki 를 읽지 못했다
constexpr int kWriteFailed = 4;    // 스냅샷 파일을 하나라도 쓰지 못했다
}

// 보기 앱의 명령 줄(design/Views.md §8): VisualMarkdownViewer [wiki 폴더] [--snapshot <폴더>] [--theme light|dark] [--size 가로x세로]
// --theme · --size 는 창으로 열 때도 처음 테마 · 창 크기로 쓴다. QtCore 만 쓴다.
struct VisualMarkdownViewerOptions
{
public:
    // 프로그램 이름을 뺀 인자를 읽는다. wiki 폴더를 주지 않으면 defaultWikiRoot 다. 잘못되면 strError 에 까닭을 쓴다
    static VisualMarkdownViewerOptions Parse(const QStringList& arguments, const QString& defaultWikiRoot);
    // QApplication 을 만들기 전에 스냅샷 모드인지 본다(그 전에 offscreen 을 정해야 한다)
    static bool HasSnapshotArgument(int argc, char* argv[]);
    static QString GetUsage();

public:
    static constexpr int kDefaultWidth = 1600;
    static constexpr int kDefaultHeight = 1000;
    static constexpr int kMaxSide = 8192;   // --size 한 변의 가장 큰 값(그림 하나가 256MB 를 넘지 않게)

public:
    QString strWikiRoot;
    bool bSnapshot = false;
    QString strSnapshotDir;
    bool bDark = false;
    QSize size = QSize(kDefaultWidth, kDefaultHeight);
    bool bSizeGiven = false;
    QString strError;   // 비지 않으면 인자가 잘못됐다
};
