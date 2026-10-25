#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/KnowledgeIndex.h"
#include "VisualMarkdown/UI/KnowledgeMatrixView.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"
#include "ViewTestSupport.h"

#include <QScrollArea>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTest>

#include <memory>

namespace
{
// ================================================================================================
// 기대값: design/Views.md §5 의 관계 행렬(31 × 31, 채워진 칸 40, 종류가 둘인 칸은 결제 모듈 2판 → 결제 모듈 하나).
// ================================================================================================
constexpr int kOrder = 31;
constexpr int kFilledCells = 40;
constexpr const char* kTwoKindRow = "결제-모듈-2판";
constexpr const char* kTwoKindColumn = "결제-모듈";
constexpr const char* kTwoKindRowName = "결제 모듈 2판";
constexpr const char* kTwoKindColumnName = "결제 모듈";
constexpr const char* kImprovesLabel = "개선";
constexpr const char* kIsALabel = "종류";
constexpr const char* kConfirmedLabel = "확인됨";
constexpr const char* kUndirectedA = "retinanet";   // RetinaNet 대안 ↔ YOLOv3: 두 칸 모두
constexpr const char* kUndirectedB = "yolov3";
constexpr const char* kAlternativeLabel = "대안";
const QSize kSmallSize(320, 240);
constexpr double kColorTolerance = 12;   // 칸 속 점 색이 기대 색과 이만큼(RGB 거리) 안이면 같은 색이다

// 열 이름표 맞힘 칸: 맨 끝 열의 짧은 이름, 기울인 네모의 꼭짓점 수, 글 끝 너머로 가는 거리, 예전 맞힘 칸(기울인 띠 전체) 길이
constexpr const char* kShortColumn = "응답-시간";
constexpr const char* kShortColumnName = "응답 시간";
constexpr int kLabelAreaCorners = 4;
constexpr double kBeyondText = 12;
constexpr double kOldLabelBand = 160;   // 예전 맞힘 칸은 170px 띠라 이 안은 모두 맞았다

// 설명 글의 자리 표시(%1 · %2)를 담은 이름: 그대로 나와야 한다
constexpr const char* kPercentRow = "가 %1 %2";
constexpr const char* kPercentColumn = "나 %2";
constexpr double kRowLabelOffset = 10;   // 행 이름표: 칸 왼쪽으로 이만큼(px) 들어간 자리

// 고르기(L7): showcase 행렬의 두 개념과 어느 행렬에도 없는 id
constexpr const char* kFasterRcnnId = "faster-r-cnn";
constexpr const char* kFasterRcnnName = "Faster R-CNN";
constexpr const char* kYolov2Id = "yolov2";
constexpr const char* kYolov2Name = "YOLOv2";
constexpr const char* kUnknownId = "no-such-concept";

// 고르기(L7)에서 누르는 자리 · 그림을 견주는 자리(px)
constexpr double kRowLabelClickOffset = 20;   // 누르는 행 이름표 자리: 칸 왼쪽으로 이만큼. kDragDelta 만큼 끌어도 같은 이름표 안이다
const QPoint kDragDelta(12, 0);               // 끌기 거리: 누르기로 보는 거리(4px)보다 멀다
constexpr int kEmptyInsetX = 2;               // 빈 곳: 위젯 왼쪽 아래 구석에서 이만큼 들어간 자리(이름표 · 칸 · 범례 판 밖)
constexpr int kEmptyInsetY = 3;
constexpr int kCornerInside = 1;              // 칸 영역 · 범례 판의 구석에서 이만큼 안쪽(경계 바로 안을 누른다)
constexpr int kBandSlack = 1;                 // 행 · 열 띠의 가장자리 여유

QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

void Click(QWidget& widget, const QPoint& pos) { QTest::mouseClick(&widget, Qt::LeftButton, Qt::NoModifier, pos); }

QPoint GetEmptyPoint(const QWidget& widget) { return QPoint(kEmptyInsetX, widget.height() - kEmptyInsetY); }

// 행 이름표 안의 자리: 그 행 첫 칸 왼쪽으로 kRowLabelClickOffset 만큼
QPoint GetRowLabelPoint(const KnowledgeMatrixView& view, int row)
{
    const QRectF rectCell = view.GetCellRect(row, 0);
    return QPointF(rectCell.left() - kRowLabelClickOffset, rectCell.center().y()).toPoint();
}

// 열 이름표 맞힘 칸(기울인 네모)의 가운데
QPoint GetColumnLabelPoint(const KnowledgeMatrixView& view, int column)
{
    const QPolygonF area = view.GetColumnLabelArea(column);
    return (((area[0] + area[3]) / 2 + (area[1] + area[2]) / 2) / 2).toPoint();
}

// 행 · 열 띠(칸 영역 안, 가장자리 kBandSlack 여유)
QRect GetRowBand(const KnowledgeMatrixView& view, int row)
{
    const QRectF rectBand(view.GetCellRect(row, 0).topLeft(), view.GetCellRect(row, kOrder - 1).bottomRight());
    return rectBand.toAlignedRect().adjusted(-kBandSlack, -kBandSlack, kBandSlack, kBandSlack);
}

QRect GetColumnBand(const KnowledgeMatrixView& view, int column)
{
    const QRectF rectBand(view.GetCellRect(0, column).topLeft(), view.GetCellRect(kOrder - 1, column).bottomRight());
    return rectBand.toAlignedRect().adjusted(-kBandSlack, -kBandSlack, kBandSlack, kBandSlack);
}

// 칸 영역 안에서 두 띠 밖의 점 가운데 두 그림이 다른 점 수
int CountChangesOutsideBands(const QImage& before, const QImage& after, const QRect& grid, const QRect& rowBand, const QRect& columnBand)
{
    int iChanged = 0;
    const QRect rectInside = grid.intersected(before.rect());
    for (int iY = rectInside.top(); iY <= rectInside.bottom(); ++iY)
        for (int iX = rectInside.left(); iX <= rectInside.right(); ++iX)
            if (!rowBand.contains(iX, iY) && !columnBand.contains(iX, iY) && before.pixel(iX, iY) != after.pixel(iX, iY))
                ++iChanged;
    return iChanged;
}
}

