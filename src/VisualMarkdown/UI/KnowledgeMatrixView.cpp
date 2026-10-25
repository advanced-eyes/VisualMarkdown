#include "VisualMarkdown/UI/KnowledgeMatrixView.h"

#include "VisualMarkdown/KnowledgeVocabulary.h"
#include "VisualMarkdown/UI/ViewCommon.h"

#include <QEvent>
#include <QFontMetricsF>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QSet>
#include <QTransform>

#include <algorithm>
#include <cmath>

namespace
{
constexpr int kMargin = 16;
constexpr int kPanelGap = 16;            // 행렬과 범례 사이
constexpr int kPanelPadding = 10;
constexpr int kPanelRowHeight = 20;
constexpr int kPanelHeaderGap = 6;
constexpr int kPanelSwatchWidth = 30;
constexpr double kLabelGap = 6;          // 이름표와 칸 사이
constexpr double kMaxLabelWidth = 170;   // 이름표가 이보다 길면 줄임표로 줄인다
constexpr double kColumnLabelAngle = -45;
constexpr double kSqrtHalf = 0.70710678118654752440;   // sin 45° = cos 45°
constexpr double kLabelFontRatio = 0.85; // 이름표 글자는 칸 크기의 이만큼까지
constexpr int kMinLabelFontPx = 9;
constexpr double kLabelHitSlack = 4;     // 열 이름표 맞힘 칸: 글 너비에 더하는 여유
constexpr int kDragThreshold = 4;        // 누른 뒤 이만큼(px) 움직이면 누르기가 아니라 끌기다(KnowledgeGraphView 와 같다)
constexpr double kSelectionBandOpacity = 0.4;   // 고른 행 · 열 띠: 테마 selectionHalo 의 불투명도에 곱한다(옅게 깐다)

// 범례: 머리 줄 셋(관계 · 상태 · 읽는 법)과 견본(견본 칸 안의 작은 칸)
constexpr int kLegendHeaderRows = 3;
constexpr double kSwatchInsetX = 2;      // 견본 칸 왼쪽 여백
constexpr double kSwatchInsetY = 4;      // 줄 위아래 여백
constexpr double kSwatchTextGap = 12;    // 견본과 글자 사이

const QString kEmptyMessage = QStringLiteral("그릴 점이 없다");
const QString kRelationHeader = QStringLiteral("관계");
const QString kStatusHeader = QStringLiteral("상태");
const QString kReadingHeader = QStringLiteral("읽는 법");
const QString kTwoKindsLabel = QStringLiteral("종류 둘: 칸을 나눔");
const QString kRowToColumnLabel = QStringLiteral("행 → 열");
const QString kJoin = QStringLiteral(" · ");
}

KnowledgeMatrixView::KnowledgeMatrixView(QWidget* parent)
    : QWidget(parent)
    , m_graph()
    , m_layout()
    , m_theme(VisualMarkdownTheme::MakeLight())
    , m_vecCellKinds()
    , m_vecBlockEnds()
    , m_sizeLegend()
    , m_strSelectedId()
    , m_iSelectedNode(-1)
    , m_bPressed(false)
    , m_bDragging(false)
    , m_ptPress()
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    RebuildCells();
}

KnowledgeMatrixView::~KnowledgeMatrixView() = default;

void KnowledgeMatrixView::SetGraph(const KnowledgeGraph& graph, const GraphLayoutOutput& layout)
{
    m_graph = graph;
    m_layout = layout;
    // 고른 것과 누르기 상태를 지운다(시그널 없음). 누른 채 그래프가 바뀌면 그 놓기는 누르기가 아니다
    m_bPressed = false;
    m_bDragging = false;
    SelectNode(-1);
    RebuildCells();
    updateGeometry();   // 크기 힌트가 바뀐다(점 수 · 이름표)
    update();
}

void KnowledgeMatrixView::SetTheme(const VisualMarkdownTheme& theme)
{
    m_theme = theme;
    RebuildCells();   // 범례 크기는 글꼴에 따른다
    updateGeometry();
    update();
}

