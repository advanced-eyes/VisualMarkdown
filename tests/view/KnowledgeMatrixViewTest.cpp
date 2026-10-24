#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/KnowledgeIndex.h"
#include "VisualMarkdown/UI/KnowledgeMatrixView.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"
#include "ViewTestSupport.h"

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

QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }
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

private:
    KnowledgeIndex m_index;          // showcase wiki 를 읽은 색인
    KnowledgeGraph m_graph;          // 그 색인으로 만든 그래프
    GraphLayoutOutput m_layout;      // 관계 행렬 배치
};

std::unique_ptr<QObject> CreateKnowledgeMatrixViewTest() { return std::make_unique<KnowledgeMatrixViewTest>(); }

#include "KnowledgeMatrixViewTest.moc"
