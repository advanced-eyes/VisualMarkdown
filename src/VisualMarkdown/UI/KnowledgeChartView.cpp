#include "VisualMarkdown/UI/KnowledgeChartView.h"

#include "VisualMarkdown/UI/ViewCommon.h"

#include <QEvent>
#include <QFontMetricsF>
#include <QHelpEvent>
#include <QLineF>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr int kMargin = 20;
constexpr double kTitleGap = 4;            // 제목 · 밑글 사이
constexpr double kSectionGap = 14;         // 제목 덩어리 · 범례 · 그림 칸 사이
constexpr double kTickGap = 8;             // 눈금 글과 그림 칸 사이
constexpr double kTickLabelWidth = 200;    // 눈금 글 칸 너비(가로 눈금 글은 그 가운데에 쓴다)
constexpr double kAxisTitleGap = 8;        // y 축 이름과 그림 칸 위 사이
constexpr double kLabelPadding = 6;        // x 이름표 칸 위아래 여백
constexpr double kRightPadding = 12;       // 오른쪽 축이 없을 때 그림 칸 오른쪽 여백
constexpr double kMinPlotSize = 10;
constexpr double kMaxRotatedLabel = 150;   // 기울인 x 이름표의 가장 긴 너비
constexpr double kRotatedAngle = -40;
constexpr double kRotatedAnchorDivisor = 3;   // 기울인 x 이름표: 글 끝을 눈금에서 (줄 높이 / 이 값)만큼 오른쪽에 둔다
constexpr double kHalfTurnDegrees = 180;      // 도 → 라디안
constexpr double kMinWrapSlot = 40;        // 칸이 이보다 좁으면 두 줄로 접지 않고 기울인다
constexpr int kWrappedLines = 2;
constexpr double kCategoryHalfWidth = 0.5; // 범주 축은 -0.5 부터 범주 수 - 0.5 까지

// y 축 자리(ChartGeometry::arrYScales · ChartSeries::iAxis)
constexpr int kLeftAxis = 0;
constexpr int kRightAxis = 1;

constexpr double kGroupRatio = 0.72;       // 막대 묶음이 범주 칸에서 차지하는 비율(나머지는 숨 쉴 틈)
constexpr double kMaxBarWidth = 48;
constexpr double kMinBarWidth = 1;
constexpr double kBarGap = 2;              // 나란한 막대 사이(바탕 색 틈)
constexpr double kBarGapMinRatio = 3;      // 막대가 틈의 이 배보다 넓을 때만 나란한 막대 사이에 틈을 둔다
constexpr double kBarRadius = 4;           // 값 쪽 끝의 둥근 모서리(바닥 쪽은 각지게)
constexpr double kMinBarHeight = 2;
constexpr double kBarHitPadding = 6;
constexpr double kMinBarHitHeight = 14;
constexpr int kMaxValueLabels = 24;        // 막대가 이보다 많으면 값 글을 쓰지 않는다
constexpr double kValueLabelGap = 2;       // 값 글(또는 "값 없음")과 막대 끝 사이
constexpr double kLineWidth = 2;
constexpr double kLineMarkerRadius = 4.5;
constexpr double kScatterMarkerRadius = 5.5;
constexpr double kMarkerRing = 2;          // 겹친 표시가 보이게 바탕 색 고리를 두른다
constexpr double kPointHitRadius = 12;
constexpr double kMarkOvershoot = 0.5;     // 범위 밖 값은 그림 칸의 반 너비 바깥에서 멈춘다(좌표가 끝없이 커지지 않게)
constexpr double kClipPadding = 8;

// 점 표시 모양: 계열마다 원 · 네모 · 마름모 · 세모를 차례로 쓴다. 비율은 반지름에 곱해 모양끼리 크기가 비슷해 보이게 맞춘 값이다
constexpr int kMarkerShapes = 4;
constexpr double kSquareMarkerRatio = 0.9;        // 네모: 가운데에서 변까지
constexpr double kDiamondMarkerRatio = 1.2;       // 마름모: 가운데에서 꼭짓점까지
constexpr double kTriangleTopRatio = 1.2;         // 세모: 가운데에서 위 꼭짓점까지
constexpr double kTriangleHalfWidthRatio = 1.1;   // 세모: 밑변의 반
constexpr double kTriangleBaseRatio = 0.8;        // 세모: 가운데에서 밑변까지

constexpr int kLegendSwatch = 28;
constexpr double kLegendSwatchTextGap = 10;  // 견본과 글자 사이(견본 칸 너비에서 뺀다)
constexpr double kLegendSwatchHeight = 12;
constexpr double kLegendBarInset = 3;        // 막대 견본: 견본 칸 왼쪽 · 오른쪽 여백
constexpr double kLegendBarRadius = 3;
constexpr double kLegendItemGap = 18;
constexpr double kLegendRowGap = 4;

constexpr int kMaxTicks = 30;              // 눈금이 이보다 많은 설정은 앞의 것만 쓴다
constexpr int kMaxDerivedCategories = 1000; // 범주가 없어 점에서 범주를 만들 때 이 자리까지만 본다
constexpr int kMaxDecimals = 6;
constexpr int kGroupDigits = 3;            // 천 단위 쉼표: 정수 부분을 세 자리씩 묶는다
constexpr int kFirstHangulSyllable = 0xAC00;   // 한글 음절(가 … 힣): 한글 단위는 숫자에 붙여 쓴다
constexpr int kLastHangulSyllable = 0xD7A3;
constexpr double kHugeValue = 1e15;        // 이보다 크면(또는 아주 작으면) 지수로 쓴다
constexpr double kTinyValue = 1e-4;
constexpr double kWidenHalfRange = 1;      // 0 · 거꾸로 · 유한하지 않은 범위는 그 값 둘레 ±1 로 넓힌다
constexpr double kRelativeWiden = 1e-6;    // ±1 이 값에 묻히는 큰 값은 값의 이만큼 넓힌다
constexpr double kTickTolerance = 1e-9;
constexpr int kMessageTitlePx = 16;
constexpr double kMessagePadding = 28;     // NONE 의 글 판 안쪽 여백
constexpr double kMessageRadius = 10;
constexpr double kMessageLineGap = 4;
constexpr double kPi = 3.14159265358979323846;

// 히트맵(design/MoreViews.md §4.3)
constexpr double kHeatCellGap = 2;             // 칸 사이 바탕 색 틈
constexpr double kHeatGapMinRatio = 3;         // 칸이 틈의 이 배보다 넓을(높을) 때만 틈을 둔다
constexpr double kHeatMaxRowPitch = 48;        // 행 높이(틈 포함)의 위 한계. 아래 한계는 그림 칸이다(행이 많으면 칸이 작아지고 글을 쓰지 않는다)
constexpr double kHeatMaxRowLabelRatio = 0.3;  // 행 이름 칸은 위젯 너비의 이만큼까지(넘치는 이름은 줄인다)
constexpr double kHeatLabelGap = 8;            // 행 이름과 격자 사이
constexpr double kHeatHeaderGap = 6;           // 열 이름과 격자 사이
constexpr double kHeatCellPadding = 4;         // 칸 글 좌우 여백
constexpr double kHeatLegendBarWidth = 200;    // 색 범례 띠
constexpr double kHeatMinLegendBarWidth = 40;  // 띠는 범례 앞 y 열 이름 · 뒤 글에 자리를 내주며 이만큼까지 줄어든다(띠가 보이게)
constexpr double kHeatMinLegendTitleWidth = 32;   // 범례 앞 y 열 이름 칸이 이보다 좁으면 이름을 뺀다(줄여도 읽을 글이 남지 않는다)
constexpr double kHeatLegendBarHeight = 12;
constexpr double kHeatLegendTextGap = 8;       // 띠 · 양 끝 글 · 이름 사이
constexpr double kHeatLegendNoteGap = 18;      // 높음과 "y 열마다 따로 칠한다" 사이
constexpr double kHeatRampMiddle = 0.5;        // 범위가 0 인 색 눈금의 칸 색 자리

const QString kHeatLowLabel = QStringLiteral("낮음");
const QString kHeatHighLabel = QStringLiteral("높음");
const QString kHeatPerColumnNote = QStringLiteral("y 열마다 따로 칠한다");
const QString kHeatEmptyCell = QStringLiteral("—");
const QString kHeatJoin = QStringLiteral(" · ");

// 도넛(design/MoreViews.md §5.3)
constexpr double kDonutInnerRatio = 0.6;        // 안쪽 반지름 = 바깥 반지름 × 이 값
constexpr double kDonutMaxDiameter = 480;       // 바깥 지름의 위 한계(글자 크기에 견줘 고리가 지나치게 커지지 않게)
constexpr double kDonutMinDiameterBesideLegend = 60;   // 범례 목록 옆 고리가 이보다 작아지는 좁은 위젯은 범례 목록 없이 고리만 그린다
constexpr double kDonutSliceGap = 2;            // 조각 사이 바탕 색 틈
constexpr double kDonutGapOverhang = 1;         // 틈 선은 고리 안팎으로 이만큼 더 그어 끝까지 가른다
constexpr double kDonutFullTurn = 360;          // 도
constexpr double kDonutQuarterTurn = 90;
constexpr double kDonutTwelveOClock = 90;       // QPainterPath 의 각(3시가 0, 반시계로 는다)에서 12시
constexpr double kDonutMinLabelAngle = 20;      // 조각 글(몫)은 이 각 이상인 조각에만 쓴다
constexpr double kDonutAngleTolerance = 1e-9;   // 각을 견줄 때의 계산 오차
constexpr double kDonutPercentScale = 100;
constexpr int kDonutShareDecimals = 1;          // 몫은 소수 한 자리까지(끝의 0 은 뺀다)
constexpr double kDonutLabelPadding = 2;        // 조각 글이 조각 안에 들어가는지 볼 때 글 상자 둘레 여유
constexpr double kDonutLegendGap = 32;          // 고리와 범례 목록 사이
constexpr double kDonutSwatchSize = 12;         // 범례 견본(정사각형)
constexpr double kDonutSwatchRadius = 3;
constexpr double kDonutSwatchTextGap = 8;       // 견본과 범주 글 사이
constexpr double kDonutColumnGap = 16;          // 범례 칸(범주 글 · 값 · 몫) 사이
constexpr double kDonutRowGap = 8;              // 범례 줄 사이
constexpr double kDonutMaxLabelRatio = 0.3;     // 범례의 범주 글 칸은 위젯 너비의 이만큼까지(넘치는 글은 줄인다)
constexpr double kDonutMarkHalfSize = 4;        // GetMarkRect: 조각 가운데 점 둘레 사각형의 반 변
constexpr double kDonutCenterLineGap = 4;       // 가운데 두 줄(y 열 이름 · 합) 사이
constexpr double kDonutCenterWidthRatio = 0.9;  // 가운데 글은 구멍을 가로지르는 현의 이만큼까지 쓴다

const QString kDonutTotalText = QStringLiteral("합 %1");
const QString kDonutToolTip = QStringLiteral("%1: %2 (전체의 %3)");
const QString kDonutValueToolTip = QStringLiteral("%1: %2");   // 값이 곧 몫일 때(ChartShare: 단위 % · 합 100 ± 1)
const QString kPercentUnit = QStringLiteral("%");              // 몫 글에 붙이는 단위

const QString kNoneTitle = QStringLiteral("차트로 그리지 않는 표");
const QString kNoValueTitle = QStringLiteral("그릴 값이 없다");
const QString kMissingLabel = QStringLiteral("값 없음");
const QString kStartLabel = QStringLiteral("시작");
const QString kIncreaseLabel = QStringLiteral("늘어남");
const QString kDecreaseLabel = QStringLiteral("줄어듦");

bool IsCategorical(EChartType type)
{
    return type == EChartType::BAR || type == EChartType::STEP_BAR || type == EChartType::LINE;
}

bool IsBars(EChartType type)
{
    return type == EChartType::BAR || type == EChartType::STEP_BAR;
}

// 정수 부분에 천 단위 쉼표를 넣는다
QString InsertGroupSeparators(const QString& number)
{
    const int iSign = number.startsWith(QLatin1Char('-')) ? 1 : 0;
    int iEnd = number.indexOf(QLatin1Char('.'));
    if (iEnd < 0)
        iEnd = number.size();
    QString strResult = number;
    for (int iInsert = iEnd - kGroupDigits; iInsert > iSign; iInsert -= kGroupDigits)
        strResult.insert(iInsert, QLatin1Char(','));
    return strResult;
}

QString FormatFixed(double value, int decimals)
{
    const double dAbs = std::abs(value);
    if (!std::isfinite(value))
        return QStringLiteral("—");
    if (dAbs >= kHugeValue || (dAbs > 0 && dAbs < kTinyValue && decimals >= kMaxDecimals))
        return QString::number(value, 'g', kMaxDecimals);
    QString strText = QString::number(value, 'f', decimals);
    if (strText.startsWith(QLatin1Char('-')) && strText.count(QLatin1Char('0')) == strText.size() - 1 - strText.count(QLatin1Char('.')))
        strText.remove(0, 1);   // "-0" · "-0.0" 은 0 으로
    return InsertGroupSeparators(strText);
}

// 값: 필요한 소수 자리만(최대 6자리)
QString FormatValue(double value)
{
    QString strText = FormatFixed(value, kMaxDecimals);
    if (strText.contains(QLatin1Char('.')) && !strText.contains(QLatin1Char('e')))
    {
        while (strText.endsWith(QLatin1Char('0')))
            strText.chop(1);
        if (strText.endsWith(QLatin1Char('.')))
            strText.chop(1);
    }
    return strText;
}

