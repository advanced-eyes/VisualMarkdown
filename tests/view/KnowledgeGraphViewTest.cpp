#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/KnowledgeIndex.h"
#include "VisualMarkdown/UI/KnowledgeGraphView.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"
#include "ViewTestSupport.h"

#include <QSignalSpy>
#include <QTest>

#include <cmath>
#include <memory>

namespace
{
// ================================================================================================
// 기대값: examples/showcase/README.md §2 와 design/Views.md §7 이다. 예시를 고치면 여기를 함께 고친다.
// ================================================================================================

const QVector<EGraphLayoutType> kGraphTypes {
    EGraphLayoutType::NETWORK, EGraphLayoutType::LINEAGE, EGraphLayoutType::TREE, EGraphLayoutType::TIMELINE, EGraphLayoutType::BIPARTITE,
};

// 점 하나: YOLOv2(방법, 2017, 문서 2개)
constexpr const char* kYolov2Id = "yolov2";
constexpr const char* kYolov2Name = "YOLOv2";
constexpr const char* kYolov2Kind = "방법";
constexpr const char* kYolov2Year = "2017";
constexpr const char* kYolov2Documents = "문서 2개";

// 합쳐지는 선: YOLOv2 개선 → YOLO(AI 제안 + 확인됨 → 확인됨, 출처 문서 2개)
constexpr const char* kMergedFrom = "yolov2";
constexpr const char* kMergedTo = "yolo";
constexpr const char* kMergedTitle = "YOLOv2 → YOLO";
constexpr const char* kImproves = "improves";
constexpr const char* kImprovesLabel = "개선";
constexpr const char* kConfirmedLabel = "확인됨";
constexpr const char* kProposedLabel = "AI 제안";
constexpr const char* kMergedSources = "문서 2개";
constexpr int kProposedEdges = 11;          // 관계망의 AI 제안 선

// 개념 파일과 합쳐지는 방향 없는 선: RetinaNet 대안 ↔ YOLOv3(출처 문서 1개 + 개념 파일)
constexpr const char* kAlternativeFrom = "retinanet";
constexpr const char* kAlternativeTo = "yolov3";
constexpr const char* kAlternativeTitle = "RetinaNet ↔ YOLOv3";
constexpr const char* kAlternative = "alternative";
constexpr const char* kAlternativeLabel = "대안";
constexpr const char* kAlternativeSources = "문서 1개";
constexpr const char* kConceptFileSource = "개념 파일";

// 문서 ↔ 개념: YOLO9000 문서가 YOLOv2 를 가리킨 횟수
constexpr const char* kYolo9000Title = "YOLO9000: Better, Faster, Stronger";
constexpr int kYolo9000ToYolov2Weight = 9;

// 개념 점 설명의 개선 줄: "개선: <개선한 개념 이름>" 과 그 선의 설명. 계보도의 개선 선은 5개이고, 맨 옛것 R-CNN 은 아무것도 개선하지 않는다
constexpr const char* kImprovedLabel = "개선: ";
constexpr int kImprovesEdges = 5;
constexpr const char* kOldestId = "r-cnn";

// 타임라인 2015 · 검출기 갈래에 위아래로 쌓인 두 점 사이의 개선 선(Faster R-CNN → Fast R-CNN)
constexpr const char* kStackedFrom = "faster-r-cnn";
constexpr const char* kStackedTo = "fast-r-cnn";
constexpr const char* kStackedTitle = "Faster R-CNN → Fast R-CNN";
constexpr double kNarrowGap = 20;           // 쌓인 상자 사이 틈(px)이 이보다 좁다
constexpr double kBorderTolerance = 3;      // 선 끝이 상자 테두리에서 이만큼(px) 안에 있다

constexpr int kUnplacedCount = 15;          // 타임라인: 연도 없는 점
constexpr const char* kUnplacedTitle = "연도 없음";
constexpr double kMaxFitZoom = 1.0;         // 맞춤은 배치를 1배보다 키우지 않는다(design/Views.md §5)
constexpr double kCenterTolerance = 1;      // 맞춘 점 상자 전체의 가운데가 맞춤 영역 가운데에서 이만큼(px) 안이다
constexpr double kCursorTolerance = 1.5;    // 커서 둘레 확대: 커서 아래 점이 이만큼(px) 안에 머문다
constexpr double kMinMovedDistance = 1;     // 커서에서 먼 점은 이만큼(px) 넘게 움직인다
constexpr double kPanTolerance = 0.01;      // 끌기 · 맞춤 뒤의 점 자리(px)
constexpr double kMaxSaneZoom = 50;         // 휠을 아무리 돌려도 확대가 이 안이다
constexpr double kMinSaneZoom = 0.01;
constexpr int kManyNotches = 100;
const QPoint kDragDelta(120, 70);

// 설명 글의 자리 표시(%1 · %2)를 담은 사용자 글: 그대로 나와야 한다
constexpr const char* kPercentLane = "갈래 %1 %2";
constexpr const char* kPercentNode = "점 %1";
constexpr const char* kPercentDocument = "문서 %1 제목 %2";
constexpr const char* kPercentConcept = "개념 %1 이름 %2";
constexpr const char* kPercentTarget = "옛 개념 %2";
constexpr const char* kPercentDefinition = "정의 %1 · %2";
constexpr const char* kPercentNote = "설명 %1 그리고 %2";
constexpr int kPercentLinkWeight = 3;

QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

// 설명은 긴 줄을 빈칸에서 접으므로 견줄 때는 빈칸 · 줄바꿈을 모두 지운다
QString RemoveSpaces(const QString& text)
{
    QString strResult;
    for (const QChar chLetter : text)
        if (!chLetter.isSpace())
            strResult += chLetter;
    return strResult;
}
}

