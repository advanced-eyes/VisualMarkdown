#include "VisualMarkdown/UI/KnowledgeGraphView.h"

#include "VisualMarkdown/KnowledgeVocabulary.h"
#include "VisualMarkdown/UI/ViewCommon.h"

#include <QEvent>
#include <QFontMetricsF>
#include <QHelpEvent>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPair>
#include <QResizeEvent>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
// 배치 좌표(확대 1배의 화면 점) 크기
constexpr double kDocumentWidth = 200;       // 문서 점 상자: 제목 두 줄이 들어가게 개념 점보다 크다
constexpr double kDocumentHeight = 52;
constexpr double kLanePaddingX = 24;         // 갈래 띠: 점 상자 범위 왼쪽 · 오른쪽 여백(같게 두어 점들이 띠 가운데에 온다)
constexpr double kScenePadding = 16;
constexpr double kNodeRadius = 7;            // 점 상자 둥근 모서리
constexpr double kNodePaddingX = 6;          // 점 상자 안쪽 여백
constexpr double kDocumentFold = 12;         // 문서 점의 접힌 모서리

// 위젯 좌표(px) 크기
constexpr int kViewMargin = 16;
constexpr int kPanelGap = 12;                // 그림과 판 사이, 판끼리 사이
constexpr int kPanelPadding = 10;
constexpr int kPanelRowHeight = 20;
constexpr int kPanelSwatchWidth = 30;        // 범례 견본 칸(글자 앞)
constexpr int kPanelHeaderGap = 6;           // 둘째 머리 줄부터 그 위에 두는 틈
constexpr int kUnplacedSwatchWidth = 18;
constexpr int kUnplacedMinWidth = 170;
constexpr int kUnplacedMaxWidth = 240;
constexpr double kUnplacedHighlightRadius = 4;   // 고른 목록 줄의 둘레 띠
constexpr double kUnplacedBoxWidth = 12;         // 목록 줄 앞의 종류 색 견본
constexpr double kUnplacedBoxHeight = 10;
constexpr double kUnplacedBoxRadius = 2;
constexpr double kLaneRadius = 6;
constexpr double kLaneLabelPadding = 10;
constexpr double kLaneLabelMinWidth = 64;    // 갈래 이름 칸: 점 상자 왼쪽에 붙는 화면 칸(확대와 상관없이 글자가 읽히게)
constexpr double kLaneLabelMaxWidth = 200;   // 이보다 긴 갈래 이름은 줄임표로 줄인다(설명에 다 나온다)
constexpr double kTickHeaderHeight = 26;     // 타임라인 연도 머리: 갈래 띠 위에 붙는 화면 칸
constexpr double kTickLabelWidth = 80;

// 범례 견본(위젯 좌표, px): 견본 칸(kPanelSwatchWidth) 안의 상자 · 선
constexpr double kSwatchTextGap = 8;          // 견본과 글자 사이(견본 칸 너비에서 뺀다)
constexpr double kSwatchBoxInset = 2;         // 상자 견본의 왼쪽 · 오른쪽 여백
constexpr double kSwatchBoxHeight = 12;
constexpr double kSwatchBoxRadius = 3;
constexpr double kSwatchBoxBorderWidth = 1.2;
constexpr double kSwatchDocumentFold = 5;
constexpr double kSwatchLineWidth = 2;
constexpr double kSwatchLinkWidth = 3;        // 문서 ↔ 개념 견본은 더 굵다(선 굵기가 무게다)
constexpr double kSwatchLineInset = 1;        // 선 견본: 왼쪽 끝과 화살촉 끝의 여백
constexpr double kSwatchUndirectedInset = 2;  // 화살촉 없는 선 견본의 오른쪽 끝 여백

// 확대 · 옮기기
constexpr double kMinZoom = 0.1;
constexpr double kMaxZoom = 6;
constexpr double kMaxFitZoom = 1.0;          // 맞춤은 배치를 1배보다 키우지 않는다(design/Views.md §5: 배치 단위는 화면 점 1개). 휠은 kMaxZoom 까지
constexpr double kWheelStepZoom = 1.2;       // 휠 한 칸(120)에 1.2배
constexpr double kWheelNotch = 120;
constexpr int kDragThreshold = 4;            // 이만큼(px) 움직이면 누르기가 아니라 끌기다

// 글자: 확대에 따라 커지고 작아지되 읽을 수 있는 범위 안에 둔다
constexpr int kMinFontPx = 10;
constexpr int kMaxNodeFontPx = 18;
constexpr double kHideLabelZoom = 0.3;       // 이보다 작게 줄이면 점 이름을 쓰지 않는다

// 선
constexpr double kEdgeWidth = 1.6;
constexpr double kSelectedEdgeWidth = 2.8;
constexpr double kMinLinkWidth = 1.2;        // 문서 ↔ 개념 선: 무게 1 의 굵기
constexpr double kLinkWidthPerWeight = 0.6;
constexpr double kMaxLinkWidth = 6.5;
constexpr double kArrowLength = 9;
constexpr double kArrowHalfWidth = 4.5;
constexpr double kMinArrowScale = 0.8;
constexpr double kMaxArrowScale = 1.6;
constexpr double kArrowTrim = 0.8;           // 선은 화살촉 길이의 이만큼 앞에서 멈춘다(촉 끝이 뭉개지지 않게)
constexpr double kShortArrowRatio = 0.6;     // 짧은 선: 화살촉은 선 길이의 이만큼까지
constexpr double kMinArrowLength = 4;        // 이보다 짧은 화살촉은 그리지 않는다
constexpr double kBorderGap = 2;             // 선 끝과 상자 테두리 사이 틈
constexpr double kParallelGap = 18;          // 같은 두 점 사이 선끼리 벌리는 거리
constexpr double kStackedGapSlack = 8;       // 쌓인 두 상자: 위아래 틈이 화살촉 길이 + 이만큼보다 좁으면 옆으로 휘는 활로 그린다
constexpr double kStackedBowArrows = 2;      // 활이 상자 오른쪽으로 나가는 손잡이 길이: 화살촉 길이의 두 배 + kStackedBowExtra
constexpr double kStackedBowExtra = 4;
constexpr double kSelfLoopRadius = 11;
constexpr double kEdgeHitDistance = 6;
constexpr int kCurveSamples = 24;
constexpr double kBipartiteBend = 0.5;       // 문서 ↔ 개념 곡선: 가로 거리의 이만큼을 손잡이로

// 점
constexpr double kHaloSize = 6;
constexpr double kNodeBorderWidth = 1.3;
constexpr double kNeighborBorderWidth = 2.2;
constexpr double kSelectedRingWidth = 2.4;
constexpr int kDocumentTitleLines = 2;
constexpr double kMinNodeRadius = 3;         // 줄여도 둥근 모서리 · 접힌 모서리 · 안쪽 여백은 이만큼(px) 남긴다
constexpr double kMinDocumentFold = 4;
constexpr double kMinNodePaddingX = 3;
constexpr double kDocumentTextInset = 2;     // 문서 제목 칸은 안쪽 여백보다 이만큼 더 들인다
constexpr int kDocumentFontShrinkPx = 1;     // 문서 제목은 개념 이름보다 이만큼 작은 글자로 쓴다

constexpr int kToolTipWidth = 360;           // 설명의 긴 줄은 이 너비에서 접는다
constexpr double kMinLength = 1e-6;
constexpr double kPi = 3.14159265358979323846;

const QString kImprovesKind = QStringLiteral("improves");   // 개념 설명에 "개선:" 줄로 보이는 관계 종류 코드
const QString kUnplacedTitle = QStringLiteral("연도 없음");
const QString kEmptyMessage = QStringLiteral("그릴 점이 없다");
const QString kOtherKindsLabel = QStringLiteral("그 밖의 종류");   // 어휘에 없는 종류를 묶은 범례 줄
const QString kJoin = QStringLiteral(" · ");
const QString kDirectedArrow = QStringLiteral(" → ");
const QString kUndirectedArrow = QStringLiteral(" ↔ ");
const QString kNoteBullet = QStringLiteral("· ");           // 설명이 여럿이면 줄마다 앞에 붙인다

int ScaleFontPx(int basePx, double zoom, int maxPx)
{
    return static_cast<int>(ViewCommon::Clamp(std::round(basePx * zoom), kMinFontPx, maxPx));
}

double GetLength(const QPointF& vector)
{
    return std::hypot(vector.x(), vector.y());
}

// 상자 안의 점 from 에서 toward 쪽으로 가는 반직선이 상자를 나가는 점(테두리에서 gap 만큼 더). toward 가 상자 안이면 toward
QPointF ClipToBox(const QRectF& box, const QPointF& from, const QPointF& toward, double gap)
{
    const QPointF ptDelta = toward - from;
    double dExit = std::numeric_limits<double>::infinity();
    if (ptDelta.x() > 0)
        dExit = std::min(dExit, (box.right() - from.x()) / ptDelta.x());
    else if (ptDelta.x() < 0)
        dExit = std::min(dExit, (box.left() - from.x()) / ptDelta.x());
    if (ptDelta.y() > 0)
        dExit = std::min(dExit, (box.bottom() - from.y()) / ptDelta.y());
    else if (ptDelta.y() < 0)
        dExit = std::min(dExit, (box.top() - from.y()) / ptDelta.y());
    const double dLength = GetLength(ptDelta);
    if (!std::isfinite(dExit) || dLength < kMinLength)
        return from;
    return from + ptDelta * std::min(dExit + gap / dLength, 1.0);
}