void KnowledgeMatrixView::SelectConcept(const QString& conceptId)
{
    // 시그널을 내지 않는다: 쓰는 쪽이 자기 고르기에 맞추려고 부르므로 되먹임을 막는다. 스크롤 · 칸 자리도 그대로 둔다
    int iNode = -1;
    if (!conceptId.isEmpty())
    {
        for (int iCandidate = 0; iCandidate < m_layout.vecNodes.size(); ++iCandidate)
        {
            if (m_layout.vecNodes[iCandidate].strId == conceptId)
            {
                iNode = iCandidate;
                break;
            }
        }
    }
    if (iNode != m_iSelectedNode)   // 같은 id(또는 고른 것 없이 빈 글 · 모르는 id)면 그대로 둔다
        SelectNode(iNode);
}

QString KnowledgeMatrixView::GetSelectedId() const
{
    return m_strSelectedId;
}

QPair<int, int> KnowledgeMatrixView::FindCellAt(const QPoint& pos) const
{
    const MatrixGeometry geometry = ComputeGeometry();
    const int iOrder = m_layout.vecNodes.size();
    if (iOrder == 0 || geometry.dCell <= 0 || !geometry.grid.contains(QPointF(pos)))
        return qMakePair(-1, -1);
    const int iRow = std::min(iOrder - 1, static_cast<int>((pos.y() - geometry.grid.top()) / geometry.dCell));
    const int iColumn = std::min(iOrder - 1, static_cast<int>((pos.x() - geometry.grid.left()) / geometry.dCell));
    return qMakePair(iRow, iColumn);
}

QString KnowledgeMatrixView::GetToolTipAt(const QPoint& pos) const
{
    const QPair<int, int> pairCell = FindCellAt(pos);
    if (pairCell.first >= 0)
        return MakeCellToolTip(pairCell.first, pairCell.second);
    return MakeLabelToolTip(FindLabelAt(pos));   // 이름표 밖이면 빈 글
}

QRectF KnowledgeMatrixView::GetCellRect(int row, int column) const
{
    const int iOrder = m_layout.vecNodes.size();
    if (row < 0 || column < 0 || row >= iOrder || column >= iOrder)
        return QRectF();
    const MatrixGeometry geometry = ComputeGeometry();
    return QRectF(geometry.grid.left() + column * geometry.dCell, geometry.grid.top() + row * geometry.dCell, geometry.dCell, geometry.dCell);
}

QRectF KnowledgeMatrixView::GetGridRect() const
{
    return ComputeGeometry().grid;
}

QRectF KnowledgeMatrixView::GetLegendRect() const
{
    return ComputeGeometry().legend;
}

QPolygonF KnowledgeMatrixView::GetColumnLabelArea(int column) const
{
    const MatrixGeometry geometry = ComputeGeometry();
    if (column < 0 || column >= m_layout.vecNodes.size() || geometry.dCell <= 0)
        return QPolygonF();
    return MakeColumnLabelArea(geometry, QFontMetricsF(MakeFont(GetLabelFontPx(geometry.dCell))), column);
}

double KnowledgeMatrixView::GetCellSize() const
{
    return ComputeGeometry().dCell;
}

QSize KnowledgeMatrixView::minimumSizeHint() const
{
    const int iOrder = m_layout.vecNodes.size();
    if (iOrder == 0)
        return QSize();
    // ComputeGeometry 를 거꾸로 푼다: 칸 영역(여백 · 이름표 · 넘치는 열 이름표 · 범례 판을 뺀 곳)이 가장 작은 칸 × 점 수가 되는 크기
    const LabelExtent extent = MeasureLabels();
    const double dGridSize = kMinCellSize * iOrder;
    double dWidth = 2 * kMargin + extent.dRowLabelWidth + dGridSize + extent.dOverhang;
    double dHeight = 2 * kMargin + extent.dColumnLabelHeight + dGridSize;
    if (!m_sizeLegend.isEmpty())
    {
        dWidth += kPanelGap + m_sizeLegend.width();
        dHeight = std::max(dHeight, 2.0 * kMargin + m_sizeLegend.height());
    }
    return QSize(static_cast<int>(std::ceil(dWidth)), static_cast<int>(std::ceil(dHeight)));
}

bool KnowledgeMatrixView::event(QEvent* event)
{
    if (event->type() == QEvent::ToolTip)
    {
        QHelpEvent* pHelp = static_cast<QHelpEvent*>(event);
        ViewCommon::ShowToolTip(this, pHelp, GetToolTipAt(pHelp->pos()));
        return true;
    }
    return QWidget::event(event);
}

void KnowledgeMatrixView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
    {
        QWidget::mousePressEvent(event);
        return;
    }
    m_bPressed = true;
    m_bDragging = false;
    m_ptPress = event->pos();
    event->accept();
}