// 단위 붙이기: % 와 한글 단위(명 · 건 등)는 붙여 쓰고, 그 밖(ms · px · h)은 띄어 쓴다
QString AppendUnit(const QString& text, const QString& unit)
{
    if (unit.isEmpty())
        return text;
    const QChar chFirst = unit.at(0);
    const bool bAttached = unit == QStringLiteral("%") || (chFirst.unicode() >= kFirstHangulSyllable && chFirst.unicode() <= kLastHangulSyllable);
    return bAttached ? text + unit : text + QLatin1Char(' ') + unit;
}

// 눈금 간격에 맞춘 소수 자리
int GetTickDecimals(const QVector<double>& ticks)
{
    double dStep = std::numeric_limits<double>::infinity();
    for (int iTick = 1; iTick < ticks.size(); ++iTick)
    {
        const double dGap = std::abs(ticks[iTick] - ticks[iTick - 1]);
        if (dGap > 0)
            dStep = std::min(dStep, dGap);
    }
    if (!std::isfinite(dStep) || dStep >= 1)
        return 0;
    return static_cast<int>(ViewCommon::Clamp(std::ceil(-std::log10(dStep) - kTickTolerance), 0, kMaxDecimals));
}

// 0 · 거꾸로 · 유한하지 않은 범위를 그 값 둘레로 넓힌다(design/Views.md §11). ±1 이 값에 묻히면 값에 비례해 넓힌다
bool WidenRange(double* min, double* max)
{
    if (std::isfinite(*min) && std::isfinite(*max) && *max > *min)
        return false;
    double dCenter = 0;
    if (std::isfinite(*min) && std::isfinite(*max))
        dCenter = *min / 2 + *max / 2;
    else if (std::isfinite(*min))
        dCenter = *min;
    else if (std::isfinite(*max))
        dCenter = *max;
    double dHalf = kWidenHalfRange;
    if (!(dCenter + dHalf > dCenter - dHalf))
        dHalf = std::abs(dCenter) * kRelativeWiden;
    double dLow = dCenter - dHalf;
    double dHigh = dCenter + dHalf;
    if (!std::isfinite(dHigh))
    {
        dHigh = dCenter;
        dLow = dCenter - 2 * dHalf;
    }
    if (!std::isfinite(dLow))
    {
        dLow = dCenter;
        dHigh = dCenter + 2 * dHalf;
    }
    if (!(dHigh > dLow) || !std::isfinite(dLow) || !std::isfinite(dHigh))
    {
        dLow = -kWidenHalfRange;
        dHigh = kWidenHalfRange;
    }
    *min = dLow;
    *max = dHigh;
    return true;
}

// 범위 안에서 0–1 자리. 폭이 double 로 넘치는 범위(−최대 … 최대)도 반으로 나눠 계산한다
double Normalize(double value, double min, double max)
{
    const double dSpan = max - min;
    if (std::isfinite(dSpan) && dSpan > 0)
        return (value - min) / dSpan;
    const double dHalfSpan = max / 2 - min / 2;
    return dHalfSpan > 0 ? (value / 2 - min / 2) / dHalfSpan : 0.5;
}

// 값 쪽 끝만 둥근 막대. dataEndTop 이면 위가 값 쪽
QPainterPath MakeBarPath(const QRectF& rect, bool dataEndTop)
{
    const double dRadius = std::min({ kBarRadius, rect.width() / 2, rect.height() / 2 });
    QPainterPath path;
    if (dataEndTop)
    {
        path.moveTo(rect.bottomLeft());
        path.lineTo(rect.left(), rect.top() + dRadius);
        path.quadTo(rect.topLeft(), QPointF(rect.left() + dRadius, rect.top()));
        path.lineTo(rect.right() - dRadius, rect.top());
        path.quadTo(rect.topRight(), QPointF(rect.right(), rect.top() + dRadius));
        path.lineTo(rect.bottomRight());
    }
    else
    {
        path.moveTo(rect.topLeft());
        path.lineTo(rect.topRight());
        path.lineTo(rect.right(), rect.bottom() - dRadius);
        path.quadTo(rect.bottomRight(), QPointF(rect.right() - dRadius, rect.bottom()));
        path.lineTo(rect.left() + dRadius, rect.bottom());
        path.quadTo(rect.bottomLeft(), QPointF(rect.left(), rect.bottom() - dRadius));
    }
    path.closeSubpath();
    return path;
}

// 글을 width 안의 줄로 접는다(빈칸에서). maxLines 를 넘거나 한 낱말이 넘치면 줄임표로 줄인다
QStringList WrapLabel(const QString& text, const QFontMetricsF& metrics, double width, int maxLines)
{
    QStringList vecLines;
    QString strLine;
    for (const QString& strWord : text.split(QLatin1Char(' '), Qt::SkipEmptyParts))
    {
        const QString strTry = strLine.isEmpty() ? strWord : strLine + QLatin1Char(' ') + strWord;
        if (strLine.isEmpty() || metrics.horizontalAdvance(strTry) <= width)
        {
            strLine = strTry;
            continue;
        }
        vecLines << strLine;
        strLine = strWord;
    }
    if (!strLine.isEmpty())
        vecLines << strLine;
    if (vecLines.size() > maxLines)
    {
        const QString strRest = QStringList(vecLines.mid(maxLines - 1)).join(QLatin1Char(' '));
        vecLines = vecLines.mid(0, maxLines - 1);
        vecLines << strRest;
    }
    for (QString& strWrapped : vecLines)
        strWrapped = metrics.elidedText(strWrapped, Qt::ElideRight, width);
    return vecLines;
}

bool FitsInLines(const QString& text, const QFontMetricsF& metrics, double width, int maxLines)
{
    QStringList vecLines;
    QString strLine;
    for (const QString& strWord : text.split(QLatin1Char(' '), Qt::SkipEmptyParts))
    {
        if (metrics.horizontalAdvance(strWord) > width)
            return false;
        const QString strTry = strLine.isEmpty() ? strWord : strLine + QLatin1Char(' ') + strWord;
        if (strLine.isEmpty() || metrics.horizontalAdvance(strTry) <= width)
        {
            strLine = strTry;
            continue;
        }
        vecLines << strLine;
        strLine = strWord;
    }
    if (!strLine.isEmpty())
        vecLines << strLine;
    return vecLines.size() <= maxLines;
}

// 오름차순 경계(n + 1 개)에서 value 가 드는 칸 자리: edges[i] <= value 인 가장 큰 i(0 … n - 1)
int FindEdgeIndex(const QVector<double>& edges, double value)
{
    int iIndex = 0;
    for (int iEdge = 1; iEdge + 1 < edges.size(); ++iEdge)
        if (edges[iEdge] <= value)
            iIndex = iEdge;
    return iIndex;
}

// 도넛 몫(0–1)을 % 글로: 소수 한 자리까지 쓰고 끝의 0 은 뺀다(61.5% · 18%)
QString FormatShare(double share)
{
    QString strText = FormatFixed(share * kDonutPercentScale, kDonutShareDecimals);
    if (strText.contains(QLatin1Char('.')) && !strText.contains(QLatin1Char('e')))
    {
        while (strText.endsWith(QLatin1Char('0')))
            strText.chop(1);
        if (strText.endsWith(QLatin1Char('.')))
            strText.chop(1);
    }
    return AppendUnit(strText, kPercentUnit);
}

// center 에서 radius 만큼, 12시에서 시계 방향으로 angle(도) 돈 자리(위젯 좌표, y 는 아래로)
QPointF MapClockPoint(const QPointF& center, double radius, double angle)
{
    const double dRadians = angle * kPi / kHalfTurnDegrees;
    return QPointF(center.x() + radius * std::sin(dRadians), center.y() - radius * std::cos(dRadians));
}

// 고리 조각: 12시에서 시계 방향으로 start 부터 sweep 도, 반지름 inner … outer. 한 바퀴면 구멍 뚫린 원이다
QPainterPath MakeSlicePath(const QPointF& center, double inner, double outer, double start, double sweep)
{
    const QRectF rectOuter(center.x() - outer, center.y() - outer, 2 * outer, 2 * outer);
    const QRectF rectInner(center.x() - inner, center.y() - inner, 2 * inner, 2 * inner);
    QPainterPath path;
    if (sweep >= kDonutFullTurn - kDonutAngleTolerance)
    {
        path.setFillRule(Qt::OddEvenFill);
        path.addEllipse(rectOuter);
        path.addEllipse(rectInner);
        return path;
    }
    // QPainterPath 의 각은 3시가 0 이고 반시계로 는다: 12시에서 시계 방향으로 start 는 90 − start 다
    const double dPathStart = kDonutTwelveOClock - start;
    path.arcMoveTo(rectOuter, dPathStart);
    path.arcTo(rectOuter, dPathStart, -sweep);
    path.arcTo(rectInner, dPathStart - sweep, sweep);
    path.closeSubpath();
    return path;
}
}

KnowledgeChartView::KnowledgeChartView(QWidget* parent)
    : QWidget(parent)
    , m_spec()
    , m_theme(VisualMarkdownTheme::MakeLight())
    , m_bHeaderVisible(true)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
}

KnowledgeChartView::~KnowledgeChartView() = default;

void KnowledgeChartView::SetSpec(const ChartSpec& spec)
{
    m_spec = spec;
    update();
}

void KnowledgeChartView::SetTheme(const VisualMarkdownTheme& theme)
{
    m_theme = theme;
    update();
}

void KnowledgeChartView::SetHeaderVisible(bool visible)
{
    if (m_bHeaderVisible == visible)
        return;
    m_bHeaderVisible = visible;
    update();
}

QPair<int, int> KnowledgeChartView::FindPointAt(const QPoint& pos) const
{
    const ChartGeometry geometry = ComputeGeometry();
    if (geometry.plot.isEmpty())
        return qMakePair(-1, -1);
    if (m_spec.eType == EChartType::HEATMAP)
    {
        // 히트맵: 그 자리 칸의 점(첫 점). 점이 없는 칸은 찾지 않는다
        const QPair<int, int> pairCell = FindHeatmapCellAt(geometry, QPointF(pos));
        if (pairCell.first < 0)
            return qMakePair(-1, -1);
        const int iPoint = ComputeHeatmapCells()[pairCell.second].value(pairCell.first, -1);
        return iPoint >= 0 ? qMakePair(pairCell.second, iPoint) : qMakePair(-1, -1);
    }
    if (m_spec.eType == EChartType::DONUT)
    {
        // 도넛: 각 · 반지름으로 찾은 조각(첫 계열의 점)
        const int iPoint = FindDonutSliceAt(geometry, QPointF(pos));
        return iPoint >= 0 ? qMakePair(0, iPoint) : qMakePair(-1, -1);
    }
    const QVector<QVector<QRectF>> vecMarks = ComputeMarks(geometry);
    const QPointF ptPos(pos);
    QPair<int, int> pairBest(-1, -1);
    double dBest = std::numeric_limits<double>::infinity();
    const bool bBars = IsBars(m_spec.eType);
    for (int iSeries = 0; iSeries < vecMarks.size(); ++iSeries)
    {
        for (int iPoint = 0; iPoint < vecMarks[iSeries].size(); ++iPoint)
        {
            const QRectF& rectMark = vecMarks[iSeries][iPoint];
            if (rectMark.isEmpty())
                continue;
            const double dDistance = QLineF(ptPos, rectMark.center()).length();
            bool bHit = false;
            if (bBars)
            {
                // 막대는 위아래로 조금 넓혀 낮은 막대도 잡히게 한다
                QRectF rectHit = rectMark.adjusted(0, -kBarHitPadding, 0, kBarHitPadding);
                if (rectHit.height() < kMinBarHitHeight)
                    rectHit = QRectF(rectHit.left(), rectMark.center().y() - kMinBarHitHeight / 2, rectHit.width(), kMinBarHitHeight);
                bHit = rectHit.contains(ptPos);
            }
            else
            {
                bHit = dDistance <= kPointHitRadius;
            }
            if (bHit && dDistance < dBest)
            {
                dBest = dDistance;
                pairBest = qMakePair(iSeries, iPoint);
            }
        }
    }
    return pairBest;
}

QString KnowledgeChartView::GetToolTipAt(const QPoint& pos) const
{
    if (m_spec.eType == EChartType::HEATMAP)
    {
        const ChartGeometry geometry = ComputeGeometry();
        return geometry.plot.isEmpty() ? QString() : MakeHeatmapToolTip(geometry, QPointF(pos));
    }
    const QPair<int, int> pairPoint = FindPointAt(pos);
    if (pairPoint.first < 0)
        return QString();
    if (m_spec.eType == EChartType::DONUT)
        return MakeDonutToolTip(pairPoint.second);
    return MakePointToolTip(pairPoint.first, pairPoint.second);
}