QPolygonF SampleQuad(const QPointF& start, const QPointF& control, const QPointF& end, int samples)
{
    QPolygonF line;
    for (int iSample = 0; iSample <= samples; ++iSample)
    {
        const double dT = static_cast<double>(iSample) / samples;
        const double dU = 1 - dT;
        line << start * (dU * dU) + control * (2 * dU * dT) + end * (dT * dT);
    }
    return line;
}

QPolygonF SampleCubic(const QPointF& start, const QPointF& control1, const QPointF& control2, const QPointF& end, int samples)
{
    QPolygonF line;
    for (int iSample = 0; iSample <= samples; ++iSample)
    {
        const double dT = static_cast<double>(iSample) / samples;
        const double dU = 1 - dT;
        line << start * (dU * dU * dU) + control1 * (3 * dU * dU * dT) + control2 * (3 * dU * dT * dT) + end * (dT * dT * dT);
    }
    return line;
}

double GetPolylineLength(const QPolygonF& line)
{
    double dLength = 0;
    for (int iPoint = 1; iPoint < line.size(); ++iPoint)
        dLength += GetLength(line[iPoint] - line[iPoint - 1]);
    return dLength;
}

// 처음에서 length 만큼 간 점(길이를 넘으면 끝 점)
QPointF GetPointAtLength(const QPolygonF& line, double length)
{
    if (line.isEmpty())
        return QPointF();
    double dRemaining = std::max(0.0, length);
    for (int iPoint = 1; iPoint < line.size(); ++iPoint)
    {
        const double dSegment = GetLength(line[iPoint] - line[iPoint - 1]);
        if (dSegment >= dRemaining && dSegment > kMinLength)
            return line[iPoint - 1] + (line[iPoint] - line[iPoint - 1]) * (dRemaining / dSegment);
        dRemaining -= dSegment;
    }
    return line.last();
}

// 끝에서 length 만큼 줄인다(남는 길이가 없으면 그대로 둔다)
void TrimEnd(QPolygonF* line, double length)
{
    const double dTotal = GetPolylineLength(*line);
    if (dTotal <= length || line->size() < 2)
        return;
    const QPointF ptNewEnd = GetPointAtLength(*line, dTotal - length);
    double dWalked = 0;
    QPolygonF trimmed;
    trimmed << line->first();
    for (int iPoint = 1; iPoint < line->size(); ++iPoint)
    {
        dWalked += GetLength((*line)[iPoint] - (*line)[iPoint - 1]);
        if (dWalked >= dTotal - length)
            break;
        trimmed << (*line)[iPoint];
    }
    trimmed << ptNewEnd;
    *line = trimmed;
}

double GetDistanceToSegment(const QPointF& point, const QPointF& a, const QPointF& b)
{
    const QPointF ptSegment = b - a;
    const double dLengthSquared = QPointF::dotProduct(ptSegment, ptSegment);
    if (dLengthSquared < kMinLength)
        return GetLength(point - a);
    const double dT = ViewCommon::Clamp(QPointF::dotProduct(point - a, ptSegment) / dLengthSquared, 0, 1);
    return GetLength(point - (a + ptSegment * dT));
}

double GetDistanceToPolyline(const QPointF& point, const QPolygonF& line)
{
    double dBest = std::numeric_limits<double>::infinity();
    for (int iPoint = 1; iPoint < line.size(); ++iPoint)
        dBest = std::min(dBest, GetDistanceToSegment(point, line[iPoint - 1], line[iPoint]));
    return dBest;
}

// 끝 tip 을 향하는 화살촉(direction 은 길이 1)
QPolygonF MakeArrow(const QPointF& tip, const QPointF& direction, double length, double halfWidth)
{
    const QPointF ptNormal(-direction.y(), direction.x());
    const QPointF ptBase = tip - direction * length;
    return QPolygonF(QVector<QPointF> { tip, ptBase + ptNormal * halfWidth, ptBase - ptNormal * halfWidth });
}

// 접힌 모서리(오른쪽 위)가 있는 문서 모양
QPainterPath MakeDocumentPath(const QRectF& rect, double fold)
{
    const double dFold = std::min({ fold, rect.width() / 2, rect.height() / 2 });
    QPainterPath path;
    path.moveTo(rect.topLeft());
    path.lineTo(rect.right() - dFold, rect.top());
    path.lineTo(rect.right(), rect.top() + dFold);
    path.lineTo(rect.bottomRight());
    path.lineTo(rect.bottomLeft());
    path.closeSubpath();
    path.moveTo(rect.right() - dFold, rect.top());
    path.lineTo(rect.right() - dFold, rect.top() + dFold);
    path.lineTo(rect.right(), rect.top() + dFold);
    return path;
}

// 글을 width 안의 줄로 접는다(빈칸에서 접고, 한 낱말이 넘치면 글자에서 자른다). maxLines 를 넘으면 마지막 줄을 줄임표로 줄인다
QStringList WrapText(const QString& text, const QFontMetricsF& metrics, double width, int maxLines)
{
    QStringList vecLines;
    QString strLine;
    const auto funcPush = [&vecLines, &strLine]()
    {
        if (!strLine.isEmpty())
            vecLines << strLine;
        strLine.clear();
    };
    for (const QString& strWord : text.split(QLatin1Char(' '), Qt::SkipEmptyParts))
    {
        const QString strTry = strLine.isEmpty() ? strWord : strLine + QLatin1Char(' ') + strWord;
        if (metrics.horizontalAdvance(strTry) <= width)
        {
            strLine = strTry;
            continue;
        }
        funcPush();
        // 한 낱말이 한 줄보다 길면 글자에서 자른다
        QString strPiece;
        for (const QChar chLetter : strWord)
        {
            if (!strPiece.isEmpty() && metrics.horizontalAdvance(strPiece + chLetter) > width)
            {
                vecLines << strPiece;
                strPiece.clear();
            }
            strPiece += chLetter;
        }
        strLine = strPiece;
    }
    funcPush();
    if (maxLines > 0 && vecLines.size() > maxLines)
    {
        const QString strRest = QStringList(vecLines.mid(maxLines - 1)).join(QLatin1Char(' '));
        vecLines = vecLines.mid(0, maxLines - 1);
        vecLines << metrics.elidedText(strRest, Qt::ElideRight, width);
    }
    return vecLines;
}

// 설명의 긴 줄을 설명 글꼴로 접는다
QString WrapForToolTip(const QString& text)
{
    return WrapText(text, QFontMetricsF(QToolTip::font()), kToolTipWidth, 0).join(QLatin1Char('\n'));
}
}

KnowledgeGraphView::KnowledgeGraphView(QWidget* parent)
    : QWidget(parent)
    , m_graph()
    , m_layout()
    , m_eType(EGraphLayoutType::NETWORK)
    , m_theme(VisualMarkdownTheme::MakeLight())
    , m_vecNodeBoxes()
    , m_vecGraphNodes()
    , m_vecParallelIndex()
    , m_vecParallelCount()
    , m_rectLaneArea()
    , m_rectScene()
    , m_dLaneLabelWidth(0)
    , m_vecLegendRows()
    , m_sizeLegend()
    , m_iUnplacedWidth(0)
    , m_dZoom(1)
    , m_ptOffset()
    , m_bAutoFit(true)
    , m_sizeFitted()
    , m_strSelectedId()
    , m_iSelectedNode(-1)
    , m_iSelectedUnplaced(-1)
    , m_setNeighborNodes()
    , m_setSelectedEdges()
    , m_bPressed(false)
    , m_bDragging(false)
    , m_ptPress()
    , m_ptPressOffset()
{
    setAttribute(Qt::WA_OpaquePaintEvent);   // 바탕을 늘 다 칠한다
    RebuildScene();
    ApplyFit();
}

KnowledgeGraphView::~KnowledgeGraphView() = default;

void KnowledgeGraphView::SetGraph(const KnowledgeGraph& graph, const GraphLayoutOutput& layout, EGraphLayoutType type)
{
    m_graph = graph;
    m_layout = layout;
    m_eType = type;
    CancelDrag();   // 끄는 중에 그래프가 바뀌면 끌기를 그만두고 손 모양 커서도 되돌린다
    SelectNode(-1);
    RebuildScene();
    FitToView();
}

void KnowledgeGraphView::SetTheme(const VisualMarkdownTheme& theme)
{
    m_theme = theme;
    BuildLegend();   // 글꼴이 바뀌면 판 크기도 바뀐다
    if (m_bAutoFit)
        ApplyFit();
    update();
}

void KnowledgeGraphView::FitToView()
{
    ApplyFit();
    m_bAutoFit = true;
    update();
}

void KnowledgeGraphView::ClearSelection()
{
    SelectNode(-1);
}

QString KnowledgeGraphView::FindConceptAt(const QPoint& pos) const
{
    const int iNode = FindNodeAt(pos);
    if (iNode >= 0)
        return m_layout.vecNodes[iNode].bDocument ? QString() : m_layout.vecNodes[iNode].strId;
    const int iUnplaced = FindUnplacedAt(pos);
    return iUnplaced >= 0 ? m_layout.vecUnplaced[iUnplaced] : QString();
}