void KnowledgeMatrixView::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_bPressed || !(event->buttons() & Qt::LeftButton))
    {
        QWidget::mouseMoveEvent(event);
        return;
    }
    if ((event->pos() - m_ptPress).manhattanLength() >= kDragThreshold)
        m_bDragging = true;
    event->accept();
}

void KnowledgeMatrixView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || !m_bPressed)
    {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    // 끌기(움직임 이벤트 없이 멀리서 놓은 것도)는 누르기가 아니다
    const bool bClick = !m_bDragging && (event->pos() - m_ptPress).manhattanLength() < kDragThreshold;
    m_bPressed = false;
    m_bDragging = false;
    if (bClick)
        HandleClick(event->pos());
    event->accept();
}

void KnowledgeMatrixView::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton)
    {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    // QWidget 은 두 번째 누르기를 mousePressEvent 로 넘기지만, 첫 누르기에서 이미 고르기 · 시그널을 했으므로 다시 하지 않는다
    event->accept();
}

void KnowledgeMatrixView::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(rect(), m_theme.background);
    const int iOrder = m_layout.vecNodes.size();
    const MatrixGeometry geometry = ComputeGeometry();
    if (iOrder == 0 || geometry.dCell <= 0)
    {
        painter.setFont(MakeFont(m_theme.iCaptionFontPx));
        painter.setPen(m_theme.mutedText);
        painter.drawText(rect(), Qt::AlignCenter, kEmptyMessage);
        return;
    }
    const double dCell = geometry.dCell;
    const QRectF& rectGrid = geometry.grid;

    // 고른 개념의 행 · 열 띠: 칸 색 · 대각선 · 격자 아래에 옅게 깐다. 겹치는 칸(제 자신의 대각선)은 한 번만 칠한다. 다른 행 · 열은 흐리지 않는다
    if (m_iSelectedNode >= 0 && m_iSelectedNode < iOrder)
    {
        // 테마 고름 후광 색을 더 옅게 쓴다(띠가 칸 색처럼 읽히지 않게)
        QColor colorBand = m_theme.selectionHalo;
        colorBand.setAlphaF(colorBand.alphaF() * kSelectionBandOpacity);
        const double dBandTop = rectGrid.top() + m_iSelectedNode * dCell;
        const double dBandLeft = rectGrid.left() + m_iSelectedNode * dCell;
        painter.fillRect(QRectF(rectGrid.left(), dBandTop, rectGrid.width(), dCell), colorBand);
        painter.fillRect(QRectF(dBandLeft, rectGrid.top(), dCell, dBandTop - rectGrid.top()), colorBand);
        painter.fillRect(QRectF(dBandLeft, dBandTop + dCell, dCell, rectGrid.bottom() - dBandTop - dCell), colorBand);
    }

    // 대각선(제 자신)은 옅게 칠해 행 · 열을 맞춰 보기 쉽게 한다. 채워진 칸은 종류 색
    painter.setPen(Qt::NoPen);
    for (int iDiagonal = 0; iDiagonal < iOrder; ++iDiagonal)
        painter.fillRect(QRectF(rectGrid.left() + iDiagonal * dCell, rectGrid.top() + iDiagonal * dCell, dCell, dCell), m_theme.grid);
    for (int iRow = 0; iRow < iOrder; ++iRow)
    {
        for (int iColumn = 0; iColumn < iOrder; ++iColumn)
        {
            const QVector<CellKind>& vecKinds = m_vecCellKinds[iRow * iOrder + iColumn];
            if (!vecKinds.isEmpty())
                DrawCell(painter, QRectF(rectGrid.left() + iColumn * dCell, rectGrid.top() + iRow * dCell, dCell, dCell), vecKinds);
        }
    }

    // 격자(가는 선)와 덩어리 경계(축 색)
    painter.setRenderHint(QPainter::Antialiasing, false);
    for (int iLine = 0; iLine <= iOrder; ++iLine)
    {
        const bool bBlock = iLine == 0 || iLine == iOrder || m_vecBlockEnds.contains(iLine);
        painter.setPen(QPen(bBlock ? m_theme.axis : m_theme.grid, 1));
        const double dX = rectGrid.left() + iLine * dCell;
        const double dY = rectGrid.top() + iLine * dCell;
        painter.drawLine(QPointF(dX, rectGrid.top()), QPointF(dX, rectGrid.bottom()));
        painter.drawLine(QPointF(rectGrid.left(), dY), QPointF(rectGrid.right(), dY));
    }
    painter.setRenderHint(QPainter::Antialiasing, true);

    // 행 이름표(오른쪽 맞춤)와 열 이름표(45° 기울임)
    const QFont fontLabel = MakeFont(GetLabelFontPx(dCell));
    const QFontMetricsF metrics(fontLabel);
    painter.setFont(fontLabel);
    painter.setPen(m_theme.text);
    const double dLabelWidth = geometry.dRowLabelWidth - kLabelGap;
    for (int iNode = 0; iNode < iOrder; ++iNode)
    {
        // 고른 개념의 이름표는 같은 글(보통 글꼴로 줄인 이름)을 고름 색 · 굵게 쓴다. 굵은 글이 조금 넓어도 자르지 않는다
        const bool bSelected = iNode == m_iSelectedNode;
        const int iClipFlag = bSelected ? Qt::TextDontClip : 0;
        if (bSelected)
        {
            painter.setFont(MakeFont(GetLabelFontPx(dCell), true));
            painter.setPen(m_theme.selectionRing);
        }
        const QString strName = metrics.elidedText(GetNodeName(iNode), Qt::ElideRight, dLabelWidth);
        painter.drawText(QRectF(rectGrid.left() - geometry.dRowLabelWidth, rectGrid.top() + iNode * dCell, dLabelWidth, dCell),
                         Qt::AlignRight | Qt::AlignVCenter | iClipFlag, strName);
        painter.save();
        painter.translate(rectGrid.left() + (iNode + 0.5) * dCell, rectGrid.top() - kLabelGap);
        painter.rotate(kColumnLabelAngle);
        painter.drawText(QRectF(0, -dCell / 2, kMaxLabelWidth, dCell), Qt::AlignLeft | Qt::AlignVCenter | iClipFlag,
                         metrics.elidedText(GetNodeName(iNode), Qt::ElideRight, kMaxLabelWidth));
        painter.restore();
        if (bSelected)
        {
            painter.setFont(fontLabel);
            painter.setPen(m_theme.text);
        }
    }

    DrawLegend(painter, geometry.legend);
}