QRectF KnowledgeChartView::GetMarkRect(int series, int point) const
{
    if (series < 0 || series >= m_spec.vecSeries.size() || point < 0 || point >= m_spec.vecSeries[series].vecPoints.size())
        return QRectF();
    const ChartGeometry geometry = ComputeGeometry();
    if (geometry.plot.isEmpty())
        return QRectF();
    if (m_spec.eType == EChartType::HEATMAP)
    {
        // 히트맵: 그 점의 칸(그 칸의 첫 점일 때만)
        const int iRow = GetHeatmapRow(m_spec.vecSeries[series].vecPoints[point], ListCategoryLabels().size());
        if (iRow < 0 || ComputeHeatmapCells()[series].value(iRow, -1) != point)
            return QRectF();
        return GetHeatmapCellRect(geometry, iRow, series);
    }
    if (m_spec.eType == EChartType::DONUT)
    {
        // 도넛: 조각 가운데 점 둘레의 작은 사각형(첫 계열, 각이 0 보다 큰 조각만)
        const QVector<DonutSlice> vecSlices = ComputeDonutSlices(nullptr);
        if (series != 0 || !(vecSlices[point].dSweep > 0))
            return QRectF();
        const QPointF ptCenter = GetDonutSliceCenter(geometry, vecSlices[point]);
        return QRectF(ptCenter.x() - kDonutMarkHalfSize, ptCenter.y() - kDonutMarkHalfSize, 2 * kDonutMarkHalfSize, 2 * kDonutMarkHalfSize);
    }
    return ComputeMarks(geometry)[series][point];
}

QRectF KnowledgeChartView::GetPlotRect() const
{
    return ComputeGeometry().plot;
}

QRectF KnowledgeChartView::GetLegendRect() const
{
    return ComputeGeometry().legend;
}

QString KnowledgeChartView::GetMessageText() const
{
    if (!ShowsMessage())
        return QString();
    QStringList vecLines { m_spec.eType == EChartType::NONE ? kNoneTitle : kNoValueTitle };
    vecLines << m_spec.vecWarnings;
    return vecLines.join(QLatin1Char('\n'));
}

const ChartSpec& KnowledgeChartView::GetSpec() const
{
    return m_spec;
}

bool KnowledgeChartView::IsHeaderVisible() const
{
    return m_bHeaderVisible;
}

bool KnowledgeChartView::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent* pHelp = static_cast<QHelpEvent*>(event);
        ViewCommon::ShowToolTip(this, pHelp, GetToolTipAt(pHelp->pos()));
        return true;
    }
    return QWidget::event(event);
}

void KnowledgeChartView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(rect(), m_theme.background);
    const ChartGeometry geometry = ComputeGeometry();

    // 제목 · 밑글(제목 줄을 끄면 두 칸 모두 비었다)
    const QFont fontTitle = MakeFont(m_theme.iTitleFontPx, true);
    const QFont fontCaption = MakeFont(m_theme.iCaptionFontPx);
    if (!geometry.title.isEmpty())
    {
        painter.setFont(fontTitle);
        painter.setPen(m_theme.text);
        painter.drawText(geometry.title, Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetricsF(fontTitle).elidedText(m_spec.strTitle, Qt::ElideRight, geometry.title.width()));
    }
    if (!geometry.caption.isEmpty())
    {
        painter.setFont(fontCaption);
        painter.setPen(m_theme.mutedText);
        painter.drawText(geometry.caption, Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetricsF(fontCaption).elidedText(m_spec.strCaption, Qt::ElideRight, geometry.caption.width()));
    }
    if (geometry.plot.isEmpty())
    {
        DrawMessage(painter, geometry);
        return;
    }
    if (m_spec.eType == EChartType::HEATMAP)
    {
        DrawHeatmapLegend(painter, geometry);
        DrawHeatmap(painter, geometry);
        return;
    }
    if (m_spec.eType == EChartType::DONUT)
    {
        DrawDonut(painter, geometry);
        DrawDonutLegend(painter, geometry);
        return;
    }
    const QVector<QVector<QRectF>> vecMarks = ComputeMarks(geometry);
    DrawGrid(painter, geometry);
    DrawXAxis(painter, geometry);
    // 표시는 그림 칸 둘레에서 자른다. 막대는 값 글이 칸 위로 조금 나가므로 위아래를 그만큼 더 둔다
    const double dLabelRoom = IsBars(m_spec.eType) ? QFontMetricsF(MakeFont(m_theme.iSmallFontPx)).height() + kClipPadding : kClipPadding;
    painter.save();
    painter.setClipRect(geometry.plot.adjusted(-kClipPadding, -dLabelRoom, kClipPadding, dLabelRoom));
    if (IsBars(m_spec.eType))
        DrawBars(painter, geometry, vecMarks);
    else
        DrawLines(painter, geometry, vecMarks);
    painter.restore();
    DrawLegend(painter, geometry);
}

KnowledgeChartView::ChartGeometry KnowledgeChartView::ComputeGeometry() const
{
    ChartGeometry geometry;
    const double dLeft = kMargin;
    const double dRight = std::max(dLeft + 1, static_cast<double>(width() - kMargin));
    double dY = kMargin;

    const QFontMetricsF metricsTitle(MakeFont(m_theme.iTitleFontPx, true));
    const QFontMetricsF metricsCaption(MakeFont(m_theme.iCaptionFontPx));
    const QFontMetricsF metricsLabel(MakeFont(m_theme.iLabelFontPx));
    // 제목 덩어리: 제목 줄(제목이 비어도 자리를 둔다) · 밑글 줄(밑글이 있을 때) · 아래 틈. 끄면 자리를 두지 않고 위 여백에서 바로 시작한다
    if (m_bHeaderVisible)
    {
        geometry.title = QRectF(dLeft, dY, dRight - dLeft, metricsTitle.height());
        dY = geometry.title.bottom() + kTitleGap;
        if (!m_spec.strCaption.isEmpty())
        {
            geometry.caption = QRectF(dLeft, dY, dRight - dLeft, metricsCaption.height());
            dY = geometry.caption.bottom();
        }
        dY += kSectionGap;
    }

    const double dBottomLimit = height() - kMargin;
    if (ShowsMessage())
    {
        geometry.message = QRectF(dLeft, dY, dRight - dLeft, std::max(1.0, dBottomLimit - dY));
        return geometry;
    }
    if (m_spec.eType == EChartType::HEATMAP)
    {
        ComputeHeatmapGeometry(&geometry, dLeft, dRight, dY, dBottomLimit);
        return geometry;
    }
    if (m_spec.eType == EChartType::DONUT)
    {
        ComputeDonutGeometry(&geometry, dLeft, dRight, dY, dBottomLimit);
        return geometry;
    }

    // 범례: 계열이 둘 이상이면 계열마다, 단계 막대는 시작 · 늘어남 · 줄어듦. 줄이 넘치면 다음 줄로
    QVector<LegendItem> vecItems;
    if (m_spec.eType == EChartType::STEP_BAR)
    {
        vecItems.push_back({ kStartLabel, m_theme.stepStart, -1, QRectF() });
        vecItems.push_back({ kIncreaseLabel, m_theme.stepIncrease, -1, QRectF() });
        vecItems.push_back({ kDecreaseLabel, m_theme.stepDecrease, -1, QRectF() });
    }
    else if (m_spec.vecSeries.size() > 1)
    {
        const bool bTwoAxes = m_spec.vecYAxes.size() > kRightAxis;
        for (int iSeries = 0; iSeries < m_spec.vecSeries.size(); ++iSeries)
        {
            QString strText = m_spec.vecSeries[iSeries].strName;
            if (bTwoAxes)
                strText += GetAxisOf(iSeries) == kLeftAxis ? QStringLiteral(" (왼쪽 축)") : QStringLiteral(" (오른쪽 축)");
            vecItems.push_back({ strText, m_theme.GetSeriesColor(iSeries), iSeries, QRectF() });
        }
    }
    if (!vecItems.isEmpty())
    {
        double dX = dLeft;
        double dRowTop = dY;
        const double dRowHeight = metricsLabel.height() + kLegendRowGap;
        for (LegendItem& item : vecItems)
        {
            const double dItemWidth = kLegendSwatch + metricsLabel.horizontalAdvance(item.strText);
            if (dX > dLeft && dX + dItemWidth > dRight)
            {
                dX = dLeft;
                dRowTop += dRowHeight;
            }
            item.rect = QRectF(dX, dRowTop, std::min(dItemWidth, dRight - dX), metricsLabel.height());
            geometry.legend = geometry.legend.united(item.rect);
            dX += dItemWidth + kLegendItemGap;
        }
        geometry.vecLegend = vecItems;
        dY = geometry.legend.bottom() + kSectionGap;
    }

    // 축 범위: 0 · 거꾸로 · 유한하지 않은 범위는 넓힌다. 범주 축은 범주 수로 다시 잡는다
    const auto funcScale = [](const ChartAxis& axis)
    {
        AxisScale scale;
        scale.bValid = true;
        scale.dMin = axis.dMin;
        scale.dMax = axis.dMax;
        const bool bWidened = WidenRange(&scale.dMin, &scale.dMax);
        const double dTolerance = (scale.dMax / 2 - scale.dMin / 2) * kTickTolerance;
        if (bWidened)
        {
            scale.vecTicks = { scale.dMin, scale.dMin / 2 + scale.dMax / 2, scale.dMax };
            return scale;
        }
        for (const double dTick : axis.vecTicks)
        {
            if (scale.vecTicks.size() >= kMaxTicks)
                break;
            if (std::isfinite(dTick) && dTick >= scale.dMin - dTolerance && dTick <= scale.dMax + dTolerance
                && (scale.vecTicks.isEmpty() || dTick > scale.vecTicks.last()))
                scale.vecTicks << dTick;
        }
        if (scale.vecTicks.size() < 2)
            scale.vecTicks = { scale.dMin, scale.dMax };
        return scale;
    };
    if (IsCategorical(m_spec.eType))
    {
        const int iCategories = std::max(1, static_cast<int>(ListCategoryLabels().size()));
        geometry.xScale.bValid = true;
        geometry.xScale.dMin = m_spec.xAxis.dMin;
        geometry.xScale.dMax = m_spec.xAxis.dMax;
        if (!(std::isfinite(geometry.xScale.dMin) && std::isfinite(geometry.xScale.dMax) && geometry.xScale.dMax > geometry.xScale.dMin))
        {
            geometry.xScale.dMin = -kCategoryHalfWidth;
            geometry.xScale.dMax = iCategories - kCategoryHalfWidth;
        }
    }
    else
    {
        geometry.xScale = funcScale(m_spec.xAxis);
    }
    geometry.arrYScales[kLeftAxis] = m_spec.vecYAxes.isEmpty() ? funcScale(ChartAxis()) : funcScale(m_spec.vecYAxes[kLeftAxis]);
    if (m_spec.vecYAxes.size() > kRightAxis)
        geometry.arrYScales[kRightAxis] = funcScale(m_spec.vecYAxes[kRightAxis]);
    else
        geometry.arrYScales[kRightAxis].bValid = false;

    // 그림 칸: 왼쪽 · 오른쪽 눈금 글 너비, 위의 y 축 이름 줄, 아래의 x 이름표 칸과 x 축 이름 줄을 뺀다
    const auto funcLabelWidth = [&metricsLabel](const AxisScale& scale)
    {
        if (!scale.bValid)
            return 0.0;
        const int iDecimals = GetTickDecimals(scale.vecTicks);
        double dWidth = 0;
        for (const double dTick : scale.vecTicks)
            dWidth = std::max(dWidth, metricsLabel.horizontalAdvance(FormatFixed(dTick, iDecimals)));
        return dWidth;
    };
    const double dLeftLabels = funcLabelWidth(geometry.arrYScales[kLeftAxis]);
    const double dRightLabels = funcLabelWidth(geometry.arrYScales[kRightAxis]);
    const bool bAxisTitles = !m_spec.vecYAxes.isEmpty();
    if (bAxisTitles)
        dY += metricsLabel.height() + kAxisTitleGap;
    double dPlotLeft = dLeft + dLeftLabels + kTickGap;
    const double dPlotRight = geometry.arrYScales[kRightAxis].bValid ? dRight - dRightLabels - kTickGap : dRight - kRightPadding;
    double dPlotWidth = std::max(kMinPlotSize, dPlotRight - dPlotLeft);

    // x 이름표: 칸에 들어가면 가로, 두 줄로 접으면 들어가면 두 줄, 아니면 기울인다
    const double dLineHeight = metricsLabel.height();
    if (IsCategorical(m_spec.eType))
    {
        const double dSlot = dPlotWidth / (geometry.xScale.dMax - geometry.xScale.dMin);
        const QStringList vecLabels = ListCategoryLabels();
        double dWidest = 0;
        bool bWrapFits = dSlot >= kMinWrapSlot;
        for (const QString& strLabel : vecLabels)
        {
            dWidest = std::max(dWidest, metricsLabel.horizontalAdvance(strLabel));
            if (bWrapFits && !FitsInLines(strLabel, metricsLabel, dSlot - kLabelPadding, kWrappedLines))
                bWrapFits = false;
        }
        if (dWidest <= dSlot - kLabelPadding)
        {
            geometry.eLabelMode = ELabelMode::HORIZONTAL;
            geometry.dLabelBand = dLineHeight + 2 * kLabelPadding;
        }
        else if (bWrapFits)
        {
            geometry.eLabelMode = ELabelMode::WRAPPED;
            geometry.dLabelBand = kWrappedLines * dLineHeight + 2 * kLabelPadding;
        }
        else
        {
            geometry.eLabelMode = ELabelMode::ROTATED;
            const double dAngle = std::abs(kRotatedAngle) * kPi / kHalfTurnDegrees;
            const double dLabelWidth = std::min(dWidest, kMaxRotatedLabel);
            geometry.dLabelBand = dLabelWidth * std::sin(dAngle) + dLineHeight * std::cos(dAngle) + 2 * kLabelPadding;
            // 기울인 이름표는 눈금에서 왼쪽 아래로 내려간다. 첫 이름표가 위젯 밖으로 나가지 않게 그림 칸 왼쪽을 민다
            const double dNeededLeft = dLeft + dLabelWidth * std::cos(dAngle) - dLineHeight / kRotatedAnchorDivisor - dSlot / 2 + kLabelPadding;
            if (dNeededLeft > dPlotLeft)
            {
                dPlotLeft = std::min(dNeededLeft, dPlotRight - kMinPlotSize);
                dPlotWidth = std::max(kMinPlotSize, dPlotRight - dPlotLeft);
            }
        }
    }
    else
    {
        geometry.eLabelMode = ELabelMode::HORIZONTAL;
        geometry.dLabelBand = dLineHeight + 2 * kLabelPadding;
    }
    const double dXTitle = MakeAxisTitle(m_spec.xAxis.strTitle, m_spec.xAxis.strUnit).isEmpty() ? 0 : dLineHeight + kTitleGap;
    const double dPlotBottom = dBottomLimit - dXTitle - geometry.dLabelBand;
    const double dPlotHeight = std::max(kMinPlotSize, dPlotBottom - dY);
    geometry.plot = QRectF(dPlotLeft, dY, dPlotWidth, dPlotHeight);
    return geometry;
}