QString KnowledgeGraphView::FindDocumentAt(const QPoint& pos) const
{
    const int iNode = FindNodeAt(pos);
    return iNode >= 0 && m_layout.vecNodes[iNode].bDocument ? m_layout.vecNodes[iNode].strId : QString();
}

int KnowledgeGraphView::FindEdgeAt(const QPoint& pos) const
{
    if (IsOnPanel(pos))
        return -1;
    const QPointF ptPos(pos);
    int iBest = -1;
    double dBest = std::numeric_limits<double>::infinity();
    for (int iEdge = 0; iEdge < m_layout.vecEdges.size(); ++iEdge)
    {
        const EdgeShape shape = BuildEdgeShape(iEdge);
        if (shape.line.size() < 2)
            continue;
        double dDistance = GetDistanceToPolyline(ptPos, shape.line);
        if (!shape.arrow.isEmpty() && shape.arrow.containsPoint(ptPos, Qt::OddEvenFill))
            dDistance = 0;
        if (dDistance <= kEdgeHitDistance && dDistance < dBest)
        {
            dBest = dDistance;
            iBest = iEdge;
        }
    }
    return iBest;
}

QString KnowledgeGraphView::GetToolTipAt(const QPoint& pos) const
{
    if (GetLegendRect().contains(pos))
        return QString();
    const int iUnplaced = FindUnplacedAt(pos);
    if (iUnplaced >= 0)
    {
        const int iGraphNode = m_graph.FindNode(m_layout.vecUnplaced[iUnplaced]);
        return iGraphNode >= 0 ? MakeConceptToolTip(iGraphNode) : m_layout.vecUnplaced[iUnplaced];
    }
    if (GetUnplacedPanelRect().contains(pos))
        return QString();
    const int iNode = FindNodeAt(pos);
    if (iNode >= 0)
    {
        const LayoutNode& node = m_layout.vecNodes[iNode];
        if (node.bDocument)
            return MakeDocumentToolTip(iNode);
        if (m_vecGraphNodes[iNode] >= 0)
            return MakeConceptToolTip(m_vecGraphNodes[iNode]);
        // 그래프에 없는 점: 배치에 있는 것만 보인다
        return node.strKind.isEmpty() ? node.strLabel : node.strLabel + QLatin1Char('\n') + KnowledgeVocabulary::Get().ToConceptLabel(node.strKind);
    }
    const int iEdge = FindEdgeAt(pos);
    if (iEdge >= 0)
        return GetEdgeToolTip(iEdge);
    for (int iLane = 0; iLane < m_layout.vecLanes.size(); ++iLane)
    {
        if (!GetLaneLabelRect(iLane).contains(pos))
            continue;
        const LayoutLane& lane = m_layout.vecLanes[iLane];
        int iMembers = 0;
        for (const LayoutNode& node : m_layout.vecNodes)
            if (node.center.y() >= lane.dTop && node.center.y() <= lane.dBottom)
                ++iMembers;
        // 사용자 글(갈래 이름)을 넣은 뒤에는 .arg 를 다시 부르지 않는다(이름 속 %1 이 바뀌지 않게 한 번에 넘긴다)
        return QStringLiteral("갈래: %1\n점 %2개").arg(lane.strLabel, QString::number(iMembers));
    }
    return QString();
}

QString KnowledgeGraphView::GetEdgeToolTip(int edge) const
{
    if (edge < 0 || edge >= m_layout.vecEdges.size())
        return QString();
    const LayoutEdge& layoutEdge = m_layout.vecEdges[edge];
    const auto funcLayoutName = [this](int node)
    {
        return node >= 0 && node < m_layout.vecNodes.size() ? m_layout.vecNodes[node].strLabel : QString();
    };
    const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
    const QString strArrow = layoutEdge.bDirected ? kDirectedArrow : kUndirectedArrow;
    if (layoutEdge.iGraphEdge < 0 || layoutEdge.iGraphEdge >= m_graph.GetEdges().size())
    {
        // 문서 ↔ 개념 선(또는 그래프에 없는 선): 배치에 있는 것만 보인다
        if (layoutEdge.strKind.isEmpty())
            return QStringLiteral("%1 → %2\n가리킨 횟수 %3")
                .arg(funcLayoutName(layoutEdge.iFrom), funcLayoutName(layoutEdge.iTo), QString::number(layoutEdge.iWeight));
        return funcLayoutName(layoutEdge.iFrom) + strArrow + funcLayoutName(layoutEdge.iTo) + QLatin1Char('\n')
               + vocabulary.ToRelationLabel(layoutEdge.strKind) + kJoin + ViewCommon::GetStatusLabel(layoutEdge.eStatus);
    }

    const KnowledgeGraphEdge& graphEdge = m_graph.GetEdges()[layoutEdge.iGraphEdge];
    const KnowledgeEdge& knowledgeEdge = graphEdge.edge;
    const QVector<KnowledgeGraphNode>& vecNodes = m_graph.GetNodes();
    const auto funcGraphName = [&vecNodes](int node, const QString& fallback)
    {
        return node >= 0 && node < vecNodes.size() ? vecNodes[node].strName : fallback;
    };
    QStringList vecLines;
    vecLines << funcGraphName(graphEdge.iFrom, knowledgeEdge.strFromId) + (graphEdge.bDirected ? kDirectedArrow : kUndirectedArrow)
                    + funcGraphName(graphEdge.iTo, knowledgeEdge.strToId);
    vecLines << vocabulary.ToRelationLabel(knowledgeEdge.strKind) + kJoin + ViewCommon::GetStatusLabel(knowledgeEdge.eStatus);
    for (const QString& strNote : knowledgeEdge.vecNotes)
        vecLines << WrapForToolTip(knowledgeEdge.vecNotes.size() > 1 ? kNoteBullet + strNote : strNote);
    QStringList vecSources;
    if (!knowledgeEdge.vecDocumentIds.isEmpty())
        vecSources << QStringLiteral("문서 %1개").arg(knowledgeEdge.vecDocumentIds.size());
    if (knowledgeEdge.bFromConceptFile)
        vecSources << QStringLiteral("개념 파일");
    if (!vecSources.isEmpty())
        vecLines << QStringLiteral("출처: ") + vecSources.join(QStringLiteral(" + "));
    return vecLines.join(QLatin1Char('\n'));
}

QPointF KnowledgeGraphView::MapToWidget(const QPointF& layoutPoint) const
{
    return layoutPoint * m_dZoom + m_ptOffset;
}

QRectF KnowledgeGraphView::GetNodeRect(int node) const
{
    if (node < 0 || node >= m_vecNodeBoxes.size())
        return QRectF();
    return MapRect(m_vecNodeBoxes[node]);
}

QPointF KnowledgeGraphView::GetEdgeMidpoint(int edge) const
{
    const EdgeShape shape = BuildEdgeShape(edge);
    if (shape.line.size() < 2)
        return QPointF(std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN());
    return GetPointAtLength(shape.line, GetPolylineLength(shape.line) / 2);
}

QRectF KnowledgeGraphView::GetLaneLabelRect(int lane) const
{
    const QRectF rectBand = GetLaneBandRect(lane);
    if (rectBand.isEmpty())
        return QRectF();
    // 띠 왼쪽 끝의 이름 칸(점 상자는 그 오른쪽에서 시작한다)
    const QRectF rectLabel(rectBand.left() + kLaneLabelPadding, rectBand.top(), m_dLaneLabelWidth - 2 * kLaneLabelPadding, rectBand.height());
    return rectLabel.isValid() ? rectLabel : QRectF();
}

QRectF KnowledgeGraphView::GetUnplacedItemRect(int index) const
{
    if (index < 0 || index >= GetVisibleUnplacedCount())
        return QRectF();
    const QRectF rectPanel = GetUnplacedPanelRect();
    return QRectF(rectPanel.left() + kPanelPadding / 2, rectPanel.top() + kPanelPadding + kPanelRowHeight * (index + 1),
                  rectPanel.width() - kPanelPadding, kPanelRowHeight);
}

QString KnowledgeGraphView::GetUnplacedTitle() const
{
    if (m_layout.vecUnplaced.isEmpty())
        return QString();
    return QStringLiteral("%1 (%2)").arg(kUnplacedTitle, QString::number(m_layout.vecUnplaced.size()));
}

QRectF KnowledgeGraphView::GetLegendRect() const
{
    if (m_sizeLegend.isEmpty())
        return QRectF();
    const int iSide = GetSideWidth();
    return QRectF(width() - kViewMargin - iSide, kViewMargin, iSide, m_sizeLegend.height());
}

