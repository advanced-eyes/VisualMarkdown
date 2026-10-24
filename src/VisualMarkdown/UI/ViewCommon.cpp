#include "VisualMarkdown/UI/ViewCommon.h"

#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <QHelpEvent>
#include <QPainter>
#include <QRectF>
#include <QToolTip>

#include <algorithm>

namespace ViewCommon
{
namespace
{
constexpr int kMinPixelSize = 1;   // QFont::setPixelSize 는 0 보다 커야 한다
constexpr double kPanelRadius = 8;
}

double Clamp(double value, double low, double high)
{
    return std::max(low, std::min(high, value));
}

QFont MakeFont(const QFont& base, const VisualMarkdownTheme& theme, int pixelSize, bool bold)
{
    QFont fontResult = base;
    if (!theme.strFontFamily.isEmpty())
        fontResult.setFamily(theme.strFontFamily);
    fontResult.setPixelSize(std::max(kMinPixelSize, pixelSize));
    fontResult.setBold(bold);
    return fontResult;
}

QString GetStatusLabel(ERelationStatus status)
{
    return KnowledgeVocabulary::Get().ToStatusLabel(status == ERelationStatus::CONFIRMED ? QStringLiteral("confirmed") : QStringLiteral("proposed"));
}

void ShowToolTip(QWidget* widget, QHelpEvent* event, const QString& tip)
{
    if (tip.isEmpty())
    {
        QToolTip::hideText();
        event->ignore();
        return;
    }
    QToolTip::showText(event->globalPos(), tip, widget);
}

void DrawPanel(QPainter& painter, const QRectF& rect, const VisualMarkdownTheme& theme)
{
    // 1px 테두리가 화소 가운데를 지나게 반 화소 들여 그린다
    painter.setPen(QPen(theme.panelBorder, 1));
    painter.setBrush(theme.panelBackground);
    painter.drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), kPanelRadius, kPanelRadius);
}
}