QVector<QVector<QRectF>> KnowledgeChartView::ComputeMarks(const ChartGeometry& geometry) const
{
    QVector<QVector<QRectF>> vecMarks;
    const int iSeriesCount = m_spec.vecSeries.size();
    const bool bBars = IsBars(m_spec.eType);
    const double dSlot = GetSlotWidth(geometry);
    const double dGroup = dSlot * kGroupRatio;
    const double dBarWidth = iSeriesCount > 0 ? std::min(dGroup / iSeriesCount, kMaxBarWidth) : 0;
    const double dGap = dBarWidth > kBarGapMinRatio * kBarGap ? kBarGap : 0;
    for (int iSeries = 0; iSeries < iSeriesCount; ++iSeries)
    {
        const ChartSeries& series = m_spec.vecSeries[iSeries];
        const int iAxis = GetAxisOf(iSeries);
        QVector<QRectF> vecSeriesMarks;
        for (const ChartPoint& point : series.vecPoints)
        {
            if (!point.optY || !std::isfinite(*point.optY) || !std::isfinite(point.dX))
            {
                vecSeriesMarks << QRectF();
                continue;
            }
            const double dCenterX = MapX(geometry, point.dX);
            const double dValueY = MapY(geometry, iAxis, *point.optY);
            if (bBars)
            {
                const double dBase = m_spec.eType == EChartType::STEP_BAR && std::isfinite(point.dBase) ? point.dBase : 0;
                const double dBaseY = MapY(geometry, iAxis, dBase);
                const double dLeft = dCenterX - dBarWidth * iSeriesCount / 2 + iSeries * dBarWidth + dGap / 2;
                double dTop = std::min(dBaseY, dValueY);
                double dHeight = std::abs(dBaseY - dValueY);
                if (dHeight < kMinBarHeight)
                {
                    dTop = dValueY - kMinBarHeight / 2;
                    dHeight = kMinBarHeight;
                }
                vecSeriesMarks << QRectF(dLeft, dTop, std::max(kMinBarWidth, dBarWidth - dGap), dHeight);
            }
            else
            {
                const double dRadius = m_spec.eType == EChartType::LINE ? kLineMarkerRadius : kScatterMarkerRadius;
                vecSeriesMarks << QRectF(dCenterX - dRadius, dValueY - dRadius, 2 * dRadius, 2 * dRadius);
            }
        }
        vecMarks << vecSeriesMarks;
    }
    return vecMarks;
}

QStringList KnowledgeChartView::ListCategoryLabels() const
{
    if (!m_spec.xAxis.vecCategories.isEmpty())
        return m_spec.xAxis.vecCategories;
    // 범주가 없으면 점의 x 칸 글로 만든다(범주 자리 순)
    QStringList vecLabels;
    for (const ChartSeries& series : m_spec.vecSeries)
    {
        for (const ChartPoint& point : series.vecPoints)
        {
            if (!std::isfinite(point.dX) || point.dX < 0 || point.dX >= kMaxDerivedCategories)
                continue;
            const int iCategory = static_cast<int>(point.dX);
            while (vecLabels.size() <= iCategory)
                vecLabels << QString();
            if (vecLabels[iCategory].isEmpty())
                vecLabels[iCategory] = point.strLabel;
        }
    }
    return vecLabels;
}

int KnowledgeChartView::GetAxisOf(int series) const
{
    if (series < 0 || series >= m_spec.vecSeries.size() || m_spec.vecYAxes.size() <= kRightAxis)
        return kLeftAxis;
    return m_spec.vecSeries[series].iAxis >= kRightAxis ? kRightAxis : kLeftAxis;
}

double KnowledgeChartView::MapX(const ChartGeometry& geometry, double x) const
{
    const double dT = ViewCommon::Clamp(Normalize(x, geometry.xScale.dMin, geometry.xScale.dMax), -kMarkOvershoot, 1 + kMarkOvershoot);
    return geometry.plot.left() + dT * geometry.plot.width();
}

double KnowledgeChartView::MapY(const ChartGeometry& geometry, int axis, double y) const
{
    const AxisScale& scale = geometry.arrYScales[axis == kRightAxis && geometry.arrYScales[kRightAxis].bValid ? kRightAxis : kLeftAxis];
    const double dT = ViewCommon::Clamp(Normalize(y, scale.dMin, scale.dMax), -kMarkOvershoot, 1 + kMarkOvershoot);
    return geometry.plot.bottom() - dT * geometry.plot.height();
}

double KnowledgeChartView::GetSlotWidth(const ChartGeometry& geometry) const
{
    if (!IsCategorical(m_spec.eType))
        return 0;
    const double dUnits = Normalize(geometry.xScale.dMin + 1, geometry.xScale.dMin, geometry.xScale.dMax);
    return std::max(0.0, dUnits * geometry.plot.width());
}

QFont KnowledgeChartView::MakeFont(int pixelSize, bool bold) const
{
    return ViewCommon::MakeFont(font(), m_theme, pixelSize, bold);
}

QString KnowledgeChartView::MakeAxisTitle(const QString& title, const QString& unit) const
{
    if (unit.isEmpty())
        return title;
    if (title.isEmpty())
        return unit;
    return QStringLiteral("%1 (%2)").arg(title, unit);
}

QString KnowledgeChartView::MakePointToolTip(int series, int point) const
{
    if (series < 0 || series >= m_spec.vecSeries.size() || point < 0 || point >= m_spec.vecSeries[series].vecPoints.size())
        return QString();
    const ChartSeries& chartSeries = m_spec.vecSeries[series];
    const ChartPoint& chartPoint = chartSeries.vecPoints[point];
    QStringList vecLines { chartSeries.strName };
    const QString strX = AppendUnit(chartPoint.strLabel, m_spec.xAxis.strUnit);
    vecLines << (m_spec.xAxis.strTitle.isEmpty() ? strX : m_spec.xAxis.strTitle + QStringLiteral(": ") + strX);
    if (!chartPoint.optY)
    {
        vecLines << kMissingLabel;
        return vecLines.join(QLatin1Char('\n'));
    }
    QString strValue = AppendUnit(FormatValue(*chartPoint.optY), chartSeries.strUnit);
    // 단계 막대: 시작 막대가 아니면 앞 단계에서 얼마나 바뀌었는지 덧붙인다
    if (m_spec.eType == EChartType::STEP_BAR && !IsStepStart(series, point))
    {
        const double dChange = *chartPoint.optY - chartPoint.dBase;
        const QString strChange = (dChange >= 0 ? QStringLiteral("+") : QString()) + FormatValue(dChange);
        strValue += QStringLiteral(" (앞 단계 %1 에서 %2)").arg(AppendUnit(FormatValue(chartPoint.dBase), chartSeries.strUnit), strChange);
    }
    vecLines << strValue;
    return vecLines.join(QLatin1Char('\n'));
}

bool KnowledgeChartView::IsStepStart(int series, int point) const
{
    if (m_spec.eType != EChartType::STEP_BAR || series < 0 || series >= m_spec.vecSeries.size())
        return false;
    const QVector<ChartPoint>& vecPoints = m_spec.vecSeries[series].vecPoints;
    if (point < 0 || point >= vecPoints.size() || !vecPoints[point].optY)
        return false;
    // 앞 점이 모두 비었으면 값이 있는 첫 막대(시작 값)다
    for (int iPoint = 0; iPoint < point; ++iPoint)
        if (vecPoints[iPoint].optY)
            return false;
    return true;
}