QRectF KnowledgeGraphView::GetFitRect() const
{
    // 위젯에서 여백 · 오른쪽 판 · 갈래 이름 칸 · 연도 머리를 뺀 칸에 장면을 맞춘다
    QRectF rectFit(kViewMargin, kViewMargin, width() - 2 * kViewMargin, height() - 2 * kViewMargin);
    const int iSide = GetSideWidth();
    if (iSide > 0)
        rectFit.setRight(rectFit.right() - iSide - kPanelGap);
    if (!m_rectLaneArea.isEmpty())
        rectFit.setLeft(rectFit.left() + m_dLaneLabelWidth);
    if (!m_layout.vecTicks.isEmpty())
        rectFit.setTop(rectFit.top() + kTickHeaderHeight);
    rectFit.setWidth(std::max(1.0, rectFit.width()));
    rectFit.setHeight(std::max(1.0, rectFit.height()));
    return rectFit;
}

QPolygonF KnowledgeGraphView::GetEdgeLine(int edge) const
{
    return BuildEdgeShape(edge).line;
}

QPolygonF KnowledgeGraphView::GetEdgeArrow(int edge) const
{
    return BuildEdgeShape(edge).arrow;
}

QString KnowledgeGraphView::GetSelectedId() const
{
    return m_strSelectedId;
}

double KnowledgeGraphView::GetZoom() const
{
    return m_dZoom;
}

bool KnowledgeGraphView::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent* pHelp = static_cast<QHelpEvent*>(event);
        ViewCommon::ShowToolTip(this, pHelp, GetToolTipAt(pHelp->pos()));
        return true;
    }
    return QWidget::event(event);
}

void KnowledgeGraphView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    // 화면에 띄우지 않고 크기만 바꾼 경우(크기 이벤트가 늦게 온다)에도 맞춘 그림을 그린다
    if (m_bAutoFit && m_sizeFitted != size())
        ApplyFit();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(rect(), m_theme.background);
    if (m_layout.vecNodes.isEmpty())
        DrawEmptyMessage(painter);
    DrawLanes(painter);
    DrawTicks(painter);
    DrawEdges(painter);
    DrawNodes(painter);
    DrawLegend(painter);
    DrawUnplaced(painter);
}

void KnowledgeGraphView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (m_bAutoFit)
        ApplyFit();
}

void KnowledgeGraphView::wheelEvent(QWheelEvent* event)
{
    const QPoint ptAngle = event->angleDelta();
    const int iDelta = ptAngle.y() != 0 ? ptAngle.y() : ptAngle.x();
    if (iDelta == 0)
    {
        event->ignore();
        return;
    }
    // 커서 아래 배치 점이 그 자리에 머물게 옮김을 다시 정한다
    const QPointF ptCursor = event->position();
    const QPointF ptLayout = MapToLayout(ptCursor);
    const double dZoom = ViewCommon::Clamp(m_dZoom * std::pow(kWheelStepZoom, iDelta / kWheelNotch), kMinZoom, kMaxZoom);
    m_dZoom = dZoom;
    m_ptOffset = ptCursor - ptLayout * dZoom;
    m_bAutoFit = false;
    update();
    event->accept();
}

void KnowledgeGraphView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }
    m_bPressed = true;
    m_bDragging = false;
    m_ptPress = event->pos();
    m_ptPressOffset = m_ptOffset;
    event->accept();
}

void KnowledgeGraphView::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_bPressed || !(event->buttons() & Qt::LeftButton))
    {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPoint ptDelta = event->pos() - m_ptPress;
    if (!m_bDragging && ptDelta.manhattanLength() >= kDragThreshold)
    {
        m_bDragging = true;
        setCursor(Qt::ClosedHandCursor);
    }
    if (m_bDragging)
    {
        m_ptOffset = m_ptPressOffset + QPointF(ptDelta);
        m_bAutoFit = false;
        update();
    }
    event->accept();
}

void KnowledgeGraphView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_bPressed)
    {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    const bool bClick = !m_bDragging;
    CancelDrag();
    if (bClick)
        HandleClick(event->pos());
    event->accept();
}

void KnowledgeGraphView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
    {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    FitToView();
    event->accept();
}

void KnowledgeGraphView::RebuildScene()
{
    // 점 상자(배치 좌표)와 그래프 점 번호
    m_vecNodeBoxes.clear();
    m_vecGraphNodes.clear();
    QRectF rectContent;
    for (const LayoutNode& node : m_layout.vecNodes)
    {
        const double dWidth = node.bDocument ? kDocumentWidth : GraphLayoutSize::kNodeWidth;
        const double dHeight = node.bDocument ? kDocumentHeight : GraphLayoutSize::kNodeHeight;
        const QRectF rectBox(node.center.x() - dWidth / 2, node.center.y() - dHeight / 2, dWidth, dHeight);
        m_vecNodeBoxes << rectBox;
        m_vecGraphNodes << (node.bDocument ? -1 : m_graph.FindNode(node.strId));
        rectContent = rectContent.united(rectBox);
    }

    // 같은 두 점 사이 선: 자리와 수(곡선으로 벌린다)
    const int iEdgeCount = m_layout.vecEdges.size();
    m_vecParallelIndex.fill(0, iEdgeCount);
    m_vecParallelCount.fill(1, iEdgeCount);
    QMap<QPair<int, int>, QVector<int>> mapPairs;
    for (int iEdge = 0; iEdge < iEdgeCount; ++iEdge)
    {
        const LayoutEdge& edge = m_layout.vecEdges[iEdge];
        mapPairs[qMakePair(std::min(edge.iFrom, edge.iTo), std::max(edge.iFrom, edge.iTo))].push_back(iEdge);
    }
    for (auto it = mapPairs.cbegin(); it != mapPairs.cend(); ++it)
    {
        for (int iIndex = 0; iIndex < it.value().size(); ++iIndex)
        {
            m_vecParallelIndex[it.value()[iIndex]] = iIndex;
            m_vecParallelCount[it.value()[iIndex]] = it.value().size();
        }
    }

    // 갈래 띠(배치 좌표): 점 상자 가로 범위 + 오른쪽 여백. 이름 칸과 연도 머리는 위젯 좌표로 띠 왼쪽 · 위에 붙인다
    m_rectLaneArea = QRectF();
    if (!m_layout.vecLanes.isEmpty() && !rectContent.isEmpty())
    {
        double dTop = std::numeric_limits<double>::max();
        double dBottom = std::numeric_limits<double>::lowest();
        for (const LayoutLane& lane : m_layout.vecLanes)
        {
            dTop = std::min(dTop, lane.dTop);
            dBottom = std::max(dBottom, lane.dBottom);
        }
        m_rectLaneArea = QRectF(QPointF(rectContent.left() - kLanePaddingX, dTop), QPointF(rectContent.right() + kLanePaddingX, dBottom));
        rectContent = rectContent.united(m_rectLaneArea);
    }
    m_rectScene = rectContent.isEmpty() ? QRectF() : rectContent.adjusted(-kScenePadding, -kScenePadding, kScenePadding, kScenePadding);
    BuildLegend();
}

