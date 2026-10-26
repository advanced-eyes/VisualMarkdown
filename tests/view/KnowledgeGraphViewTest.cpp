#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/KnowledgeIndex.h"
#include "VisualMarkdown/UI/KnowledgeGraphView.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"
#include "ViewTestSupport.h"

#include <QLineF>
#include <QPolygonF>
#include <QSignalSpy>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <memory>

namespace
{
// ================================================================================================
// 기대값: examples/showcase/README.md §2 와 design/Views.md §7 이다. 예시를 고치면 여기를 함께 고친다.
// ================================================================================================

const QVector<EGraphLayoutType> kGraphTypes {
    EGraphLayoutType::NETWORK, EGraphLayoutType::LINEAGE, EGraphLayoutType::TREE, EGraphLayoutType::TIMELINE, EGraphLayoutType::BIPARTITE,
    EGraphLayoutType::EGO,
};

// 이웃 보기(design/MoreViews.md §6.2): 기본 초점 Faster R-CNN 은 테두리가 두 배이고 이름이 굵다. 그림이 달라지는 곳은 초점 상자 둘레뿐이다
constexpr const char* kEgoFocusId = "faster-r-cnn";
constexpr const char* kEgoRingOneId = "yolo";   // 1 걸음 점(누르면 관계망처럼 고르고 시그널을 낸다)
constexpr int kFocusMargin = 4;                 // 초점 상자에서 이만큼(px) 밖까지가 테두리 띠, 이만큼 안부터가 글자 칸이다
constexpr int kEgoContextEdges = 7;             // 기본 초점의 둘레 선(design/MoreViews.md §7.3)
constexpr double kEdgeMargin = 4;               // 선(꺾은선 · 화살촉)에서 이만큼(px) 안이 그 선이 칠하는 자리다
constexpr double kMaxContextInkShare = 0.8;     // 둘레 선 자리의 잉크(바탕과의 색 거리 합)는 나무 선으로 그린 것의 이만큼보다 적다

// 점에서 꺾은선까지의 거리
double GetDistanceToPolyline(const QPointF& point, const QPolygonF& line)
{
    double dBest = qInf();
    for (int iPoint = 1; iPoint < line.size(); ++iPoint)
    {
        const QPointF ptSegment = line[iPoint] - line[iPoint - 1];
        const double dLengthSquared = QPointF::dotProduct(ptSegment, ptSegment);
        const double dT = dLengthSquared > 0 ? qBound(0.0, QPointF::dotProduct(point - line[iPoint - 1], ptSegment) / dLengthSquared, 1.0) : 0.0;
        dBest = qMin(dBest, QLineF(point, line[iPoint - 1] + ptSegment * dT).length());
    }
    return dBest;
}

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

// 범례 판 끄기 · 밖에서 고르기 · 빈 곳 누르기(Ocean 이 쓰는 위젯 API L1–L3)
constexpr int kPanelGap = 12;                   // 맞춤 영역과 오른쪽 판 사이 틈(px, KnowledgeGraphView 와 같은 값)
constexpr double kFitTolerance = 0.01;          // 맞춤 영역 너비를 견줄 때의 여유(px)
constexpr const char* kUnknownId = "no-such-concept";   // 어느 배치에도 없는 개념 id
constexpr int kUnplacedPick = 2;                // 밖에서 고르는 연도 없음 줄
constexpr int kLegendProbeStep = 4;             // 범례가 있던 자리에서 점이 없는 곳을 찾는 걸음(px)
constexpr int kZoomIntoCorner = 6;              // 왼쪽 위 구석으로 휠을 이만큼 돌리면 점 대부분이 위젯 밖이다

// 신호 안에서 SetGraph 를 불러도 id 를 복사해 낸다(L10)
constexpr const char* kRenamedPrefix = "other-";   // 신호 안에서 넣는 배치는 id 마다 이 글을 앞에 붙인다
constexpr int kMaxRefills = 4096;                  // 풀린 저장소를 덮을 때까지 만드는 사본 수의 위

// L10: source 의 사본(떼어 내 새 저장소를 잡은 것)을 만들어 pRefills 에 둔다. 사본 하나의 원소 자리가 받은 id 의 자리(pId)를 덮으면,
// 또는 kMaxRefills 개가 되면 그친다. source 는 누른 배치와 같은 배치를 이름만 바꾼 것이라 저장소 크기가 같고, 같은 크기 블록은
// 막 풀린 블록을 다시 쓴다. 그래서 배치 안 id 의 참조를 그대로 실은 신호는 그 자리에서 사본의 id(앞에 kRenamedPrefix)를 읽어 틀린다.
// 풀린 메모리가 우연히 그대로 남아 시험이 통과하는 일을 막는다. 복사해 실은 id 는 스택에 있어 덮이지 않는다(그때는 kMaxRefills 개를 만든다)
template <typename Container>
void RefillFreedStorage(const QString* pId, const Container& source, QVector<Container>* pRefills)
{
    if (source.isEmpty())
        return;
    const quintptr uId = reinterpret_cast<quintptr>(pId);
    while (pRefills->size() < kMaxRefills)
    {
        Container copy = source;
        copy.detach();
        const quintptr uBegin = reinterpret_cast<quintptr>(&copy.constFirst());
        const quintptr uEnd = reinterpret_cast<quintptr>(&copy.constLast() + 1);
        pRefills->append(copy);
        if (uId >= uBegin && uId < uEnd)
            return;
    }
}

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

// 지식 사이 보기 위젯(KnowledgeGraphView, design/Views.md §7)을 showcase 의 배치 6가지(이웃 포함)로 시험한다(offscreen).
// 그리기(비지 않음 · 두 번 그리면 같음 · 테마), 맞춤, 위치로 찾기, 누르기 · 휠 · 끌기, 마우스 올림 설명, 이웃의 초점 강조.
class KnowledgeGraphViewTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(m_index.Load(QStringLiteral(VISUALMARKDOWN_SHOWCASE_DIR)), VISUALMARKDOWN_SHOWCASE_DIR);
        m_graph = KnowledgeGraph::BuildFromIndex(m_index);
        m_vecLayouts.resize(static_cast<int>(EGraphLayoutType::EGO) + 1);
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