void KnowledgeMatrixView::RebuildCells()
{
    const int iOrder = m_layout.vecNodes.size();
    m_vecCellKinds = QVector<QVector<CellKind>>(iOrder * iOrder);
    for (const LayoutMatrixCell& cell : m_layout.vecCells)
    {
        if (cell.iRow < 0 || cell.iColumn < 0 || cell.iRow >= iOrder || cell.iColumn >= iOrder)
            continue;
        const QVector<int> vecEdges = FindCellEdges(cell.iRow, cell.iColumn);
        QVector<CellKind>& vecKinds = m_vecCellKinds[cell.iRow * iOrder + cell.iColumn];
        for (const QString& strKind : cell.vecKinds)
        {
            CellKind kind;
            kind.strKind = strKind;
            // 칸에 그 종류 선이 하나도 없으면(배치만 받은 경우) 확인됨으로 본다
            bool bAny = false;
            for (const int iEdge : vecEdges)
            {
                const LayoutEdge& edge = m_layout.vecEdges[iEdge];
                if (edge.strKind != strKind)
                    continue;
                bAny = true;
                kind.bConfirmed = kind.bConfirmed || edge.eStatus == ERelationStatus::CONFIRMED;
            }
            if (!bAny)
                kind.bConfirmed = true;
            vecKinds << kind;
        }
    }

    // 덩어리 경계: 차례로 놓인 두 점의 덩어리가 다르면 그 사이
    m_vecBlockEnds.clear();
    for (int iNode = 1; iNode < iOrder; ++iNode)
    {
        const int iPrevious = m_graph.FindNode(m_layout.vecNodes[iNode - 1].strId);
        const int iCurrent = m_graph.FindNode(m_layout.vecNodes[iNode].strId);
        if (iPrevious >= 0 && iCurrent >= 0 && m_graph.GetNodes()[iPrevious].iComponent != m_graph.GetNodes()[iCurrent].iComponent)
            m_vecBlockEnds << iNode;
    }

    // 범례 크기: 머리 줄 · 종류 · 상태 · 칸 나눔 · 읽는 법
    m_sizeLegend = QSize();
    const QStringList vecKinds = ListLegendKinds();
    if (vecKinds.isEmpty())
        return;
    const QFontMetricsF metricsLabel(MakeFont(m_theme.iLabelFontPx));
    const QFontMetricsF metricsHeader(MakeFont(m_theme.iSmallFontPx, true));
    const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
    QStringList vecLabels;
    for (const QString& strKind : vecKinds)
        vecLabels << vocabulary.ToRelationLabel(strKind);
    vecLabels << ViewCommon::GetStatusLabel(ERelationStatus::CONFIRMED) << ViewCommon::GetStatusLabel(ERelationStatus::PROPOSED) << kTwoKindsLabel
              << kRowToColumnLabel;
    double dWidth = std::max({ metricsHeader.horizontalAdvance(kRelationHeader), metricsHeader.horizontalAdvance(kStatusHeader),
                               metricsHeader.horizontalAdvance(kReadingHeader) });
    for (const QString& strLabel : vecLabels)
        dWidth = std::max(dWidth, kPanelSwatchWidth + metricsLabel.horizontalAdvance(strLabel));
    // 머리 줄 사이마다(첫 머리 줄 위는 빼고) 틈을 둔다
    const int iRows = kLegendHeaderRows + vecLabels.size();
    m_sizeLegend = QSize(static_cast<int>(std::ceil(dWidth)) + 2 * kPanelPadding,
                         2 * kPanelPadding + iRows * kPanelRowHeight + (kLegendHeaderRows - 1) * kPanelHeaderGap);
}

