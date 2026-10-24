#include "VisualMarkdownViewer/VisualMarkdownViewerPalette.h"

#include <QColor>
#include <QString>

namespace
{
constexpr int kOpaqueAlpha = 255;
const QString kHighlightKind = QStringLiteral("method");   // 고른 항목: 방법 점의 채움 색(밝음 · 어둠 모두 글자와 대비가 크다)

QColor MakeOpaque(QColor color)
{
    color.setAlpha(kOpaqueAlpha);
    return color;
}
}

namespace VisualMarkdownViewerPalette
{
QPalette Make(const VisualMarkdownTheme& theme)
{
    // 단추 · 바탕 색으로 만들면 테두리 · 그림자 색(Light · Mid · Dark …)이 그 밝기에 맞게 나온다. 글자 · 바탕 색은 테마 그대로 덮는다
    const QColor panel = MakeOpaque(theme.panelBackground);
    QPalette palette(panel, theme.background);
    palette.setColor(QPalette::Window, theme.background);
    palette.setColor(QPalette::WindowText, theme.text);
    palette.setColor(QPalette::Base, theme.background);
    palette.setColor(QPalette::AlternateBase, panel);
    palette.setColor(QPalette::Text, theme.text);
    palette.setColor(QPalette::Button, panel);
    palette.setColor(QPalette::ButtonText, theme.text);
    palette.setColor(QPalette::ToolTipBase, panel);
    palette.setColor(QPalette::ToolTipText, theme.text);
    palette.setColor(QPalette::PlaceholderText, theme.mutedText);
    palette.setColor(QPalette::Highlight, theme.GetConceptColors(kHighlightKind).fill);
    palette.setColor(QPalette::HighlightedText, theme.text);
    palette.setColor(QPalette::Link, theme.GetConceptColors(kHighlightKind).border);
    for (const QPalette::ColorRole eRole : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
        palette.setColor(QPalette::Disabled, eRole, theme.mutedText);
    return palette;
}
}