// 지식 사이 보기 위젯(KnowledgeGraphView, design/Views.md §7)을 showcase 의 배치 5가지로 시험한다(offscreen).
// 그리기(비지 않음 · 두 번 그리면 같음 · 테마), 맞춤, 위치로 찾기, 누르기 · 휠 · 끌기, 마우스 올림 설명.
class KnowledgeGraphViewTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(m_index.Load(QStringLiteral(VISUALMARKDOWN_SHOWCASE_DIR)), VISUALMARKDOWN_SHOWCASE_DIR);
        m_graph = KnowledgeGraph::BuildFromIndex(m_index);
        m_vecLayouts.resize(static_cast<int>(EGraphLayoutType::BIPARTITE) + 1);
        for (const EGraphLayoutType eType : kGraphTypes)
            QVERIFY(ViewTestSupport::RunLayout(m_graph, eType, &m_vecLayouts[static_cast<int>(eType)]));
        QVERIFY(!GetLayout(EGraphLayoutType::NETWORK).vecNodes.isEmpty());
    }

    void layouts_data()
    {
        QTest::addColumn<int>("type");
        for (const EGraphLayoutType eType : kGraphTypes)
            QTest::newRow(qPrintable(ViewTestSupport::GetLayoutName(eType))) << static_cast<int>(eType);
    }

    // 보기마다 1600 × 1000 에 비지 않게 그리고, 두 번 그리면 같다. 밝은 · 어두운 테마 모두(PNG 를 남길 수 있다)
    void rendersEveryLayout_data() { layouts_data(); }
    void rendersEveryLayout()
    {
        QFETCH(int, type);
        const EGraphLayoutType eType = static_cast<EGraphLayoutType>(type);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, GetLayout(eType), eType);
        for (const bool bDark : { false, true })
        {
            const VisualMarkdownTheme theme = bDark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
            view.SetTheme(theme);
            const QImage image = ViewTestSupport::Render(view);
            QVERIFY2(ViewTestSupport::HasEnoughInk(image, theme.background), qPrintable(ViewTestSupport::GetLayoutName(eType)));
            QCOMPARE(ViewTestSupport::Render(view), image);
            const QString strName = QStringLiteral("graph-%1-%2").arg(ViewTestSupport::GetLayoutName(eType), bDark ? "dark" : "light");
            QVERIFY2(ViewTestSupport::SaveSnapshot(image, strName), qPrintable(strName));
        }
    }

    // 맞추면 모든 점 상자가 위젯 안에 있고 범례 판과 겹치지 않는다. 갈래 이름 칸은 점 상자와 겹치지 않는다
    void fitKeepsEverythingVisible_data() { layouts_data(); }
    void fitKeepsEverythingVisible()
    {
        QFETCH(int, type);
        const EGraphLayoutType eType = static_cast<EGraphLayoutType>(type);
        const GraphLayoutOutput& layout = GetLayout(eType);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, eType);
        ViewTestSupport::SendWheel(view, QPoint(400, 300), 3);   // 맞춤이 앞의 확대를 지운다
        view.FitToView();
        const QRectF rectWidget(view.rect());
        const QRectF rectLegend = view.GetLegendRect();
        QVERIFY(!rectLegend.isEmpty());
        QVERIFY2(rectWidget.contains(rectLegend), qUtf8Printable(ViewTestSupport::DescribeRect(rectLegend)));
        for (int iNode = 0; iNode < layout.vecNodes.size(); ++iNode)
        {
            const QRectF rectNode = view.GetNodeRect(iNode);
            const QString strWhat = layout.vecNodes[iNode].strId + QLatin1Char(' ') + ViewTestSupport::DescribeRect(rectNode);
            QVERIFY2(!rectNode.isEmpty(), qUtf8Printable(strWhat));
            QVERIFY2(rectWidget.contains(view.MapToWidget(layout.vecNodes[iNode].center)), qUtf8Printable(strWhat));
            QVERIFY2(rectWidget.contains(rectNode), qUtf8Printable(strWhat));
            QVERIFY2(!rectNode.intersects(rectLegend), qUtf8Printable(strWhat + QStringLiteral(" 범례 ") + ViewTestSupport::DescribeRect(rectLegend)));
            for (int iLane = 0; iLane < layout.vecLanes.size(); ++iLane)
            {
                const QRectF rectLabel = view.GetLaneLabelRect(iLane);
                QVERIFY2(!rectLabel.isEmpty(), qUtf8Printable(layout.vecLanes[iLane].strLabel));
                QVERIFY2(!rectLabel.intersects(rectNode), qUtf8Printable(layout.vecLanes[iLane].strLabel + QLatin1Char(' ') + strWhat));
            }
        }
    }

    // 맞춤은 배치를 1배보다 크게 키우지 않는다(design/Views.md §5: 배치 단위는 화면 점 1개). 작은 계보도는 1배에서 멈추고
    // 점 상자 전체가 맞춤 영역 가운데에 온다. 휠 확대는 1배를 넘을 수 있다
    void fitNeverEnlargesBeyondOne()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::LINEAGE);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::LINEAGE);
        view.FitToView();
        QVERIFY2(view.GetZoom() <= kMaxFitZoom, qPrintable(QString::number(view.GetZoom())));
        QCOMPARE(view.GetZoom(), kMaxFitZoom);   // 작은 계보도는 더 키울 수 있지만 1배에서 멈춘다
        QRectF rectNodes;
        for (int iNode = 0; iNode < layout.vecNodes.size(); ++iNode)
            rectNodes = rectNodes.united(view.GetNodeRect(iNode));
        const QRectF rectFit = view.GetFitRect();
        QVERIFY(rectFit.contains(rectNodes));
        const QPointF ptOffset = rectNodes.center() - rectFit.center();
        QVERIFY2(std::abs(ptOffset.x()) < kCenterTolerance && std::abs(ptOffset.y()) < kCenterTolerance,
                 qPrintable(QStringLiteral("%1, %2 / fit %3 nodes %4").arg(ptOffset.x()).arg(ptOffset.y()).arg(ViewTestSupport::DescribeRect(rectFit), ViewTestSupport::DescribeRect(rectNodes))));
        ViewTestSupport::SendWheel(view, rectFit.center().toPoint(), 3);
        QVERIFY(view.GetZoom() > kMaxFitZoom);
    }

    // 타임라인 2015 열처럼 같은 열에 붙어 쌓인 두 상자 사이의 선은 위아래 틈에 그리지 않고, 출발 상자 오른쪽 가운데에서 나가
    // 대상 상자 오른쪽 가운데로 들어오는 활 모양으로 그린다. 화살촉은 대상 상자 테두리에 보이고, 위에 올리면 그 선을 찾는다
    void stackedBoxesEdgeBowsOut()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::TIMELINE);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::TIMELINE);
        const int iEdge = FindEdge(layout, DecodeUtf8(kStackedFrom), DecodeUtf8(kImproves), DecodeUtf8(kStackedTo));
        QVERIFY(iEdge >= 0);
        const QRectF rectFrom = view.GetNodeRect(layout.vecEdges[iEdge].iFrom);
        const QRectF rectTo = view.GetNodeRect(layout.vecEdges[iEdge].iTo);
        // 앞 조건: 가로 범위가 겹치고 위아래 틈이 좁다(같은 해 · 같은 갈래에 쌓였다)
        QVERIFY(rectFrom.left() < rectTo.right() && rectTo.left() < rectFrom.right());
        const double dGap = std::max(rectFrom.top(), rectTo.top()) - std::min(rectFrom.bottom(), rectTo.bottom());
        QVERIFY2(dGap < kNarrowGap, qPrintable(QString::number(dGap)));

        const QPolygonF line = view.GetEdgeLine(iEdge);
        const QPolygonF arrow = view.GetEdgeArrow(iEdge);
        QVERIFY(line.size() > 2);
        QCOMPARE(arrow.size(), 3);
        const QPointF ptTip = arrow[0];
        const double dArrowLength = QLineF(ptTip, (arrow[1] + arrow[2]) / 2).length();
        QVERIFY(dArrowLength > 0);
        // 시작: 출발 상자 오른쪽 가운데, 끝(화살촉 끝): 대상 상자 오른쪽 테두리
        QVERIFY2(std::abs(line.first().x() - rectFrom.right()) <= kBorderTolerance && std::abs(line.first().y() - rectFrom.center().y()) <= kBorderTolerance,
                 qPrintable(QStringLiteral("%1,%2 %3").arg(line.first().x()).arg(line.first().y()).arg(ViewTestSupport::DescribeRect(rectFrom))));
        QVERIFY2(std::abs(ptTip.x() - rectTo.right()) <= kBorderTolerance && std::abs(ptTip.y() - rectTo.center().y()) <= kBorderTolerance,
                 qPrintable(QStringLiteral("%1,%2 %3").arg(ptTip.x()).arg(ptTip.y()).arg(ViewTestSupport::DescribeRect(rectTo))));
        // 경로는 두 상자 오른쪽 밖으로 화살촉 길이 이상 나간다(상자 사이 틈에 갇히지 않는다)
        double dRightmost = line.first().x();
        for (const QPointF& ptPoint : line)
            dRightmost = std::max(dRightmost, ptPoint.x());
        QVERIFY2(dRightmost >= std::max(rectFrom.right(), rectTo.right()) + dArrowLength,
                 qPrintable(QStringLiteral("%1 vs %2 + %3").arg(dRightmost).arg(std::max(rectFrom.right(), rectTo.right())).arg(dArrowLength)));
        // 히트 테스트도 같은 경로: 경로 가운데에서 그 선을 찾고, 설명은 그 선이다
        const QPoint ptMiddle = view.GetEdgeMidpoint(iEdge).toPoint();
        QVERIFY(!rectFrom.contains(ptMiddle) && !rectTo.contains(ptMiddle));
        QCOMPARE(view.FindEdgeAt(ptMiddle), iEdge);
        QVERIFY2(view.GetToolTipAt(ptMiddle).contains(DecodeUtf8(kStackedTitle)), qUtf8Printable(view.GetToolTipAt(ptMiddle)));

        // 그 밖의 선은 그대로: 다른 해 사이의 선(YOLOv2 → YOLO)은 곧은 선이다
        const int iStraight = FindEdge(layout, DecodeUtf8(kMergedFrom), DecodeUtf8(kImproves), DecodeUtf8(kMergedTo));
        QVERIFY(iStraight >= 0);
        QCOMPARE(view.GetEdgeLine(iStraight).size(), 2);
        QCOMPARE(view.GetEdgeArrow(iStraight).size(), 3);
    }

    // 점 상자 가운데에서 그 점을 찾는다(개념은 FindConceptAt, 문서는 FindDocumentAt)
    void findsNodeAtItsCentre_data() { layouts_data(); }
    void findsNodeAtItsCentre()
    {
        QFETCH(int, type);
        const EGraphLayoutType eType = static_cast<EGraphLayoutType>(type);
        const GraphLayoutOutput& layout = GetLayout(eType);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, eType);
        int iDocuments = 0;
        for (const LayoutNode& node : layout.vecNodes)
        {
            const QPoint ptCentre = view.MapToWidget(node.center).toPoint();
            if (node.bDocument)
            {
                ++iDocuments;
                QCOMPARE(view.FindDocumentAt(ptCentre), node.strId);
                QCOMPARE(view.FindConceptAt(ptCentre), QString());
            }
            else
            {
                QCOMPARE(view.FindConceptAt(ptCentre), node.strId);
                QCOMPARE(view.FindDocumentAt(ptCentre), QString());
            }
        }
        QCOMPARE(iDocuments > 0, eType == EGraphLayoutType::BIPARTITE);
        QCOMPARE(view.FindConceptAt(QPoint(1, view.height() - 2)), QString());
        QCOMPARE(view.FindEdgeAt(QPoint(1, view.height() - 2)), -1);
    }

    // 점을 누르면 고르고(시그널 · 강조로 그림이 바뀐다), 빈 곳을 누르면 고른 것을 지운다
    void clickSelectsNodeAndEmptyClickClears()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::NETWORK);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        const QImage imageBefore = ViewTestSupport::Render(view);
        QSignalSpy spyConcept(&view, &KnowledgeGraphView::ConceptActivated);
        QSignalSpy spyDocument(&view, &KnowledgeGraphView::DocumentActivated);

        const int iNode = FindNode(layout, DecodeUtf8(kYolov2Id));
        QVERIFY(iNode >= 0);
        const QPoint ptNode = view.MapToWidget(layout.vecNodes[iNode].center).toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, ptNode);
        QCOMPARE(spyConcept.count(), 1);
        QCOMPARE(spyConcept.at(0).at(0).toString(), DecodeUtf8(kYolov2Id));
        QCOMPARE(spyDocument.count(), 0);
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        const QImage imageSelected = ViewTestSupport::Render(view);
        QVERIFY(imageSelected != imageBefore);
        QVERIFY(ViewTestSupport::SaveSnapshot(imageSelected, QStringLiteral("graph-network-selected-light")));

        const QPoint ptEmpty(2, view.height() - 3);
        QCOMPARE(view.FindConceptAt(ptEmpty), QString());
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, ptEmpty);
        QCOMPARE(view.GetSelectedId(), QString());
        QCOMPARE(spyConcept.count(), 1);
        QCOMPARE(ViewTestSupport::Render(view), imageBefore);

        view.SetTheme(VisualMarkdownTheme::MakeDark());
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, ptNode);
        QVERIFY(ViewTestSupport::SaveSnapshot(ViewTestSupport::Render(view), QStringLiteral("graph-network-selected-dark")));
        view.ClearSelection();
        QCOMPARE(view.GetSelectedId(), QString());
    }

    // 문서 ↔ 개념: 문서 점을 누르면 DocumentActivated 를 낸다
    void clickDocumentEmitsDocumentActivated()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::BIPARTITE);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::BIPARTITE);
        QSignalSpy spyConcept(&view, &KnowledgeGraphView::ConceptActivated);
        QSignalSpy spyDocument(&view, &KnowledgeGraphView::DocumentActivated);
        const int iDocument = FindDocumentNode(layout, DecodeUtf8(kYolo9000Title));
        QVERIFY(iDocument >= 0);
        const QImage imageBefore = ViewTestSupport::Render(view);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.MapToWidget(layout.vecNodes[iDocument].center).toPoint());
        QCOMPARE(spyDocument.count(), 1);
        QCOMPARE(spyDocument.at(0).at(0).toString(), layout.vecNodes[iDocument].strId);
        QCOMPARE(spyConcept.count(), 0);
        QCOMPARE(view.GetSelectedId(), layout.vecNodes[iDocument].strId);
        const QImage imageSelected = ViewTestSupport::Render(view);
        QVERIFY(imageSelected != imageBefore);
        QVERIFY(ViewTestSupport::SaveSnapshot(imageSelected, QStringLiteral("graph-bipartite-selected-light")));
    }

    // 휠은 커서 둘레로 확대한다: 커서 아래 점은 그 자리에 머물고 다른 점은 움직인다
    void wheelZoomsAroundCursor()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::NETWORK);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        const int iAnchor = FindNode(layout, DecodeUtf8(kYolov2Id));
        const int iOther = FindNode(layout, DecodeUtf8(kMergedTo)) == iAnchor ? 0 : FindNode(layout, DecodeUtf8(kMergedTo));
        QVERIFY(iAnchor >= 0 && iOther >= 0 && iAnchor != iOther);
        const QPointF ptAnchor = view.MapToWidget(layout.vecNodes[iAnchor].center);
        const QPointF ptOther = view.MapToWidget(layout.vecNodes[iOther].center);
        const double dZoom = view.GetZoom();

        ViewTestSupport::SendWheel(view, ptAnchor.toPoint(), 2);
        QVERIFY(view.GetZoom() > dZoom);
        const QPointF ptAnchorAfter = view.MapToWidget(layout.vecNodes[iAnchor].center);
        QVERIFY2(QLineF(ptAnchorAfter, QPointF(ptAnchor.toPoint())).length() <= kCursorTolerance,
                 qPrintable(QStringLiteral("%1,%2 → %3,%4").arg(ptAnchor.x()).arg(ptAnchor.y()).arg(ptAnchorAfter.x()).arg(ptAnchorAfter.y())));
        QVERIFY(QLineF(view.MapToWidget(layout.vecNodes[iOther].center), ptOther).length() > kMinMovedDistance);

        ViewTestSupport::SendWheel(view, ptAnchor.toPoint(), -4);
        QVERIFY(view.GetZoom() < dZoom);
    }

    // 확대는 정한 범위 안에서 멈추고 좌표는 늘 유한하다
    void zoomIsClamped()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::NETWORK);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        const QPoint ptCursor(700, 400);
        for (int iNotch = 0; iNotch < kManyNotches; ++iNotch)
            ViewTestSupport::SendWheel(view, ptCursor, 1);
        const double dMaxZoom = view.GetZoom();
        QVERIFY2(dMaxZoom > 1 && dMaxZoom <= kMaxSaneZoom, qPrintable(QString::number(dMaxZoom)));
        ViewTestSupport::SendWheel(view, ptCursor, 1);
        QCOMPARE(view.GetZoom(), dMaxZoom);
        for (int iNotch = 0; iNotch < 2 * kManyNotches; ++iNotch)
            ViewTestSupport::SendWheel(view, ptCursor, -1);
        const double dMinZoom = view.GetZoom();
        QVERIFY2(dMinZoom >= kMinSaneZoom && dMinZoom < 1, qPrintable(QString::number(dMinZoom)));
        const QPointF ptMapped = view.MapToWidget(layout.vecNodes[0].center);
        QVERIFY(std::isfinite(ptMapped.x()) && std::isfinite(ptMapped.y()));
        QVERIFY(ViewTestSupport::HasEnoughInk(ViewTestSupport::Render(view), VisualMarkdownTheme::MakeLight().background) || dMinZoom < 1);
    }

    // 끌면 그림이 끈 만큼 옮겨지고(누르기로 치지 않는다), 두 번 누르면 다시 맞춘다
    void dragPansAndDoubleClickFits()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::NETWORK);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        QSignalSpy spyConcept(&view, &KnowledgeGraphView::ConceptActivated);
        const QPointF ptFitted = view.MapToWidget(layout.vecNodes[0].center);
        const double dFittedZoom = view.GetZoom();

        // 점 위에서 시작해도 끌기는 고르기가 아니다
        const QPoint ptStart = ptFitted.toPoint();
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonPress, ptStart, Qt::LeftButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseMove, ptStart + kDragDelta / 2, Qt::NoButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseMove, ptStart + kDragDelta, Qt::NoButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonRelease, ptStart + kDragDelta, Qt::LeftButton, Qt::NoButton);
        const QPointF ptDragged = view.MapToWidget(layout.vecNodes[0].center);
        QVERIFY2(QLineF(ptDragged, ptFitted + QPointF(kDragDelta)).length() < kPanTolerance,
                 qPrintable(QStringLiteral("%1,%2").arg(ptDragged.x()).arg(ptDragged.y())));
        QCOMPARE(spyConcept.count(), 0);
        QCOMPARE(view.GetSelectedId(), QString());

        ViewTestSupport::SendWheel(view, QPoint(500, 500), 3);
        QVERIFY(view.GetZoom() != dFittedZoom);
        QTest::mouseDClick(&view, Qt::LeftButton, Qt::NoModifier, QPoint(2, view.height() - 3));
        QCOMPARE(view.GetZoom(), dFittedZoom);
        QVERIFY(QLineF(view.MapToWidget(layout.vecNodes[0].center), ptFitted).length() < kPanTolerance);
    }

    // 어두운 테마로 바꾸면 바탕 점이 바뀐다
    void darkThemeChangesBackground()
    {
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, GetLayout(EGraphLayoutType::NETWORK), EGraphLayoutType::NETWORK);
        const QPoint ptCorner(2, view.height() - 3);
        view.SetTheme(VisualMarkdownTheme::MakeLight());
        const QColor light = ViewTestSupport::Render(view).pixelColor(ptCorner);
        view.SetTheme(VisualMarkdownTheme::MakeDark());
        const QColor dark = ViewTestSupport::Render(view).pixelColor(ptCorner);
        QCOMPARE(light, VisualMarkdownTheme::MakeLight().background);
        QCOMPARE(dark, VisualMarkdownTheme::MakeDark().background);
        QVERIFY(light != dark);
    }

    // 점 위 설명: 이름 · 종류 · 연도 · 정의 · 문서 수
    void nodeToolTipShowsConcept()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::NETWORK);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        const int iNode = FindNode(layout, DecodeUtf8(kYolov2Id));
        const QString strTip = view.GetToolTipAt(view.MapToWidget(layout.vecNodes[iNode].center).toPoint());
        const KnowledgeGraphNode& node = m_graph.GetNodes()[m_graph.FindNode(DecodeUtf8(kYolov2Id))];
        QVERIFY(!node.strDefinition.isEmpty());
        for (const QString& strPart : { DecodeUtf8(kYolov2Name), DecodeUtf8(kYolov2Kind), DecodeUtf8(kYolov2Year), DecodeUtf8(kYolov2Documents) })
            QVERIFY2(strTip.contains(strPart), qUtf8Printable(strPart + QStringLiteral(" in ") + strTip));
        QVERIFY2(QString(strTip).remove(QLatin1Char('\n')).remove(QLatin1Char(' ')).contains(QString(node.strDefinition).remove(QLatin1Char(' '))),
                 qUtf8Printable(strTip));
    }

    // 개념 점 설명에 그 개념이 개선한 것이 함께 나온다(examples/showcase/README.md §2.2: B 를 볼 때 A 와 무엇을 고쳤는지).
    // 계보도의 개선 선 5개마다 from 점(B) 설명에 to(A) 이름과 그 선의 설명이 모두 있다. 개선한 것이 없는 점에는 "개선:" 줄이 없다
    void conceptToolTipListsImprovements()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::LINEAGE);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::LINEAGE);
        int iImproves = 0;
        for (const LayoutEdge& layoutEdge : layout.vecEdges)
        {
            const KnowledgeGraphEdge& graphEdge = m_graph.GetEdges()[layoutEdge.iGraphEdge];
            if (graphEdge.edge.strKind != DecodeUtf8(kImproves))
                continue;
            ++iImproves;
            const QString strTip = view.GetToolTipAt(view.MapToWidget(layout.vecNodes[layoutEdge.iFrom].center).toPoint());
            const QString strFlatTip = RemoveSpaces(strTip);
            const QString strTarget = m_graph.GetNodes()[graphEdge.iTo].strName;
            QVERIFY2(strFlatTip.contains(RemoveSpaces(DecodeUtf8(kImprovedLabel) + strTarget)),
                     qUtf8Printable(strTarget + QStringLiteral(" in ") + strTip));
            QVERIFY(!graphEdge.edge.vecNotes.isEmpty());   // showcase 의 개선 선 다섯 모두 설명이 있다
            for (const QString& strNote : graphEdge.edge.vecNotes)
                QVERIFY2(strFlatTip.contains(RemoveSpaces(strNote)), qUtf8Printable(strNote + QStringLiteral(" in ") + strTip));
        }
        QCOMPARE(iImproves, kImprovesEdges);

        // 맨 옛것(R-CNN)은 아무것도 개선하지 않는다
        const int iOldest = FindNode(layout, DecodeUtf8(kOldestId));
        QVERIFY(iOldest >= 0);
        const QString strOldestTip = view.GetToolTipAt(view.MapToWidget(layout.vecNodes[iOldest].center).toPoint());
        QVERIFY(!strOldestTip.isEmpty());
        for (const QString& strLine : strOldestTip.split(QLatin1Char('\n')))
            QVERIFY2(!strLine.startsWith(DecodeUtf8(kImprovedLabel).trimmed()), qUtf8Printable(strOldestTip));

        // 개념 정보라 모든 보기(연도 없음 목록 포함)에서 같은 설명이다
        const QString strYolov2Tip = view.GetToolTipAt(view.MapToWidget(layout.vecNodes[FindNode(layout, DecodeUtf8(kYolov2Id))].center).toPoint());
        for (const EGraphLayoutType eType : kGraphTypes)
        {
            const GraphLayoutOutput& other = GetLayout(eType);
            KnowledgeGraphView otherView;
            otherView.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
            otherView.SetGraph(m_graph, other, eType);
            const int iNode = FindNode(other, DecodeUtf8(kYolov2Id));
            QVERIFY(iNode >= 0);
            QCOMPARE(otherView.GetToolTipAt(otherView.MapToWidget(other.vecNodes[iNode].center).toPoint()), strYolov2Tip);
        }
        // 연도 없음 목록 줄의 설명도 관계망 점의 설명과 같다
        const GraphLayoutOutput& timeline = GetLayout(EGraphLayoutType::TIMELINE);
        const GraphLayoutOutput& network = GetLayout(EGraphLayoutType::NETWORK);
        KnowledgeGraphView timelineView;
        timelineView.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        timelineView.SetGraph(m_graph, timeline, EGraphLayoutType::TIMELINE);
        KnowledgeGraphView networkView;
        networkView.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        networkView.SetGraph(m_graph, network, EGraphLayoutType::NETWORK);
        for (int iItem = 0; iItem < timeline.vecUnplaced.size(); ++iItem)
        {
            const int iNetworkNode = FindNode(network, timeline.vecUnplaced[iItem]);
            QVERIFY(iNetworkNode >= 0);
            QCOMPARE(timelineView.GetToolTipAt(timelineView.GetUnplacedItemRect(iItem).center().toPoint()),
                     networkView.GetToolTipAt(networkView.MapToWidget(network.vecNodes[iNetworkNode].center).toPoint()));
        }
    }

    // 합친 선 위 설명: from → to, 종류, 상태(하나라도 확인됨이면 확인됨), 설명, 출처 문서 수. 보기마다 같은 선이면 같은 설명이다
    void mergedEdgeToolTip_data()
    {
        QTest::addColumn<int>("type");
        QTest::newRow("network") << static_cast<int>(EGraphLayoutType::NETWORK);
        QTest::newRow("lineage") << static_cast<int>(EGraphLayoutType::LINEAGE);
        QTest::newRow("timeline") << static_cast<int>(EGraphLayoutType::TIMELINE);
    }
    void mergedEdgeToolTip()
    {
        QFETCH(int, type);
        const EGraphLayoutType eType = static_cast<EGraphLayoutType>(type);
        const GraphLayoutOutput& layout = GetLayout(eType);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, eType);
        const int iEdge = FindEdge(layout, DecodeUtf8(kMergedFrom), DecodeUtf8(kImproves), DecodeUtf8(kMergedTo));
        QVERIFY(iEdge >= 0);
        const KnowledgeEdge& edge = m_graph.GetEdges()[layout.vecEdges[iEdge].iGraphEdge].edge;
        QCOMPARE(edge.vecDocumentIds.size(), 2);
        QVERIFY(!edge.vecNotes.isEmpty());

        const QPointF ptMiddle = view.GetEdgeMidpoint(iEdge);
        QVERIFY(std::isfinite(ptMiddle.x()) && std::isfinite(ptMiddle.y()));
        QCOMPARE(view.FindEdgeAt(ptMiddle.toPoint()), iEdge);
        QCOMPARE(view.FindConceptAt(ptMiddle.toPoint()), QString());
        const QString strTip = view.GetToolTipAt(ptMiddle.toPoint());
        for (const QString& strPart : { DecodeUtf8(kMergedTitle), DecodeUtf8(kImprovesLabel), DecodeUtf8(kConfirmedLabel),
                                        DecodeUtf8(kMergedSources) })
            QVERIFY2(strTip.contains(strPart), qUtf8Printable(strPart + QStringLiteral(" in ") + strTip));
        QVERIFY2(!strTip.contains(DecodeUtf8(kProposedLabel)), qUtf8Printable(strTip));
        QVERIFY2(!strTip.contains(DecodeUtf8(kConceptFileSource)), qUtf8Printable(strTip));
        const QString strFlatTip = QString(strTip).remove(QLatin1Char('\n')).remove(QLatin1Char(' '));
        for (const QString& strNote : edge.vecNotes)
            QVERIFY2(strFlatTip.contains(QString(strNote).remove(QLatin1Char(' '))), qUtf8Printable(strNote + QStringLiteral(" in ") + strTip));
    }

    // AI 제안 선 위 설명에는 상태가 AI 제안이다. 관계망의 AI 제안 선 11개 모두
    void proposedEdgeToolTip()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::NETWORK);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        int iProposed = 0;
        for (int iEdge = 0; iEdge < layout.vecEdges.size(); ++iEdge)
        {
            if (layout.vecEdges[iEdge].eStatus != ERelationStatus::PROPOSED)
                continue;
            ++iProposed;
            const QPoint ptMiddle = view.GetEdgeMidpoint(iEdge).toPoint();
            const QString strTip = view.GetEdgeToolTip(iEdge);
            QVERIFY2(strTip.contains(DecodeUtf8(kProposedLabel)), qUtf8Printable(strTip));
            if (view.FindEdgeAt(ptMiddle) == iEdge && view.FindConceptAt(ptMiddle).isEmpty())
                QCOMPARE(view.GetToolTipAt(ptMiddle), strTip);
        }
        QCOMPARE(iProposed, kProposedEdges);
    }

    // 개념 파일과 합친 방향 없는 선: ↔ 로 잇고 출처에 개념 파일이 붙는다
    void conceptFileEdgeToolTip()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::NETWORK);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        const int iEdge = FindEdge(layout, DecodeUtf8(kAlternativeFrom), DecodeUtf8(kAlternative), DecodeUtf8(kAlternativeTo));
        QVERIFY(iEdge >= 0);
        QVERIFY(!layout.vecEdges[iEdge].bDirected);
        const QString strTip = view.GetEdgeToolTip(iEdge);
        for (const QString& strPart : { DecodeUtf8(kAlternativeTitle), DecodeUtf8(kAlternativeLabel), DecodeUtf8(kConfirmedLabel),
                                        DecodeUtf8(kAlternativeSources), DecodeUtf8(kConceptFileSource) })
            QVERIFY2(strTip.contains(strPart), qUtf8Printable(strPart + QStringLiteral(" in ") + strTip));
    }

    // 문서 ↔ 개념 선 위 설명: 문서 제목 → 개념 이름과 가리킨 횟수
    void bipartiteLinkToolTip()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::BIPARTITE);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::BIPARTITE);
        const int iDocument = FindDocumentNode(layout, DecodeUtf8(kYolo9000Title));
        const int iConcept = FindNode(layout, DecodeUtf8(kYolov2Id));
        int iLink = -1;
        for (int iEdge = 0; iEdge < layout.vecEdges.size(); ++iEdge)
            if (layout.vecEdges[iEdge].iFrom == iDocument && layout.vecEdges[iEdge].iTo == iConcept)
                iLink = iEdge;
        QVERIFY(iLink >= 0);
        QCOMPARE(layout.vecEdges[iLink].iWeight, kYolo9000ToYolov2Weight);
        const QString strTip = view.GetEdgeToolTip(iLink);
        QVERIFY2(strTip.contains(DecodeUtf8(kYolo9000Title)) && strTip.contains(DecodeUtf8(kYolov2Name)), qUtf8Printable(strTip));
        QVERIFY2(strTip.contains(QStringLiteral("가리킨 횟수 %1").arg(kYolo9000ToYolov2Weight)), qUtf8Printable(strTip));
        const QPointF ptMiddle = view.GetEdgeMidpoint(iLink);
        QVERIFY(std::isfinite(ptMiddle.x()));
    }

    // 타임라인: 연도 없는 점 15개를 "연도 없음" 목록으로 보이고, 목록 줄도 찾기 · 설명 · 누르기가 된다
    void timelineListsUnplaced()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::TIMELINE);
        QCOMPARE(layout.vecUnplaced.size(), kUnplacedCount);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::TIMELINE);
        QVERIFY(view.GetToolTipAt(view.GetUnplacedItemRect(0).center().toPoint()).contains(
            m_graph.GetNodes()[m_graph.FindNode(layout.vecUnplaced[0])].strName));
        QSignalSpy spyConcept(&view, &KnowledgeGraphView::ConceptActivated);
        const QRectF rectWidget(view.rect());
        for (int iItem = 0; iItem < layout.vecUnplaced.size(); ++iItem)
        {
            const QRectF rectItem = view.GetUnplacedItemRect(iItem);
            QVERIFY2(!rectItem.isEmpty() && rectWidget.contains(rectItem), qUtf8Printable(layout.vecUnplaced[iItem]));
            QVERIFY(!rectItem.intersects(view.GetLegendRect()));
            QCOMPARE(view.FindConceptAt(rectItem.center().toPoint()), layout.vecUnplaced[iItem]);
        }
        QVERIFY(view.GetUnplacedItemRect(kUnplacedCount).isEmpty());
        const QPoint ptLast = view.GetUnplacedItemRect(kUnplacedCount - 1).center().toPoint();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, ptLast);
        QCOMPARE(spyConcept.count(), 1);
        QCOMPARE(spyConcept.at(0).at(0).toString(), layout.vecUnplaced.last());
        QCOMPARE(view.GetSelectedId(), layout.vecUnplaced.last());
        QVERIFY(view.GetUnplacedTitle().startsWith(DecodeUtf8(kUnplacedTitle)));

        // 다른 보기에는 목록이 없다
        KnowledgeGraphView network;
        network.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        network.SetGraph(m_graph, GetLayout(EGraphLayoutType::NETWORK), EGraphLayoutType::NETWORK);
        QVERIFY(network.GetUnplacedItemRect(0).isEmpty());
    }

    // 갈래 이름 칸 위에서는 갈래 이름이 설명으로 나온다
    void laneLabelToolTip()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::LINEAGE);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::LINEAGE);
        QVERIFY(!layout.vecLanes.isEmpty());
        for (int iLane = 0; iLane < layout.vecLanes.size(); ++iLane)
        {
            const QString strTip = view.GetToolTipAt(view.GetLaneLabelRect(iLane).center().toPoint());
            QVERIFY2(strTip.contains(layout.vecLanes[iLane].strLabel), qUtf8Printable(strTip));
        }
    }

    // 확대 · 축소 · 옮기기를 해도 갈래 이름 칸은 점 상자 왼쪽에 붙어 있어 점과 겹치지 않는다
    void laneLabelsStayClearWhenZoomed_data()
    {
        QTest::addColumn<int>("type");
        QTest::newRow("lineage") << static_cast<int>(EGraphLayoutType::LINEAGE);
        QTest::newRow("timeline") << static_cast<int>(EGraphLayoutType::TIMELINE);
    }
    void laneLabelsStayClearWhenZoomed()
    {
        QFETCH(int, type);
        const EGraphLayoutType eType = static_cast<EGraphLayoutType>(type);
        const GraphLayoutOutput& layout = GetLayout(eType);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, eType);
        for (const int iNotches : { 4, -9, 12 })
        {
            ViewTestSupport::SendWheel(view, QPoint(700, 450), iNotches);
            for (int iLane = 0; iLane < layout.vecLanes.size(); ++iLane)
            {
                const QRectF rectLabel = view.GetLaneLabelRect(iLane);
                QVERIFY(!rectLabel.isEmpty());
                for (int iNode = 0; iNode < layout.vecNodes.size(); ++iNode)
                    QVERIFY2(!rectLabel.intersects(view.GetNodeRect(iNode)), qUtf8Printable(layout.vecNodes[iNode].strId));
            }
        }
    }

    // 설명에 넣는 사용자 글(갈래 이름 · 문서 제목 · 개념 이름 · 선 설명)에 %1 · %2 가 있어도 그대로 나온다.
    // 숫자를 뒤에 따로 .arg 로 넣으면 앞에 넣은 글의 %1 이 그 숫자로 바뀐다
    void userTextWithPercentSignsIsKept()
    {
        // 갈래 이름(배치만, 그래프 없음)
        GraphLayoutOutput laneLayout;
        laneLayout.vecNodes << MakeNode(QStringLiteral("p"), DecodeUtf8(kPercentNode), QPointF(60, 30), false);
        laneLayout.vecLanes.push_back({ DecodeUtf8(kPercentLane), 0, 60 });
        KnowledgeGraphView laneView;
        laneView.resize(ViewTestSupport::kSnapshotWidth / 2, ViewTestSupport::kSnapshotHeight / 2);
        laneView.SetGraph(KnowledgeGraph(), laneLayout, EGraphLayoutType::LINEAGE);
        const QString strLaneTip = laneView.GetToolTipAt(laneView.GetLaneLabelRect(0).center().toPoint());
        QVERIFY2(strLaneTip.contains(QStringLiteral("갈래: ") + DecodeUtf8(kPercentLane)) && strLaneTip.contains(QStringLiteral("점 1개")),
                 qUtf8Printable(strLaneTip));
        QVERIFY(laneView.GetToolTipAt(laneView.MapToWidget(laneLayout.vecNodes[0].center).toPoint()).startsWith(DecodeUtf8(kPercentNode)));

        // 문서 제목 · 개념 이름(문서 ↔ 개념 배치만): 문서 점 설명과 가리킴 선 설명
        GraphLayoutOutput linkLayout;
        linkLayout.vecNodes << MakeNode(QStringLiteral("d"), DecodeUtf8(kPercentDocument), QPointF(60, 26), true)
                            << MakeNode(QStringLiteral("c"), DecodeUtf8(kPercentConcept), QPointF(420, 26), false);
        LayoutEdge link;
        link.iFrom = 0;
        link.iTo = 1;
        link.bDirected = false;
        link.iWeight = kPercentLinkWeight;
        linkLayout.vecEdges << link;
        KnowledgeGraphView linkView;
        linkView.resize(ViewTestSupport::kSnapshotWidth / 2, ViewTestSupport::kSnapshotHeight / 2);
        linkView.SetGraph(KnowledgeGraph(), linkLayout, EGraphLayoutType::BIPARTITE);
        const QString strLinkTip = linkView.GetEdgeToolTip(0);
        QVERIFY2(strLinkTip.contains(DecodeUtf8(kPercentDocument) + QStringLiteral(" → ") + DecodeUtf8(kPercentConcept))
                     && strLinkTip.contains(QStringLiteral("가리킨 횟수 %1").arg(kPercentLinkWeight)),
                 qUtf8Printable(strLinkTip));
        const QString strDocumentTip = linkView.GetToolTipAt(linkView.MapToWidget(linkLayout.vecNodes[0].center).toPoint());
        QVERIFY2(strDocumentTip.contains(DecodeUtf8(kPercentDocument)) && strDocumentTip.contains(QStringLiteral("문서 · 개념 1개")),
                 qUtf8Printable(strDocumentTip));

        // 개념 이름 · 정의 · 선 설명(그래프에서 만든 관계망): 점 설명과 선 설명
        KnowledgeConcept first;
        first.strId = QStringLiteral("a");
        first.strKind = QStringLiteral("method");
        first.strName = DecodeUtf8(kPercentConcept);
        first.strSummary = DecodeUtf8(kPercentDefinition);
        KnowledgeConcept second;
        second.strId = QStringLiteral("b");
        second.strKind = QStringLiteral("method");
        second.strName = DecodeUtf8(kPercentTarget);
        KnowledgeEdge edge;
        edge.strFromId = first.strId;
        edge.strKind = DecodeUtf8(kImproves);
        edge.strToId = second.strId;
        edge.eStatus = ERelationStatus::CONFIRMED;
        edge.vecDocumentIds << QStringLiteral("doc");
        edge.vecNotes << DecodeUtf8(kPercentNote);
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts({ first, second }, { edge }, {});
        GraphLayoutOutput network;
        QVERIFY(ViewTestSupport::RunLayout(graph, EGraphLayoutType::NETWORK, &network));
        KnowledgeGraphView networkView;
        networkView.resize(ViewTestSupport::kSnapshotWidth / 2, ViewTestSupport::kSnapshotHeight / 2);
        networkView.SetGraph(graph, network, EGraphLayoutType::NETWORK);
        const int iFirst = FindNode(network, first.strId);
        QVERIFY(iFirst >= 0);
        const QString strNodeTip = networkView.GetToolTipAt(networkView.MapToWidget(network.vecNodes[iFirst].center).toPoint());
        for (const QString& strPart : { DecodeUtf8(kPercentConcept), DecodeUtf8(kPercentDefinition), DecodeUtf8(kImprovedLabel) + DecodeUtf8(kPercentTarget),
                                        DecodeUtf8(kPercentNote) })
            QVERIFY2(strNodeTip.contains(strPart), qUtf8Printable(strPart + QStringLiteral(" in ") + strNodeTip));
        QCOMPARE(network.vecEdges.size(), 1);
        const QString strEdgeTip = networkView.GetEdgeToolTip(0);
        for (const QString& strPart : { DecodeUtf8(kPercentConcept) + QStringLiteral(" → ") + DecodeUtf8(kPercentTarget), DecodeUtf8(kPercentNote),
                                        QStringLiteral("문서 1개") })
            QVERIFY2(strEdgeTip.contains(strPart), qUtf8Printable(strPart + QStringLiteral(" in ") + strEdgeTip));
    }

    // 끄는 중에 그래프가 바뀌면 끌기를 그만두고 손 모양 커서를 되돌린다. 그 뒤의 놓기는 고르기로 치지 않는다
    void setGraphWhileDraggingRestoresCursor()
    {
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, GetLayout(EGraphLayoutType::NETWORK), EGraphLayoutType::NETWORK);
        const QPoint ptStart(2, view.height() - 3);
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonPress, ptStart, Qt::LeftButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseMove, ptStart + kDragDelta, Qt::NoButton, Qt::LeftButton);
        QVERIFY(view.cursor().shape() == Qt::ClosedHandCursor);

        // 최상위 창은 unsetCursor 뒤에도 WA_SetCursor 가 남으므로(Qt 5) 커서 모양으로 본다
        const GraphLayoutOutput& lineage = GetLayout(EGraphLayoutType::LINEAGE);
        view.SetGraph(m_graph, lineage, EGraphLayoutType::LINEAGE);
        QVERIFY(view.cursor().shape() == Qt::ArrowCursor);

        QSignalSpy spyConcept(&view, &KnowledgeGraphView::ConceptActivated);
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonRelease, view.MapToWidget(lineage.vecNodes[0].center).toPoint(), Qt::LeftButton, Qt::NoButton);
        QCOMPARE(spyConcept.count(), 0);
        QCOMPARE(view.GetSelectedId(), QString());
    }

    // 빈 배치 · 맞지 않는 그래프도 죽지 않고 그린다
    void emptyLayoutDoesNotCrash()
    {
        KnowledgeGraphView view;
        view.resize(400, 300);
        QVERIFY(!ViewTestSupport::Render(view).isNull());
        view.SetGraph(KnowledgeGraph(), GraphLayoutOutput(), EGraphLayoutType::NETWORK);
        QVERIFY(!ViewTestSupport::Render(view).isNull());
        QCOMPARE(view.FindConceptAt(QPoint(200, 150)), QString());
        QCOMPARE(view.FindEdgeAt(QPoint(200, 150)), -1);
        QCOMPARE(view.GetToolTipAt(QPoint(200, 150)), QString());
        QVERIFY(view.GetNodeRect(0).isEmpty());
        QVERIFY(std::isnan(view.GetEdgeMidpoint(0).x()));
        view.FitToView();
        ViewTestSupport::SendWheel(view, QPoint(10, 10), 1);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));

        // 배치와 다른(빈) 그래프: 점 정보를 못 찾아도 배치만으로 그린다
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(KnowledgeGraph(), GetLayout(EGraphLayoutType::NETWORK), EGraphLayoutType::NETWORK);
        QVERIFY(ViewTestSupport::HasEnoughInk(ViewTestSupport::Render(view), VisualMarkdownTheme::MakeLight().background));
        const QPoint ptNode = view.MapToWidget(GetLayout(EGraphLayoutType::NETWORK).vecNodes[0].center).toPoint();
        QCOMPARE(view.FindConceptAt(ptNode), GetLayout(EGraphLayoutType::NETWORK).vecNodes[0].strId);
        QVERIFY(!view.GetToolTipAt(ptNode).isEmpty());
        for (int iEdge = 0; iEdge < GetLayout(EGraphLayoutType::NETWORK).vecEdges.size(); ++iEdge)
            view.GetEdgeToolTip(iEdge);
    }