KnowledgeMatrixView::MatrixGeometry KnowledgeMatrixView::ComputeGeometry() const
{
    MatrixGeometry geometry;
    const int iOrder = m_layout.vecNodes.size();
    if (!m_sizeLegend.isEmpty())
        geometry.legend = QRectF(width() - kMargin - m_sizeLegend.width(), kMargin, m_sizeLegend.width(), m_sizeLegend.height());
    if (iOrder == 0)
        return geometry;

    const LabelExtent extent = MeasureLabels();
    geometry.dRowLabelWidth = extent.dRowLabelWidth;
    const double dColumnLabelHeight = extent.dColumnLabelHeight;
    const double dOverhang = extent.dOverhang;

    const double dAreaLeft = kMargin;
    const double dAreaRight = geometry.legend.isEmpty() ? width() - kMargin : geometry.legend.left() - kPanelGap;
    const double dAreaTop = kMargin;
    const double dAreaBottom = height() - kMargin;
    const double dAvailableWidth = dAreaRight - dAreaLeft - geometry.dRowLabelWidth - dOverhang;
    const double dAvailableHeight = dAreaBottom - dAreaTop - dColumnLabelHeight;
    geometry.dCell = ViewCommon::Clamp(std::floor(std::min(dAvailableWidth, dAvailableHeight) / iOrder), kMinCellSize, kMaxCellSize);

    // 이름표 · 칸 · 넘치는 열 이름표를 합친 덩어리를 가로 가운데에 둔다(넘치면 왼쪽에 붙인다)
    const double dGridSize = geometry.dCell * iOrder;
    const double dBlockWidth = geometry.dRowLabelWidth + dGridSize + dOverhang;
    const double dBlockLeft = dAreaLeft + std::max(0.0, (dAreaRight - dAreaLeft - dBlockWidth) / 2);
    geometry.grid = QRectF(dBlockLeft + geometry.dRowLabelWidth, dAreaTop + dColumnLabelHeight, dGridSize, dGridSize);
    return geometry;
}

KnowledgeMatrixView::LabelExtent KnowledgeMatrixView::MeasureLabels() const
{
    // 이름표 너비는 가장 긴 이름(정한 너비까지). 열 이름표는 45° 라 위로 너비 × sin45, 오른쪽으로 너비 × cos45 를 차지한다
    const QFontMetricsF metrics(MakeFont(m_theme.iLabelFontPx));
    double dLabel = 0;
    for (int iNode = 0; iNode < m_layout.vecNodes.size(); ++iNode)
        dLabel = std::max(dLabel, metrics.horizontalAdvance(GetNodeName(iNode)));
    dLabel = std::min(dLabel, kMaxLabelWidth);
    LabelExtent extent;
    extent.dRowLabelWidth = dLabel + kLabelGap;
    extent.dColumnLabelHeight = (dLabel + metrics.height()) * kSqrtHalf + kLabelGap;
    extent.dOverhang = dLabel * kSqrtHalf;
    return extent;
}