void KnowledgeGraphView::BuildLegend()
{
    m_vecLegendRows.clear();
    const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
    const QStringList vecConceptCodes = vocabulary.GetConceptCodes();
    const QStringList vecRelationCodes = vocabulary.GetRelationCodes();

    // 그리는 점 · 선에 있는 종류만 어휘 순으로 보인다. 어휘에 없는 종류는 "그 밖의 종류" 하나로 묶는다
    bool bDocuments = false;
    QSet<QString> setConceptKinds;
    for (const LayoutNode& node : m_layout.vecNodes)
    {
        if (node.bDocument)
            bDocuments = true;
        else
            setConceptKinds.insert(node.strKind);
    }
    for (const QString& strId : m_layout.vecUnplaced)
    {
        const int iGraphNode = m_graph.FindNode(strId);
        if (iGraphNode >= 0)
            setConceptKinds.insert(m_graph.GetNodes()[iGraphNode].strKind);
    }
    bool bLinks = false;
    QSet<QString> setRelationKinds;
    for (const LayoutEdge& edge : m_layout.vecEdges)
    {
        if (edge.iGraphEdge < 0 && edge.strKind.isEmpty())
            bLinks = true;
        else
            setRelationKinds.insert(edge.strKind);
    }

    if (bDocuments || !setConceptKinds.isEmpty())
    {
        m_vecLegendRows.push_back({ ELegendRow::HEADER, QStringLiteral("점"), QString(), true });
        if (bDocuments)
            m_vecLegendRows.push_back({ ELegendRow::DOCUMENT, QStringLiteral("문서"), QString(), true });
        for (const QString& strCode : vecConceptCodes)
            if (setConceptKinds.contains(strCode))
                m_vecLegendRows.push_back({ ELegendRow::CONCEPT, vocabulary.ToConceptLabel(strCode), strCode, true });
        if (std::any_of(setConceptKinds.cbegin(), setConceptKinds.cend(), [&vecConceptCodes](const QString& kind) { return !vecConceptCodes.contains(kind); }))
            m_vecLegendRows.push_back({ ELegendRow::CONCEPT, kOtherKindsLabel, QString(), true });
    }
    if (bLinks || !setRelationKinds.isEmpty())
    {
        m_vecLegendRows.push_back({ ELegendRow::HEADER, QStringLiteral("선"), QString(), true });
        if (bLinks)
            m_vecLegendRows.push_back({ ELegendRow::LINK, QStringLiteral("가리킴(굵을수록 많이)"), QString(), false });
        for (const QString& strCode : vecRelationCodes)
            if (setRelationKinds.contains(strCode))
                m_vecLegendRows.push_back({ ELegendRow::RELATION, vocabulary.ToRelationLabel(strCode), strCode, !vocabulary.IsUndirected(strCode) });
        if (std::any_of(setRelationKinds.cbegin(), setRelationKinds.cend(), [&vecRelationCodes](const QString& kind) { return !vecRelationCodes.contains(kind); }))
            m_vecLegendRows.push_back({ ELegendRow::RELATION, kOtherKindsLabel, QString(), true });
        if (!setRelationKinds.isEmpty())
        {
            m_vecLegendRows.push_back({ ELegendRow::HEADER, QStringLiteral("상태"), QString(), true });
            m_vecLegendRows.push_back({ ELegendRow::CONFIRMED, ViewCommon::GetStatusLabel(ERelationStatus::CONFIRMED), QString(), false });
            m_vecLegendRows.push_back({ ELegendRow::PROPOSED, ViewCommon::GetStatusLabel(ERelationStatus::PROPOSED), QString(), false });
        }
    }

    // 판 크기(위젯 좌표, 확대와 상관없다)
    m_sizeLegend = QSize();
    if (!m_vecLegendRows.isEmpty())
    {
        const QFontMetricsF metricsLabel(MakeFont(m_theme.iLabelFontPx));
        const QFontMetricsF metricsHeader(MakeFont(m_theme.iSmallFontPx, true));
        double dWidth = 0;
        int iHeight = 2 * kPanelPadding;
        for (int iRow = 0; iRow < m_vecLegendRows.size(); ++iRow)
        {
            const LegendRow& row = m_vecLegendRows[iRow];
            if (row.eType == ELegendRow::HEADER)
            {
                dWidth = std::max(dWidth, metricsHeader.horizontalAdvance(row.strText));
                iHeight += iRow > 0 ? kPanelHeaderGap : 0;
            }
            else
            {
                dWidth = std::max(dWidth, kPanelSwatchWidth + metricsLabel.horizontalAdvance(row.strText));
            }
            iHeight += kPanelRowHeight;
        }
        m_sizeLegend = QSize(static_cast<int>(std::ceil(dWidth)) + 2 * kPanelPadding, iHeight);
    }

    // 연도 없음 목록 너비: 제목과 가장 긴 이름이 들어가게(정한 범위 안)
    m_iUnplacedWidth = 0;
    if (!m_layout.vecUnplaced.isEmpty())
    {
        const QFontMetricsF metricsLabel(MakeFont(m_theme.iLabelFontPx));
        const QFontMetricsF metricsTitle(MakeFont(m_theme.iLabelFontPx, true));
        double dWidth = metricsTitle.horizontalAdvance(GetUnplacedTitle());
        for (const QString& strId : m_layout.vecUnplaced)
        {
            const int iGraphNode = m_graph.FindNode(strId);
            const QString strName = iGraphNode >= 0 ? m_graph.GetNodes()[iGraphNode].strName : strId;
            dWidth = std::max(dWidth, kUnplacedSwatchWidth + metricsLabel.horizontalAdvance(strName));
        }
        m_iUnplacedWidth = static_cast<int>(ViewCommon::Clamp(std::ceil(dWidth) + 2 * kPanelPadding, kUnplacedMinWidth, kUnplacedMaxWidth));
    }

    // 갈래 이름 칸 너비: 가장 긴 갈래 이름이 들어가게(정한 범위 안). 확대와 상관없이 같은 글꼴로 쓴다
    m_dLaneLabelWidth = 0;
    if (!m_rectLaneArea.isEmpty())
    {
        const QFontMetricsF metricsLane(MakeFont(m_theme.iNodeFontPx, true));
        double dWidth = 0;
        for (const LayoutLane& lane : m_layout.vecLanes)
            dWidth = std::max(dWidth, metricsLane.horizontalAdvance(lane.strLabel));
        m_dLaneLabelWidth = ViewCommon::Clamp(std::ceil(dWidth) + 2 * kLaneLabelPadding, kLaneLabelMinWidth, kLaneLabelMaxWidth);
    }
}

void KnowledgeGraphView::ApplyFit()
{
    const QRectF rectFit = GetFitRect();
    m_sizeFitted = size();
    if (m_rectScene.isEmpty())
    {
        m_dZoom = 1;
        m_ptOffset = rectFit.topLeft();
        return;
    }
    m_dZoom = ViewCommon::Clamp(std::min(rectFit.width() / m_rectScene.width(), rectFit.height() / m_rectScene.height()), kMinZoom, kMaxFitZoom);
    m_ptOffset = rectFit.center() - m_rectScene.center() * m_dZoom;
}

void KnowledgeGraphView::SelectNode(int node)
{
    m_iSelectedNode = node >= 0 && node < m_layout.vecNodes.size() ? node : -1;
    m_iSelectedUnplaced = -1;
    m_strSelectedId = m_iSelectedNode >= 0 ? m_layout.vecNodes[m_iSelectedNode].strId : QString();
    m_setNeighborNodes.clear();
    m_setSelectedEdges.clear();
    if (m_iSelectedNode >= 0)
    {
        for (int iEdge = 0; iEdge < m_layout.vecEdges.size(); ++iEdge)
        {
            const LayoutEdge& edge = m_layout.vecEdges[iEdge];
            if (edge.iFrom != m_iSelectedNode && edge.iTo != m_iSelectedNode)
                continue;
            m_setSelectedEdges.insert(iEdge);
            m_setNeighborNodes.insert(edge.iFrom == m_iSelectedNode ? edge.iTo : edge.iFrom);
        }
        m_setNeighborNodes.remove(m_iSelectedNode);
    }
    update();
}

void KnowledgeGraphView::SelectUnplaced(int index)
{
    SelectNode(-1);
    m_iSelectedUnplaced = index;
    m_strSelectedId = m_layout.vecUnplaced.value(index);
    update();
}

void KnowledgeGraphView::HandleClick(const QPoint& pos)
{
    const int iNode = FindNodeAt(pos);
    if (iNode >= 0)
    {
        SelectNode(iNode);
        const LayoutNode& node = m_layout.vecNodes[iNode];
        if (node.bDocument)
            emit DocumentActivated(node.strId);
        else
            emit ConceptActivated(node.strId);
        return;
    }
    const int iUnplaced = FindUnplacedAt(pos);
    if (iUnplaced >= 0)
    {
        SelectUnplaced(iUnplaced);
        emit ConceptActivated(m_layout.vecUnplaced[iUnplaced]);
        return;
    }
    if (IsOnPanel(pos))
        return;   // 범례 · 목록의 빈 곳은 고른 것을 그대로 둔다
    ClearSelection();
}

void KnowledgeGraphView::CancelDrag()
{
    m_bPressed = false;
    if (m_bDragging)
        unsetCursor();
    m_bDragging = false;
}

void KnowledgeGraphView::DrawLanes(QPainter& painter) const
{
    if (m_layout.vecLanes.isEmpty() || m_rectLaneArea.isEmpty())
        return;
    const QFont fontLabel = MakeFont(m_theme.iNodeFontPx, true);
    const QFontMetricsF metrics(fontLabel);
    painter.setFont(fontLabel);
    for (int iLane = 0; iLane < m_layout.vecLanes.size(); ++iLane)
    {
        painter.setPen(Qt::NoPen);
        painter.setBrush(iLane % 2 == 0 ? m_theme.laneBand : m_theme.laneBandAlternate);
        painter.drawRoundedRect(GetLaneBandRect(iLane), kLaneRadius, kLaneRadius);
        const QRectF rectLabel = GetLaneLabelRect(iLane);
        if (rectLabel.isEmpty())
            continue;
        painter.setPen(m_theme.text);
        painter.drawText(rectLabel, Qt::AlignLeft | Qt::AlignVCenter,
                         metrics.elidedText(m_layout.vecLanes[iLane].strLabel, Qt::ElideRight, rectLabel.width()));
    }
}

void KnowledgeGraphView::DrawTicks(QPainter& painter) const
{
    const QRectF rectHeader = GetTickHeaderRect();
    if (m_layout.vecTicks.isEmpty() || rectHeader.isEmpty())
        return;
    painter.setFont(MakeFont(m_theme.iLabelFontPx));
    const double dBottom = MapToWidget(QPointF(0, m_rectLaneArea.isEmpty() ? m_rectScene.bottom() - kScenePadding : m_rectLaneArea.bottom())).y();
    for (const LayoutTick& tick : m_layout.vecTicks)
    {
        const double dX = MapToWidget(QPointF(tick.dPosition, 0)).x();
        painter.setPen(QPen(m_theme.grid, 1));
        painter.drawLine(QPointF(dX, rectHeader.bottom()), QPointF(dX, dBottom));
        painter.setPen(m_theme.mutedText);
        painter.drawText(QRectF(dX - kTickLabelWidth / 2, rectHeader.top(), kTickLabelWidth, rectHeader.height()), Qt::AlignCenter, tick.strLabel);
    }
}