private:
    const GraphLayoutOutput& GetLayout(EGraphLayoutType type) const
    {
        return m_vecLayouts[static_cast<int>(type)];
    }

    static int FindNode(const GraphLayoutOutput& layout, const QString& id)
    {
        for (int iNode = 0; iNode < layout.vecNodes.size(); ++iNode)
            if (!layout.vecNodes[iNode].bDocument && layout.vecNodes[iNode].strId == id)
                return iNode;
        return -1;
    }

    // 합성 배치의 점 하나(종류는 방법, 문서는 종류 없음)
    static LayoutNode MakeNode(const QString& id, const QString& label, const QPointF& center, bool document)
    {
        LayoutNode node;
        node.strId = id;
        node.strLabel = label;
        node.strKind = document ? QString() : QStringLiteral("method");
        node.center = center;
        node.bDocument = document;
        return node;
    }

    static int FindDocumentNode(const GraphLayoutOutput& layout, const QString& title)
    {
        for (int iNode = 0; iNode < layout.vecNodes.size(); ++iNode)
            if (layout.vecNodes[iNode].bDocument && layout.vecNodes[iNode].strLabel == title)
                return iNode;
        return -1;
    }

    // 배치 선 가운데 그래프 선이 (from · 종류 · to) 인 것
    int FindEdge(const GraphLayoutOutput& layout, const QString& from, const QString& kind, const QString& to) const
    {
        for (int iEdge = 0; iEdge < layout.vecEdges.size(); ++iEdge)
        {
            const int iGraphEdge = layout.vecEdges[iEdge].iGraphEdge;
            if (iGraphEdge < 0)
                continue;
            const KnowledgeEdge& edge = m_graph.GetEdges()[iGraphEdge].edge;
            if (edge.strFromId == from && edge.strKind == kind && edge.strToId == to)
                return iEdge;
        }
        return -1;
    }

private:
    KnowledgeIndex m_index;                     // showcase wiki 를 읽은 색인
    KnowledgeGraph m_graph;                     // 그 색인으로 만든 그래프
    QVector<GraphLayoutOutput> m_vecLayouts;    // EGraphLayoutType 값 → 배치 결과
};

std::unique_ptr<QObject> CreateKnowledgeGraphViewTest() { return std::make_unique<KnowledgeGraphViewTest>(); }

#include "KnowledgeGraphViewTest.moc"