QVector<int> KnowledgeMatrixView::FindCellEdges(int row, int column) const
{
    QVector<int> vecEdges;
    for (int iEdge = 0; iEdge < m_layout.vecEdges.size(); ++iEdge)
    {
        const LayoutEdge& edge = m_layout.vecEdges[iEdge];
        if ((edge.iFrom == row && edge.iTo == column) || (!edge.bDirected && edge.iFrom == column && edge.iTo == row))
            vecEdges << iEdge;
    }
    return vecEdges;
}

QString KnowledgeMatrixView::MakeCellToolTip(int row, int column) const
{
    const int iOrder = m_layout.vecNodes.size();
    if (row < 0 || column < 0 || row >= iOrder || column >= iOrder || m_vecCellKinds[row * iOrder + column].isEmpty())
        return QString();
    const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
    QStringList vecLines { GetNodeName(row) + QStringLiteral(" → ") + GetNodeName(column) };
    const QVector<int> vecEdges = FindCellEdges(row, column);
    if (vecEdges.isEmpty())
    {
        for (const CellKind& kind : m_vecCellKinds[row * iOrder + column])
            vecLines << vocabulary.ToRelationLabel(kind.strKind) + kJoin
                            + ViewCommon::GetStatusLabel(kind.bConfirmed ? ERelationStatus::CONFIRMED : ERelationStatus::PROPOSED);
        return vecLines.join(QLatin1Char('\n'));
    }
    for (const int iEdge : vecEdges)
    {
        const LayoutEdge& edge = m_layout.vecEdges[iEdge];
        QString strLine = vocabulary.ToRelationLabel(edge.strKind) + kJoin + ViewCommon::GetStatusLabel(edge.eStatus);
        if (!edge.bDirected)
            strLine += QStringLiteral(" (방향 없음)");
        if (edge.iGraphEdge >= 0 && edge.iGraphEdge < m_graph.GetEdges().size())
        {
            const KnowledgeEdge& knowledgeEdge = m_graph.GetEdges()[edge.iGraphEdge].edge;
            QStringList vecSources;
            if (!knowledgeEdge.vecDocumentIds.isEmpty())
                vecSources << QStringLiteral("문서 %1개").arg(knowledgeEdge.vecDocumentIds.size());
            if (knowledgeEdge.bFromConceptFile)
                vecSources << QStringLiteral("개념 파일");
            if (!vecSources.isEmpty())
                strLine += kJoin + vecSources.join(QStringLiteral(" + "));
        }
        vecLines << strLine;
    }
    return vecLines.join(QLatin1Char('\n'));
}

QString KnowledgeMatrixView::MakeLabelToolTip(int node) const
{
    if (node < 0 || node >= m_layout.vecNodes.size())
        return QString();
    const LayoutNode& layoutNode = m_layout.vecNodes[node];
    const QString strName = GetNodeName(node);
    return layoutNode.strKind.isEmpty() ? strName : strName + QLatin1Char('\n') + KnowledgeVocabulary::Get().ToConceptLabel(layoutNode.strKind);
}

QString KnowledgeMatrixView::GetNodeName(int node) const
{
    if (node < 0 || node >= m_layout.vecNodes.size())
        return QString();
    const LayoutNode& layoutNode = m_layout.vecNodes[node];
    if (!layoutNode.strLabel.isEmpty())
        return layoutNode.strLabel;
    const int iGraphNode = m_graph.FindNode(layoutNode.strId);
    return iGraphNode >= 0 ? m_graph.GetNodes()[iGraphNode].strName : layoutNode.strId;
}

int KnowledgeMatrixView::FindLabelAt(const QPoint& pos) const
{
    // 이름표: 행은 칸 왼쪽 띠, 열은 칸 위의 기울어진 글
    const MatrixGeometry geometry = ComputeGeometry();
    const int iOrder = m_layout.vecNodes.size();
    if (iOrder == 0 || geometry.dCell <= 0)
        return -1;
    const QRectF& rectGrid = geometry.grid;
    const QPointF ptPos(pos);
    if (ptPos.x() >= rectGrid.left() - geometry.dRowLabelWidth && ptPos.x() < rectGrid.left() && ptPos.y() >= rectGrid.top() && ptPos.y() < rectGrid.bottom())
        return std::min(iOrder - 1, static_cast<int>((ptPos.y() - rectGrid.top()) / geometry.dCell));
    if (ptPos.y() < rectGrid.top())
    {
        // 열 이름표: 그린 글 너비(작은 여유 포함)의 기울인 네모만 맞힌다. 글 너머 빈 곳은 맞히지 않는다
        const QFontMetricsF metrics(MakeFont(GetLabelFontPx(geometry.dCell)));
        for (int iColumn = 0; iColumn < iOrder; ++iColumn)
            if (MakeColumnLabelArea(geometry, metrics, iColumn).containsPoint(ptPos, Qt::OddEvenFill))
                return iColumn;
    }
    return -1;
}