    // 이웃 보기: 초점 점(bFocus)은 테두리를 보통의 두 배로 두껍게 하고 이름을 굵게 쓴다(design/MoreViews.md §6.2).
    // 초점 표시를 지운 같은 배치와 견주면 초점 상자 둘레만 다르고(테두리 띠도, 안쪽 글자도), 그 밖은 관계망과 같다:
    // 초점 표시를 지운 배치는 관계망으로 넘겨도 같은 그림이다. 밝은 · 어두운 테마 모두
    void egoFocusIsEmphasized()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::EGO);
        QVERIFY(!layout.vecNodes.isEmpty());
        QCOMPARE(layout.vecNodes[0].strId, DecodeUtf8(kEgoFocusId));
        QVERIFY(layout.vecNodes[0].bFocus);
        GraphLayoutOutput plain = layout;
        for (LayoutNode& node : plain.vecNodes)
            node.bFocus = false;
        for (const bool bDark : { false, true })
        {
            const VisualMarkdownTheme theme = bDark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
            KnowledgeGraphView focusView;
            KnowledgeGraphView plainView;
            KnowledgeGraphView networkView;
            for (KnowledgeGraphView* pView : { &focusView, &plainView, &networkView })
            {
                pView->resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
                pView->SetTheme(theme);
            }
            focusView.SetGraph(m_graph, layout, EGraphLayoutType::EGO);
            plainView.SetGraph(m_graph, plain, EGraphLayoutType::EGO);
            networkView.SetGraph(m_graph, plain, EGraphLayoutType::NETWORK);
            const QImage imageFocus = ViewTestSupport::Render(focusView);
            const QImage imagePlain = ViewTestSupport::Render(plainView);
            QVERIFY(ViewTestSupport::Render(networkView) == imagePlain);
            QVERIFY(imageFocus != imagePlain);

            const QRectF rectFocus = focusView.GetNodeRect(0);
            QCOMPARE(rectFocus, plainView.GetNodeRect(0));
            const QRect rectOuter = rectFocus.adjusted(-kFocusMargin, -kFocusMargin, kFocusMargin, kFocusMargin).toAlignedRect();
            const QRect rectInner = rectFocus.adjusted(kFocusMargin, kFocusMargin, -kFocusMargin, -kFocusMargin).toAlignedRect();
            int iBorderChanges = 0;
            int iLabelChanges = 0;
            QString strOutside;
            for (int iY = 0; iY < imageFocus.height(); ++iY)
            {
                for (int iX = 0; iX < imageFocus.width(); ++iX)
                {
                    if (imageFocus.pixel(iX, iY) == imagePlain.pixel(iX, iY))
                        continue;
                    if (!rectOuter.contains(iX, iY))
                        strOutside = QStringLiteral("%1,%2").arg(iX).arg(iY);
                    else if (rectInner.contains(iX, iY))
                        ++iLabelChanges;
                    else
                        ++iBorderChanges;
                }
            }
            QVERIFY2(strOutside.isEmpty(), qPrintable(strOutside + QStringLiteral(" outside ") + ViewTestSupport::DescribeRect(rectOuter)));
            QVERIFY2(iBorderChanges > 0, "the focus border is not thicker");
            QVERIFY2(iLabelChanges > 0, "the focus label is not bold");
            QVERIFY(ViewTestSupport::SaveSnapshot(imageFocus, QStringLiteral("graph-ego-focus-%1").arg(bDark ? "dark" : "light")));
        }
    }

    // 이웃 보기의 둘레 선(bContext)은 테마의 dContextOpacity 만큼 흐리게 그린다(선과 화살촉, design/MoreViews.md §6.2).
    // 같은 배치를 모두 나무 선으로 그린 그림과 견주면 둘레 선 자리만 다르고(점 상자 · 범례 · 나무 선은 그대로), 그 자리의 잉크가 적다.
    // 찾기 · 마우스 올림 설명은 같다. 밝은 · 어두운 테마 모두
    void egoContextEdgesAreFainter()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::EGO);
        GraphLayoutOutput allTree = layout;
        QVector<int> vecContextEdges;
        for (int iEdge = 0; iEdge < allTree.vecEdges.size(); ++iEdge)
        {
            if (allTree.vecEdges[iEdge].bContext)
                vecContextEdges << iEdge;
            allTree.vecEdges[iEdge].bContext = false;
        }
        QCOMPARE(vecContextEdges.size(), kEgoContextEdges);
        for (const bool bDark : { false, true })
        {
            const VisualMarkdownTheme theme = bDark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
            KnowledgeGraphView contextView;
            KnowledgeGraphView treeView;
            for (KnowledgeGraphView* pView : { &contextView, &treeView })
            {
                pView->resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
                pView->SetTheme(theme);
            }
            contextView.SetGraph(m_graph, layout, EGraphLayoutType::EGO);
            treeView.SetGraph(m_graph, allTree, EGraphLayoutType::EGO);
            const QImage imageContext = ViewTestSupport::Render(contextView);
            const QImage imageTree = ViewTestSupport::Render(treeView);
            QVERIFY(imageContext != imageTree);

            // 다른 점은 둘레 선 가까이에만 있고, 둘레 선 자리(점 상자 밖)의 잉크는 나무 선으로 그린 것보다 적다
            QVector<QPolygonF> vecLines;
            QVector<QRectF> vecArrows;
            for (const int iEdge : vecContextEdges)
            {
                QCOMPARE(contextView.GetEdgeLine(iEdge), treeView.GetEdgeLine(iEdge));   // 모양은 같다
                vecLines << contextView.GetEdgeLine(iEdge);
                vecArrows << contextView.GetEdgeArrow(iEdge).boundingRect().adjusted(-kEdgeMargin, -kEdgeMargin, kEdgeMargin, kEdgeMargin);
            }
            QVector<QRectF> vecBoxes;
            for (int iNode = 0; iNode < layout.vecNodes.size(); ++iNode)
                vecBoxes << contextView.GetNodeRect(iNode);
            const auto funcNearContext = [&vecLines, &vecArrows](const QPointF& point)
            {
                for (int iEdge = 0; iEdge < vecLines.size(); ++iEdge)
                    if (GetDistanceToPolyline(point, vecLines[iEdge]) <= kEdgeMargin || vecArrows[iEdge].contains(point))
                        return true;
                return false;
            };
            QString strOutside;
            double dContextInk = 0;
            double dTreeInk = 0;
            for (int iY = 0; iY < imageContext.height(); ++iY)
            {
                for (int iX = 0; iX < imageContext.width(); ++iX)
                {
                    const QPointF ptPixel(iX + 0.5, iY + 0.5);
                    const bool bDifferent = imageContext.pixel(iX, iY) != imageTree.pixel(iX, iY);
                    if (bDifferent && !funcNearContext(ptPixel))
                        strOutside = QStringLiteral("%1,%2").arg(iX).arg(iY);
                    if (!bDifferent || std::any_of(vecBoxes.cbegin(), vecBoxes.cend(), [&ptPixel](const QRectF& box) { return box.contains(ptPixel); }))
                        continue;
                    dContextInk += ViewTestSupport::GetColorDistance(imageContext.pixelColor(iX, iY), theme.background);
                    dTreeInk += ViewTestSupport::GetColorDistance(imageTree.pixelColor(iX, iY), theme.background);
                }
            }
            QVERIFY2(strOutside.isEmpty(), qPrintable(strOutside));
            QVERIFY2(dTreeInk > 0 && dContextInk < kMaxContextInkShare * dTreeInk, qPrintable(QStringLiteral("%1 / %2").arg(dContextInk).arg(dTreeInk)));

            // 찾기 · 마우스 올림은 나무 선과 같다
            for (const int iEdge : vecContextEdges)
            {
                const QPoint ptMiddle = contextView.GetEdgeMidpoint(iEdge).toPoint();
                QCOMPARE(contextView.FindEdgeAt(ptMiddle), treeView.FindEdgeAt(ptMiddle));
                QCOMPARE(contextView.GetToolTipAt(ptMiddle), treeView.GetToolTipAt(ptMiddle));
            }
            QVERIFY(ViewTestSupport::SaveSnapshot(imageContext, QStringLiteral("graph-ego-context-%1").arg(bDark ? "dark" : "light")));
        }
    }

    // 이웃 보기도 누르기 · 고르기 · 시그널이 관계망과 같다: 1 걸음 점을 누르면 ConceptActivated 를 내고 그 점과 이웃을 강조한다
    void egoClickSelectsLikeNetwork()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::EGO);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::EGO);
        const QImage imageBefore = ViewTestSupport::Render(view);
        QSignalSpy spyConcept(&view, &KnowledgeGraphView::ConceptActivated);
        const int iNode = FindNode(layout, DecodeUtf8(kEgoRingOneId));
        QVERIFY(iNode > 0);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.GetNodeRect(iNode).center().toPoint());
        QCOMPARE(spyConcept.count(), 1);
        QCOMPARE(spyConcept.at(0).at(0).toString(), DecodeUtf8(kEgoRingOneId));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kEgoRingOneId));
        QVERIFY(ViewTestSupport::Render(view) != imageBefore);
        // 초점 점도 누를 수 있다
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.GetNodeRect(0).center().toPoint());
        QCOMPARE(spyConcept.count(), 2);
        QCOMPARE(spyConcept.at(1).at(0).toString(), DecodeUtf8(kEgoFocusId));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kEgoFocusId));
    }

    // L1: 범례 판을 끄면 판을 그리지 않고 GetLegendRect 가 빈 사각형이다. 연도 없음 판이 없는 보기는 맞춤 영역이 범례 폭 + 판 틈만큼 넓고,
    // 맞춘 점 상자는 모두 위젯 · 맞춤 영역 안이다. 테마 · 그래프를 바꿔도 꺼진 채이고, 다시 켜면 처음 그림과 같다
    void legendCanBeHidden_data() { layouts_data(); }
    void legendCanBeHidden()
    {
        QFETCH(int, type);
        const EGraphLayoutType eType = static_cast<EGraphLayoutType>(type);
        const GraphLayoutOutput& layout = GetLayout(eType);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, eType);
        QVERIFY(view.IsLegendVisible());   // 기본은 켜 있다
        const QImage imageShown = ViewTestSupport::Render(view);
        const QRectF rectLegend = view.GetLegendRect();
        const QRectF rectFitShown = view.GetFitRect();
        QVERIFY(!rectLegend.isEmpty());

        view.SetLegendVisible(false);
        QVERIFY(!view.IsLegendVisible());
        QVERIFY2(view.GetLegendRect().isEmpty(), qUtf8Printable(ViewTestSupport::DescribeRect(view.GetLegendRect())));
        const QRectF rectFit = view.GetFitRect();
        QCOMPARE(rectFit.left(), rectFitShown.left());
        QCOMPARE(rectFit.top(), rectFitShown.top());
        QCOMPARE(rectFit.height(), rectFitShown.height());
        if (layout.vecUnplaced.isEmpty())
        {
            QVERIFY2(std::abs(rectFit.width() - (rectFitShown.width() + rectLegend.width() + kPanelGap)) < kFitTolerance,
                     qUtf8Printable(ViewTestSupport::DescribeRect(rectFit) + QLatin1Char(' ') + ViewTestSupport::DescribeRect(rectFitShown)
                                    + QLatin1Char(' ') + ViewTestSupport::DescribeRect(rectLegend)));
        }
        else
        {
            // 오른쪽 판은 연도 없음 판이다: 맞춤 영역은 그 판 왼쪽에서 판 틈만큼 앞에서 끝난다
            const QRectF rectItem = view.GetUnplacedItemRect(0);
            QVERIFY(!rectItem.isEmpty());
            QVERIFY(rectFit.width() >= rectFitShown.width());
            QVERIFY2(rectFit.right() + kPanelGap <= rectItem.left(), qUtf8Printable(ViewTestSupport::DescribeRect(rectFit) + ViewTestSupport::DescribeRect(rectItem)));
        }
        const QImage imageHidden = ViewTestSupport::Render(view);
        QVERIFY(imageHidden != imageShown);
        QVERIFY(ViewTestSupport::HasEnoughInk(imageHidden, VisualMarkdownTheme::MakeLight().background));
        QVERIFY(ViewTestSupport::SaveSnapshot(imageHidden, QStringLiteral("extra-graph-%1-nolegend-light").arg(ViewTestSupport::GetLayoutName(eType))));
        const QRectF rectWidget(view.rect());
        for (int iNode = 0; iNode < layout.vecNodes.size(); ++iNode)
        {
            const QRectF rectNode = view.GetNodeRect(iNode);
            const QString strWhat = layout.vecNodes[iNode].strId + QLatin1Char(' ') + ViewTestSupport::DescribeRect(rectNode);
            QVERIFY2(rectWidget.contains(rectNode), qUtf8Printable(strWhat));
            QVERIFY2(rectFit.contains(rectNode), qUtf8Printable(strWhat + QStringLiteral(" fit ") + ViewTestSupport::DescribeRect(rectFit)));
        }

        // 테마 · 그래프를 바꿔도 꺼진 채다
        view.SetTheme(VisualMarkdownTheme::MakeDark());
        QVERIFY(!view.IsLegendVisible());
        QVERIFY(view.GetLegendRect().isEmpty());
        view.SetTheme(VisualMarkdownTheme::MakeLight());
        view.SetGraph(m_graph, layout, eType);
        QVERIFY(!view.IsLegendVisible());
        QVERIFY(view.GetLegendRect().isEmpty());
        QCOMPARE(ViewTestSupport::Render(view), imageHidden);

        // 다시 켜면 처음 그림과 같다
        view.SetLegendVisible(true);
        QVERIFY(view.IsLegendVisible());
        QCOMPARE(view.GetLegendRect(), rectLegend);
        QCOMPARE(view.GetFitRect(), rectFitShown);
        QCOMPARE(ViewTestSupport::Render(view), imageShown);
    }

    // L1: 사용자가 확대 · 옮긴 뒤에는 범례 판을 끄고 켜도 확대 · 옮김이 그대로다(다시 맞추지 않는다).
    // 맞추면 넓어진 맞춤 영역을 쓰고, 그 뒤 범례 판을 켜면 다시 맞춰 점 상자가 판과 겹치지 않는다
    void legendToggleKeepsUserZoom()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::NETWORK);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        ViewTestSupport::SendWheel(view, QPoint(500, 400), 2);
        const double dZoom = view.GetZoom();
        const QPointF ptNode = view.MapToWidget(layout.vecNodes[0].center);
        view.SetLegendVisible(false);
        QCOMPARE(view.GetZoom(), dZoom);
        QCOMPARE(view.MapToWidget(layout.vecNodes[0].center), ptNode);
        view.SetLegendVisible(true);
        QCOMPARE(view.GetZoom(), dZoom);
        QCOMPARE(view.MapToWidget(layout.vecNodes[0].center), ptNode);

        view.SetLegendVisible(false);
        view.FitToView();
        const QRectF rectFit = view.GetFitRect();
        for (int iNode = 0; iNode < layout.vecNodes.size(); ++iNode)
            QVERIFY2(rectFit.contains(view.GetNodeRect(iNode)), qUtf8Printable(layout.vecNodes[iNode].strId));
        view.SetLegendVisible(true);
        const QRectF rectLegend = view.GetLegendRect();
        QVERIFY(!rectLegend.isEmpty());
        for (int iNode = 0; iNode < layout.vecNodes.size(); ++iNode)
            QVERIFY2(!view.GetNodeRect(iNode).intersects(rectLegend), qUtf8Printable(layout.vecNodes[iNode].strId));
    }

    // L1: 타임라인은 범례 판을 꺼도 연도 없음 판이 그대로다(줄마다 찾기 · 누르기가 되고 맞춤 영역과 겹치지 않는다)
    void legendHiddenKeepsUnplacedPanel()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::TIMELINE);
        QCOMPARE(layout.vecUnplaced.size(), kUnplacedCount);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::TIMELINE);
        view.SetLegendVisible(false);
        QVERIFY(view.GetLegendRect().isEmpty());
        QVERIFY(view.GetUnplacedTitle().startsWith(DecodeUtf8(kUnplacedTitle)));
        const QRectF rectWidget(view.rect());
        const QRectF rectFit = view.GetFitRect();
        for (int iItem = 0; iItem < layout.vecUnplaced.size(); ++iItem)
        {
            const QRectF rectItem = view.GetUnplacedItemRect(iItem);
            QVERIFY2(!rectItem.isEmpty() && rectWidget.contains(rectItem), qUtf8Printable(layout.vecUnplaced[iItem]));
            QVERIFY2(!rectItem.intersects(rectFit), qUtf8Printable(layout.vecUnplaced[iItem]));
            QCOMPARE(view.FindConceptAt(rectItem.center().toPoint()), layout.vecUnplaced[iItem]);
        }
        QSignalSpy spyConcept(&view, &KnowledgeGraphView::ConceptActivated);
        QSignalSpy spyCleared(&view, &KnowledgeGraphView::SelectionCleared);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.GetUnplacedItemRect(kUnplacedCount - 1).center().toPoint());
        QCOMPARE(spyConcept.count(), 1);
        QCOMPARE(spyConcept.at(0).at(0).toString(), layout.vecUnplaced.last());
        QCOMPARE(view.GetSelectedId(), layout.vecUnplaced.last());
        // 목록 판 안의 빈 곳(제목 줄): 고른 것을 그대로 두고 신호가 없다
        const QRectF rectFirst = view.GetUnplacedItemRect(0);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, (rectFirst.center() - QPointF(0, rectFirst.height())).toPoint());
        QCOMPARE(view.GetSelectedId(), layout.vecUnplaced.last());
        QCOMPARE(spyCleared.count(), 0);
        QCOMPARE(spyConcept.count(), 1);
    }

    // L2: 밖에서 개념을 고르면 그 점을 누른 것과 같은 그림이지만 신호를 내지 않는다. 같은 id 를 다시 고르면 그대로다.
    // 확대 · 옮김은 바꾸지 않는다(점이 위젯 밖이어도 옮기지 않는다)
    void selectConceptHighlightsWithoutSignal()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::NETWORK);
        const int iNode = FindNode(layout, DecodeUtf8(kYolov2Id));
        QVERIFY(iNode >= 0);
        KnowledgeGraphView clicked;
        clicked.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        clicked.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        QTest::mouseClick(&clicked, Qt::LeftButton, Qt::NoModifier, clicked.MapToWidget(layout.vecNodes[iNode].center).toPoint());
        QCOMPARE(clicked.GetSelectedId(), DecodeUtf8(kYolov2Id));
        const QImage imageClicked = ViewTestSupport::Render(clicked);

        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        QSignalSpy spyConcept(&view, &KnowledgeGraphView::ConceptActivated);
        QSignalSpy spyDocument(&view, &KnowledgeGraphView::DocumentActivated);
        QSignalSpy spyCleared(&view, &KnowledgeGraphView::SelectionCleared);
        const double dZoom = view.GetZoom();
        const QPointF ptNode = view.MapToWidget(layout.vecNodes[0].center);
        view.SelectConcept(DecodeUtf8(kYolov2Id));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        QCOMPARE(view.GetZoom(), dZoom);
        QCOMPARE(view.MapToWidget(layout.vecNodes[0].center), ptNode);
        QCOMPARE(ViewTestSupport::Render(view), imageClicked);
        view.SelectConcept(DecodeUtf8(kYolov2Id));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        QCOMPARE(ViewTestSupport::Render(view), imageClicked);

        // 구석으로 확대한 뒤 다른 점을 골라도 확대 · 옮김은 그대로다
        ViewTestSupport::SendWheel(view, QPoint(1, 1), kZoomIntoCorner);
        const int iOther = FindNode(layout, DecodeUtf8(kMergedTo));
        QVERIFY(iOther >= 0);
        const double dCornerZoom = view.GetZoom();
        const QPointF ptOther = view.MapToWidget(layout.vecNodes[iOther].center);
        view.SelectConcept(DecodeUtf8(kMergedTo));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kMergedTo));
        QCOMPARE(view.GetZoom(), dCornerZoom);
        QCOMPARE(view.MapToWidget(layout.vecNodes[iOther].center), ptOther);

        QCOMPARE(spyConcept.count(), 0);
        QCOMPARE(spyDocument.count(), 0);
        QCOMPARE(spyCleared.count(), 0);
    }

    // L2: 모르는 id · 빈 글자 · 문서 id(문서 ↔ 개념)를 고르면 고른 것을 지운다. 고르지 않은 그림과 같고 신호가 없다
    void selectConceptUnknownClears()
    {
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, GetLayout(EGraphLayoutType::NETWORK), EGraphLayoutType::NETWORK);
        const QImage imageNone = ViewTestSupport::Render(view);
        QSignalSpy spyConcept(&view, &KnowledgeGraphView::ConceptActivated);
        QSignalSpy spyDocument(&view, &KnowledgeGraphView::DocumentActivated);
        QSignalSpy spyCleared(&view, &KnowledgeGraphView::SelectionCleared);
        for (const QString& strId : { DecodeUtf8(kUnknownId), QString() })
        {
            view.SelectConcept(DecodeUtf8(kYolov2Id));
            QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
            QVERIFY(ViewTestSupport::Render(view) != imageNone);
            view.SelectConcept(strId);
            QCOMPARE(view.GetSelectedId(), QString());
            QCOMPARE(ViewTestSupport::Render(view), imageNone);
        }

        // 문서 ↔ 개념: 문서 id 는 개념이 아니라 지운다(문서를 눌러 고른 것도)
        const GraphLayoutOutput& bipartite = GetLayout(EGraphLayoutType::BIPARTITE);
        const int iDocument = FindDocumentNode(bipartite, DecodeUtf8(kYolo9000Title));
        QVERIFY(iDocument >= 0);
        const QString strDocumentId = bipartite.vecNodes[iDocument].strId;
        KnowledgeGraphView documentView;
        documentView.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        documentView.SetGraph(m_graph, bipartite, EGraphLayoutType::BIPARTITE);
        const QImage imageDocumentNone = ViewTestSupport::Render(documentView);
        QTest::mouseClick(&documentView, Qt::LeftButton, Qt::NoModifier, documentView.MapToWidget(bipartite.vecNodes[iDocument].center).toPoint());
        QCOMPARE(documentView.GetSelectedId(), strDocumentId);
        QSignalSpy spyDocumentConcept(&documentView, &KnowledgeGraphView::ConceptActivated);
        QSignalSpy spyDocumentDocument(&documentView, &KnowledgeGraphView::DocumentActivated);
        QSignalSpy spyDocumentCleared(&documentView, &KnowledgeGraphView::SelectionCleared);
        documentView.SelectConcept(strDocumentId);
        QCOMPARE(documentView.GetSelectedId(), QString());
        QCOMPARE(ViewTestSupport::Render(documentView), imageDocumentNone);
        documentView.SelectConcept(DecodeUtf8(kYolov2Id));
        QCOMPARE(documentView.GetSelectedId(), DecodeUtf8(kYolov2Id));
        documentView.SelectConcept(strDocumentId);
        QCOMPARE(documentView.GetSelectedId(), QString());
        QCOMPARE(ViewTestSupport::Render(documentView), imageDocumentNone);

        QCOMPARE(spyConcept.count(), 0);
        QCOMPARE(spyDocument.count(), 0);
        QCOMPARE(spyCleared.count(), 0);
        QCOMPARE(spyDocumentConcept.count(), 0);
        QCOMPARE(spyDocumentDocument.count(), 0);
        QCOMPARE(spyDocumentCleared.count(), 0);
    }

    // L2: 타임라인 연도 없음 목록의 id 를 고르면 그 줄을 누른 것과 같은 그림이다(신호 없음). 배치 점으로 옮겨 고를 수도 있다
    void selectConceptPicksUnplacedRow()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::TIMELINE);
        QVERIFY(layout.vecUnplaced.size() > kUnplacedPick);
        const QString strUnplacedId = layout.vecUnplaced[kUnplacedPick];
        KnowledgeGraphView clicked;
        clicked.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        clicked.SetGraph(m_graph, layout, EGraphLayoutType::TIMELINE);
        QTest::mouseClick(&clicked, Qt::LeftButton, Qt::NoModifier, clicked.GetUnplacedItemRect(kUnplacedPick).center().toPoint());
        QCOMPARE(clicked.GetSelectedId(), strUnplacedId);
        const QImage imageClicked = ViewTestSupport::Render(clicked);

        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::TIMELINE);
        QSignalSpy spyConcept(&view, &KnowledgeGraphView::ConceptActivated);
        QSignalSpy spyCleared(&view, &KnowledgeGraphView::SelectionCleared);
        view.SelectConcept(strUnplacedId);
        QCOMPARE(view.GetSelectedId(), strUnplacedId);
        QCOMPARE(ViewTestSupport::Render(view), imageClicked);
        view.SelectConcept(DecodeUtf8(kYolov2Id));
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        view.SelectConcept(strUnplacedId);
        QCOMPARE(view.GetSelectedId(), strUnplacedId);
        QCOMPARE(ViewTestSupport::Render(view), imageClicked);
        QCOMPARE(spyConcept.count(), 0);
        QCOMPARE(spyCleared.count(), 0);
    }

    // L3: 빈 곳을 누르면 고른 것을 지운 뒤 SelectionCleared 를 낸다(받을 때 GetSelectedId 는 빈 글자). 고른 것이 없어도 낸다
    void emptyClickEmitsSelectionCleared()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::NETWORK);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        QSignalSpy spyConcept(&view, &KnowledgeGraphView::ConceptActivated);
        QSignalSpy spyCleared(&view, &KnowledgeGraphView::SelectionCleared);
        QStringList vecSelectedAtSignal;
        connect(&view, &KnowledgeGraphView::SelectionCleared, this, [&view, &vecSelectedAtSignal]() { vecSelectedAtSignal << view.GetSelectedId(); });

        const int iNode = FindNode(layout, DecodeUtf8(kYolov2Id));
        QVERIFY(iNode >= 0);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.MapToWidget(layout.vecNodes[iNode].center).toPoint());
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        QCOMPARE(spyCleared.count(), 0);

        const QPoint ptEmpty(2, view.height() - 3);
        QCOMPARE(view.FindConceptAt(ptEmpty), QString());
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, ptEmpty);
        QCOMPARE(spyCleared.count(), 1);
        QCOMPARE(vecSelectedAtSignal, QStringList { QString() });
        QCOMPARE(view.GetSelectedId(), QString());

        // 고른 것이 없어도 낸다
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, ptEmpty);
        QCOMPARE(spyCleared.count(), 2);
        QCOMPARE(vecSelectedAtSignal, (QStringList { QString(), QString() }));
        QCOMPARE(spyConcept.count(), 1);
    }

    // L3: 부르는 함수(ClearSelection · SelectConcept · SetGraph), 끌기, 범례 판 · 목록 판 안 누르기는 SelectionCleared 를 내지 않는다.
    // 빈 곳 두 번 누르기(누름 · 놓음 · 두 번 누름 · 놓음)는 첫 누르기에서 한 번만 낸다
    void selectionClearedOnlyForEmptyClicks()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::NETWORK);
        const int iNode = FindNode(layout, DecodeUtf8(kYolov2Id));
        QVERIFY(iNode >= 0);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        QSignalSpy spyCleared(&view, &KnowledgeGraphView::SelectionCleared);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.MapToWidget(layout.vecNodes[iNode].center).toPoint());
        view.ClearSelection();
        view.SelectConcept(DecodeUtf8(kYolov2Id));
        view.SelectConcept(DecodeUtf8(kUnknownId));
        view.SelectConcept(DecodeUtf8(kYolov2Id));
        view.SelectConcept(QString());
        view.SelectConcept(DecodeUtf8(kYolov2Id));
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        QCOMPARE(view.GetSelectedId(), QString());
        QCOMPARE(spyCleared.count(), 0);

        // 빈 곳에서 끌기
        const QPoint ptEmpty(2, view.height() - 3);
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonPress, ptEmpty, Qt::LeftButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseMove, ptEmpty + kDragDelta, Qt::NoButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonRelease, ptEmpty + kDragDelta, Qt::LeftButton, Qt::NoButton);
        QCOMPARE(spyCleared.count(), 0);

        // 범례 판 안 누르기: 고른 것을 그대로 둔다
        view.FitToView();
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.MapToWidget(layout.vecNodes[iNode].center).toPoint());
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.GetLegendRect().center().toPoint());
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        QCOMPARE(spyCleared.count(), 0);

        // 빈 곳 두 번 누르기: 첫 누르기에서 한 번
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonPress, ptEmpty, Qt::LeftButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonRelease, ptEmpty, Qt::LeftButton, Qt::NoButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonDblClick, ptEmpty, Qt::LeftButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonRelease, ptEmpty, Qt::LeftButton, Qt::NoButton);
        QCOMPARE(spyCleared.count(), 1);
        QCOMPARE(view.GetSelectedId(), QString());

        // 연도 없음 목록 판 안의 빈 곳(제목 줄)
        KnowledgeGraphView timeline;
        timeline.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        timeline.SetGraph(m_graph, GetLayout(EGraphLayoutType::TIMELINE), EGraphLayoutType::TIMELINE);
        QSignalSpy spyTimelineCleared(&timeline, &KnowledgeGraphView::SelectionCleared);
        const QRectF rectFirst = timeline.GetUnplacedItemRect(0);
        QVERIFY(!rectFirst.isEmpty());
        QTest::mouseClick(&timeline, Qt::LeftButton, Qt::NoModifier, (rectFirst.center() - QPointF(0, rectFirst.height())).toPoint());
        QCOMPARE(spyTimelineCleared.count(), 0);
    }

    // L1 · L3: 범례 판을 끄면 범례가 있던 자리는 빈 곳이다. 누르면 고른 것을 지우고 SelectionCleared 를 한 번 낸다(켜 있을 때는 판 안이라
    // 그대로다). 찾기 · 마우스 올림은 그 자리의 점도 찾는다(켜 있을 때는 판이 가린다)
    void hiddenLegendAreaIsEmptySpace()
    {
        const GraphLayoutOutput& layout = GetLayout(EGraphLayoutType::NETWORK);
        const int iNode = FindNode(layout, DecodeUtf8(kYolov2Id));
        QVERIFY(iNode >= 0);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        const QRectF rectLegend = view.GetLegendRect();
        QVERIFY(!rectLegend.isEmpty());
        QSignalSpy spyCleared(&view, &KnowledgeGraphView::SelectionCleared);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, view.MapToWidget(layout.vecNodes[iNode].center).toPoint());
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, rectLegend.center().toPoint());
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        QCOMPARE(spyCleared.count(), 0);

        view.SetLegendVisible(false);
        QCOMPARE(view.GetSelectedId(), DecodeUtf8(kYolov2Id));
        // 다시 맞춘 뒤 범례가 있던 자리에서 점이 없는 곳
        QPoint ptFree(-1, -1);
        for (int iY = static_cast<int>(rectLegend.top()) + 1; iY < rectLegend.bottom() && ptFree.x() < 0; iY += kLegendProbeStep)
            for (int iX = static_cast<int>(rectLegend.left()) + 1; iX < rectLegend.right() && ptFree.x() < 0; iX += kLegendProbeStep)
                if (view.FindConceptAt(QPoint(iX, iY)).isEmpty() && view.FindDocumentAt(QPoint(iX, iY)).isEmpty())
                    ptFree = QPoint(iX, iY);
        QVERIFY(ptFree.x() >= 0);
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, ptFree);
        QCOMPARE(spyCleared.count(), 1);
        QCOMPARE(view.GetSelectedId(), QString());

        // 그 점에서 그림을 끌어 점을 범례가 있던 자리 가운데로 옮긴다: 켜 있으면 찾지 않고, 끄면(확대 · 옮김은 그대로) 그 점을 찾는다
        view.SetLegendVisible(true);
        QCOMPARE(view.GetLegendRect(), rectLegend);
        const QPoint ptTarget = rectLegend.center().toPoint();
        const QPoint ptStart = view.MapToWidget(layout.vecNodes[iNode].center).toPoint();
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonPress, ptStart, Qt::LeftButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseMove, ptTarget, Qt::NoButton, Qt::LeftButton);
        ViewTestSupport::SendMouse(view, QEvent::MouseButtonRelease, ptTarget, Qt::LeftButton, Qt::NoButton);
        QVERIFY(view.GetNodeRect(iNode).contains(QPointF(ptTarget)));
        QCOMPARE(view.FindConceptAt(ptTarget), QString());
        QCOMPARE(view.GetToolTipAt(ptTarget), QString());
        const double dZoom = view.GetZoom();
        const QRectF rectNode = view.GetNodeRect(iNode);
        view.SetLegendVisible(false);
        QCOMPARE(view.GetZoom(), dZoom);
        QCOMPARE(view.GetNodeRect(iNode), rectNode);
        QCOMPARE(view.FindConceptAt(ptTarget), DecodeUtf8(kYolov2Id));
        QVERIFY(view.GetToolTipAt(ptTarget).contains(DecodeUtf8(kYolov2Name)));
        QCOMPARE(spyCleared.count(), 1);
    }

    // L10: 개념 점을 누르면, 첫 받는 쪽이 신호 안에서 SetGraph 로 다른 배치를 넣어도 받는 쪽 모두가 누른 개념 id 를 받는다
    void conceptSignalSurvivesSetGraphInSlot()
    {
        const GraphLayoutOutput replacement = MakeRenamedLayout(EGraphLayoutType::NETWORK);
        QVERIFY(!replacement.vecNodes.isEmpty());
        GraphLayoutOutput layout;
        QVERIFY(ViewTestSupport::RunLayout(m_graph, EGraphLayoutType::NETWORK, &layout));   // 고정 배치와 저장소를 나누지 않는 새 배치
        const int iNode = FindNode(layout, DecodeUtf8(kYolov2Id));
        QVERIFY(iNode >= 0);
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::NETWORK);
        const QPoint ptNode = view.MapToWidget(layout.vecNodes[iNode].center).toPoint();
        layout = GraphLayoutOutput();   // 위젯만 그 배치를 갖게 한다: 신호 안의 SetGraph 가 옛 배치 저장소를 푼다
        QStringList vecFirst;
        QStringList vecSecond;
        ClickReplacingGraph(view, &KnowledgeGraphView::ConceptActivated, ptNode, replacement, EGraphLayoutType::NETWORK, false, &vecFirst, &vecSecond);
        QCOMPARE(vecSecond, QStringList { DecodeUtf8(kYolov2Id) });
        QCOMPARE(vecFirst, QStringList { DecodeUtf8(kYolov2Id) });
    }

    // L10: 문서 ↔ 개념에서 문서 점을 누르면, 첫 받는 쪽이 신호 안에서 SetGraph 를 불러도 받는 쪽 모두가 누른 문서 id 를 받는다
    void documentSignalSurvivesSetGraphInSlot()
    {
        const GraphLayoutOutput replacement = MakeRenamedLayout(EGraphLayoutType::BIPARTITE);
        QVERIFY(!replacement.vecNodes.isEmpty());
        GraphLayoutOutput layout;
        QVERIFY(ViewTestSupport::RunLayout(m_graph, EGraphLayoutType::BIPARTITE, &layout));
        const int iDocument = FindDocumentNode(layout, DecodeUtf8(kYolo9000Title));
        QVERIFY(iDocument >= 0);
        const QString strDocumentId = layout.vecNodes[iDocument].strId;
        QVERIFY(!strDocumentId.isEmpty());
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::BIPARTITE);
        const QPoint ptDocument = view.MapToWidget(layout.vecNodes[iDocument].center).toPoint();
        layout = GraphLayoutOutput();
        QStringList vecFirst;
        QStringList vecSecond;
        ClickReplacingGraph(view, &KnowledgeGraphView::DocumentActivated, ptDocument, replacement, EGraphLayoutType::BIPARTITE, false, &vecFirst, &vecSecond);
        QCOMPARE(vecSecond, QStringList { strDocumentId });
        QCOMPARE(vecFirst, QStringList { strDocumentId });
    }

    // L10: 타임라인 연도 없음 목록 줄을 누르면, 첫 받는 쪽이 신호 안에서 SetGraph 를 불러도 받는 쪽 모두가 그 줄의 개념 id 를 받는다
    void unplacedSignalSurvivesSetGraphInSlot()
    {
        const GraphLayoutOutput replacement = MakeRenamedLayout(EGraphLayoutType::TIMELINE);
        QVERIFY(!replacement.vecUnplaced.isEmpty());
        GraphLayoutOutput layout;
        QVERIFY(ViewTestSupport::RunLayout(m_graph, EGraphLayoutType::TIMELINE, &layout));
        QVERIFY(layout.vecUnplaced.size() > kUnplacedPick);
        const QString strUnplacedId = layout.vecUnplaced[kUnplacedPick];
        KnowledgeGraphView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetGraph(m_graph, layout, EGraphLayoutType::TIMELINE);
        const QRectF rectItem = view.GetUnplacedItemRect(kUnplacedPick);
        QVERIFY(!rectItem.isEmpty());
        layout = GraphLayoutOutput();
        QStringList vecFirst;
        QStringList vecSecond;
        ClickReplacingGraph(view, &KnowledgeGraphView::ConceptActivated, rectItem.center().toPoint(), replacement, EGraphLayoutType::TIMELINE, true, &vecFirst, &vecSecond);
        QCOMPARE(vecSecond, QStringList { strUnplacedId });
        QCOMPARE(vecFirst, QStringList { strUnplacedId });
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

    // L10: type 배치를 새로 만들고 점 · 연도 없음 id 마다 앞에 kRenamedPrefix 를 붙인다(누르는 배치와 저장소 크기가 같은 다른 배치)
    GraphLayoutOutput MakeRenamedLayout(EGraphLayoutType type) const
    {
        GraphLayoutOutput layout;
        if (!ViewTestSupport::RunLayout(m_graph, type, &layout))
            return GraphLayoutOutput();
        for (LayoutNode& node : layout.vecNodes)
            node.strId.prepend(DecodeUtf8(kRenamedPrefix));
        for (QString& strId : layout.vecUnplaced)
            strId.prepend(DecodeUtf8(kRenamedPrefix));
        return layout;
    }

    // L10: pos 를 누른다. 첫 받는 쪽은 신호 안에서 replacement 를 넣고(SetGraph), 풀린 저장소를 replacement 의 사본으로 덮은 뒤
    // (unplaced 면 연도 없음 목록, 아니면 점 목록) 받은 id 를 pFirst 에 적는다. 그다음에 이은 받는 쪽은 받은 id 를 pSecond 에 적는다.
    // 사본은 두 받는 쪽이 다 받을 때까지 둔다. 이음은 이 함수가 끝나면 끊는다
    void ClickReplacingGraph(KnowledgeGraphView& view, void (KnowledgeGraphView::*signal)(const QString&), const QPoint& pos,
                             const GraphLayoutOutput& replacement, EGraphLayoutType type, bool unplaced, QStringList* pFirst,
                             QStringList* pSecond) const
    {
        QVector<QVector<LayoutNode>> vecNodeRefills;
        QVector<QStringList> vecUnplacedRefills;
        // 사본을 둘 자리를 미리 잡는다(사본을 두다가 늘어나는 자리가 풀린 저장소를 잡지 않게)
        if (unplaced)
            vecUnplacedRefills.reserve(kMaxRefills);
        else
            vecNodeRefills.reserve(kMaxRefills);
        QObject receiver;
        connect(&view, signal, &receiver,
                [this, &view, &replacement, type, unplaced, &vecNodeRefills, &vecUnplacedRefills, pFirst](const QString& id) {
                    view.SetGraph(m_graph, replacement, type);
                    if (unplaced)
                        RefillFreedStorage(&id, replacement.vecUnplaced, &vecUnplacedRefills);
                    else
                        RefillFreedStorage(&id, replacement.vecNodes, &vecNodeRefills);
                    *pFirst << id;   // 자기 SetGraph 뒤에도 받은 id 를 읽는다
                });
        connect(&view, signal, &receiver, [pSecond](const QString& id) { *pSecond << id; });
        QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, pos);
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