void KnowledgeChartView::DrawGrid(QPainter& painter, const ChartGeometry& geometry) const
{
    const QRectF& rectPlot = geometry.plot;
    const AxisScale& leftScale = geometry.arrYScales[kLeftAxis];
    const AxisScale& rightScale = geometry.arrYScales[kRightAxis];
    const QFont fontLabel = MakeFont(m_theme.iLabelFontPx);
    const QFontMetricsF metrics(fontLabel);
    painter.setFont(fontLabel);

    // 왼쪽 축: 가로 격자와 눈금 글. 오른쪽 축은 눈금 글만(격자는 왼쪽 축 하나)
    const int iLeftDecimals = GetTickDecimals(leftScale.vecTicks);
    for (const double dTick : leftScale.vecTicks)
    {
        const double dY = MapY(geometry, kLeftAxis, dTick);
        painter.setPen(QPen(m_theme.grid, 1));
        painter.drawLine(QPointF(rectPlot.left(), dY), QPointF(rectPlot.right(), dY));
        painter.setPen(m_theme.mutedText);
        painter.drawText(QRectF(rectPlot.left() - kTickGap - kTickLabelWidth, dY - metrics.height() / 2, kTickLabelWidth, metrics.height()),
                         Qt::AlignRight | Qt::AlignVCenter, FormatFixed(dTick, iLeftDecimals));
    }
    if (rightScale.bValid)
    {
        const int iRightDecimals = GetTickDecimals(rightScale.vecTicks);
        for (const double dTick : rightScale.vecTicks)
        {
            const double dY = MapY(geometry, kRightAxis, dTick);
            painter.setPen(QPen(m_theme.axis, 1));
            painter.drawLine(QPointF(rectPlot.right(), dY), QPointF(rectPlot.right() + kTickGap / 2, dY));
            painter.setPen(m_theme.mutedText);
            painter.drawText(QRectF(rectPlot.right() + kTickGap, dY - metrics.height() / 2, kTickLabelWidth, metrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                             FormatFixed(dTick, iRightDecimals));
        }
        painter.setPen(QPen(m_theme.axis, 1));
        painter.drawLine(rectPlot.topRight(), rectPlot.bottomRight());
    }

    // y 축 이름(단위): 그림 칸 위, 왼쪽 축은 왼쪽 맞춤 · 오른쪽 축은 오른쪽 맞춤
    const double dTitleTop = rectPlot.top() - kAxisTitleGap - metrics.height();
    painter.setPen(m_theme.mutedText);
    if (!m_spec.vecYAxes.isEmpty())
    {
        const QString strLeft = MakeAxisTitle(m_spec.vecYAxes[kLeftAxis].strTitle, m_spec.vecYAxes[kLeftAxis].strUnit);
        const double dWidth = rightScale.bValid ? rectPlot.width() / 2 : rectPlot.right() - kMargin;
        painter.drawText(QRectF(kMargin, dTitleTop, dWidth, metrics.height()), Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(strLeft, Qt::ElideRight, dWidth));
    }
    if (rightScale.bValid)
    {
        const QString strRight = MakeAxisTitle(m_spec.vecYAxes[kRightAxis].strTitle, m_spec.vecYAxes[kRightAxis].strUnit);
        const double dWidth = rectPlot.width() / 2;
        painter.drawText(QRectF(width() - kMargin - dWidth, dTitleTop, dWidth, metrics.height()), Qt::AlignRight | Qt::AlignVCenter,
                         metrics.elidedText(strRight, Qt::ElideRight, dWidth));
    }

    // 산점도: 세로 격자(수치 x 눈금)
    if (!IsCategorical(m_spec.eType))
    {
        painter.setPen(QPen(m_theme.grid, 1));
        for (const double dTick : geometry.xScale.vecTicks)
        {
            const double dX = MapX(geometry, dTick);
            painter.drawLine(QPointF(dX, rectPlot.top()), QPointF(dX, rectPlot.bottom()));
        }
    }
}

void KnowledgeChartView::DrawXAxis(QPainter& painter, const ChartGeometry& geometry) const
{
    const QRectF& rectPlot = geometry.plot;
    const QFont fontLabel = MakeFont(m_theme.iLabelFontPx);
    const QFontMetricsF metrics(fontLabel);
    painter.setFont(fontLabel);

    // 바닥 선: 0 이 범위 안이면 0 높이(막대의 바닥), 아니면 그림 칸 아래
    double dBaseline = rectPlot.bottom();
    const double dZero = Normalize(0, geometry.arrYScales[kLeftAxis].dMin, geometry.arrYScales[kLeftAxis].dMax);
    if (dZero > 0 && dZero < 1)
        dBaseline = MapY(geometry, kLeftAxis, 0);
    painter.setPen(QPen(m_theme.axis, 1));
    painter.drawLine(QPointF(rectPlot.left(), rectPlot.bottom()), QPointF(rectPlot.right(), rectPlot.bottom()));
    if (dBaseline != rectPlot.bottom())
        painter.drawLine(QPointF(rectPlot.left(), dBaseline), QPointF(rectPlot.right(), dBaseline));

    painter.setPen(m_theme.mutedText);
    const double dLabelTop = rectPlot.bottom() + kLabelPadding;
    const double dLineHeight = metrics.height();
    if (IsCategorical(m_spec.eType))
    {
        const QStringList vecLabels = ListCategoryLabels();
        const double dSlot = GetSlotWidth(geometry);
        const double dAngle = kRotatedAngle;
        for (int iCategory = 0; iCategory < vecLabels.size(); ++iCategory)
        {
            const double dX = MapX(geometry, iCategory);
            if (dX < rectPlot.left() - 1 || dX > rectPlot.right() + 1)
                continue;
            const QString& strLabel = vecLabels[iCategory];
            switch (geometry.eLabelMode)
            {
            case ELabelMode::HORIZONTAL:
                painter.drawText(QRectF(dX - dSlot / 2, dLabelTop, dSlot, dLineHeight), Qt::AlignHCenter | Qt::AlignVCenter, strLabel);
                break;
            case ELabelMode::WRAPPED:
            {
                const QStringList vecLines = WrapLabel(strLabel, metrics, dSlot - kLabelPadding, kWrappedLines);
                for (int iLine = 0; iLine < vecLines.size(); ++iLine)
                    painter.drawText(QRectF(dX - dSlot / 2, dLabelTop + iLine * dLineHeight, dSlot, dLineHeight), Qt::AlignHCenter | Qt::AlignVCenter,
                                     vecLines[iLine]);
                break;
            }
            case ELabelMode::ROTATED:
            {
                // 글 끝이 범주 가운데 눈금에 닿고 왼쪽 아래로 내려간다
                painter.save();
                painter.translate(dX + dLineHeight / kRotatedAnchorDivisor, dLabelTop);
                painter.rotate(dAngle);
                const QString strElided = metrics.elidedText(strLabel, Qt::ElideRight, kMaxRotatedLabel);
                painter.drawText(QRectF(-kMaxRotatedLabel, 0, kMaxRotatedLabel, dLineHeight), Qt::AlignRight | Qt::AlignVCenter, strElided);
                painter.restore();
                break;
            }
            }
        }
    }
    else
    {
        const int iDecimals = GetTickDecimals(geometry.xScale.vecTicks);
        for (const double dTick : geometry.xScale.vecTicks)
        {
            const double dX = MapX(geometry, dTick);
            painter.drawText(QRectF(dX - kTickLabelWidth / 2, dLabelTop, kTickLabelWidth, dLineHeight), Qt::AlignHCenter | Qt::AlignVCenter,
                             FormatFixed(dTick, iDecimals));
        }
    }

    // x 축 이름(단위): 이름표 칸 아래 가운데
    const QString strTitle = MakeAxisTitle(m_spec.xAxis.strTitle, m_spec.xAxis.strUnit);
    if (!strTitle.isEmpty())
    {
        const QRectF rectTitle(rectPlot.left(), rectPlot.bottom() + geometry.dLabelBand + kTitleGap, rectPlot.width(), dLineHeight);
        painter.drawText(rectTitle, Qt::AlignHCenter | Qt::AlignVCenter, metrics.elidedText(strTitle, Qt::ElideRight, rectTitle.width()));
    }
}

void KnowledgeChartView::DrawBars(QPainter& painter, const ChartGeometry& geometry, const QVector<QVector<QRectF>>& marks) const
{
    const bool bStep = m_spec.eType == EChartType::STEP_BAR;
    const QFont fontValue = MakeFont(m_theme.iSmallFontPx);
    const QFontMetricsF metrics(fontValue);
    int iBars = 0;
    for (const QVector<QRectF>& vecSeriesMarks : marks)
        for (const QRectF& rectMark : vecSeriesMarks)
            if (!rectMark.isEmpty())
                ++iBars;
    const bool bValueLabels = iBars <= kMaxValueLabels;
    const double dSlot = GetSlotWidth(geometry);
    const int iSeriesCount = m_spec.vecSeries.size();
    const double dBarWidth = iSeriesCount > 0 ? std::min(dSlot * kGroupRatio / iSeriesCount, kMaxBarWidth) : 0;

    for (int iSeries = 0; iSeries < marks.size(); ++iSeries)
    {
        const ChartSeries& series = m_spec.vecSeries[iSeries];
        const int iAxis = GetAxisOf(iSeries);
        QRectF rectPrevious;
        double dPreviousValue = 0;
        for (int iPoint = 0; iPoint < marks[iSeries].size(); ++iPoint)
        {
            const ChartPoint& point = series.vecPoints[iPoint];
            const QRectF& rectBar = marks[iSeries][iPoint];
            if (rectBar.isEmpty())
            {
                // 빈 값: 막대를 그리지 않고 그 자리에 "값 없음" 을 작게 쓴다(0 과 헷갈리지 않게)
                if (!point.optY && std::isfinite(point.dX) && dBarWidth >= metrics.horizontalAdvance(kMissingLabel))
                {
                    const double dLeft = MapX(geometry, point.dX) - dBarWidth * iSeriesCount / 2 + iSeries * dBarWidth;
                    const double dBaseY = MapY(geometry, iAxis, 0);
                    painter.setFont(fontValue);
                    painter.setPen(m_theme.mutedText);
                    painter.drawText(QRectF(dLeft, dBaseY - metrics.height() - kValueLabelGap, dBarWidth, metrics.height()), Qt::AlignHCenter | Qt::AlignBottom,
                                     kMissingLabel);
                }
                continue;
            }
            const double dValue = *point.optY;
            const double dBase = bStep ? point.dBase : 0;
            const bool bUp = dValue >= dBase;
            QColor color = m_theme.GetSeriesColor(iSeries);
            if (bStep)
                color = IsStepStart(iSeries, iPoint) ? m_theme.stepStart : (bUp ? m_theme.stepIncrease : m_theme.stepDecrease);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawPath(MakeBarPath(rectBar, bUp));

            // 단계 막대: 앞 막대의 값 높이에서 이 막대까지 가는 잇는 선
            if (bStep && !rectPrevious.isEmpty())
            {
                const double dY = MapY(geometry, iAxis, dPreviousValue);
                painter.setPen(QPen(m_theme.axis, 1));
                painter.drawLine(QPointF(rectPrevious.right(), dY), QPointF(rectBar.left(), dY));
            }
            rectPrevious = rectBar;
            dPreviousValue = dValue;

            // 값 글: 막대 값 쪽 끝 바깥(들어갈 때만)
            if (bValueLabels)
            {
                const QString strValue = FormatValue(dValue);
                if (metrics.horizontalAdvance(strValue) <= rectBar.width() + 2 * kBarGap + dSlot * (1 - kGroupRatio) / std::max(1, iSeriesCount))
                {
                    const QRectF rectValue = bUp ? QRectF(rectBar.left() - dSlot / 2, rectBar.top() - metrics.height() - kValueLabelGap, rectBar.width() + dSlot,
                                                          metrics.height())
                                                 : QRectF(rectBar.left() - dSlot / 2, rectBar.bottom() + kValueLabelGap, rectBar.width() + dSlot, metrics.height());
                    painter.setFont(fontValue);
                    painter.setPen(m_theme.mutedText);
                    painter.drawText(rectValue, Qt::AlignHCenter | Qt::AlignVCenter, strValue);
                }
            }
        }
    }
}

void KnowledgeChartView::DrawLines(QPainter& painter, const ChartGeometry& geometry, const QVector<QVector<QRectF>>& marks) const
{
    Q_UNUSED(geometry)
    for (int iSeries = 0; iSeries < marks.size(); ++iSeries)
    {
        const QColor color = m_theme.GetSeriesColor(iSeries);
        if (m_spec.eType == EChartType::LINE)
        {
            // 값이 있는 이웃 점끼리 잇는다. 빈 값에서 끊는다
            painter.setPen(QPen(color, kLineWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            painter.setBrush(Qt::NoBrush);
            QPolygonF line;
            for (const QRectF& rectMark : marks[iSeries])
            {
                if (rectMark.isEmpty())
                {
                    if (line.size() > 1)
                        painter.drawPolyline(line);
                    line.clear();
                    continue;
                }
                line << rectMark.center();
            }
            if (line.size() > 1)
                painter.drawPolyline(line);
        }
        for (const QRectF& rectMark : marks[iSeries])
            if (!rectMark.isEmpty())
                DrawMarker(painter, rectMark.center(), rectMark.width() / 2, iSeries);
    }
}

void KnowledgeChartView::DrawMarker(QPainter& painter, const QPointF& center, double radius, int series) const
{
    // 계열마다 모양을 바꿔 색만으로 가르지 않는다: 원 · 네모 · 마름모 · 세모
    painter.setPen(QPen(m_theme.background, kMarkerRing));
    painter.setBrush(m_theme.GetSeriesColor(series));
    switch (((series % kMarkerShapes) + kMarkerShapes) % kMarkerShapes)
    {
    case 0:
        painter.drawEllipse(center, radius, radius);
        break;
    case 1:
        painter.drawRect(QRectF(center.x() - radius * kSquareMarkerRatio, center.y() - radius * kSquareMarkerRatio, radius * (2 * kSquareMarkerRatio),
                                radius * (2 * kSquareMarkerRatio)));
        break;
    case 2:
        painter.drawPolygon(QPolygonF(QVector<QPointF> { center + QPointF(0, -radius * kDiamondMarkerRatio), center + QPointF(radius * kDiamondMarkerRatio, 0),
                                                         center + QPointF(0, radius * kDiamondMarkerRatio), center + QPointF(-radius * kDiamondMarkerRatio, 0) }));
        break;
    default:
        painter.drawPolygon(QPolygonF(QVector<QPointF> { center + QPointF(0, -radius * kTriangleTopRatio),
                                                         center + QPointF(radius * kTriangleHalfWidthRatio, radius * kTriangleBaseRatio),
                                                         center + QPointF(-radius * kTriangleHalfWidthRatio, radius * kTriangleBaseRatio) }));
        break;
    }
}

void KnowledgeChartView::DrawLegend(QPainter& painter, const ChartGeometry& geometry) const
{
    if (geometry.vecLegend.isEmpty())
        return;
    const QFont fontLabel = MakeFont(m_theme.iLabelFontPx);
    painter.setFont(fontLabel);
    for (const LegendItem& item : geometry.vecLegend)
    {
        const double dMidY = item.rect.center().y();
        const QRectF rectSwatch(item.rect.left(), dMidY - kLegendSwatchHeight / 2, kLegendSwatch - kLegendSwatchTextGap, kLegendSwatchHeight);
        if (item.iSeries < 0 || IsBars(m_spec.eType))
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(item.color);
            painter.drawRoundedRect(rectSwatch.adjusted(kLegendBarInset, 0, -kLegendBarInset, 0), kLegendBarRadius, kLegendBarRadius);
        }
        else
        {
            if (m_spec.eType == EChartType::LINE)
            {
                painter.setPen(QPen(item.color, kLineWidth, Qt::SolidLine, Qt::RoundCap));
                painter.drawLine(QPointF(rectSwatch.left(), dMidY), QPointF(rectSwatch.right(), dMidY));
            }
            DrawMarker(painter, rectSwatch.center(), kLineMarkerRadius, item.iSeries);
        }
        painter.setPen(m_theme.text);
        painter.drawText(item.rect.adjusted(kLegendSwatch, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, item.strText);
    }
}

void KnowledgeChartView::DrawMessage(QPainter& painter, const ChartGeometry& geometry) const
{
    const QFont fontTitle = MakeFont(kMessageTitlePx, true);
    const QFont fontLine = MakeFont(m_theme.iCaptionFontPx);
    const QFontMetricsF metricsTitle(fontTitle);
    const QFontMetricsF metricsLine(fontLine);
    const QStringList vecLines = GetMessageText().split(QLatin1Char('\n'));
    // 글 덩어리를 감싼 판을 가운데에 둔다(빈 차트가 아니라 일부러 그리지 않은 것으로 보이게)
    double dTextWidth = metricsTitle.horizontalAdvance(vecLines.first());
    for (int iLine = 1; iLine < vecLines.size(); ++iLine)
        dTextWidth = std::max(dTextWidth, metricsLine.horizontalAdvance(vecLines[iLine]));
    const double dTextHeight = metricsTitle.height() + (vecLines.size() - 1) * (metricsLine.height() + kMessageLineGap) + kSectionGap;
    const double dCardWidth = std::min(dTextWidth + 2 * kMessagePadding, geometry.message.width());
    const double dCardHeight = std::min(dTextHeight + 2 * kMessagePadding, geometry.message.height());
    const QRectF rectCard(geometry.message.center().x() - dCardWidth / 2, geometry.message.center().y() - dCardHeight / 2, dCardWidth, dCardHeight);
    painter.setPen(QPen(m_theme.panelBorder, 1));
    painter.setBrush(m_theme.laneBand);
    painter.drawRoundedRect(rectCard.adjusted(0.5, 0.5, -0.5, -0.5), kMessageRadius, kMessageRadius);

    const QRectF rectText = rectCard.adjusted(kMessagePadding, kMessagePadding, -kMessagePadding, -kMessagePadding);
    double dY = rectCard.center().y() - dTextHeight / 2;
    for (int iLine = 0; iLine < vecLines.size(); ++iLine)
    {
        const bool bTitle = iLine == 0;
        const QFontMetricsF& metrics = bTitle ? metricsTitle : metricsLine;
        painter.setFont(bTitle ? fontTitle : fontLine);
        painter.setPen(bTitle ? m_theme.text : m_theme.mutedText);
        const QRectF rectLine(rectText.left(), dY, rectText.width(), metrics.height());
        painter.drawText(rectLine, Qt::AlignHCenter | Qt::AlignVCenter, metrics.elidedText(vecLines[iLine], Qt::ElideRight, rectLine.width()));
        dY += metrics.height() + (bTitle ? kSectionGap : kMessageLineGap);
    }
}

bool KnowledgeChartView::ShowsMessage() const
{
    if (m_spec.eType == EChartType::NONE || m_spec.vecSeries.isEmpty())
        return true;
    if (m_spec.eType == EChartType::DONUT)
    {
        // 도넛: 0 보다 큰 값이 없으면 "그릴 값이 없다"
        double dTotal = 0;
        ComputeDonutSlices(&dTotal);
        return !(dTotal > 0);
    }
    if (m_spec.eType != EChartType::HEATMAP)
        return false;
    // 히트맵: 값이 있는 칸이 하나도 없으면 "그릴 값이 없다"
    const QVector<QVector<int>> vecCells = ComputeHeatmapCells();
    for (int iSeries = 0; iSeries < vecCells.size(); ++iSeries)
    {
        for (const int iPoint : vecCells[iSeries])
        {
            if (iPoint < 0)
                continue;
            const std::optional<double>& optY = m_spec.vecSeries[iSeries].vecPoints[iPoint].optY;
            if (optY && std::isfinite(*optY))
                return false;
        }
    }
    return true;
}

void KnowledgeChartView::ComputeHeatmapGeometry(ChartGeometry* geometry, double left, double right, double top, double bottom) const
{
    HeatmapGeometry& heatmap = geometry->heatmap;
    const QFontMetricsF metrics(MakeFont(m_theme.iLabelFontPx));
    const double dLineHeight = metrics.height();
    const double dWidth = right - left;

    // 색 범례(한 줄): 색 눈금이 하나면 [y 열 이름] 가장 작은 값 [띠] 가장 큰 값(단위 붙임), 여럿이면 낮음 [띠] 높음 · y 열마다 따로 칠한다
    if (m_spec.vecYAxes.size() == 1)
    {
        const ChartAxis& axis = m_spec.vecYAxes.first();
        heatmap.strLegendTitle = axis.strTitle;
        heatmap.strLegendLow = AppendUnit(FormatValue(axis.dMin), axis.strUnit);
        heatmap.strLegendHigh = AppendUnit(FormatValue(axis.dMax), axis.strUnit);
    }
    else
    {
        heatmap.strLegendLow = kHeatLowLabel;
        heatmap.strLegendHigh = kHeatHighLabel;
        heatmap.strLegendNote = kHeatPerColumnNote;
    }
    // 위젯 너비를 넘지 않게: 띠를 가장 작은 너비까지 줄여도 넘치면 앞의 y 열 이름을 '…' 로 줄이고(남는 칸이 너무 좁으면 뺀다. 이름 전부는 마우스 올림 설명에),
    // 양 끝 값만으로도 넘치는 아주 좁은 위젯에서는 띠를 더 줄인다. 양 끝 값은 줄이지 않는다
    const double dLowWidth = metrics.horizontalAdvance(heatmap.strLegendLow);
    const double dHighWidth = metrics.horizontalAdvance(heatmap.strLegendHigh);
    const double dValuesWidth = dLowWidth + 2 * kHeatLegendTextGap + dHighWidth;
    bool bLegendTitle = !heatmap.strLegendTitle.isEmpty();
    double dTitleTextWidth = bLegendTitle ? metrics.horizontalAdvance(heatmap.strLegendTitle) : 0;
    const double dTitleRoom = dWidth - dValuesWidth - kHeatMinLegendBarWidth - kHeatLegendTextGap;
    if (bLegendTitle && dTitleTextWidth > dTitleRoom)
    {
        bLegendTitle = dTitleRoom >= kHeatMinLegendTitleWidth;
        dTitleTextWidth = bLegendTitle ? std::floor(dTitleRoom) : 0;
    }
    const double dTitleWidth = bLegendTitle ? dTitleTextWidth + kHeatLegendTextGap : 0;
    const double dNoteWidth = heatmap.strLegendNote.isEmpty() ? 0 : kHeatLegendNoteGap + metrics.horizontalAdvance(heatmap.strLegendNote);
    const double dTextsWidth = dTitleWidth + dValuesWidth + dNoteWidth;
    double dBarWidth = std::round(ViewCommon::Clamp(dWidth - dTextsWidth, kHeatMinLegendBarWidth, kHeatLegendBarWidth));
    if (dValuesWidth + dBarWidth > dWidth)
        dBarWidth = std::max(0.0, std::floor(dWidth - dValuesWidth));
    double dX = left;
    if (bLegendTitle)
    {
        heatmap.legendTitle = QRectF(dX, top, dTitleTextWidth, dLineHeight);
        dX += dTitleWidth;
    }
    heatmap.legendLow = QRectF(dX, top, dLowWidth, dLineHeight);
    dX = heatmap.legendLow.right() + kHeatLegendTextGap;
    heatmap.legendBar = QRectF(std::round(dX), std::round(top + (dLineHeight - kHeatLegendBarHeight) / 2), dBarWidth, kHeatLegendBarHeight);
    dX = heatmap.legendBar.right() + kHeatLegendTextGap;
    heatmap.legendHigh = QRectF(dX, top, dHighWidth, dLineHeight);
    dX = heatmap.legendHigh.right();
    if (!heatmap.strLegendNote.isEmpty())
    {
        const double dNoteLeft = dX + kHeatLegendNoteGap;
        const double dNoteRoom = std::max(0.0, std::min(dNoteWidth - kHeatLegendNoteGap, right - dNoteLeft));
        if (dNoteRoom > 0)
            heatmap.legendNote = QRectF(dNoteLeft, top, dNoteRoom, dLineHeight);
    }
    for (const QRectF& rectPart : { heatmap.legendTitle, heatmap.legendLow, heatmap.legendBar, heatmap.legendHigh, heatmap.legendNote })
        if (!rectPart.isEmpty())
            geometry->legend = geometry->legend.united(rectPart);
    double dY = geometry->legend.bottom() + kSectionGap;

    // 행 이름 칸: 가장 긴 이름(그 위 구석의 x 축 이름도)의 너비. 위젯 너비의 30% 까지이고 넘치는 이름은 줄인다
    const QStringList vecRowLabels = ListCategoryLabels();
    const int iRows = std::max(1, static_cast<int>(vecRowLabels.size()));
    const int iColumns = std::max(1, static_cast<int>(m_spec.vecSeries.size()));
    double dRowLabelWidth = metrics.horizontalAdvance(MakeAxisTitle(m_spec.xAxis.strTitle, m_spec.xAxis.strUnit));
    for (const QString& strLabel : vecRowLabels)
        dRowLabelWidth = std::max(dRowLabelWidth, metrics.horizontalAdvance(strLabel));
    dRowLabelWidth = std::min(dRowLabelWidth, std::max(0.0, dWidth * kHeatMaxRowLabelRatio));
    const double dGridLeft = left + dRowLabelWidth + kHeatLabelGap;
    const double dGridWidth = std::max(kMinPlotSize, right - dGridLeft);
    heatmap.dGapX = dGridWidth / iColumns > kHeatGapMinRatio * kHeatCellGap ? kHeatCellGap : 0;
    const double dColumnPitch = (dGridWidth + heatmap.dGapX) / iColumns;

    // 열 이름: 모두 한 줄에 들어가면 한 줄, 아니면 두 줄로 접는다(그래도 넘치면 줄인다)
    heatmap.iHeaderLines = 1;
    for (const ChartSeries& series : m_spec.vecSeries)
        if (metrics.horizontalAdvance(series.strName) > dColumnPitch - heatmap.dGapX)
            heatmap.iHeaderLines = kWrappedLines;
    const double dHeaderHeight = heatmap.iHeaderLines * dLineHeight;
    heatmap.columnLabels = QRectF(dGridLeft, dY, dGridWidth, dHeaderHeight);
    heatmap.corner = QRectF(left, dY, dRowLabelWidth, dHeaderHeight);
    dY += dHeaderHeight + kHeatHeaderGap;

    // 행: 그림 칸 높이를 나누되 행 높이는 위 한계까지. 경계는 정수 화소에 맞춰 칸 가장자리가 흐리지 않게 한다
    const double dAvailable = std::max(kMinPlotSize, bottom - dY);
    heatmap.dGapY = dAvailable / iRows > kHeatGapMinRatio * kHeatCellGap ? kHeatCellGap : 0;
    const double dRowPitch = std::min((dAvailable + heatmap.dGapY) / iRows, kHeatMaxRowPitch);
    for (int iColumn = 0; iColumn <= iColumns; ++iColumn)
        heatmap.vecColumnEdges << std::round(dGridLeft + iColumn * dColumnPitch);
    for (int iRow = 0; iRow <= iRows; ++iRow)
        heatmap.vecRowEdges << std::round(dY + iRow * dRowPitch);
    const double dPlotLeft = heatmap.vecColumnEdges.first();
    const double dPlotTop = heatmap.vecRowEdges.first();
    geometry->plot = QRectF(dPlotLeft, dPlotTop, std::max(1.0, heatmap.vecColumnEdges.last() - heatmap.dGapX - dPlotLeft),
                            std::max(1.0, heatmap.vecRowEdges.last() - heatmap.dGapY - dPlotTop));
    heatmap.rowLabels = QRectF(left, dPlotTop, dRowLabelWidth, geometry->plot.height());
}

QVector<QVector<int>> KnowledgeChartView::ComputeHeatmapCells() const
{
    const int iRows = ListCategoryLabels().size();
    QVector<QVector<int>> vecCells;
    for (const ChartSeries& series : m_spec.vecSeries)
    {
        QVector<int> vecColumn(iRows, -1);
        for (int iPoint = 0; iPoint < series.vecPoints.size(); ++iPoint)
        {
            const int iRow = GetHeatmapRow(series.vecPoints[iPoint], iRows);
            if (iRow >= 0 && vecColumn[iRow] < 0)
                vecColumn[iRow] = iPoint;
        }
        vecCells << vecColumn;
    }
    return vecCells;
}

int KnowledgeChartView::GetHeatmapRow(const ChartPoint& point, int rows) const
{
    if (!std::isfinite(point.dX) || point.dX < 0 || point.dX >= rows || point.dX != std::floor(point.dX))
        return -1;
    return static_cast<int>(point.dX);
}

QRectF KnowledgeChartView::GetHeatmapCellRect(const ChartGeometry& geometry, int row, int column) const
{
    const HeatmapGeometry& heatmap = geometry.heatmap;
    if (row < 0 || column < 0 || row + 1 >= heatmap.vecRowEdges.size() || column + 1 >= heatmap.vecColumnEdges.size())
        return QRectF();
    const double dLeft = heatmap.vecColumnEdges[column];
    const double dTop = heatmap.vecRowEdges[row];
    return QRectF(dLeft, dTop, std::max(0.0, heatmap.vecColumnEdges[column + 1] - heatmap.dGapX - dLeft),
                  std::max(0.0, heatmap.vecRowEdges[row + 1] - heatmap.dGapY - dTop));
}

QPair<int, int> KnowledgeChartView::FindHeatmapCellAt(const ChartGeometry& geometry, const QPointF& pos) const
{
    // 칸 오른쪽 · 아래의 틈은 그 칸으로 본다(틈에서 설명이 깜빡이지 않게)
    const QRectF& rectPlot = geometry.plot;
    const HeatmapGeometry& heatmap = geometry.heatmap;
    if (rectPlot.isEmpty() || heatmap.vecRowEdges.size() < 2 || heatmap.vecColumnEdges.size() < 2 || pos.x() < rectPlot.left() || pos.x() >= rectPlot.right()
        || pos.y() < rectPlot.top() || pos.y() >= rectPlot.bottom())
        return qMakePair(-1, -1);
    const int iColumn = FindEdgeIndex(heatmap.vecColumnEdges, pos.x());
    const int iRow = FindEdgeIndex(heatmap.vecRowEdges, pos.y());
    if (iColumn >= m_spec.vecSeries.size() || iRow >= ListCategoryLabels().size())
        return qMakePair(-1, -1);
    return qMakePair(iRow, iColumn);
}

QColor KnowledgeChartView::GetHeatmapColor(int series, double value) const
{
    // t = (값 − 가장 작은 값) / (가장 큰 값 − 가장 작은 값). 범위가 0 이거나 축이 없으면 가운데
    double dT = kHeatRampMiddle;
    const int iAxis = m_spec.vecSeries[series].iAxis;
    if (iAxis >= 0 && iAxis < m_spec.vecYAxes.size())
        dT = Normalize(value, m_spec.vecYAxes[iAxis].dMin, m_spec.vecYAxes[iAxis].dMax);
    if (!std::isfinite(dT))
        dT = kHeatRampMiddle;
    return m_theme.GetRampColor(dT);
}

QString KnowledgeChartView::MakeHeatmapToolTip(const ChartGeometry& geometry, const QPointF& pos) const
{
    const QStringList vecRowLabels = ListCategoryLabels();
    const HeatmapGeometry& heatmap = geometry.heatmap;
    const QRectF& rectPlot = geometry.plot;

    // 칸: "행 이름 · 열 이름: 값 단위", 값이 없거나 점이 없는 칸은 "값 없음"
    const QPair<int, int> pairCell = FindHeatmapCellAt(geometry, pos);
    if (pairCell.first >= 0)
    {
        const int iRow = pairCell.first;
        const ChartSeries& series = m_spec.vecSeries[pairCell.second];
        const int iPoint = ComputeHeatmapCells()[pairCell.second].value(iRow, -1);
        QString strValue = kMissingLabel;
        if (iPoint >= 0 && series.vecPoints[iPoint].optY && std::isfinite(*series.vecPoints[iPoint].optY))
            strValue = AppendUnit(FormatValue(*series.vecPoints[iPoint].optY), series.strUnit);
        return vecRowLabels.value(iRow) + kHeatJoin + series.strName + QStringLiteral(": ") + strValue;
    }
    // 행 이름 칸(격자 왼쪽) · 열 이름 칸(격자 위): 줄이지 않은 이름 전부
    if (pos.x() >= heatmap.rowLabels.left() && pos.x() < rectPlot.left() && pos.y() >= rectPlot.top() && pos.y() < rectPlot.bottom())
        return vecRowLabels.value(FindEdgeIndex(heatmap.vecRowEdges, pos.y()));
    if (pos.y() >= heatmap.columnLabels.top() && pos.y() < rectPlot.top() && pos.x() >= rectPlot.left() && pos.x() < rectPlot.right())
    {
        const int iColumn = FindEdgeIndex(heatmap.vecColumnEdges, pos.x());
        return iColumn < m_spec.vecSeries.size() ? m_spec.vecSeries[iColumn].strName : QString();
    }
    // 색 범례: 색 눈금마다 이름과 범위
    if (geometry.legend.contains(pos))
    {
        QStringList vecLines;
        for (const ChartAxis& axis : m_spec.vecYAxes)
            vecLines << axis.strTitle + QStringLiteral(": ") + AppendUnit(FormatValue(axis.dMin), axis.strUnit) + QStringLiteral(" – ")
                            + AppendUnit(FormatValue(axis.dMax), axis.strUnit);
        return vecLines.join(QLatin1Char('\n'));
    }
    return QString();
}

void KnowledgeChartView::DrawHeatmapLegend(QPainter& painter, const ChartGeometry& geometry) const
{
    const HeatmapGeometry& heatmap = geometry.heatmap;
    const QFont fontLabel = MakeFont(m_theme.iLabelFontPx);
    const QFontMetricsF metrics(fontLabel);
    painter.setFont(fontLabel);
    if (!heatmap.legendTitle.isEmpty())
    {
        // 범례 앞 y 열 이름: 자리가 모자라면 '…' 로 줄인다(들어가면 그대로)
        painter.setPen(m_theme.mutedText);
        painter.drawText(heatmap.legendTitle, Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(heatmap.strLegendTitle, Qt::ElideRight, heatmap.legendTitle.width()));
    }
    painter.setPen(m_theme.text);
    painter.drawText(heatmap.legendLow, Qt::AlignLeft | Qt::AlignVCenter, heatmap.strLegendLow);
    painter.drawText(heatmap.legendHigh, Qt::AlignLeft | Qt::AlignVCenter, heatmap.strLegendHigh);

    // 띠: 화소 열마다 그 자리의 색(칸과 같은 보간). 왼쪽 끝이 낮음, 오른쪽 끝이 높음
    const QRect rectBar = heatmap.legendBar.toRect();
    const int iBarWidth = rectBar.width();
    for (int iX = 0; iX < iBarWidth; ++iX)
    {
        const double dT = iBarWidth > 1 ? static_cast<double>(iX) / (iBarWidth - 1) : kHeatRampMiddle;
        painter.fillRect(QRect(rectBar.left() + iX, rectBar.top(), 1, rectBar.height()), m_theme.GetRampColor(dT));
    }
    if (!heatmap.legendNote.isEmpty())
    {
        painter.setPen(m_theme.mutedText);
        painter.drawText(heatmap.legendNote, Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(heatmap.strLegendNote, Qt::ElideRight, heatmap.legendNote.width()));
    }
}

void KnowledgeChartView::DrawHeatmap(QPainter& painter, const ChartGeometry& geometry) const
{
    const HeatmapGeometry& heatmap = geometry.heatmap;
    const QStringList vecRowLabels = ListCategoryLabels();
    const QVector<QVector<int>> vecCells = ComputeHeatmapCells();
    const QFont fontLabel = MakeFont(m_theme.iLabelFontPx);
    const QFontMetricsF metrics(fontLabel);
    const double dLineHeight = metrics.height();
    painter.setFont(fontLabel);

    // 구석: x 축 이름(행 이름 칸 위, 격자 쪽으로 맞춘다)
    const QString strCorner = MakeAxisTitle(m_spec.xAxis.strTitle, m_spec.xAxis.strUnit);
    if (!strCorner.isEmpty() && heatmap.corner.width() > 0)
    {
        painter.setPen(m_theme.mutedText);
        painter.drawText(heatmap.corner, Qt::AlignRight | Qt::AlignBottom, metrics.elidedText(strCorner, Qt::ElideRight, heatmap.corner.width()));
    }

    // 열 이름: 그 열 위 가운데(한 줄 또는 두 줄), 마지막 줄이 격자 쪽에 붙는다. 넘치면 줄인다
    painter.setPen(m_theme.text);
    for (int iColumn = 0; iColumn < m_spec.vecSeries.size() && iColumn + 1 < heatmap.vecColumnEdges.size(); ++iColumn)
    {
        const double dLeft = heatmap.vecColumnEdges[iColumn];
        const double dWidth = heatmap.vecColumnEdges[iColumn + 1] - heatmap.dGapX - dLeft;
        if (dWidth <= 0)
            continue;
        const QString& strName = m_spec.vecSeries[iColumn].strName;
        const QStringList vecLines = heatmap.iHeaderLines > 1 ? WrapLabel(strName, metrics, dWidth, heatmap.iHeaderLines)
                                                              : QStringList { metrics.elidedText(strName, Qt::ElideRight, dWidth) };
        const double dTop = heatmap.columnLabels.bottom() - vecLines.size() * dLineHeight;
        for (int iLine = 0; iLine < vecLines.size(); ++iLine)
            painter.drawText(QRectF(dLeft, dTop + iLine * dLineHeight, dWidth, dLineHeight), Qt::AlignHCenter | Qt::AlignVCenter, vecLines[iLine]);
    }

    // 행 이름: 그 행 왼쪽에 오른쪽 맞춤, 넘치면 줄인다. 행이 글 줄보다 낮으면 겹치지 않게 몇 행 건너 하나씩 쓴다
    const int iRowCount = std::min(static_cast<int>(vecRowLabels.size()), static_cast<int>(heatmap.vecRowEdges.size()) - 1);
    const double dRowPitch = iRowCount > 0 ? (heatmap.vecRowEdges.last() - heatmap.vecRowEdges.first()) / iRowCount : dLineHeight;
    const int iRowStride = dRowPitch > 0 ? std::max(1, static_cast<int>(std::ceil(dLineHeight / dRowPitch - kTickTolerance))) : 1;
    for (int iRow = 0; iRow < iRowCount && heatmap.rowLabels.width() > 0; iRow += iRowStride)
    {
        const double dCenter = (heatmap.vecRowEdges[iRow] + heatmap.vecRowEdges[iRow + 1] - heatmap.dGapY) / 2;
        painter.drawText(QRectF(heatmap.rowLabels.left(), dCenter - dLineHeight / 2, heatmap.rowLabels.width(), dLineHeight), Qt::AlignRight | Qt::AlignVCenter,
                         metrics.elidedText(vecRowLabels[iRow], Qt::ElideRight, heatmap.rowLabels.width()));
    }

    // 칸: 값이 있으면 색 눈금 자리 색으로 채우고 값 글(들어갈 때만, 채움과 대비가 큰 글자 색). 빈 칸은 격자 색 1px 테두리와 흐린 "—"
    for (int iColumn = 0; iColumn < vecCells.size(); ++iColumn)
    {
        const ChartSeries& series = m_spec.vecSeries[iColumn];
        for (int iRow = 0; iRow < vecCells[iColumn].size(); ++iRow)
        {
            const QRectF rectCell = GetHeatmapCellRect(geometry, iRow, iColumn);
            if (rectCell.isEmpty())
                continue;
            const int iPoint = vecCells[iColumn][iRow];
            const std::optional<double> optY = iPoint >= 0 ? series.vecPoints[iPoint].optY : std::nullopt;
            const bool bTextFits = dLineHeight <= rectCell.height();
            if (optY && std::isfinite(*optY))
            {
                const QColor fill = GetHeatmapColor(iColumn, *optY);
                painter.fillRect(rectCell, fill);
                const QString strValue = FormatValue(*optY);
                if (bTextFits && metrics.horizontalAdvance(strValue) + 2 * kHeatCellPadding <= rectCell.width())
                {
                    painter.setPen(m_theme.GetFillTextColor(fill));
                    painter.drawText(rectCell, Qt::AlignCenter, strValue);
                }
                continue;
            }
            painter.setPen(QPen(m_theme.grid, 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(rectCell.adjusted(0.5, 0.5, -0.5, -0.5));
            if (bTextFits && metrics.horizontalAdvance(kHeatEmptyCell) + 2 * kHeatCellPadding <= rectCell.width())
            {
                painter.setPen(m_theme.mutedText);
                painter.drawText(rectCell, Qt::AlignCenter, kHeatEmptyCell);
            }
        }
    }
}

void KnowledgeChartView::ComputeDonutGeometry(ChartGeometry* geometry, double left, double right, double top, double bottom) const
{
    // ShowsMessage 가 아니면(0 보다 큰 값이 있으면) 첫 계열이 있다
    DonutGeometry& donut = geometry->donut;
    const QFontMetricsF metrics(MakeFont(m_theme.iLabelFontPx));
    const double dLineHeight = metrics.height();
    const double dWidth = right - left;
    const QVector<ChartPoint>& vecPoints = m_spec.vecSeries.first().vecPoints;
    const QVector<DonutSlice> vecSlices = ComputeDonutSlices(nullptr);

    // 범례 칸 너비: 범주 글(위젯 너비의 30 % 까지, 넘치면 줄인다) · 값과 단위 · 몫. 값이 곧 몫이면(단위 % · 합 100 ± 1) 몫 칸이 없다
    const bool bShare = ShowsDonutShare();
    for (int iPoint = 0; iPoint < vecPoints.size(); ++iPoint)
    {
        donut.dLabelWidth = std::max(donut.dLabelWidth, metrics.horizontalAdvance(vecPoints[iPoint].strLabel));
        donut.dValueWidth = std::max(donut.dValueWidth, metrics.horizontalAdvance(MakeDonutValueText(vecPoints[iPoint])));
        if (bShare && vecSlices[iPoint].bSlice)
            donut.dShareWidth = std::max(donut.dShareWidth, metrics.horizontalAdvance(FormatShare(vecSlices[iPoint].dShare)));
    }
    donut.dLabelWidth = std::min(donut.dLabelWidth, std::max(0.0, dWidth * kDonutMaxLabelRatio));
    double dLegendWidth = kDonutSwatchSize + kDonutSwatchTextGap + donut.dLabelWidth + kDonutColumnGap + donut.dValueWidth;
    if (bShare)
        dLegendWidth += kDonutColumnGap + donut.dShareWidth;

    // 고리: 바깥 지름은 그림 칸(제목 · 밑글 아래, 범례 목록 왼쪽)의 높이 · 너비 가운데 작은 쪽이되 480px 를 넘지 않는다(정수 화소).
    // 고리는 그림 칸 위에 붙인다(가운데로 띄우지 않는다). 안쪽 반지름은 바깥의 60 %.
    // 범례 목록 옆 고리가 60px 보다 작아지는 좁은 위젯에서는 범례 목록을 쓰지 않고 고리만 그린다(위젯 밖으로 나가거나 고리가 점이 되지 않게)
    const double dRingWidth = dWidth - dLegendWidth - kDonutLegendGap;
    const bool bLegend = dRingWidth >= kDonutMinDiameterBesideLegend;
    const double dDiameter = std::max(kMinPlotSize, std::floor(std::min({ bottom - top, bLegend ? dRingWidth : dWidth, kDonutMaxDiameter })));
    geometry->plot = QRectF(left, top, dDiameter, dDiameter);
    donut.center = geometry->plot.center();
    donut.dOuter = dDiameter / 2;
    donut.dInner = donut.dOuter * kDonutInnerRatio;

    // 범례 목록: 고리 오른쪽, 고리 가운데 높이에 맞춘다. 고리보다 높으면 위에 맞추고 아래로 넘치는 줄은 쓰지 않는다
    const int iRows = vecPoints.size();
    const double dLegendHeight = iRows * dLineHeight + std::max(0, iRows - 1) * kDonutRowGap;
    const double dLegendLeft = std::round(geometry->plot.right() + kDonutLegendGap);
    double dRowTop = std::round(std::max(top, donut.center.y() - dLegendHeight / 2));
    for (int iRow = 0; iRow < iRows; ++iRow)
    {
        QRectF rectRow(dLegendLeft, dRowTop, dLegendWidth, dLineHeight);
        if (!bLegend || rectRow.bottom() > bottom)
            rectRow = QRectF();
        donut.vecLegendRows << rectRow;
        if (!rectRow.isEmpty())
            geometry->legend = geometry->legend.united(rectRow);
        dRowTop += dLineHeight + kDonutRowGap;
    }
}

QVector<KnowledgeChartView::DonutSlice> KnowledgeChartView::ComputeDonutSlices(double* total) const
{
    QVector<DonutSlice> vecSlices;
    if (total != nullptr)
        *total = 0;
    if (m_spec.vecSeries.isEmpty())
        return vecSlices;
    const QVector<ChartPoint>& vecPoints = m_spec.vecSeries.first().vecPoints;

    // 몫은 가장 큰 값으로 나눈 값들의 합에 대한 비율로 구한다(아주 큰 값을 더해도 넘치지 않게)
    double dLargest = 0;
    for (const ChartPoint& point : vecPoints)
        if (point.optY && std::isfinite(*point.optY))
            dLargest = std::max(dLargest, *point.optY);
    double dScaledTotal = 0;
    if (dLargest > 0)
        for (const ChartPoint& point : vecPoints)
            if (point.optY && std::isfinite(*point.optY) && *point.optY > 0)
                dScaledTotal += *point.optY / dLargest;

    // 조각은 12시에서 시계 방향, 행(점) 순서로 잇는다. 조각이 아닌 점 · 0 인 점은 각이 0 이다
    double dStart = 0;
    for (const ChartPoint& point : vecPoints)
    {
        DonutSlice slice;
        slice.bSlice = point.optY && std::isfinite(*point.optY) && *point.optY >= 0;
        slice.dStart = dStart;
        if (slice.bSlice && dScaledTotal > 0)
        {
            slice.dShare = *point.optY / dLargest / dScaledTotal;
            slice.dSweep = slice.dShare * kDonutFullTurn;
        }
        dStart += slice.dSweep;
        vecSlices << slice;
    }
    if (total != nullptr)
        *total = dLargest * dScaledTotal;
    return vecSlices;
}

int KnowledgeChartView::FindDonutSliceAt(const ChartGeometry& geometry, const QPointF& pos) const
{
    const DonutGeometry& donut = geometry.donut;
    const double dDistance = QLineF(donut.center, pos).length();
    if (geometry.plot.isEmpty() || dDistance < donut.dInner || dDistance > donut.dOuter)
        return -1;
    // 12시에서 시계 방향으로 잰 각(0 … 360)
    double dAngle = std::atan2(pos.x() - donut.center.x(), donut.center.y() - pos.y()) * kHalfTurnDegrees / kPi;
    if (dAngle < 0)
        dAngle += kDonutFullTurn;
    // 조각 사이 틈은 그 각이 든 조각으로 본다(틈에서 설명이 깜빡이지 않게).
    // 마지막 조각의 끝(계산 오차로 360 에 조금 못 미칠 수 있다)부터 12시까지는 마지막 조각이다
    const QVector<DonutSlice> vecSlices = ComputeDonutSlices(nullptr);
    int iLast = -1;
    for (int iPoint = 0; iPoint < vecSlices.size(); ++iPoint)
    {
        const DonutSlice& slice = vecSlices[iPoint];
        if (!(slice.dSweep > 0))
            continue;
        if (dAngle >= slice.dStart && dAngle < slice.dStart + slice.dSweep)
            return iPoint;
        iLast = iPoint;
    }
    return iLast;
}

QPointF KnowledgeChartView::GetDonutSliceCenter(const ChartGeometry& geometry, const DonutSlice& slice) const
{
    const DonutGeometry& donut = geometry.donut;
    return MapClockPoint(donut.center, (donut.dInner + donut.dOuter) / 2, slice.dStart + slice.dSweep / 2);
}

QString KnowledgeChartView::MakeDonutValueText(const ChartPoint& point) const
{
    if (!point.optY)
        return kMissingLabel;
    return AppendUnit(FormatValue(*point.optY), m_spec.vecSeries.isEmpty() ? QString() : m_spec.vecSeries.first().strUnit);
}

QString KnowledgeChartView::MakeDonutToolTip(int point) const
{
    const QVector<DonutSlice> vecSlices = ComputeDonutSlices(nullptr);
    if (point < 0 || point >= vecSlices.size() || !vecSlices[point].bSlice)
        return QString();
    const ChartPoint& chartPoint = m_spec.vecSeries.first().vecPoints[point];
    // 값이 곧 몫이면(단위 % · 합 100 ± 1) "(전체의 n%)" 를 쓰지 않는다
    if (!ShowsDonutShare())
        return kDonutValueToolTip.arg(chartPoint.strLabel, MakeDonutValueText(chartPoint));
    return kDonutToolTip.arg(chartPoint.strLabel, MakeDonutValueText(chartPoint), FormatShare(vecSlices[point].dShare));
}

bool KnowledgeChartView::ShowsDonutShare() const
{
    double dTotal = 0;
    ComputeDonutSlices(&dTotal);
    const QString strUnit = m_spec.vecSeries.isEmpty() ? QString() : m_spec.vecSeries.first().strUnit;
    return !(ChartShare::IsPercentUnit(strUnit) && ChartShare::IsWholePercent(dTotal));
}

void KnowledgeChartView::DrawDonut(QPainter& painter, const ChartGeometry& geometry) const
{
    const DonutGeometry& donut = geometry.donut;
    const QVector<DonutSlice> vecSlices = ComputeDonutSlices(nullptr);

    // 조각: 12시에서 시계 방향, 행 순서. 색은 조각 자리의 계열 색(조각은 범주의 이름이라 범주색이다)
    painter.setPen(Qt::NoPen);
    int iDrawn = 0;
    for (int iPoint = 0; iPoint < vecSlices.size(); ++iPoint)
    {
        const DonutSlice& slice = vecSlices[iPoint];
        if (!(slice.dSweep > 0))
            continue;
        painter.fillPath(MakeSlicePath(donut.center, donut.dInner, donut.dOuter, slice.dStart, slice.dSweep), m_theme.GetSeriesColor(iPoint));
        ++iDrawn;
    }

    // 조각 사이 2px 바탕 색 틈: 조각이 둘 이상이면 조각마다 시작 각에서 고리를 가로지르는 바탕 색 선
    if (iDrawn > 1)
    {
        painter.setPen(QPen(m_theme.background, kDonutSliceGap, Qt::SolidLine, Qt::FlatCap));
        for (const DonutSlice& slice : vecSlices)
            if (slice.dSweep > 0)
                painter.drawLine(MapClockPoint(donut.center, donut.dInner - kDonutGapOverhang, slice.dStart),
                                 MapClockPoint(donut.center, donut.dOuter + kDonutGapOverhang, slice.dStart));
    }

    // 조각 글: 몫. 20° 이상인 조각에만 고리 띠 가운데 반지름 · 조각 가운데 각에 쓴다. 글 상자를 감싼 원이 띠 안이고 조각 양 끝 반지름 선에
    // 닿지 않을 때만 쓴다(작은 고리에서 조각 밖으로 넘치지 않게). 글자 색은 채움과 대비가 큰 쪽
    const QFont fontLabel = MakeFont(m_theme.iLabelFontPx);
    const QFontMetricsF metrics(fontLabel);
    painter.setFont(fontLabel);
    const double dMiddle = (donut.dInner + donut.dOuter) / 2;
    for (int iPoint = 0; iPoint < vecSlices.size(); ++iPoint)
    {
        const DonutSlice& slice = vecSlices[iPoint];
        if (slice.dSweep + kDonutAngleTolerance < kDonutMinLabelAngle)
            continue;
        const QString strShare = FormatShare(slice.dShare);
        const double dReach = std::hypot(metrics.horizontalAdvance(strShare), metrics.height()) / 2 + kDonutLabelPadding;
        const double dHalfAngle = std::min(slice.dSweep / 2, kDonutQuarterTurn) * kPi / kHalfTurnDegrees;
        if (dReach > (donut.dOuter - donut.dInner) / 2 || dReach > dMiddle * std::sin(dHalfAngle))
            continue;
        const QPointF ptCenter = GetDonutSliceCenter(geometry, slice);
        painter.setPen(m_theme.GetFillTextColor(m_theme.GetSeriesColor(iPoint)));
        painter.drawText(QRectF(ptCenter.x() - dReach, ptCenter.y() - dReach, 2 * dReach, 2 * dReach), Qt::AlignCenter, strShare);
    }
    DrawDonutCenter(painter, geometry);
}

void KnowledgeChartView::DrawDonutCenter(QPainter& painter, const ChartGeometry& geometry) const
{
    // 가운데: y 열 이름(흐린 글자), 그 아래 "합 {전체}{단위}"(굵은 글자). 두 줄이 구멍에 들어갈 때만 쓴다.
    // 합은 제목 크기로 쓰고, 구멍이 작아 들어가지 않으면 이름표 크기로 줄인다(그래도 넘치면 '…' 로 줄인다)
    const DonutGeometry& donut = geometry.donut;
    double dTotal = 0;
    ComputeDonutSlices(&dTotal);
    const ChartSeries& series = m_spec.vecSeries.first();
    const QString strName = m_spec.vecYAxes.isEmpty() ? series.strName : m_spec.vecYAxes.first().strTitle;
    const QString strTotal = kDonutTotalText.arg(AppendUnit(FormatValue(dTotal), series.strUnit));
    const QFont fontName = MakeFont(m_theme.iLabelFontPx);
    const QFontMetricsF metricsName(fontName);
    const bool bName = !strName.isEmpty();
    // 글 줄 너비: 글 덩어리 위 · 아래 끝 높이에서 구멍을 가로지르는 현의 90 %. 덩어리가 구멍보다 높으면 0
    const auto funcLineWidth = [&donut](double halfBlock)
    {
        if (halfBlock >= donut.dInner * kDonutCenterWidthRatio)
            return 0.0;
        return 2 * std::sqrt(donut.dInner * donut.dInner - halfBlock * halfBlock) * kDonutCenterWidthRatio;
    };
    QFont fontTotal = MakeFont(m_theme.iTitleFontPx, true);
    double dHalfBlock = ((bName ? metricsName.height() + kDonutCenterLineGap : 0) + QFontMetricsF(fontTotal).height()) / 2;
    if (QFontMetricsF(fontTotal).horizontalAdvance(strTotal) > funcLineWidth(dHalfBlock))
    {
        fontTotal = MakeFont(m_theme.iLabelFontPx, true);
        dHalfBlock = ((bName ? metricsName.height() + kDonutCenterLineGap : 0) + QFontMetricsF(fontTotal).height()) / 2;
    }
    const QFontMetricsF metricsTotal(fontTotal);
    const double dLineWidth = funcLineWidth(dHalfBlock);
    if (!(dLineWidth > 0))
        return;
    double dY = donut.center.y() - dHalfBlock;
    if (bName)
    {
        painter.setFont(fontName);
        painter.setPen(m_theme.mutedText);
        painter.drawText(QRectF(donut.center.x() - dLineWidth / 2, dY, dLineWidth, metricsName.height()), Qt::AlignCenter,
                         metricsName.elidedText(strName, Qt::ElideRight, dLineWidth));
        dY += metricsName.height() + kDonutCenterLineGap;
    }
    painter.setFont(fontTotal);
    painter.setPen(m_theme.text);
    painter.drawText(QRectF(donut.center.x() - dLineWidth / 2, dY, dLineWidth, metricsTotal.height()), Qt::AlignCenter,
                     metricsTotal.elidedText(strTotal, Qt::ElideRight, dLineWidth));
}

void KnowledgeChartView::DrawDonutLegend(QPainter& painter, const ChartGeometry& geometry) const
{
    // 범례 목록: 줄마다 견본 · 범주 글 · 값과 단위 · 몫. 조각이 아닌 점(값 없음 · 0 보다 작음)은 견본을 비우고(테두리만) 흐린 글자로 쓰며 몫이 없다.
    // 값이 곧 몫이면(단위 % · 합 100 ± 1) 몫 칸을 쓰지 않는다
    const DonutGeometry& donut = geometry.donut;
    const QVector<ChartPoint>& vecPoints = m_spec.vecSeries.first().vecPoints;
    const QVector<DonutSlice> vecSlices = ComputeDonutSlices(nullptr);
    const bool bShare = ShowsDonutShare();
    const QFont fontLabel = MakeFont(m_theme.iLabelFontPx);
    const QFontMetricsF metrics(fontLabel);
    painter.setFont(fontLabel);
    for (int iPoint = 0; iPoint < vecPoints.size() && iPoint < donut.vecLegendRows.size(); ++iPoint)
    {
        const QRectF& rectRow = donut.vecLegendRows[iPoint];
        if (rectRow.isEmpty())
            continue;
        const DonutSlice& slice = vecSlices[iPoint];
        const QRectF rectSwatch(rectRow.left(), std::round(rectRow.center().y() - kDonutSwatchSize / 2), kDonutSwatchSize, kDonutSwatchSize);
        if (slice.bSlice)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_theme.GetSeriesColor(iPoint));
            painter.drawRoundedRect(rectSwatch, kDonutSwatchRadius, kDonutSwatchRadius);
        }
        else
        {
            painter.setPen(QPen(m_theme.axis, 1));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(rectSwatch.adjusted(0.5, 0.5, -0.5, -0.5), kDonutSwatchRadius, kDonutSwatchRadius);
        }
        double dX = rectRow.left() + kDonutSwatchSize + kDonutSwatchTextGap;
        painter.setPen(slice.bSlice ? m_theme.text : m_theme.mutedText);
        painter.drawText(QRectF(dX, rectRow.top(), donut.dLabelWidth, rectRow.height()), Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(vecPoints[iPoint].strLabel, Qt::ElideRight, donut.dLabelWidth));
        dX += donut.dLabelWidth + kDonutColumnGap;
        painter.drawText(QRectF(dX, rectRow.top(), donut.dValueWidth, rectRow.height()), Qt::AlignRight | Qt::AlignVCenter, MakeDonutValueText(vecPoints[iPoint]));
        dX += donut.dValueWidth + kDonutColumnGap;
        if (bShare && slice.bSlice)
        {
            painter.setPen(m_theme.mutedText);
            painter.drawText(QRectF(dX, rectRow.top(), donut.dShareWidth, rectRow.height()), Qt::AlignRight | Qt::AlignVCenter, FormatShare(slice.dShare));
        }
    }
}