void KnowledgeMatrixView::SelectNode(int node)
{
    m_iSelectedNode = node >= 0 && node < m_layout.vecNodes.size() ? node : -1;
    m_strSelectedId = m_iSelectedNode >= 0 ? m_layout.vecNodes[m_iSelectedNode].strId : QString();
    update();
}

void KnowledgeMatrixView::HandleClick(const QPoint& pos)
{
    // 범례 판은 칸 · 이름표 위에 그리므로(작은 위젯에서 겹칠 때) 먼저 본다. 판 안은 고른 것을 그대로 둔다
    if (ComputeGeometry().legend.contains(QPointF(pos)))
        return;
    const int iNode = FindLabelAt(pos);
    if (iNode >= 0)
    {
        SelectNode(iNode);
        const QString strId = m_layout.vecNodes[iNode].strId;   // 받는 쪽이 SetGraph 를 불러도 남게 복사한다
        emit ConceptActivated(strId);   // 이미 고른 개념이어도 낸다
        return;
    }
    if (FindCellAt(pos).first >= 0)
        return;   // 칸 누르기는 고르기가 아니다(고른 것을 그대로 둔다)
    SelectNode(-1);
    emit SelectionCleared();   // 고른 것이 없었어도 낸다(쓰는 쪽이 위젯 밖에서 고른 것도 풀 수 있게)
}

int KnowledgeMatrixView::GetLabelFontPx(double cell) const
{
    return static_cast<int>(ViewCommon::Clamp(std::floor(cell * kLabelFontRatio), kMinLabelFontPx, m_theme.iLabelFontPx));
}

QPolygonF KnowledgeMatrixView::MakeColumnLabelArea(const MatrixGeometry& geometry, const QFontMetricsF& metrics, int column) const
{
    // 그리는 글(줄임표로 줄인 이름)의 너비 + 작은 여유, 높이는 글 한 줄(칸보다 크지 않게). 칸 위 가운데에서 45° 기울인다.
    // 고른 열은 같은 글을 굵게 그리므로 굵은 글 너비를 쓴다(보통 글은 정한 너비까지, 굵은 글은 그린 너비까지)
    const QString strText = metrics.elidedText(GetNodeName(column), Qt::ElideRight, kMaxLabelWidth);
    const bool bSelected = column == m_iSelectedNode;
    const double dTextWidth = bSelected ? QFontMetricsF(MakeFont(GetLabelFontPx(geometry.dCell), true)).horizontalAdvance(strText) : metrics.horizontalAdvance(strText);
    const double dWidth = std::min(dTextWidth + kLabelHitSlack, bSelected ? std::max(kMaxLabelWidth, dTextWidth) : kMaxLabelWidth);
    const double dHeight = std::min(metrics.height(), geometry.dCell);
    QTransform transform;
    transform.translate(geometry.grid.left() + (column + 0.5) * geometry.dCell, geometry.grid.top() - kLabelGap);
    transform.rotate(kColumnLabelAngle);
    return transform.map(QPolygonF(QVector<QPointF> { QPointF(0, -dHeight / 2), QPointF(dWidth, -dHeight / 2), QPointF(dWidth, dHeight / 2),
                                                      QPointF(0, dHeight / 2) }));
}

QStringList KnowledgeMatrixView::ListLegendKinds() const
{
    QSet<QString> setKinds;
    for (const QVector<CellKind>& vecKinds : m_vecCellKinds)
        for (const CellKind& kind : vecKinds)
            setKinds.insert(kind.strKind);
    // 어휘 순, 어휘에 없는 종류는 그 뒤에 코드 순
    QStringList vecKinds;
    const QStringList vecCodes = KnowledgeVocabulary::Get().GetRelationCodes();
    for (const QString& strCode : vecCodes)
        if (setKinds.contains(strCode))
            vecKinds << strCode;
    QStringList vecUnknown;
    for (const QString& strKind : setKinds)
        if (!vecCodes.contains(strKind))
            vecUnknown << strKind;
    std::sort(vecUnknown.begin(), vecUnknown.end());
    return vecKinds + vecUnknown;
}