// 관계 행렬 위젯(KnowledgeMatrixView, design/Views.md §7)을 showcase 행렬 배치로 시험한다(offscreen).
class KnowledgeMatrixViewTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(m_index.Load(QStringLiteral(VISUALMARKDOWN_SHOWCASE_DIR)), VISUALMARKDOWN_SHOWCASE_DIR);
        m_graph = KnowledgeGraph::BuildFromIndex(m_index);
        QVERIFY(ViewTestSupport::RunLayout(m_graph, EGraphLayoutType::MATRIX, &m_layout));
        QCOMPARE(m_layout.vecMatrixOrder.size(), kOrder);
        QCOMPARE(m_layout.vecCells.size(), kFilledCells);
    }

    // 1600 × 1000 에 비지 않게 그리고, 두 번 그리면 같다. 밝은 · 어두운 테마(PNG 를 남길 수 있다). 테마를 바꾸면 바탕 점이 바뀐다
    void rendersMatrix()
    {
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const QPoint ptCorner(2, view.height() - 3);
        QColor previousCorner;
        for (const bool bDark : { false, true })
        {
            const VisualMarkdownTheme theme = bDark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
            view.SetTheme(theme);
            const QImage image = ViewTestSupport::Render(view);
            QVERIFY(ViewTestSupport::HasEnoughInk(image, theme.background));
            QCOMPARE(ViewTestSupport::Render(view), image);
            QCOMPARE(image.pixelColor(ptCorner), theme.background);
            QVERIFY(image.pixelColor(ptCorner) != previousCorner);
            previousCorner = image.pixelColor(ptCorner);
            QVERIFY(ViewTestSupport::SaveSnapshot(image, QStringLiteral("graph-matrix-%1").arg(bDark ? "dark" : "light")));
        }
    }

    // 큰 위젯에서는 행렬 · 범례가 위젯 안에 들어오고 겹치지 않는다. 칸은 정한 범위 안이다
    void gridFitsWidget()
    {
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const QRectF rectWidget(view.rect());
        const QRectF rectGrid = view.GetGridRect();
        QVERIFY(rectWidget.contains(rectGrid));
        QVERIFY(rectWidget.contains(view.GetLegendRect()));
        QVERIFY(!rectGrid.intersects(view.GetLegendRect()));
        QVERIFY(view.GetCellSize() >= KnowledgeMatrixView::kMinCellSize && view.GetCellSize() <= KnowledgeMatrixView::kMaxCellSize);
        QCOMPARE(view.GetCellRect(0, 0).topLeft(), rectGrid.topLeft());
        QCOMPARE(view.GetCellRect(kOrder - 1, kOrder - 1).bottomRight(), rectGrid.bottomRight());
        QVERIFY(view.GetCellRect(kOrder, 0).isEmpty());
        QVERIFY(view.GetCellRect(0, -1).isEmpty());
    }

    // 종류가 둘인 칸(결제 모듈 2판 → 결제 모듈)을 찾고, 설명에 두 종류와 상태가 모두 나온다
    void findsTwoKindCell()
    {
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const int iRow = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kTwoKindRow));
        const int iColumn = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kTwoKindColumn));
        QVERIFY(iRow >= 0 && iColumn >= 0);
        const QPoint ptCell = view.GetCellRect(iRow, iColumn).center().toPoint();
        QCOMPARE(view.FindCellAt(ptCell), qMakePair(iRow, iColumn));
        const QString strTip = view.GetToolTipAt(ptCell);
        for (const QString& strPart : { DecodeUtf8(kTwoKindRowName) + QStringLiteral(" → ") + DecodeUtf8(kTwoKindColumnName), DecodeUtf8(kImprovesLabel),
                                        DecodeUtf8(kIsALabel), DecodeUtf8(kConfirmedLabel) })
            QVERIFY2(strTip.contains(strPart), qUtf8Printable(strPart + QStringLiteral(" in ") + strTip));

        // 거꾸로 칸(결제 모듈 → 결제 모듈 2판)은 비었다: 찾기는 되지만 설명은 없다
        const QPoint ptReverse = view.GetCellRect(iColumn, iRow).center().toPoint();
        QCOMPARE(view.FindCellAt(ptReverse), qMakePair(iColumn, iRow));
        QCOMPARE(view.GetToolTipAt(ptReverse), QString());

        // 칸은 대각선으로 나뉜다: 왼쪽 위는 개선(AI 제안이라 옅게), 오른쪽 아래는 종류(확인됨)
        const VisualMarkdownTheme theme = VisualMarkdownTheme::MakeLight();
        const QImage image = ViewTestSupport::Render(view);
        const QRectF rectCell = view.GetCellRect(iRow, iColumn);
        const QColor upperLeft = image.pixelColor(QPointF(rectCell.left() + rectCell.width() / 4, rectCell.top() + rectCell.height() / 4).toPoint());
        const QColor lowerRight = image.pixelColor(QPointF(rectCell.right() - rectCell.width() / 4, rectCell.bottom() - rectCell.height() / 4).toPoint());
        QVERIFY2(ViewTestSupport::GetColorDistance(upperLeft, theme.MakeProposed(theme.GetRelationColor(QStringLiteral("improves")))) < kColorTolerance,
                 qPrintable(upperLeft.name()));
        QVERIFY2(ViewTestSupport::GetColorDistance(lowerRight, theme.GetRelationColor(QStringLiteral("isA"))) < kColorTolerance, qPrintable(lowerRight.name()));
    }

    // 종류가 셋 이상인 칸은 같은 너비의 세로 띠로 나눈다(합성 배치). 설명에 세 종류와 상태가 모두 나온다
    void threeKindsAreStriped()
    {
        GraphLayoutOutput layout;
        for (const QString& strId : { QStringLiteral("a"), QStringLiteral("b") })
        {
            LayoutNode node;
            node.strId = strId;
            node.strLabel = strId.toUpper();
            node.strKind = QStringLiteral("method");
            layout.vecNodes << node;
            layout.vecMatrixOrder << strId;
        }
        const QStringList vecKinds { QStringLiteral("improves"), QStringLiteral("uses"), QStringLiteral("isA") };
        layout.vecCells.push_back({ 0, 1, vecKinds });
        for (const QString& strKind : vecKinds)
        {
            LayoutEdge edge;
            edge.iFrom = 0;
            edge.iTo = 1;
            edge.strKind = strKind;
            edge.eStatus = strKind == QStringLiteral("uses") ? ERelationStatus::PROPOSED : ERelationStatus::CONFIRMED;
            layout.vecEdges << edge;
        }
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth / 2, ViewTestSupport::kSnapshotHeight / 2);
        view.SetGraph(KnowledgeGraph(), layout);
        QCOMPARE(view.GetCellSize(), KnowledgeMatrixView::kMaxCellSize);
        // 넓은 위젯에서도 칸은 배치의 칸(확대 1배)보다 커지지 않는다(design/Views.md §5)
        QVERIFY(view.GetCellSize() <= GraphLayoutSize::kMatrixCellSize);
        const QRectF rectCell = view.GetCellRect(0, 1);
        const QString strTip = view.GetToolTipAt(rectCell.center().toPoint());
        for (const QString& strPart : { QStringLiteral("A → B"), QStringLiteral("개선 · 확인됨"), QStringLiteral("사용 · AI 제안"), QStringLiteral("종류 · 확인됨") })
            QVERIFY2(strTip.contains(strPart), qUtf8Printable(strPart + QStringLiteral(" in ") + strTip));

        const VisualMarkdownTheme theme = VisualMarkdownTheme::MakeLight();
        const QImage image = ViewTestSupport::Render(view);
        const QVector<QColor> vecExpected { theme.GetRelationColor(vecKinds[0]), theme.MakeProposed(theme.GetRelationColor(vecKinds[1])),
                                           theme.GetRelationColor(vecKinds[2]) };
        for (int iStripe = 0; iStripe < vecExpected.size(); ++iStripe)
        {
            // iStripe 번째 띠의 가운데(칸 너비 × (2 × iStripe + 1) / (2 × 띠 수))
            const QPointF ptStripe(rectCell.left() + rectCell.width() * (2 * iStripe + 1) / (2 * vecExpected.size()), rectCell.center().y());
            const QColor pixel = image.pixelColor(ptStripe.toPoint());
            QVERIFY2(ViewTestSupport::GetColorDistance(pixel, vecExpected[iStripe]) < kColorTolerance,
                     qPrintable(QStringLiteral("%1: %2 vs %3").arg(iStripe).arg(pixel.name(), vecExpected[iStripe].name())));
        }
    }

    // 방향 없는 선은 두 칸을 모두 채운다
    void undirectedEdgeFillsBothCells()
    {
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const int iA = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kUndirectedA));
        const int iB = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kUndirectedB));
        QVERIFY(iA >= 0 && iB >= 0);
        for (const QPair<int, int>& pairCell : { qMakePair(iA, iB), qMakePair(iB, iA) })
        {
            const QString strTip = view.GetToolTipAt(view.GetCellRect(pairCell.first, pairCell.second).center().toPoint());
            QVERIFY2(strTip.contains(DecodeUtf8(kAlternativeLabel)), qUtf8Printable(strTip));
        }
    }

    // 채워진 칸 40개는 모두 설명이 있고, 대각선 칸은 설명이 없다. 칸 밖은 (-1, -1)
    void everyFilledCellHasToolTip()
    {
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        for (const LayoutMatrixCell& cell : m_layout.vecCells)
        {
            const QPoint ptCell = view.GetCellRect(cell.iRow, cell.iColumn).center().toPoint();
            QCOMPARE(view.FindCellAt(ptCell), qMakePair(cell.iRow, cell.iColumn));
            QVERIFY2(!view.GetToolTipAt(ptCell).isEmpty(), qUtf8Printable(m_layout.vecMatrixOrder[cell.iRow]));
        }
        for (int iDiagonal = 0; iDiagonal < kOrder; ++iDiagonal)
            QCOMPARE(view.GetToolTipAt(view.GetCellRect(iDiagonal, iDiagonal).center().toPoint()), QString());
        QCOMPARE(view.FindCellAt(QPoint(1, 1)), qMakePair(-1, -1));
        const QRectF rectGrid = view.GetGridRect();
        QCOMPARE(view.FindCellAt(QPointF(rectGrid.right() + 2, rectGrid.center().y()).toPoint()), qMakePair(-1, -1));
    }

    // 열 이름표의 맞힘 칸은 이름 글 너비(작은 여유 포함)만큼이다. 짧은 이름(맨 끝 열 "응답 시간")은 글 위에서는 설명이 나오고,
    // 같은 기울인 띠에서 글 너머 빈 곳(예전 맞힘 칸 170px 안)은 빈 글이다
    void columnLabelHitAreaFollowsText()
    {
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const int iColumn = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kShortColumn));
        QCOMPARE(iColumn, kOrder - 1);
        const QPolygonF area = view.GetColumnLabelArea(iColumn);
        QCOMPARE(area.size(), kLabelAreaCorners);
        const QPointF ptStart = (area[0] + area[3]) / 2;   // 이름표 시작(칸 쪽) 가운데
        const QPointF ptEnd = (area[1] + area[2]) / 2;     // 이름표 끝 가운데
        const double dLength = QLineF(ptStart, ptEnd).length();
        QVERIFY(dLength > 0);
        const QPointF ptCentre = (ptStart + ptEnd) / 2;
        const QString strTip = view.GetToolTipAt(ptCentre.toPoint());
        QVERIFY2(strTip.contains(DecodeUtf8(kShortColumnName)), qUtf8Printable(strTip));

        const QPointF ptBeyond = ptEnd + (ptEnd - ptStart) / dLength * kBeyondText;
        QVERIFY2(QLineF(ptStart, ptBeyond).length() < kOldLabelBand, qPrintable(QString::number(QLineF(ptStart, ptBeyond).length())));
        QCOMPARE(view.FindCellAt(ptBeyond.toPoint()), qMakePair(-1, -1));
        QCOMPARE(view.GetToolTipAt(ptBeyond.toPoint()), QString());
        QVERIFY(view.GetColumnLabelArea(kOrder).isEmpty());
    }

    // 이름표 · 칸 설명에 넣는 이름에 %1 · %2 가 있어도 그대로 나온다(합성 배치)
    void labelsWithPercentSignsAreKept()
    {
        GraphLayoutOutput layout;
        for (const QString& strLabel : { DecodeUtf8(kPercentRow), DecodeUtf8(kPercentColumn) })
        {
            LayoutNode node;
            node.strId = strLabel;
            node.strLabel = strLabel;
            node.strKind = QStringLiteral("method");
            layout.vecNodes << node;
            layout.vecMatrixOrder << strLabel;
        }
        layout.vecCells.push_back({ 0, 1, QStringList { QStringLiteral("uses") } });
        LayoutEdge edge;
        edge.iFrom = 0;
        edge.iTo = 1;
        edge.strKind = QStringLiteral("uses");
        layout.vecEdges << edge;
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth / 2, ViewTestSupport::kSnapshotHeight / 2);
        view.SetGraph(KnowledgeGraph(), layout);
        const QString strCellTip = view.GetToolTipAt(view.GetCellRect(0, 1).center().toPoint());
        QVERIFY2(strCellTip.contains(DecodeUtf8(kPercentRow) + QStringLiteral(" → ") + DecodeUtf8(kPercentColumn)), qUtf8Printable(strCellTip));
        const QRectF rectRow = view.GetCellRect(0, 0);
        const QString strRowTip = view.GetToolTipAt(QPointF(rectRow.left() - kRowLabelOffset, rectRow.center().y()).toPoint());
        QVERIFY2(strRowTip.startsWith(DecodeUtf8(kPercentRow)), qUtf8Printable(strRowTip));
        const QPolygonF area = view.GetColumnLabelArea(1);
        const QString strColumnTip = view.GetToolTipAt((((area[0] + area[3]) / 2 + (area[1] + area[2]) / 2) / 2).toPoint());
        QVERIFY2(strColumnTip.startsWith(DecodeUtf8(kPercentColumn)), qUtf8Printable(strColumnTip));
    }

    // 행 이름표 위에서는 그 개념 이름이 설명으로 나온다
    void rowLabelToolTip()
    {
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const int iRow = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kTwoKindRow));
        const QRectF rectCell = view.GetCellRect(iRow, 0);
        const QPoint ptLabel = QPointF(rectCell.left() - 20, rectCell.center().y()).toPoint();
        QCOMPARE(view.FindCellAt(ptLabel), qMakePair(-1, -1));
        QVERIFY2(view.GetToolTipAt(ptLabel).contains(DecodeUtf8(kTwoKindRowName)), qUtf8Printable(view.GetToolTipAt(ptLabel)));
    }

    // 작은 위젯에서도 칸은 가장 작은 칸보다 작아지지 않고, 그려도 죽지 않는다
    void smallWidgetKeepsMinimumCell()
    {
        KnowledgeMatrixView view;
        view.resize(kSmallSize);
        view.SetGraph(m_graph, m_layout);
        QCOMPARE(view.GetCellSize(), KnowledgeMatrixView::kMinCellSize);
        QVERIFY(ViewTestSupport::HasEnoughInk(ViewTestSupport::Render(view), VisualMarkdownTheme::MakeLight().background));
    }

    // 크기 힌트(보기 앱이 창보다 큰 행렬을 스크롤로 보게 한다): 가장 작은 칸에서 이름표 · 칸 · 범례가 모두 위젯 안에 드는 크기다.
    // 점 수만큼 크면 칸이 1px 커지고(넉넉하지 않다), 그만큼 작으면 칸이 위젯 밖으로 나간다(모자라지 않다).
    // 스크롤 영역에 넣으면 힌트보다 작게 줄지 않고, 그래프를 바꾸면 새 힌트를 따른다(updateGeometry)
    void minimumSizeHintFitsMinimumCells()
    {
        QVERIFY(!KnowledgeMatrixView().minimumSizeHint().isValid());   // 그릴 점이 없으면 힌트가 없다

        KnowledgeMatrixView view;
        view.SetGraph(m_graph, m_layout);
        const QSize sizeHint = view.minimumSizeHint();
        QVERIFY(sizeHint.isValid());
        view.resize(sizeHint);
        QCOMPARE(view.GetCellSize(), KnowledgeMatrixView::kMinCellSize);
        const QRectF rectWidget(view.rect());
        QVERIFY2(rectWidget.contains(view.GetGridRect()), qPrintable(ViewTestSupport::DescribeRect(view.GetGridRect())));
        QVERIFY2(rectWidget.contains(view.GetLegendRect()), qPrintable(ViewTestSupport::DescribeRect(view.GetLegendRect())));
        QVERIFY(!view.GetGridRect().intersects(view.GetLegendRect()));
        for (int iColumn = 0; iColumn < kOrder; ++iColumn)
        {
            const QRectF rectLabel = view.GetColumnLabelArea(iColumn).boundingRect();
            QVERIFY2(rectWidget.contains(rectLabel) && !rectLabel.intersects(view.GetLegendRect()),
                     qPrintable(QString::number(iColumn) + QLatin1Char(' ') + ViewTestSupport::DescribeRect(rectLabel)));
        }
        view.resize(sizeHint + QSize(kOrder, kOrder));
        QCOMPARE(view.GetCellSize(), KnowledgeMatrixView::kMinCellSize + 1);
        view.resize(sizeHint - QSize(kOrder, kOrder));
        QVERIFY(!QRectF(view.rect()).contains(view.GetGridRect()));

        QScrollArea area;
        area.setWidgetResizable(true);
        KnowledgeMatrixView* pView = new KnowledgeMatrixView;
        area.setWidget(pView);
        area.resize(kSmallSize);
        area.show();
        QVERIFY(QTest::qWaitForWindowExposed(&area));
        pView->SetGraph(m_graph, m_layout);
        QTRY_VERIFY2(pView->width() >= sizeHint.width() && pView->height() >= sizeHint.height(),
                     qPrintable(QStringLiteral("%1×%2").arg(pView->width()).arg(pView->height())));
        QCOMPARE(pView->GetCellSize(), KnowledgeMatrixView::kMinCellSize);
        QVERIFY(area.horizontalScrollBar()->maximum() > 0);
        QVERIFY(area.verticalScrollBar()->maximum() > 0);
    }

    // 빈 배치도 죽지 않는다
    void emptyLayoutDoesNotCrash()
    {
        KnowledgeMatrixView view;
        view.resize(400, 300);
        QVERIFY(!ViewTestSupport::Render(view).isNull());
        view.SetGraph(KnowledgeGraph(), GraphLayoutOutput());
        QVERIFY(!ViewTestSupport::Render(view).isNull());
        QCOMPARE(view.FindCellAt(QPoint(200, 150)), qMakePair(-1, -1));
        QCOMPARE(view.GetToolTipAt(QPoint(200, 150)), QString());
        QVERIFY(view.GetCellRect(0, 0).isEmpty());

        // 배치와 다른(빈) 그래프: 이름은 배치에서 가져온다
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(KnowledgeGraph(), m_layout);
        QVERIFY(ViewTestSupport::HasEnoughInk(ViewTestSupport::Render(view), VisualMarkdownTheme::MakeLight().background));
        for (const LayoutMatrixCell& cell : m_layout.vecCells)
            view.GetToolTipAt(view.GetCellRect(cell.iRow, cell.iColumn).center().toPoint());
    }

    // L7a: 행 이름표("Faster R-CNN")를 누르면 그 개념을 고르고 ConceptActivated 를 한 번 낸다. 그 행 · 열 띠와 이름표 그림이 바뀌고,
    // 띠 밖의 칸은 그대로다(흐리지 않는다). 마우스 올림 글은 그대로다. 이미 고른 개념을 다시 누르면 또 낸다
    void rowLabelClickActivates()
    {
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const int iRow = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kFasterRcnnId));
        QVERIFY(iRow >= 0);
        const QPoint ptLabel = GetRowLabelPoint(view, iRow);
        QCOMPARE(view.FindCellAt(ptLabel), qMakePair(-1, -1));
        const QString strTip = view.GetToolTipAt(ptLabel);
        QVERIFY2(strTip.contains(DecodeUtf8(kFasterRcnnName)), qUtf8Printable(strTip));
        QCOMPARE(view.GetSelectedId(), QString());
        const QImage imageBefore = ViewTestSupport::Render(view);

        QSignalSpy spyConcept(&view, &KnowledgeMatrixView::ConceptActivated);
        QSignalSpy spyCleared(&view, &KnowledgeMatrixView::SelectionCleared);
        Click(view, ptLabel);
        QCOMPARE(spyConcept.count(), 1);
        QCOMPARE(spyConcept.at(0).at(0).toString(), DecodeUtf8(kFasterRcnnId));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kFasterRcnnId));
        QCOMPARE(spyCleared.count(), 0);
        QCOMPARE(view.GetToolTipAt(ptLabel), strTip);

        const QImage imageAfter = ViewTestSupport::Render(view);
        const QRect rectRowBand = GetRowBand(view, iRow);
        const QRect rectColumnBand = GetColumnBand(view, iRow);
        QVERIFY(imageAfter.copy(rectRowBand) != imageBefore.copy(rectRowBand));
        QVERIFY(imageAfter.copy(rectColumnBand) != imageBefore.copy(rectColumnBand));
        const QRectF rectCell = view.GetCellRect(iRow, 0);
        const QRect rectRowLabel = QRectF(0, rectCell.top(), rectCell.left(), rectCell.height()).toAlignedRect();
        QVERIFY(imageAfter.copy(rectRowLabel) != imageBefore.copy(rectRowLabel));   // 이름표 글자(굵게)
        QCOMPARE(CountChangesOutsideBands(imageBefore, imageAfter, view.GetGridRect().toAlignedRect(), rectRowBand, rectColumnBand), 0);
        // 띠는 칸 색 아래에 깐다: 띠 안의 채워진 칸(종류 하나 — 가운데가 나눔 선이 아닌 칸) 색은 그대로다
        int iBandCells = 0;
        for (const LayoutMatrixCell& cell : m_layout.vecCells)
        {
            if ((cell.iRow != iRow && cell.iColumn != iRow) || cell.vecKinds.size() != 1)
                continue;
            const QPoint ptCell = view.GetCellRect(cell.iRow, cell.iColumn).center().toPoint();
            QCOMPARE(imageAfter.pixelColor(ptCell), imageBefore.pixelColor(ptCell));
            ++iBandCells;
        }
        QVERIFY(iBandCells > 0);

        Click(view, ptLabel);
        QCOMPARE(spyConcept.count(), 2);
        QCOMPARE(spyConcept.at(1).at(0).toString(), DecodeUtf8(kFasterRcnnId));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kFasterRcnnId));
        QCOMPARE(ViewTestSupport::Render(view), imageAfter);
        QCOMPARE(spyCleared.count(), 0);
    }

    // L7a: 열 이름표 맞힘 칸(GetColumnLabelArea) 안을 누르면 그 개념을 고른다. 그림은 그 개념의 행 이름표를 누른 것과 같다.
    // 고른 열 이름표는 굵게 쓰므로 맞힘 칸이 줄지 않고, 다른 열 이름표를 누르면 고른 것이 옮겨 간다
    void columnLabelClickActivates()
    {
        const int iColumn = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kYolov2Id));
        const int iOther = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kFasterRcnnId));
        QVERIFY(iColumn >= 0 && iOther >= 0);
        KnowledgeMatrixView rowClicked;
        rowClicked.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        rowClicked.SetGraph(m_graph, m_layout);
        Click(rowClicked, GetRowLabelPoint(rowClicked, iColumn));
        QCOMPARE(rowClicked.GetSelectedId(), DecodeUtf8(kYolov2Id));

        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const QPolygonF areaBefore = view.GetColumnLabelArea(iColumn);
        const QPoint ptLabel = GetColumnLabelPoint(view, iColumn);
        QVERIFY(areaBefore.containsPoint(QPointF(ptLabel), Qt::OddEvenFill));
        QCOMPARE(view.FindCellAt(ptLabel), qMakePair(-1, -1));
        QVERIFY2(view.GetToolTipAt(ptLabel).contains(DecodeUtf8(kYolov2Name)), qUtf8Printable(view.GetToolTipAt(ptLabel)));

        QSignalSpy spyConcept(&view, &KnowledgeMatrixView::ConceptActivated);
        QSignalSpy spyCleared(&view, &KnowledgeMatrixView::SelectionCleared);
        Click(view, ptLabel);
        QCOMPARE(spyConcept.count(), 1);
        QCOMPARE(spyConcept.at(0).at(0).toString(), DecodeUtf8(kYolov2Id));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        QCOMPARE(ViewTestSupport::Render(view), ViewTestSupport::Render(rowClicked));
        const QPolygonF areaAfter = view.GetColumnLabelArea(iColumn);
        QCOMPARE(areaAfter.size(), areaBefore.size());
        const double dLengthBefore = QLineF((areaBefore[0] + areaBefore[3]) / 2, (areaBefore[1] + areaBefore[2]) / 2).length();
        const double dLengthAfter = QLineF((areaAfter[0] + areaAfter[3]) / 2, (areaAfter[1] + areaAfter[2]) / 2).length();
        QVERIFY2(dLengthAfter >= dLengthBefore, qPrintable(QStringLiteral("%1 < %2").arg(dLengthAfter).arg(dLengthBefore)));

        Click(view, GetColumnLabelPoint(view, iOther));
        QCOMPARE(spyConcept.count(), 2);
        QCOMPARE(spyConcept.at(1).at(0).toString(), DecodeUtf8(kFasterRcnnId));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kFasterRcnnId));
        QCOMPARE(view.GetColumnLabelArea(iColumn), areaBefore);   // 고르지 않은 열은 처음 맞힘 칸
        QCOMPARE(spyCleared.count(), 0);
    }

    // L7b: 밖에서 고르면 그 이름표를 누른 그림과 같고 신호가 없다. 같은 id 를 다시 고르면 그대로다. 모르는 id · 빈 글이면 고른 것을 지우고
    // 고르지 않은 그림과 같다(신호 없음). 스크롤 영역 안의 큰 행렬에서 위젯 밖의 행을 골라도 옮기지 않는다
    void selectConceptHighlightsWithoutSignal()
    {
        const int iNode = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kYolov2Id));
        QVERIFY(iNode >= 0);
        KnowledgeMatrixView clicked;
        clicked.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        clicked.SetGraph(m_graph, m_layout);
        Click(clicked, GetRowLabelPoint(clicked, iNode));
        QCOMPARE(clicked.GetSelectedId(), DecodeUtf8(kYolov2Id));
        const QImage imageClicked = ViewTestSupport::Render(clicked);

        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const QImage imageNone = ViewTestSupport::Render(view);
        QVERIFY(imageClicked != imageNone);
        QSignalSpy spyConcept(&view, &KnowledgeMatrixView::ConceptActivated);
        QSignalSpy spyCleared(&view, &KnowledgeMatrixView::SelectionCleared);
        view.SelectConcept(DecodeUtf8(kYolov2Id));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        QCOMPARE(ViewTestSupport::Render(view), imageClicked);
        view.SelectConcept(DecodeUtf8(kYolov2Id));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        QCOMPARE(ViewTestSupport::Render(view), imageClicked);

        for (const QString& strId : { DecodeUtf8(kUnknownId), QString() })
        {
            view.SelectConcept(DecodeUtf8(kYolov2Id));
            QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
            view.SelectConcept(strId);
            QCOMPARE(view.GetSelectedId(), QString());
            QCOMPARE(ViewTestSupport::Render(view), imageNone);
            view.SelectConcept(strId);   // 고른 것이 없을 때도 그대로
            QCOMPARE(view.GetSelectedId(), QString());
        }
        QCOMPARE(spyConcept.count(), 0);
        QCOMPARE(spyCleared.count(), 0);

        // 스크롤 영역: 맨 끝 행(위젯 밖)을 골라도 스크롤 · 칸 자리가 그대로다
        QScrollArea area;
        area.setWidgetResizable(true);
        KnowledgeMatrixView* pView = new KnowledgeMatrixView;
        area.setWidget(pView);
        area.resize(kSmallSize);
        area.show();
        QVERIFY(QTest::qWaitForWindowExposed(&area));
        pView->SetGraph(m_graph, m_layout);
        const QSize sizeHint = pView->minimumSizeHint();
        QTRY_VERIFY(pView->width() >= sizeHint.width() && pView->height() >= sizeHint.height());
        const int iHorizontal = area.horizontalScrollBar()->value();
        const int iVertical = area.verticalScrollBar()->value();
        const QRectF rectGrid = pView->GetGridRect();
        const QString strLast = m_layout.vecMatrixOrder.last();
        QVERIFY(!QRectF(area.viewport()->rect()).contains(pView->GetCellRect(kOrder - 1, kOrder - 1)));
        QSignalSpy spyScrollConcept(pView, &KnowledgeMatrixView::ConceptActivated);
        pView->SelectConcept(strLast);
        QCOMPARE(pView->GetSelectedId(), strLast);
        QCoreApplication::processEvents();
        QCOMPARE(area.horizontalScrollBar()->value(), iHorizontal);
        QCOMPARE(area.verticalScrollBar()->value(), iVertical);
        QCOMPARE(pView->GetGridRect(), rectGrid);
        QCOMPARE(spyScrollConcept.count(), 0);
    }

    // L7c: 고른 뒤 빈 곳(이름표 · 칸 · 범례 판 밖)을 누르면 고른 것을 지운 뒤 SelectionCleared 를 한 번 낸다(받을 때 GetSelectedId 는 빈 글).
    // 고른 것이 없어도 낸다. 열 이름표 글 너머 빈 곳도 빈 곳이다
    void emptyClickEmitsSelectionCleared()
    {
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const QImage imageNone = ViewTestSupport::Render(view);
        QSignalSpy spyConcept(&view, &KnowledgeMatrixView::ConceptActivated);
        QSignalSpy spyCleared(&view, &KnowledgeMatrixView::SelectionCleared);
        QStringList vecSelectedAtSignal;
        connect(&view, &KnowledgeMatrixView::SelectionCleared, this, [&view, &vecSelectedAtSignal]() { vecSelectedAtSignal << view.GetSelectedId(); });

        const int iRow = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kFasterRcnnId));
        QVERIFY(iRow >= 0);
        Click(view, GetRowLabelPoint(view, iRow));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kFasterRcnnId));
        QCOMPARE(spyCleared.count(), 0);

        const QPoint ptEmpty = GetEmptyPoint(view);
        QCOMPARE(view.FindCellAt(ptEmpty), qMakePair(-1, -1));
        QCOMPARE(view.GetToolTipAt(ptEmpty), QString());
        QVERIFY(!view.GetLegendRect().contains(QPointF(ptEmpty)));
        Click(view, ptEmpty);
        QCOMPARE(spyCleared.count(), 1);
        QCOMPARE(vecSelectedAtSignal, QStringList { QString() });
        QCOMPARE(view.GetSelectedId(), QString());
        QCOMPARE(ViewTestSupport::Render(view), imageNone);

        // 고른 것이 없어도 낸다
        Click(view, ptEmpty);
        QCOMPARE(spyCleared.count(), 2);
        QCOMPARE(vecSelectedAtSignal, (QStringList { QString(), QString() }));

        // 열 이름표 글 너머(같은 기울인 띠): 이름표가 아니라 빈 곳
        Click(view, GetRowLabelPoint(view, iRow));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kFasterRcnnId));
        const QPolygonF area = view.GetColumnLabelArea(kOrder - 1);
        const QPointF ptStart = (area[0] + area[3]) / 2;
        const QPointF ptEnd = (area[1] + area[2]) / 2;
        const QPoint ptBeyond = (ptEnd + (ptEnd - ptStart) / QLineF(ptStart, ptEnd).length() * kBeyondText).toPoint();
        QCOMPARE(view.GetToolTipAt(ptBeyond), QString());
        Click(view, ptBeyond);
        QCOMPARE(spyCleared.count(), 3);
        QCOMPARE(view.GetSelectedId(), QString());
        QCOMPARE(spyConcept.count(), 2);
    }

    // L7c: 칸(채워진 칸 · 빈 칸 · 대각선 칸) · 범례 판 안을 누르면 신호가 없고 고른 것 · 그림이 그대로다. 고른 것이 없을 때도 신호가 없다
    void cellAndLegendClicksDoNotClear()
    {
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const int iTwoKindRow = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kTwoKindRow));
        const int iTwoKindColumn = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kTwoKindColumn));
        QVERIFY(iTwoKindRow >= 0 && iTwoKindColumn >= 0);
        QVERIFY(!view.GetLegendRect().isEmpty());
        const QVector<QPoint> vecPoints { view.GetCellRect(iTwoKindRow, iTwoKindColumn).center().toPoint(),
                                          view.GetCellRect(iTwoKindColumn, iTwoKindRow).center().toPoint(),
                                          view.GetCellRect(0, 0).center().toPoint(),
                                          (view.GetCellRect(kOrder - 1, kOrder - 1).bottomRight() - QPointF(kCornerInside, kCornerInside)).toPoint(),
                                          view.GetLegendRect().center().toPoint(),
                                          view.GetLegendRect().topLeft().toPoint() + QPoint(kCornerInside, kCornerInside) };
        QSignalSpy spyConcept(&view, &KnowledgeMatrixView::ConceptActivated);
        QSignalSpy spyCleared(&view, &KnowledgeMatrixView::SelectionCleared);
        for (const QPoint& ptClick : vecPoints)
        {
            QVERIFY2(view.FindCellAt(ptClick).first >= 0 || view.GetLegendRect().contains(QPointF(ptClick)),
                     qPrintable(QStringLiteral("(%1, %2)").arg(ptClick.x()).arg(ptClick.y())));
            Click(view, ptClick);
            QCOMPARE(view.GetSelectedId(), QString());
        }
        QCOMPARE(spyConcept.count(), 0);
        QCOMPARE(spyCleared.count(), 0);

        view.SelectConcept(DecodeUtf8(kFasterRcnnId));
        const QImage imageSelected = ViewTestSupport::Render(view);
        for (const QPoint& ptClick : vecPoints)
        {
            Click(view, ptClick);
            QCOMPARE(view.GetSelectedId(), DecodeUtf8(kFasterRcnnId));
        }
        QCOMPARE(ViewTestSupport::Render(view), imageSelected);
        QCOMPARE(spyConcept.count(), 0);
        QCOMPARE(spyCleared.count(), 0);
    }

    // L7: SetGraph 는 고른 것을 소리 없이 지운다(그림은 처음과 같다). SetTheme 은 고른 것을 그대로 둔다
    void setGraphClearsSilently()
    {
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const QImage imageNone = ViewTestSupport::Render(view);
        const int iRow = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kFasterRcnnId));
        QVERIFY(iRow >= 0);
        Click(view, GetRowLabelPoint(view, iRow));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kFasterRcnnId));

        QSignalSpy spyConcept(&view, &KnowledgeMatrixView::ConceptActivated);
        QSignalSpy spyCleared(&view, &KnowledgeMatrixView::SelectionCleared);
        view.SetTheme(VisualMarkdownTheme::MakeLight());
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kFasterRcnnId));
        view.SetGraph(m_graph, m_layout);
        QCOMPARE(view.GetSelectedId(), QString());
        QCOMPARE(ViewTestSupport::Render(view), imageNone);
        view.SelectConcept(DecodeUtf8(kYolov2Id));
        view.SetGraph(KnowledgeGraph(), GraphLayoutOutput());
        QCOMPARE(view.GetSelectedId(), QString());
        QVERIFY(!ViewTestSupport::Render(view).isNull());
        QCOMPARE(spyConcept.count(), 0);
        QCOMPARE(spyCleared.count(), 0);
    }

    // L7: 고르지 않은 그림은 L7 전과 같다: 그리기 시험(rendersMatrix · findsTwoKindCell 등)이 그대로 지나고, 두 테마 모두 처음 그림 =
    // 빈 글을 고른 그림 = 골랐다가 푼(밖에서 · 빈 곳 누르기) 그림이다
    void defaultDrawingUnchanged()
    {
        for (const bool bDark : { false, true })
        {
            const VisualMarkdownTheme theme = bDark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
            KnowledgeMatrixView fresh;
            fresh.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
            fresh.SetTheme(theme);
            fresh.SetGraph(m_graph, m_layout);
            const QImage imageNone = ViewTestSupport::Render(fresh);

            KnowledgeMatrixView view;
            view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
            view.SetTheme(theme);
            view.SetGraph(m_graph, m_layout);
            view.SelectConcept(QString());
            QCOMPARE(view.GetSelectedId(), QString());
            QCOMPARE(ViewTestSupport::Render(view), imageNone);
            view.SelectConcept(DecodeUtf8(kYolov2Id));
            QVERIFY(ViewTestSupport::Render(view) != imageNone);
            view.SelectConcept(QString());
            QCOMPARE(ViewTestSupport::Render(view), imageNone);
            Click(view, GetRowLabelPoint(view, m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kFasterRcnnId))));
            QCOMPARE(view.GetSelectedId(), DecodeUtf8(kFasterRcnnId));
            QVERIFY(ViewTestSupport::Render(view) != imageNone);
            Click(view, GetEmptyPoint(view));
            QCOMPARE(view.GetSelectedId(), QString());
            QCOMPARE(ViewTestSupport::Render(view), imageNone);
        }
    }

    // L7a · L7c: 끌기(누른 뒤 kDragDelta 만큼 옮겨 놓기)는 누르기가 아니다: 이름표 · 빈 곳 어디서든 신호가 없다. 두 번 누르기는 첫 누르기에서 한 번만 낸다.
    // 왼쪽이 아닌 단추는 누르기가 아니다
    void dragAndDoubleClickAreNotClicks()
    {
        KnowledgeMatrixView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, m_layout);
        const int iRow = m_layout.vecMatrixOrder.indexOf(DecodeUtf8(kFasterRcnnId));
        QVERIFY(iRow >= 0);
        const QPoint ptLabel = GetRowLabelPoint(view, iRow);
        QVERIFY(view.GetToolTipAt(ptLabel + kDragDelta).contains(DecodeUtf8(kFasterRcnnName)));   // 끈 끝도 같은 이름표
        const QPoint ptEmpty = GetEmptyPoint(view);
        QSignalSpy spyConcept(&view, &KnowledgeMatrixView::ConceptActivated);
        QSignalSpy spyCleared(&view, &KnowledgeMatrixView::SelectionCleared);

        ViewTestSupport::SendMouse(view, QEvent::MouseButtonPress, ptLabel, Qt::LeftButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseMove, ptLabel + kDragDelta / 2, Qt::NoButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseMove, ptLabel + kDragDelta, Qt::NoButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonRelease, ptLabel + kDragDelta, Qt::LeftButton, Qt::NoButton);
        QCOMPARE(spyConcept.count(), 0);
        QCOMPARE(view.GetSelectedId(), QString());

        view.SelectConcept(DecodeUtf8(kYolov2Id));
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonPress, ptEmpty, Qt::LeftButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseMove, ptEmpty - kDragDelta.transposed(), Qt::NoButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonRelease, ptEmpty - kDragDelta.transposed(), Qt::LeftButton, Qt::NoButton);
        QCOMPARE(spyCleared.count(), 0);
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));

        // 왼쪽이 아닌 단추
        QTest::mouseClick(&view, Qt::RightButton, Qt::NoModifier, ptLabel);
        QTest::mouseClick(&view, Qt::RightButton, Qt::NoModifier, ptEmpty);
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        QCOMPARE(spyConcept.count(), 0);
        QCOMPARE(spyCleared.count(), 0);

        // 두 번 누르기(누름 · 놓음 · 두 번 누름 · 놓음): 이름표는 ConceptActivated 한 번, 빈 곳은 SelectionCleared 한 번
        for (const QPoint& ptClick : { ptLabel, ptEmpty })
        {
            ViewTestSupport::SendMouse(view, QEvent::MouseButtonPress, ptClick, Qt::LeftButton, Qt::LeftButton);
            ViewTestSupport::SendMouse(view, QEvent::MouseButtonRelease, ptClick, Qt::LeftButton, Qt::NoButton);
            ViewTestSupport::SendMouse(view, QEvent::MouseButtonDblClick, ptClick, Qt::LeftButton, Qt::LeftButton);
            ViewTestSupport::SendMouse(view, QEvent::MouseButtonRelease, ptClick, Qt::LeftButton, Qt::NoButton);
        }
        QCOMPARE(spyConcept.count(), 1);
        QCOMPARE(spyConcept.at(0).at(0).toString(), DecodeUtf8(kFasterRcnnId));
        QCOMPARE(spyCleared.count(), 1);
        QCOMPARE(view.GetSelectedId(), QString());
    }

private:
    KnowledgeIndex m_index;          // showcase wiki 를 읽은 색인
    KnowledgeGraph m_graph;          // 그 색인으로 만든 그래프
    GraphLayoutOutput m_layout;      // 관계 행렬 배치
};

std::unique_ptr<QObject> CreateKnowledgeMatrixViewTest() { return std::make_unique<KnowledgeMatrixViewTest>(); }

#include "KnowledgeMatrixViewTest.moc"