void KnowledgeGraphView::DrawEdges(QPainter& painter) const
{
    const bool bSelection = m_iSelectedNode >= 0;
    // 강조하지 않는 선을 먼저, 강조하는 선을 그 위에 그린다
    for (const bool bHighlightPass : { false, true })
    {
        for (int iEdge = 0; iEdge < m_layout.vecEdges.size(); ++iEdge)
        {
            const bool bHighlighted = m_setSelectedEdges.contains(iEdge);
            if (bHighlighted != bHighlightPass)
                continue;
            const EdgeShape shape = BuildEdgeShape(iEdge);
            if (shape.line.size() < 2)
                continue;
            const LayoutEdge& edge = m_layout.vecEdges[iEdge];
            const bool bLink = edge.iGraphEdge < 0 && edge.strKind.isEmpty();
            const bool bProposed = !bLink && edge.eStatus == ERelationStatus::PROPOSED;
            QColor color = bLink ? m_theme.bipartiteLink : m_theme.GetRelationColor(edge.strKind);
            if (bLink && bHighlighted)
                color = m_theme.mutedText;
            if (bProposed)
                color = m_theme.MakeProposed(color);
            double dWidth = bHighlighted ? kSelectedEdgeWidth : kEdgeWidth;
            if (bLink)
                dWidth = ViewCommon::Clamp(kMinLinkWidth + (edge.iWeight - 1) * kLinkWidthPerWeight, kMinLinkWidth, kMaxLinkWidth);
            painter.setOpacity(bSelection && !bHighlighted ? m_theme.dDimOpacity : 1.0);
            QPen pen(color, dWidth, Qt::SolidLine, bProposed ? Qt::FlatCap : Qt::RoundCap, Qt::RoundJoin);
            if (bProposed && !m_theme.vecProposedDash.isEmpty())
                pen.setDashPattern(m_theme.vecProposedDash);
            else if (bProposed)
                pen.setStyle(Qt::DashLine);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPolyline(shape.line);
            if (!shape.arrow.isEmpty())
            {
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawPolygon(shape.arrow);
            }
        }
    }
    painter.setOpacity(1.0);
}

void KnowledgeGraphView::DrawNodes(QPainter& painter) const
{
    const bool bSelection = m_iSelectedNode >= 0;
    const int iFontPx = GetNodeFontPx();
    const QFont fontConcept = MakeFont(iFontPx);
    const QFont fontDocument = MakeFont(std::max(kMinFontPx, iFontPx - kDocumentFontShrinkPx));
    const QFontMetricsF metricsConcept(fontConcept);
    const QFontMetricsF metricsDocument(fontDocument);
    const bool bLabels = m_dZoom >= kHideLabelZoom;
    const double dRadius = ViewCommon::Clamp(kNodeRadius * m_dZoom, kMinNodeRadius, 2 * kNodeRadius);
    const double dFold = ViewCommon::Clamp(kDocumentFold * m_dZoom, kMinDocumentFold, 2 * kDocumentFold);
    const double dPadding = std::max(kMinNodePaddingX, kNodePaddingX * m_dZoom);
    const QRectF rectWidget(rect());
    for (int iNode = 0; iNode < m_layout.vecNodes.size(); ++iNode)
    {
        const LayoutNode& node = m_layout.vecNodes[iNode];
        const QRectF rectNode = GetNodeRect(iNode);
        if (!rectNode.intersects(rectWidget))
            continue;
        const bool bSelected = iNode == m_iSelectedNode;
        const bool bNeighbor = m_setNeighborNodes.contains(iNode);
        painter.setOpacity(bSelection && !bSelected && !bNeighbor ? m_theme.dDimOpacity : 1.0);
        if (bSelected)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_theme.selectionHalo);
            painter.drawRoundedRect(rectNode.adjusted(-kHaloSize, -kHaloSize, kHaloSize, kHaloSize), dRadius + kHaloSize, dRadius + kHaloSize);
        }
        const VisualMarkdownNodeColors colors = node.bDocument ? m_theme.document : m_theme.GetConceptColors(node.strKind);
        const double dBorder = bSelected ? kSelectedRingWidth : (bNeighbor ? kNeighborBorderWidth : kNodeBorderWidth);
        painter.setPen(QPen(bSelected ? m_theme.selectionRing : colors.border, dBorder));
        painter.setBrush(colors.fill);
        if (node.bDocument)
            painter.drawPath(MakeDocumentPath(rectNode, dFold));
        else
            painter.drawRoundedRect(rectNode, dRadius, dRadius);
        if (!bLabels)
            continue;

        painter.setPen(m_theme.nodeText);
        if (node.bDocument)
        {
            // 문서 제목은 두 줄까지(접힌 모서리를 비켜 쓴다)
            const QRectF rectText = rectNode.adjusted(dPadding + kDocumentTextInset, kDocumentTextInset, -dPadding - dFold / 2, -kDocumentTextInset);
            const QStringList vecLines = WrapText(node.strLabel, metricsDocument, rectText.width(), kDocumentTitleLines);
            const double dLineHeight = metricsDocument.height();
            double dY = rectText.center().y() - dLineHeight * vecLines.size() / 2;
            painter.setFont(fontDocument);
            for (const QString& strLine : vecLines)
            {
                painter.drawText(QRectF(rectText.left(), dY, rectText.width(), dLineHeight), Qt::AlignLeft | Qt::AlignVCenter, strLine);
                dY += dLineHeight;
            }
        }
        else
        {
            const QRectF rectText = rectNode.adjusted(dPadding, 0, -dPadding, 0);
            painter.setFont(fontConcept);
            painter.drawText(rectText, Qt::AlignCenter, metricsConcept.elidedText(node.strLabel, Qt::ElideRight, rectText.width()));
        }
    }
    painter.setOpacity(1.0);
}

void KnowledgeGraphView::DrawLegend(QPainter& painter) const
{
    const QRectF rectLegend = GetLegendRect();
    if (rectLegend.isEmpty())
        return;
    painter.setOpacity(1.0);
    ViewCommon::DrawPanel(painter, rectLegend, m_theme);

    const QFont fontLabel = MakeFont(m_theme.iLabelFontPx);
    const QFont fontHeader = MakeFont(m_theme.iSmallFontPx, true);
    double dY = rectLegend.top() + kPanelPadding;
    for (int iRow = 0; iRow < m_vecLegendRows.size(); ++iRow)
    {
        const LegendRow& row = m_vecLegendRows[iRow];
        if (row.eType == ELegendRow::HEADER && iRow > 0)
            dY += kPanelHeaderGap;
        const QRectF rectRow(rectLegend.left() + kPanelPadding, dY, rectLegend.width() - 2 * kPanelPadding, kPanelRowHeight);
        dY += kPanelRowHeight;
        if (row.eType == ELegendRow::HEADER)
        {
            painter.setFont(fontHeader);
            painter.setPen(m_theme.mutedText);
            painter.drawText(rectRow, Qt::AlignLeft | Qt::AlignVCenter, row.strText);
            continue;
        }
        const QRectF rectSwatch(rectRow.left(), rectRow.top(), kPanelSwatchWidth - kSwatchTextGap, rectRow.height());
        const double dMidY = rectSwatch.center().y();
        const QRectF rectBox(rectSwatch.left() + kSwatchBoxInset, dMidY - kSwatchBoxHeight / 2, rectSwatch.width() - 2 * kSwatchBoxInset, kSwatchBoxHeight);
        switch (row.eType)
        {
        case ELegendRow::DOCUMENT:
            painter.setPen(QPen(m_theme.document.border, 1));
            painter.setBrush(m_theme.document.fill);
            painter.drawPath(MakeDocumentPath(rectBox, kSwatchDocumentFold));
            break;
        case ELegendRow::CONCEPT:
        {
            const VisualMarkdownNodeColors colors = m_theme.GetConceptColors(row.strKind);
            painter.setPen(QPen(colors.border, kSwatchBoxBorderWidth));
            painter.setBrush(colors.fill);
            painter.drawRoundedRect(rectBox, kSwatchBoxRadius, kSwatchBoxRadius);
            break;
        }
        case ELegendRow::RELATION:
        case ELegendRow::LINK:
        case ELegendRow::CONFIRMED:
        case ELegendRow::PROPOSED:
        {
            QColor color = m_theme.confirmedSample;
            double dWidth = kSwatchLineWidth;
            if (row.eType == ELegendRow::RELATION)
                color = m_theme.GetRelationColor(row.strKind);
            else if (row.eType == ELegendRow::LINK)
            {
                color = m_theme.bipartiteLink;
                dWidth = kSwatchLinkWidth;
            }
            else if (row.eType == ELegendRow::PROPOSED)
                color = m_theme.MakeProposed(color);
            QPen pen(color, dWidth, Qt::SolidLine, row.eType == ELegendRow::PROPOSED ? Qt::FlatCap : Qt::RoundCap);
            if (row.eType == ELegendRow::PROPOSED && !m_theme.vecProposedDash.isEmpty())
                pen.setDashPattern(m_theme.vecProposedDash);
            const QPointF ptStart(rectSwatch.left() + kSwatchLineInset, dMidY);
            const QPointF ptEnd(rectSwatch.right() - (row.bDirected ? kSwatchLineInset : kSwatchUndirectedInset), dMidY);
            painter.setPen(pen);
            painter.drawLine(ptStart, row.bDirected ? ptEnd - QPointF(kArrowLength * kArrowTrim, 0) : ptEnd);
            if (row.bDirected)
            {
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawPolygon(MakeArrow(ptEnd, QPointF(1, 0), kArrowLength, kArrowHalfWidth));
            }
            break;
        }
        case ELegendRow::HEADER:
            break;
        }
        painter.setFont(fontLabel);
        painter.setPen(m_theme.text);
        painter.drawText(rectRow.adjusted(kPanelSwatchWidth, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, row.strText);
    }
}

void KnowledgeGraphView::DrawUnplaced(QPainter& painter) const
{
    const QRectF rectPanel = GetUnplacedPanelRect();
    if (rectPanel.isEmpty())
        return;
    painter.setOpacity(1.0);
    ViewCommon::DrawPanel(painter, rectPanel, m_theme);

    const QFont fontTitle = MakeFont(m_theme.iLabelFontPx, true);
    const QFont fontLabel = MakeFont(m_theme.iLabelFontPx);
    const QFontMetricsF metricsLabel(fontLabel);
    painter.setFont(fontTitle);
    painter.setPen(m_theme.text);
    painter.drawText(QRectF(rectPanel.left() + kPanelPadding, rectPanel.top() + kPanelPadding, rectPanel.width() - 2 * kPanelPadding, kPanelRowHeight),
                     Qt::AlignLeft | Qt::AlignVCenter, GetUnplacedTitle());
    painter.setFont(fontLabel);
    const int iVisible = GetVisibleUnplacedCount();
    for (int iItem = 0; iItem < iVisible; ++iItem)
    {
        const QRectF rectItem = GetUnplacedItemRect(iItem);
        const QString& strId = m_layout.vecUnplaced[iItem];
        const int iGraphNode = m_graph.FindNode(strId);
        const QString strName = iGraphNode >= 0 ? m_graph.GetNodes()[iGraphNode].strName : strId;
        const VisualMarkdownNodeColors colors = m_theme.GetConceptColors(iGraphNode >= 0 ? m_graph.GetNodes()[iGraphNode].strKind : QString());
        if (iItem == m_iSelectedUnplaced)
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(m_theme.selectionHalo);
            painter.drawRoundedRect(rectItem, kUnplacedHighlightRadius, kUnplacedHighlightRadius);
        }
        const QRectF rectSwatch(rectItem.left() + kPanelPadding / 2, rectItem.center().y() - kUnplacedBoxHeight / 2, kUnplacedBoxWidth, kUnplacedBoxHeight);
        painter.setPen(QPen(colors.border, 1));
        painter.setBrush(colors.fill);
        painter.drawRoundedRect(rectSwatch, kUnplacedBoxRadius, kUnplacedBoxRadius);
        const QRectF rectText = rectItem.adjusted(kPanelPadding / 2 + kUnplacedSwatchWidth, 0, -kPanelPadding / 2, 0);
        painter.setPen(m_theme.text);
        painter.drawText(rectText, Qt::AlignLeft | Qt::AlignVCenter, metricsLabel.elidedText(strName, Qt::ElideRight, rectText.width()));
    }
    if (iVisible < m_layout.vecUnplaced.size())
    {
        const QRectF rectMore(rectPanel.left() + kPanelPadding, rectPanel.top() + kPanelPadding + kPanelRowHeight * (iVisible + 1),
                              rectPanel.width() - 2 * kPanelPadding, kPanelRowHeight);
        painter.setPen(m_theme.mutedText);
        painter.drawText(rectMore, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("… 외 %1개").arg(m_layout.vecUnplaced.size() - iVisible));
    }
}