QFont KnowledgeMatrixView::MakeFont(int pixelSize, bool bold) const
{
    return ViewCommon::MakeFont(font(), m_theme, pixelSize, bold);
}

void KnowledgeMatrixView::DrawCell(QPainter& painter, const QRectF& rect, const QVector<CellKind>& kinds) const
{
    const auto funcColor = [this](const CellKind& kind)
    {
        const QColor color = m_theme.GetRelationColor(kind.strKind);
        return kind.bConfirmed ? color : m_theme.MakeProposed(color);
    };
    painter.setPen(Qt::NoPen);
    if (kinds.size() == 1)
    {
        painter.fillRect(rect, funcColor(kinds.first()));
        return;
    }
    if (kinds.size() == 2)
    {
        // 대각선으로 나눈다: 왼쪽 위 삼각형이 첫 종류, 오른쪽 아래가 둘째 종류(어휘 순)
        painter.setBrush(funcColor(kinds[0]));
        painter.drawPolygon(QPolygonF(QVector<QPointF> { rect.topLeft(), rect.topRight(), rect.bottomLeft() }));
        painter.setBrush(funcColor(kinds[1]));
        painter.drawPolygon(QPolygonF(QVector<QPointF> { rect.topRight(), rect.bottomRight(), rect.bottomLeft() }));
        return;
    }
    // 셋 이상: 같은 너비의 세로 띠
    const double dStripe = rect.width() / kinds.size();
    for (int iKind = 0; iKind < kinds.size(); ++iKind)
        painter.fillRect(QRectF(rect.left() + iKind * dStripe, rect.top(), dStripe, rect.height()), funcColor(kinds[iKind]));
}

void KnowledgeMatrixView::DrawLegend(QPainter& painter, const QRectF& rect) const
{
    if (rect.isEmpty())
        return;
    ViewCommon::DrawPanel(painter, rect, m_theme);
    const QFont fontLabel = MakeFont(m_theme.iLabelFontPx);
    const QFont fontHeader = MakeFont(m_theme.iSmallFontPx, true);
    const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
    double dY = rect.top() + kPanelPadding;
    const auto funcHeader = [&](const QString& text, bool first)
    {
        if (!first)
            dY += kPanelHeaderGap;
        painter.setFont(fontHeader);
        painter.setPen(m_theme.mutedText);
        painter.drawText(QRectF(rect.left() + kPanelPadding, dY, rect.width() - 2 * kPanelPadding, kPanelRowHeight), Qt::AlignLeft | Qt::AlignVCenter, text);
        dY += kPanelRowHeight;
    };
    const auto funcRow = [&](const QString& text, const QVector<CellKind>& swatch)
    {
        const QRectF rectSwatch(rect.left() + kPanelPadding + kSwatchInsetX, dY + kSwatchInsetY, kPanelSwatchWidth - kSwatchInsetX - kSwatchTextGap,
                                kPanelRowHeight - 2 * kSwatchInsetY);
        if (!swatch.isEmpty())
            DrawCell(painter, rectSwatch, swatch);
        painter.setFont(fontLabel);
        painter.setPen(m_theme.text);
        painter.drawText(QRectF(rect.left() + kPanelPadding + kPanelSwatchWidth, dY, rect.width() - 2 * kPanelPadding - kPanelSwatchWidth, kPanelRowHeight),
                         Qt::AlignLeft | Qt::AlignVCenter, text);
        dY += kPanelRowHeight;
    };

    const QStringList vecKinds = ListLegendKinds();
    funcHeader(kRelationHeader, true);
    for (const QString& strKind : vecKinds)
        funcRow(vocabulary.ToRelationLabel(strKind), { { strKind, true } });
    // 상태 견본은 첫 종류 색으로 진하기만 다르게 보인다
    funcHeader(kStatusHeader, false);
    const QString strSample = vecKinds.first();
    funcRow(ViewCommon::GetStatusLabel(ERelationStatus::CONFIRMED), { { strSample, true } });
    funcRow(ViewCommon::GetStatusLabel(ERelationStatus::PROPOSED), { { strSample, false } });
    funcHeader(kReadingHeader, false);
    funcRow(kTwoKindsLabel, { { vecKinds.first(), true }, { vecKinds.value(1, vecKinds.first()), true } });
    funcRow(kRowToColumnLabel, {});
}
