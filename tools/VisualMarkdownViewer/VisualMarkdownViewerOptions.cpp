#include "VisualMarkdownViewer/VisualMarkdownViewerOptions.h"

#include <QRegularExpression>
#include <QRegularExpressionMatch>

#include <cstring>

namespace
{
const QString kSnapshotOption = QStringLiteral("--snapshot");
const QString kThemeOption = QStringLiteral("--theme");
const QString kSizeOption = QStringLiteral("--size");
const QString kLightTheme = QStringLiteral("light");
const QString kDarkTheme = QStringLiteral("dark");
const QString kOptionPrefix = QStringLiteral("--");
constexpr const char* kSnapshotArgument = "--snapshot";
}

VisualMarkdownViewerOptions VisualMarkdownViewerOptions::Parse(const QStringList& arguments, const QString& defaultWikiRoot)
{
    VisualMarkdownViewerOptions options;
    bool bWikiGiven = false;
    bool bThemeGiven = false;
    for (int iArgument = 0; iArgument < arguments.size() && options.strError.isEmpty(); ++iArgument)
    {
        const QString& strArgument = arguments[iArgument];
        const bool bValueOption = strArgument == kSnapshotOption || strArgument == kThemeOption || strArgument == kSizeOption;
        if (!bValueOption)
        {
            if (strArgument.startsWith(QLatin1Char('-')))
                options.strError = QStringLiteral("알 수 없는 인자: %1").arg(strArgument);
            else if (bWikiGiven)
                options.strError = QStringLiteral("알 수 없는 인자: %1 (wiki 폴더는 하나만 준다)").arg(strArgument);
            else
            {
                bWikiGiven = true;
                options.strWikiRoot = strArgument;
            }
            continue;
        }

        // 값을 받는 인자: 값이 없거나 다음 인자가 또 옵션이면 잘못이다
        if (iArgument + 1 >= arguments.size() || arguments[iArgument + 1].startsWith(kOptionPrefix) || arguments[iArgument + 1].isEmpty())
        {
            options.strError = QStringLiteral("%1 뒤에 값이 없다").arg(strArgument);
            break;
        }
        const QString strValue = arguments[++iArgument];
        if (strArgument == kSnapshotOption)
        {
            if (options.bSnapshot)
                options.strError = QStringLiteral("%1 을 두 번 줬다").arg(strArgument);
            options.bSnapshot = true;
            options.strSnapshotDir = strValue;
        }
        else if (strArgument == kThemeOption)
        {
            if (bThemeGiven)
                options.strError = QStringLiteral("%1 을 두 번 줬다").arg(strArgument);
            else if (strValue == kLightTheme || strValue == kDarkTheme)
                options.bDark = strValue == kDarkTheme;
            else
                options.strError = QStringLiteral("잘못된 %1 값: %2 (light 또는 dark)").arg(strArgument, strValue);
            bThemeGiven = true;
        }
        else
        {
            static const QRegularExpression reSize(QStringLiteral("^([0-9]{1,6})x([0-9]{1,6})$"));
            const QRegularExpressionMatch match = reSize.match(strValue);
            const int iWidth = match.hasMatch() ? match.captured(1).toInt() : 0;
            const int iHeight = match.hasMatch() ? match.captured(2).toInt() : 0;
            if (options.bSizeGiven)
                options.strError = QStringLiteral("%1 을 두 번 줬다").arg(strArgument);
            else if (iWidth < 1 || iHeight < 1 || iWidth > kMaxSide || iHeight > kMaxSide)
                options.strError = QStringLiteral("잘못된 %1 값: %2 (가로x세로, 한 변은 1-%3, 예: 1600x1000)").arg(strArgument, strValue, QString::number(kMaxSide));
            options.size = QSize(iWidth, iHeight);
            options.bSizeGiven = true;
        }
    }
    if (!bWikiGiven)
        options.strWikiRoot = defaultWikiRoot;
    return options;
}

bool VisualMarkdownViewerOptions::HasSnapshotArgument(int argc, char* argv[])
{
    for (int iArgument = 1; iArgument < argc; ++iArgument)
        if (argv[iArgument] != nullptr && std::strcmp(argv[iArgument], kSnapshotArgument) == 0)
            return true;
    return false;
}

QString VisualMarkdownViewerOptions::GetUsage()
{
    return QStringLiteral(
        "사용: VisualMarkdownViewer [wiki 폴더] [--snapshot <폴더>] [--theme light|dark] [--size 가로x세로]\n"
        "  wiki 폴더   documents/ · concepts/ 가 있는 폴더. 주지 않으면 examples/showcase/wiki\n"
        "  --snapshot  보기 6개와 표마다의 차트를 그 폴더에 PNG 로 남기고 끝난다(목록 snapshot.tsv, 창을 띄우지 않는다)\n"
        "  --theme     light(기본) 또는 dark\n"
        "  --size      그림 크기(창으로 열 때는 창 크기). 기본 1600x1000");
}