void KnowledgeGraphView::DrawEmptyMessage(QPainter& painter) const
{
    painter.setFont(MakeFont(m_theme.iCaptionFontPx));
    painter.setPen(m_theme.mutedText);
    painter.drawText(GetFitRect(), Qt::AlignCenter, kEmptyMessage);
}

QPointF KnowledgeGraphView::MapToLayout(const QPointF& widgetPoint) const
{
    return (widgetPoint - m_ptOffset) / m_dZoom;
}

QRectF KnowledgeGraphView::MapRect(const QRectF& layoutRect) const
{
    return QRectF(MapToWidget(layoutRect.topLeft()), MapToWidget(layoutRect.bottomRight()));
}

KnowledgeGraphView::EdgeShape KnowledgeGraphView::BuildEdgeShape(int edge) const
{
    EdgeShape shape;
    if (edge < 0 || edge >= m_layout.vecEdges.size())
        return shape;
    const LayoutEdge& layoutEdge = m_layout.vecEdges[edge];
    const int iNodeCount = m_vecNodeBoxes.size();
    if (layoutEdge.iFrom < 0 || layoutEdge.iFrom >= iNodeCount || layoutEdge.iTo < 0 || layoutEdge.iTo >= iNodeCount)
        return shape;
    const QRectF rectFrom = GetNodeRect(layoutEdge.iFrom);
    const QRectF rectTo = GetNodeRect(layoutEdge.iTo);
    const double dArrowScale = ViewCommon::Clamp(m_dZoom, kMinArrowScale, kMaxArrowScale);
    const double dArrowLength = kArrowLength * dArrowScale;
    const double dArrowHalfWidth = kArrowHalfWidth * dArrowScale;

    if (layoutEdge.iFrom == layoutEdge.iTo)
    {
        // 제 자신으로 가는 선: 상자 오른쪽 위 모서리에 작은 고리(상자 안쪽은 상자가 덮는다)
        const QPointF ptCenter = rectFrom.topRight();
        for (int iSample = 0; iSample <= kCurveSamples; ++iSample)
        {
            const double dAngle = 2 * kPi * iSample / kCurveSamples;
            shape.line << ptCenter + QPointF(std::cos(dAngle), std::sin(dAngle)) * kSelfLoopRadius;
        }
        return shape;
    }

    const bool bLink = layoutEdge.iGraphEdge < 0 && layoutEdge.strKind.isEmpty();
    // 같은 열에 붙어 쌓인 두 상자: 가로 범위가 겹치고 위아래 틈이 화살촉 + 여유보다 좁다(타임라인 · 계보도의 같은 칸)
    const bool bOverlapX = rectFrom.left() < rectTo.right() && rectTo.left() < rectFrom.right();
    const double dGapY = std::max(rectFrom.top(), rectTo.top()) - std::min(rectFrom.bottom(), rectTo.bottom());
    const bool bStacked = bOverlapX && dGapY < dArrowLength + kStackedGapSlack;
    if (bLink || m_eType == EGraphLayoutType::BIPARTITE)
    {
        // 문서 ↔ 개념: 한 열의 옆 가운데에서 다른 열의 옆 가운데로 부드러운 곡선
        const bool bLeftToRight = rectFrom.center().x() <= rectTo.center().x();
        const QPointF ptStart(bLeftToRight ? rectFrom.right() + kBorderGap : rectFrom.left() - kBorderGap, rectFrom.center().y());
        const QPointF ptEnd(bLeftToRight ? rectTo.left() - kBorderGap : rectTo.right() + kBorderGap, rectTo.center().y());
        const QPointF ptBend((ptEnd.x() - ptStart.x()) * kBipartiteBend, 0);
        shape.line = SampleCubic(ptStart, ptStart + ptBend, ptEnd - ptBend, ptEnd, kCurveSamples);
    }
    else if (bStacked)
    {
        // 좁은 틈에 그리면 화살촉이 보이지 않는다. 출발 상자 오른쪽 가운데에서 나가 대상 상자 오른쪽 가운데로 들어오는 활로 그린다.
        // 같은 두 상자 사이 선이 여럿이면 차례로 더 크게 휜다
        const double dBow = kStackedBowArrows * dArrowLength + kStackedBowExtra + m_vecParallelIndex.value(edge, 0) * kParallelGap;
        const QPointF ptStart(rectFrom.right() + kBorderGap, rectFrom.center().y());
        const QPointF ptEnd(rectTo.right() + kBorderGap, rectTo.center().y());
        shape.line = SampleCubic(ptStart, ptStart + QPointF(dBow, 0), ptEnd + QPointF(dBow, 0), ptEnd, kCurveSamples);
    }
    else
    {
        // 같은 두 점 사이 선이 여럿이면 곡선으로 벌린다. 벌리는 쪽은 점 번호가 작은 쪽 → 큰 쪽 방향을 기준으로 정해 방향이 달라도 겹치지 않는다
        const int iLow = std::min(layoutEdge.iFrom, layoutEdge.iTo);
        const int iHigh = std::max(layoutEdge.iFrom, layoutEdge.iTo);
        const QPointF ptAxis = GetNodeRect(iHigh).center() - GetNodeRect(iLow).center();
        const double dAxisLength = GetLength(ptAxis);
        if (dAxisLength < kMinLength)
            return shape;
        const QPointF ptNormal(-ptAxis.y() / dAxisLength, ptAxis.x() / dAxisLength);
        const int iParallelCount = m_vecParallelCount.value(edge, 1);
        const double dOffset = (m_vecParallelIndex.value(edge, 0) - (iParallelCount - 1) / 2.0) * kParallelGap;
        const QPointF ptFromCenter = rectFrom.center();
        const QPointF ptToCenter = rectTo.center();
        if (std::abs(dOffset) < kMinLength)
        {
            const QPointF ptStart = ClipToBox(rectFrom, ptFromCenter, ptToCenter, kBorderGap);
            const QPointF ptEnd = ClipToBox(rectTo, ptToCenter, ptFromCenter, kBorderGap);
            if (QPointF::dotProduct(ptEnd - ptStart, ptToCenter - ptFromCenter) <= 0)
                return shape;   // 상자가 맞닿아 선을 그을 자리가 없다
            shape.line << ptStart << ptEnd;
        }
        else
        {
            // 2차 곡선의 꼭대기는 손잡이까지의 반이라 손잡이를 벌릴 거리의 두 배에 둔다
            const QPointF ptControl = (ptFromCenter + ptToCenter) / 2 + ptNormal * (2 * dOffset);
            const QPointF ptStart = ClipToBox(rectFrom, ptFromCenter, ptControl, kBorderGap);
            const QPointF ptEnd = ClipToBox(rectTo, ptToCenter, ptControl, kBorderGap);
            shape.line = SampleQuad(ptStart, ptControl, ptEnd, kCurveSamples);
        }
    }

    // 방향 있는 선: to 끝에 화살촉. 선은 촉 안에서 멈춘다. 아주 짧은 선은 촉을 줄여 방향만은 보이게 한다
    if (layoutEdge.bDirected)
    {
        const double dTotal = GetPolylineLength(shape.line);
        const double dLength = std::min(dArrowLength, dTotal * kShortArrowRatio);
        if (dLength >= kMinArrowLength)
        {
            const QPointF ptTip = shape.line.last();
            const QPointF ptBack = GetPointAtLength(shape.line, dTotal - dLength);
            const double dBackLength = GetLength(ptTip - ptBack);
            if (dBackLength > kMinLength)
            {
                shape.arrow = MakeArrow(ptTip, (ptTip - ptBack) / dBackLength, dLength, dArrowHalfWidth * dLength / dArrowLength);
                TrimEnd(&shape.line, dLength * kArrowTrim);
            }
        }
    }
    return shape;
}

