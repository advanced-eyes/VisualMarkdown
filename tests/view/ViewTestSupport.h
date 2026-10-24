#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/Algorithm/GraphLayout/KnowledgeGraphLayout.h"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QImage>
#include <QMouseEvent>
#include <QRectF>
#include <QString>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>

// 위젯 시험이 함께 쓰는 것: 그리기 · 잉크 세기 · PNG 남기기 · 색 견주기 · 마우스 보내기 · 배치 · 실패 글
namespace ViewTestSupport
{
constexpr int kSnapshotWidth = 1600;      // 시험 그림 크기(design 요청: 1600 × 1000)
constexpr int kSnapshotHeight = 1000;
constexpr double kMinInkRatio = 0.005;    // 바탕 색이 아닌 점이 전체의 0.5% 넘게 있어야 비지 않은 그림이다
constexpr int kWheelNotch = 120;          // 휠 한 칸
constexpr const char* kSnapshotVariable = "VISUALMARKDOWN_SNAPSHOT_DIR";   // 정하면 그 폴더에 PNG 를 남긴다

// WCAG 2 상대 휘도(sRGB): 채널 값 → 선형 값 → 휘도. 대비는 (밝은 휘도 + 0.05) / (어두운 휘도 + 0.05)
constexpr double kChannelMax = 255.0;
constexpr double kLinearThreshold = 0.04045;
constexpr double kLinearSlope = 12.92;
constexpr double kGammaOffset = 0.055;
constexpr double kGammaScale = 1.055;
constexpr double kGamma = 2.4;
constexpr double kRedWeight = 0.2126;
constexpr double kGreenWeight = 0.7152;
constexpr double kBlueWeight = 0.0722;
constexpr double kContrastOffset = 0.05;

// 위젯을 그 크기의 그림으로 그린다(화면에 띄우지 않는다)
inline QImage Render(QWidget& widget)
{
    QImage image(widget.size(), QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    widget.render(&image);
    return image;
}

// 바탕 색과 다른 점의 수
inline int CountInk(const QImage& image, const QColor& background)
{
    const QRgb rgbBackground = background.rgba();
    int iInk = 0;
    for (int iY = 0; iY < image.height(); ++iY)
    {
        const QRgb* pLine = reinterpret_cast<const QRgb*>(image.constScanLine(iY));
        for (int iX = 0; iX < image.width(); ++iX)
            if (pLine[iX] != rgbBackground)
                ++iInk;
    }
    return iInk;
}

inline bool HasEnoughInk(const QImage& image, const QColor& background)
{
    return CountInk(image, background) > kMinInkRatio * image.width() * image.height();
}

// VISUALMARKDOWN_SNAPSHOT_DIR 를 정했을 때만 <폴더>/<name>.png 로 남긴다. 정하지 않으면 아무것도 쓰지 않고 true 다
inline bool SaveSnapshot(const QImage& image, const QString& name)
{
    const QString strDir = qEnvironmentVariable(kSnapshotVariable);
    if (strDir.isEmpty())
        return true;
    if (!QDir().mkpath(strDir))
        return false;
    return image.save(QDir(strDir).filePath(name + QStringLiteral(".png")), "PNG");
}

// 두 색이 얼마나 다른지(RGB 거리)
inline double GetColorDistance(const QColor& a, const QColor& b)
{
    const double dRed = a.red() - b.red();
    const double dGreen = a.green() - b.green();
    const double dBlue = a.blue() - b.blue();
    return std::sqrt(dRed * dRed + dGreen * dGreen + dBlue * dBlue);
}

// WCAG 2 휘도 · 대비. 테마 시험(글자 · 선 · 채움 위 글자)과 차트 시험(히트맵 칸 글)이 함께 쓴다
inline double ToLinear(int channel)
{
    const double dValue = channel / kChannelMax;
    return dValue <= kLinearThreshold ? dValue / kLinearSlope : std::pow((dValue + kGammaOffset) / kGammaScale, kGamma);
}

inline double GetLuminance(const QColor& color)
{
    return kRedWeight * ToLinear(color.red()) + kGreenWeight * ToLinear(color.green()) + kBlueWeight * ToLinear(color.blue());
}

inline double GetContrast(const QColor& a, const QColor& b)
{
    const double dLighter = std::max(GetLuminance(a), GetLuminance(b));
    const double dDarker = std::min(GetLuminance(a), GetLuminance(b));
    return (dLighter + kContrastOffset) / (dDarker + kContrastOffset);
}

// 그림의 rect 안에 그 색과 똑같은 점이 하나라도 있는지(범례 견본 찾기)
inline bool HasColorIn(const QImage& image, const QRect& rect, const QColor& color)
{
    const QRect rectInside = rect.intersected(image.rect());
    const QRgb rgbColor = color.rgba();
    for (int iY = rectInside.top(); iY <= rectInside.bottom(); ++iY)
    {
        const QRgb* pLine = reinterpret_cast<const QRgb*>(image.constScanLine(iY));
        for (int iX = rectInside.left(); iX <= rectInside.right(); ++iX)
            if (pLine[iX] == rgbColor)
                return true;
    }
    return false;
}

// 마우스 이벤트를 바로 보낸다(눌린 단추를 함께 실어 끌기를 흉내 낸다)
inline void SendMouse(QWidget& widget, QEvent::Type type, const QPoint& pos, Qt::MouseButton button, Qt::MouseButtons buttons)
{
    QMouseEvent event(type, QPointF(pos), QPointF(widget.mapToGlobal(pos)), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&widget, &event);
}

// 휠을 notches 칸 돌린다(양수는 위로: 확대)
inline void SendWheel(QWidget& widget, const QPoint& pos, int notches)
{
    QWheelEvent event(QPointF(pos), QPointF(widget.mapToGlobal(pos)), QPoint(), QPoint(0, notches * kWheelNotch), Qt::NoButton, Qt::NoModifier,
                      Qt::NoScrollPhase, false);
    QApplication::sendEvent(&widget, &event);
}

inline bool RunLayout(const KnowledgeGraph& graph, EGraphLayoutType type, GraphLayoutOutput* out)
{
    KnowledgeGraphLayout layout;
    layout.SetStrategy(type);
    GraphLayoutInput in;
    in.pGraph = &graph;
    return layout.Execute(in, *out);
}

inline QString GetLayoutName(EGraphLayoutType type)
{
    switch (type)
    {
    case EGraphLayoutType::NETWORK: return QStringLiteral("network");
    case EGraphLayoutType::LINEAGE: return QStringLiteral("lineage");
    case EGraphLayoutType::TREE: return QStringLiteral("tree");
    case EGraphLayoutType::TIMELINE: return QStringLiteral("timeline");
    case EGraphLayoutType::MATRIX: return QStringLiteral("matrix");
    case EGraphLayoutType::BIPARTITE: return QStringLiteral("bipartite");
    case EGraphLayoutType::EGO: return QStringLiteral("ego");
    }
    return QStringLiteral("unknown");
}

// 실패 글에 넣는 사각형: (x, y w×h)
inline QString DescribeRect(const QRectF& rect)
{
    return QStringLiteral("(%1, %2 %3×%4)").arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height());
}
}