int KnowledgeGraphView::FindNodeAt(const QPoint& pos) const
{
    if (IsOnPanel(pos))
        return -1;
    const QPointF ptPos(pos);
    // 나중에 그린 점이 위에 있다
    for (int iNode = m_vecNodeBoxes.size() - 1; iNode >= 0; --iNode)
        if (GetNodeRect(iNode).contains(ptPos))
            return iNode;
    return -1;
}

int KnowledgeGraphView::FindUnplacedAt(const QPoint& pos) const
{
    const int iVisible = GetVisibleUnplacedCount();
    for (int iItem = 0; iItem < iVisible; ++iItem)
        if (GetUnplacedItemRect(iItem).contains(QPointF(pos)))
            return iItem;
    return -1;
}

bool KnowledgeGraphView::IsOnPanel(const QPoint& pos) const
{
    return GetLegendRect().contains(QPointF(pos)) || GetUnplacedPanelRect().contains(QPointF(pos));
}

QRectF KnowledgeGraphView::GetLaneBandRect(int lane) const
{
    if (lane < 0 || lane >= m_layout.vecLanes.size() || m_rectLaneArea.isEmpty())
        return QRectF();
    // 띠는 점 상자 범위(배치 좌표)에 왼쪽 이름 칸(위젯 좌표)을 붙인 것이다
    const LayoutLane& layoutLane = m_layout.vecLanes[lane];
    const QPointF ptTopLeft = MapToWidget(QPointF(m_rectLaneArea.left(), layoutLane.dTop));
    const QPointF ptBottomRight = MapToWidget(QPointF(m_rectLaneArea.right(), layoutLane.dBottom));
    return QRectF(QPointF(ptTopLeft.x() - m_dLaneLabelWidth, ptTopLeft.y()), ptBottomRight);
}

QRectF KnowledgeGraphView::GetTickHeaderRect() const
{
    if (m_layout.vecTicks.isEmpty() || m_rectScene.isEmpty())
        return QRectF();
    // 갈래 띠(없으면 장면) 바로 위의 화면 칸
    const QRectF rectArea = m_rectLaneArea.isEmpty() ? m_rectScene.adjusted(kScenePadding, kScenePadding, -kScenePadding, -kScenePadding) : m_rectLaneArea;
    const QPointF ptTopLeft = MapToWidget(rectArea.topLeft());
    const QPointF ptTopRight = MapToWidget(rectArea.topRight());
    const double dLeft = m_rectLaneArea.isEmpty() ? ptTopLeft.x() : ptTopLeft.x() - m_dLaneLabelWidth;
    return QRectF(dLeft, ptTopLeft.y() - kTickHeaderHeight, ptTopRight.x() - dLeft, kTickHeaderHeight);
}

QRectF KnowledgeGraphView::GetUnplacedPanelRect() const
{
    if (m_layout.vecUnplaced.isEmpty())
        return QRectF();
    const int iSide = GetSideWidth();
    const QRectF rectLegend = GetLegendRect();
    const double dTop = rectLegend.isEmpty() ? kViewMargin : rectLegend.bottom() + kPanelGap;
    const double dNeeded = 2 * kPanelPadding + kPanelRowHeight * (m_layout.vecUnplaced.size() + 1);
    const double dHeight = std::min(dNeeded, height() - kViewMargin - dTop);
    if (dHeight < 2 * kPanelPadding + 2 * kPanelRowHeight)
        return QRectF();   // 제목과 한 줄도 들어가지 않는다
    return QRectF(width() - kViewMargin - iSide, dTop, iSide, dHeight);
}

int KnowledgeGraphView::GetSideWidth() const
{
    int iWidth = m_sizeLegend.isEmpty() ? 0 : m_sizeLegend.width();
    if (!m_layout.vecUnplaced.isEmpty())
        iWidth = std::max(iWidth, m_iUnplacedWidth);
    return iWidth;
}

int KnowledgeGraphView::GetVisibleUnplacedCount() const
{
    const QRectF rectPanel = GetUnplacedPanelRect();
    if (rectPanel.isEmpty())
        return 0;
    const int iTotal = m_layout.vecUnplaced.size();
    const int iRows = static_cast<int>((rectPanel.height() - 2 * kPanelPadding) / kPanelRowHeight) - 1;   // 제목 줄을 뺀다
    if (iRows >= iTotal)
        return iTotal;
    return std::max(0, iRows - 1);   // 마지막 줄에 "… 외 n개" 를 쓴다
}

QFont KnowledgeGraphView::MakeFont(int pixelSize, bool bold) const
{
    return ViewCommon::MakeFont(font(), m_theme, pixelSize, bold);
}

int KnowledgeGraphView::GetNodeFontPx() const
{
    return ScaleFontPx(m_theme.iNodeFontPx, m_dZoom, kMaxNodeFontPx);
}

QString KnowledgeGraphView::MakeConceptToolTip(int graphNode) const
{
    if (graphNode < 0 || graphNode >= m_graph.GetNodes().size())
        return QString();
    const KnowledgeGraphNode& node = m_graph.GetNodes()[graphNode];
    QStringList vecLines { node.strName };
    QStringList vecMeta;
    if (!node.strKind.isEmpty())
        vecMeta << KnowledgeVocabulary::Get().ToConceptLabel(node.strKind);
    if (!node.strYear.isEmpty())
        vecMeta << node.strYear;
    if (!vecMeta.isEmpty())
        vecLines << vecMeta.join(kJoin);
    if (!node.strDefinition.isEmpty())
        vecLines << WrapForToolTip(node.strDefinition);
    // 이 개념이 개선한 것(이 개념이 from 인 개선 선, 그래프 선 순서): "개선: <이름>" 과 그 선의 설명.
    // 선 위에 올리지 않아도 무엇을 고쳤는지 보이게 한다(examples/showcase/README.md §2.2). 보기와 상관없이 같은 글이다
    const QString strImproved = KnowledgeVocabulary::Get().ToRelationLabel(kImprovesKind) + QStringLiteral(": ");
    for (const KnowledgeGraphEdge& graphEdge : m_graph.GetEdges())
    {
        if (graphEdge.iFrom != graphNode || graphEdge.edge.strKind != kImprovesKind)
            continue;
        const bool bKnownTarget = graphEdge.iTo >= 0 && graphEdge.iTo < m_graph.GetNodes().size();
        vecLines << strImproved + (bKnownTarget ? m_graph.GetNodes()[graphEdge.iTo].strName : graphEdge.edge.strToId);
        const QStringList& vecNotes = graphEdge.edge.vecNotes;
        for (const QString& strNote : vecNotes)
            vecLines << WrapForToolTip(vecNotes.size() > 1 ? kNoteBullet + strNote : strNote);
    }
    vecLines << QStringLiteral("문서 %1개").arg(node.iDocumentCount);
    return vecLines.join(QLatin1Char('\n'));
}

QString KnowledgeGraphView::MakeDocumentToolTip(int node) const
{
    if (node < 0 || node >= m_layout.vecNodes.size())
        return QString();
    int iLinks = 0;
    for (const LayoutEdge& edge : m_layout.vecEdges)
        if (edge.iFrom == node || edge.iTo == node)
            ++iLinks;
    return QStringLiteral("%1\n문서 · 개념 %2개").arg(WrapForToolTip(m_layout.vecNodes[node].strLabel), QString::number(iLinks));
}
