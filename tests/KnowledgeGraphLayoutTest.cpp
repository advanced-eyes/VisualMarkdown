#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/Algorithm/GraphLayout/KnowledgeGraphLayout.h"
#include "VisualMarkdown/KnowledgeIndex.h"
#include "VisualMarkdown/KnowledgeTypes.h"
#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <QHash>
#include <QMap>
#include <QSet>
#include <QTest>

#include <cmath>
#include <memory>

namespace
{
// ================================================================================================
// 기대값: design/Views.md §5 끝의 "showcase 에서 나와야 하는 것" 과 examples/showcase/README.md §2 다.
// 예시를 고치면 README · ShowcaseWikiTest 와 함께 여기를 고친다.
// ================================================================================================

struct NameCount
{
    const char* pName;
    int iCount;
};

// 문서 제목(문서 ↔ 개념 왼쪽 열의 이름표)
constexpr const char* kYoloTitle = "You Only Look Once: Unified, Real-Time Object Detection";
constexpr const char* kYolo9000Title = "YOLO9000: Better, Faster, Stronger";
constexpr const char* kYolov3Title = "YOLOv3: An Incremental Improvement";
constexpr const char* kTwoStageTitle = "2단계 검출기의 흐름";
constexpr const char* kFocalLossTitle = "Focal Loss for Dense Object Detection";
constexpr const char* kPaymentTitle = "결제 서비스 개편 기획서";

// 그래프: 점 31 · 선 36(방향 없는 선 5), 덩어리 6개
constexpr int kNodeCount = 31;
constexpr int kEdgeCount = 36;
constexpr int kUndirectedEdgeCount = 5;
const QList<int> kComponentSizes { 19, 8, 1, 1, 1, 1 };
const QStringList kSingleComponents { "coco", "pascal-voc", "승인율", "응답-시간" };   // 크기 1 덩어리(선 없는 점), 이름 열쇠 순
const QVector<NameCount> kEdgeCounts { { "1단계-검출기", 6 }, { "검출기", 3 }, { "faster-r-cnn", 6 }, { "coco", 0 } };
const QStringList kDocumentTitles { kPaymentTitle, kFocalLossTitle, kTwoStageTitle, kYolov3Title, kYolo9000Title, kYoloTitle };   // learned 늦은 것부터
constexpr const char* kHiddenConcept = "dpm";          // 아무도 가리키지 않아 숨는다(연도 2008)
constexpr const char* kMissingConcept = "darknet-53";  // YOLOv3 문서가 가리키지만 개념 파일이 없다

// 문서 ↔ 개념: 문서마다 연결 수(개념 파일이 없는 Darknet-53 은 뺀다)와 무게 합(카드 링크 + 관계 끝 + 표 concepts)
struct DocumentLinks
{
    const char* pTitle;
    int iLinks;
    int iWeightSum;
};
const QVector<DocumentLinks> kDocumentLinks {
    { kYoloTitle, 5, 13 }, { kYolo9000Title, 7, 20 }, { kYolov3Title, 5, 13 },
    { kTwoStageTitle, 7, 26 }, { kFocalLossTitle, 4, 9 }, { kPaymentTitle, 9, 23 },
};
struct LinkWeight
{
    const char* pTitle;
    const char* pConcept;
    int iWeight;
};
const QVector<LinkWeight> kLinkWeights {
    { kYoloTitle, "yolo", 5 },             // 카드 2 · 관계 끝 2 · 표 1
    { kYolo9000Title, "yolov2", 9 },       // 카드 2 · 관계 끝 4 · 표 3
    { kYolov3Title, "yolov3", 7 },         // 카드 2 · 관계 끝 3(빠진 Darknet-53 관계의 from 도 센다) · 표 2
    { kTwoStageTitle, "fast-r-cnn", 7 },   // 카드 1 · 관계 끝 4 · 표 2
    { kPaymentTitle, "승인율", 1 },         // 카드 1
};
constexpr int kBipartiteConcepts = 28;   // 보이는 점 31 가운데 다루는 문서가 없는 DETR · 검출기 · 거래 로그를 뺀다
const QStringList kConceptsWithoutDocuments { "detr", "검출기", "거래-로그" };

// 갈래: 이름과 점(계보도는 옛것부터, 타임라인은 순서를 보지 않는다)
struct ExpectedLane
{
    const char* pLabel;
    QStringList vecIds;
};

// 계보도: 점 8 · 선 5, 갈래 3개(큰 것부터, 같으면 가장 옛 점의 이름 열쇠 순)
constexpr int kLineageNodes = 8;
constexpr int kLineageEdges = 5;
const QVector<ExpectedLane> kLineageLanes {
    { "R-CNN", { "r-cnn", "fast-r-cnn", "faster-r-cnn" } },
    { "YOLO", { "yolo", "yolov2", "yolov3" } },
    { "결제 모듈", { "결제-모듈", "결제-모듈-2판" } },
};

// 분류 트리: 점 14 · 선 12, 나무 2개(왼쪽부터 뿌리 · 크기). 자식은 이름 열쇠 순(왼쪽부터)
constexpr int kTreeNodes = 14;
constexpr int kTreeEdges = 12;
const QVector<NameCount> kTrees { { "검출기", 12 }, { "결제-모듈", 2 } };
struct ExpectedChildren
{
    const char* pParent;
    QStringList vecChildren;
};
const QVector<ExpectedChildren> kTreeChildren {
    { "검출기", { "1단계-검출기", "2단계-검출기", "detr" } },
    { "1단계-검출기", { "retinanet", "ssd", "yolo", "yolov2", "yolov3" } },
    { "2단계-검출기", { "faster-r-cnn", "fast-r-cnn", "r-cnn" } },
    { "결제-모듈", { "결제-모듈-2판" } },
};
// 큰 나무의 잎(왼쪽부터 이어진 칸)
const QStringList kDetectorTreeLeaves { "retinanet", "ssd", "yolo", "yolov2", "yolov3", "faster-r-cnn", "fast-r-cnn", "r-cnn", "detr" };

// 타임라인: 놓인 점 16 · 연도 없는 점 15, 갈래 3개(점이 많은 것부터, 같으면 이름 순, "그 밖" 은 늘 마지막), 눈금은 있는 해마다
constexpr int kTimelinePlaced = 16;
constexpr int kTimelineUnplaced = 15;
const QVector<ExpectedLane> kTimelineLanes {
    { "검출기", { "r-cnn", "fast-r-cnn", "faster-r-cnn", "ssd", "yolo", "retinanet", "yolov2", "yolov3", "detr" } },
    { "결제 모듈", { "결제-모듈-2판" } },
    { "그 밖", { "coco", "rpn", "roi-풀링", "배치-정규화", "darknet-19", "focal-loss" } },   // 점은 결제 모듈보다 많지만 마지막
};
const QStringList kTimelineYears { "2014", "2015", "2016", "2017", "2018", "2020", "2026" };

// 관계 행렬: 31 × 31, 덩어리 순(큰 것부터) 안에서 이름 열쇠 순
const QStringList kMatrixOrder {
    "1단계-검출기", "2단계-검출기", "darknet-19", "detr", "faster-r-cnn", "fast-r-cnn", "focal-loss", "r-cnn", "retinanet", "roi-풀링",
    "rpn", "ssd", "yolo", "yolov2", "yolov3", "객체-검출", "검출기", "배치-정규화", "클래스-불균형",
    "거래-로그", "결제-모듈", "결제-모듈-2판", "결제-서비스", "규칙-기반-탐지", "부정-결제", "사기-탐지", "학습-기반-탐지",
    "coco", "pascal-voc", "승인율", "응답-시간",
};
constexpr int kMatrixCells = 40;   // 방향 있는 선 31 이 30칸(둘이 한 칸을 나눈다) + 방향 없는 선 5 × 2칸
struct ExpectedCell
{
    const char* pRow;
    const char* pColumn;
    QStringList vecKinds;
};
const QVector<ExpectedCell> kCellsWithSeveralKinds { { "결제-모듈-2판", "결제-모듈", { "improves", "isA" } } };

const QVector<EGraphLayoutType> kAllTypes {
    EGraphLayoutType::NETWORK, EGraphLayoutType::LINEAGE, EGraphLayoutType::TREE,
    EGraphLayoutType::TIMELINE, EGraphLayoutType::MATRIX, EGraphLayoutType::BIPARTITE,
};

constexpr double kTolerance = 1e-6;   // 경계 · 자리 견줌의 여유(배치 단위)
constexpr int kRoundTripDigits = 17;  // FormatNumber 의 자리 수

// 빽빽한 그래프(denseNetworkHasNoOverlaps): 중심 하나에 isA 로 닿는 잎들과 서로 다 이어진 점들
constexpr int kDenseLeafCount = 40;
constexpr int kDenseCliqueSize = 7;
constexpr int kLeafNumberWidth = 2;   // 잎 id 의 번호 자리 수(leaf-00)
constexpr int kDecimalBase = 10;

// ================================================================================================

QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

QString GetTypeName(EGraphLayoutType type)
{
    switch (type)
    {
    case EGraphLayoutType::NETWORK: return QStringLiteral("NETWORK");
    case EGraphLayoutType::LINEAGE: return QStringLiteral("LINEAGE");
    case EGraphLayoutType::TREE: return QStringLiteral("TREE");
    case EGraphLayoutType::TIMELINE: return QStringLiteral("TIMELINE");
    case EGraphLayoutType::MATRIX: return QStringLiteral("MATRIX");
    case EGraphLayoutType::BIPARTITE: return QStringLiteral("BIPARTITE");
    }
    return QStringLiteral("?");
}

// 순서를 보지 않고 두 목록을 견준다. 같으면 빈 글, 다르면 빠진 것과 더 있는 것을 적는다(겹친 항목도 센다)
QString DiffItems(const QString& what, const QStringList& actual, const QStringList& expected)
{
    QStringList vecMissing = expected;
    QStringList vecExtra;
    for (const QString& strItem : actual)
        if (!vecMissing.removeOne(strItem))
            vecExtra << strItem;
    if (vecMissing.isEmpty() && vecExtra.isEmpty())
        return QString();
    vecMissing.sort();
    vecExtra.sort();
    return what + DecodeUtf8(": 빠진 것 [") + vecMissing.join(QStringLiteral(", ")) + DecodeUtf8("] · 더 있는 것 [")
        + vecExtra.join(QStringLiteral(", ")) + QLatin1Char(']');
}

bool RunLayout(const KnowledgeGraph& graph, EGraphLayoutType type, GraphLayoutOutput* out)
{
    KnowledgeGraphLayout layout;
    layout.SetStrategy(type);
    GraphLayoutInput in;
    in.pGraph = &graph;
    return layout.Execute(in, *out);
}

// 숫자는 17자리로 적는다. 글이 같으면 double 이 비트까지 같다(-0 과 0, nan 도 글이 다르다)
QString FormatNumber(double value) { return QString::number(value, 'g', kRoundTripDigits); }

QString FormatBool(bool value) { return value ? QStringLiteral("1") : QStringLiteral("0"); }

// 배치 출력 전체를 줄 목록으로 적는다(같은 입력이면 같은 출력인지 견줄 때)
QStringList DescribeLayout(const GraphLayoutOutput& out)
{
    QStringList vecLines;
    for (const LayoutNode& node : out.vecNodes)
        vecLines << QStringList { QStringLiteral("node"), node.strId, node.strLabel, node.strKind, FormatNumber(node.center.x()),
                                  FormatNumber(node.center.y()), FormatBool(node.bDocument) }.join(QLatin1Char('|'));
    for (const LayoutEdge& edge : out.vecEdges)
        vecLines << QStringList { QStringLiteral("edge"), QString::number(edge.iFrom), QString::number(edge.iTo), edge.strKind,
                                  QString::number(static_cast<int>(edge.eStatus)), FormatBool(edge.bDirected),
                                  QString::number(edge.iGraphEdge), QString::number(edge.iWeight) }.join(QLatin1Char('|'));
    for (const LayoutTick& tick : out.vecTicks)
        vecLines << QStringList { QStringLiteral("tick"), FormatNumber(tick.dPosition), tick.strLabel }.join(QLatin1Char('|'));
    for (const LayoutLane& lane : out.vecLanes)
        vecLines << QStringList { QStringLiteral("lane"), lane.strLabel, FormatNumber(lane.dTop), FormatNumber(lane.dBottom) }.join(QLatin1Char('|'));
    vecLines << QStringLiteral("order|") + out.vecMatrixOrder.join(QLatin1Char('|'));
    for (const LayoutMatrixCell& cell : out.vecCells)
        vecLines << QStringList { QStringLiteral("cell"), QString::number(cell.iRow), QString::number(cell.iColumn), cell.vecKinds.join(QLatin1Char(',')) }
                        .join(QLatin1Char('|'));
    vecLines << QStringLiteral("unplaced|") + out.vecUnplaced.join(QLatin1Char('|'));
    vecLines << QStringList { QStringLiteral("bounds"), FormatNumber(out.bounds.x()), FormatNumber(out.bounds.y()), FormatNumber(out.bounds.width()),
                              FormatNumber(out.bounds.height()) }.join(QLatin1Char('|'));
    return vecLines;
}

// 그래프 전체를 줄 목록으로 적는다
QStringList DescribeGraph(const KnowledgeGraph& graph)
{
    QStringList vecLines;
    for (const KnowledgeGraphNode& node : graph.GetNodes())
        vecLines << QStringList { node.strId, node.strName, node.strNameKey, node.strKind, node.strYear, QString::number(node.iYear), node.strDefinition,
                                  QString::number(node.iDocumentCount), QString::number(node.iEdgeCount), QString::number(node.iComponent) }
                        .join(QLatin1Char('|'));
    for (const KnowledgeGraphEdge& edge : graph.GetEdges())
        vecLines << QStringList { QString::number(edge.iFrom), QString::number(edge.iTo), FormatBool(edge.bDirected), edge.edge.strFromId,
                                  edge.edge.strKind, edge.edge.strToId, QString::number(static_cast<int>(edge.edge.eStatus)),
                                  edge.edge.vecDocumentIds.join(QLatin1Char(',')), FormatBool(edge.edge.bFromConceptFile),
                                  edge.edge.vecNotes.join(QLatin1Char(',')) }.join(QLatin1Char('|'));
    for (const QVector<int>& vecComponent : graph.GetComponents())
    {
        QStringList vecMembers;
        for (const int iNode : vecComponent)
            vecMembers << QString::number(iNode);
        vecLines << QStringLiteral("component|") + vecMembers.join(QLatin1Char(','));
    }
    for (const KnowledgeGraphDocument& document : graph.GetDocuments())
        vecLines << QStringLiteral("document|") + document.strId + QLatin1Char('|') + document.strTitle;
    for (const KnowledgeGraphLink& link : graph.GetLinks())
        vecLines << QStringList { QStringLiteral("link"), QString::number(link.iDocument), QString::number(link.iNode), QString::number(link.iWeight) }
                        .join(QLatin1Char('|'));
    return vecLines;
}

QRectF MakeBox(const LayoutNode& node)
{
    return QRectF(node.center.x() - GraphLayoutSize::kNodeWidth / 2, node.center.y() - GraphLayoutSize::kNodeHeight / 2,
                  GraphLayoutSize::kNodeWidth, GraphLayoutSize::kNodeHeight);
}

bool ContainsWithTolerance(const QRectF& outer, const QRectF& inner)
{
    return inner.left() >= outer.left() - kTolerance && inner.right() <= outer.right() + kTolerance
        && inner.top() >= outer.top() - kTolerance && inner.bottom() <= outer.bottom() + kTolerance;
}

bool ContainsWithTolerance(const QRectF& outer, const QPointF& point)
{
    return ContainsWithTolerance(outer, QRectF(point, point));
}

// 점 상자끼리 겹치는 쌍(없으면 빈 목록). 맞닿기만 한 것은 겹침이 아니다
QStringList FindOverlaps(const GraphLayoutOutput& out)
{
    QStringList vecPairs;
    for (int iFirst = 0; iFirst < out.vecNodes.size(); ++iFirst)
        for (int iSecond = iFirst + 1; iSecond < out.vecNodes.size(); ++iSecond)
            if (MakeBox(out.vecNodes[iFirst]).intersects(MakeBox(out.vecNodes[iSecond])))
                vecPairs << out.vecNodes[iFirst].strId + QStringLiteral(" / ") + out.vecNodes[iSecond].strId;
    return vecPairs;
}

// 출력의 숫자 가운데 유한하지 않은 것(nan · inf)을 적는다
QStringList FindNonFinite(const GraphLayoutOutput& out)
{
    QStringList vecBad;
    for (const LayoutNode& node : out.vecNodes)
        if (!std::isfinite(node.center.x()) || !std::isfinite(node.center.y()))
            vecBad << QStringLiteral("node ") + node.strId;
    for (const LayoutTick& tick : out.vecTicks)
        if (!std::isfinite(tick.dPosition))
            vecBad << QStringLiteral("tick ") + tick.strLabel;
    for (const LayoutLane& lane : out.vecLanes)
        if (!std::isfinite(lane.dTop) || !std::isfinite(lane.dBottom))
            vecBad << QStringLiteral("lane ") + lane.strLabel;
    if (!std::isfinite(out.bounds.x()) || !std::isfinite(out.bounds.y()) || !std::isfinite(out.bounds.width()) || !std::isfinite(out.bounds.height()))
        vecBad << QStringLiteral("bounds");
    return vecBad;
}

QStringList ListNodeIds(const GraphLayoutOutput& out)
{
    QStringList vecIds;
    for (const LayoutNode& node : out.vecNodes)
        vecIds << node.strId;
    return vecIds;
}

QStringList ListNodeIds(const KnowledgeGraph& graph)
{
    QStringList vecIds;
    for (const KnowledgeGraphNode& node : graph.GetNodes())
        vecIds << node.strId;
    return vecIds;
}

int FindLayoutNode(const GraphLayoutOutput& out, const QString& id)
{
    for (int iNode = 0; iNode < out.vecNodes.size(); ++iNode)
        if (out.vecNodes[iNode].strId == id)
            return iNode;
    return -1;
}

QPointF GetCenter(const GraphLayoutOutput& out, const QString& id)
{
    const int iNode = FindLayoutNode(out, id);
    return iNode >= 0 ? out.vecNodes[iNode].center : QPointF(qQNaN(), qQNaN());
}

// 가운데가 갈래 띠(dTop–dBottom) 안에 든 점
QStringList ListNodesInLane(const GraphLayoutOutput& out, const LayoutLane& lane)
{
    QStringList vecIds;
    for (const LayoutNode& node : out.vecNodes)
        if (node.center.y() >= lane.dTop && node.center.y() <= lane.dBottom)
            vecIds << node.strId;
    return vecIds;
}

QStringList ListLaneLabels(const GraphLayoutOutput& out)
{
    QStringList vecLabels;
    for (const LayoutLane& lane : out.vecLanes)
        vecLabels << lane.strLabel;
    return vecLabels;
}

QStringList ListExpectedLaneLabels(const QVector<ExpectedLane>& lanes)
{
    QStringList vecLabels;
    for (const ExpectedLane& lane : lanes)
        vecLabels << DecodeUtf8(lane.pLabel);
    return vecLabels;
}

// 선 하나를 "from 종류 to" 글로(출력의 점 id 로)
QString DescribeLayoutEdge(const GraphLayoutOutput& out, const LayoutEdge& edge)
{
    const QString strFrom = edge.iFrom >= 0 && edge.iFrom < out.vecNodes.size() ? out.vecNodes[edge.iFrom].strId : QStringLiteral("?");
    const QString strTo = edge.iTo >= 0 && edge.iTo < out.vecNodes.size() ? out.vecNodes[edge.iTo].strId : QStringLiteral("?");
    return strFrom + QLatin1Char(' ') + edge.strKind + QLatin1Char(' ') + strTo;
}

QString DescribeGraphEdge(const KnowledgeGraphEdge& edge)
{
    return edge.edge.strFromId + QLatin1Char(' ') + edge.edge.strKind + QLatin1Char(' ') + edge.edge.strToId;
}

int FindCell(const GraphLayoutOutput& out, int row, int column)
{
    for (int iCell = 0; iCell < out.vecCells.size(); ++iCell)
        if (out.vecCells[iCell].iRow == row && out.vecCells[iCell].iColumn == column)
            return iCell;
    return -1;
}

QString DescribePoint(const QString& id, const QPointF& point)
{
    return id + QStringLiteral(" (") + FormatNumber(point.x()) + QStringLiteral(", ") + FormatNumber(point.y()) + QLatin1Char(')');
}

// 합성 그래프용 기록
KnowledgeConcept MakeConcept(const QString& id, const QString& name, const QString& year = QString())
{
    KnowledgeConcept concept;
    concept.strId = id;
    concept.strKind = QStringLiteral("method");
    concept.strName = name;
    concept.strYear = year;
    concept.strSummary = name + DecodeUtf8(" 의 정의");
    return concept;
}

KnowledgeEdge MakeEdge(const QString& from, const QString& kind, const QString& to)
{
    KnowledgeEdge edge;
    edge.strFromId = from;
    edge.strKind = kind;
    edge.strToId = to;
    edge.eStatus = ERelationStatus::CONFIRMED;
    return edge;
}

ConceptLink MakeLink(const QString& conceptId)
{
    return { conceptId, conceptId };
}
}

// 그래프 모델(KnowledgeGraph)과 배치 6가지(KnowledgeGraphLayout, design/Views.md §5)를 showcase 와 작은 합성 그래프로 시험한다.
// showcase 기대값은 Views.md §5 끝의 목록이고, 그 밖에 배치 조건(겹침 없음 · 방향 · 깊이 · 순서 · 경계 · 유한한 값)과
// 같은 입력이면 같은 출력인지를 본다.
class KnowledgeGraphLayoutTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(m_index.Load(QStringLiteral(VISUALMARKDOWN_SHOWCASE_DIR)), VISUALMARKDOWN_SHOWCASE_DIR);
        QVERIFY2(m_index.GetWarnings().isEmpty(), qUtf8Printable(m_index.GetWarnings().join(QLatin1Char('\n'))));
        m_graph = KnowledgeGraph::BuildFromIndex(m_index);
        m_vecOutputs.resize(kAllTypes.size());
        for (const EGraphLayoutType eType : kAllTypes)
            QVERIFY2(RunLayout(m_graph, eType, &m_vecOutputs[static_cast<int>(eType)]), qUtf8Printable(GetTypeName(eType)));
    }

    // 점: 보이는 개념 31(색인과 같은 이름 열쇠 순). 이름 · 종류 · 연도 · 정의 · 다루는 문서 수 · 선 수
    void graphNodesFollowIndex()
    {
        const QVector<KnowledgeConcept> vecConcepts = m_index.ListConcepts();
        const QVector<KnowledgeGraphNode>& vecNodes = m_graph.GetNodes();
        QCOMPARE(vecNodes.size(), kNodeCount);
        QCOMPARE(vecNodes.size(), vecConcepts.size());
        int iEdgeCountSum = 0;
        for (int iNode = 0; iNode < vecNodes.size(); ++iNode)
        {
            const KnowledgeGraphNode& node = vecNodes[iNode];
            const KnowledgeConcept& concept = vecConcepts[iNode];
            QCOMPARE(node.strId, concept.strId);
            QCOMPARE(node.strName, concept.strName);
            QCOMPARE(node.strKind, concept.strKind);
            QCOMPARE(node.strYear, concept.strYear);
            QCOMPARE(node.iYear, concept.strYear.isEmpty() ? -1 : concept.strYear.toInt());
            QCOMPARE(node.strDefinition, concept.strSummary);
            QVERIFY2(!node.strNameKey.isEmpty(), qUtf8Printable(node.strId));
            QCOMPARE(node.iDocumentCount, m_index.ListDocumentIdsPointingTo(node.strId).size());
            QCOMPARE(m_graph.FindNode(node.strId), iNode);
            iEdgeCountSum += node.iEdgeCount;
        }
        QCOMPARE(iEdgeCountSum, 2 * kEdgeCount);   // 제 자신으로 가는 선이 없다
        for (const NameCount& expected : kEdgeCounts)
        {
            const int iNode = m_graph.FindNode(DecodeUtf8(expected.pName));
            QVERIFY2(iNode >= 0, expected.pName);
            QCOMPARE(DecodeUtf8(expected.pName) + QLatin1Char(' ') + QString::number(vecNodes[iNode].iEdgeCount),
                     DecodeUtf8(expected.pName) + QLatin1Char(' ') + QString::number(expected.iCount));
        }
        QCOMPARE(m_graph.FindNode(DecodeUtf8(kHiddenConcept)), -1);
        QCOMPARE(m_graph.FindNode(DecodeUtf8(kMissingConcept)), -1);
    }

    // 선: 색인의 선 36 그대로(순서도), 두 끝의 점 번호, 방향 없는 선 5
    void graphEdgesFollowIndex()
    {
        const QVector<KnowledgeEdge> vecIndexEdges = m_index.ListEdges();
        const QVector<KnowledgeGraphEdge>& vecEdges = m_graph.GetEdges();
        const QVector<KnowledgeGraphNode>& vecNodes = m_graph.GetNodes();
        QCOMPARE(vecEdges.size(), kEdgeCount);
        QCOMPARE(vecEdges.size(), vecIndexEdges.size());
        const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
        int iUndirected = 0;
        for (int iEdge = 0; iEdge < vecEdges.size(); ++iEdge)
        {
            const KnowledgeGraphEdge& edge = vecEdges[iEdge];
            const KnowledgeEdge& expected = vecIndexEdges[iEdge];
            QCOMPARE(DescribeGraphEdge(edge), expected.strFromId + QLatin1Char(' ') + expected.strKind + QLatin1Char(' ') + expected.strToId);
            QCOMPARE(edge.edge.eStatus, expected.eStatus);
            QCOMPARE(edge.edge.vecDocumentIds, expected.vecDocumentIds);
            QCOMPARE(edge.edge.bFromConceptFile, expected.bFromConceptFile);
            QCOMPARE(edge.edge.vecNotes, expected.vecNotes);
            QVERIFY(edge.iFrom >= 0 && edge.iTo >= 0);
            QCOMPARE(vecNodes[edge.iFrom].strId, expected.strFromId);
            QCOMPARE(vecNodes[edge.iTo].strId, expected.strToId);
            QCOMPARE(edge.bDirected, !vocabulary.IsUndirected(expected.strKind));
            if (!edge.bDirected)
                ++iUndirected;
        }
        QCOMPARE(iUndirected, kUndirectedEdgeCount);
    }

    // 덩어리 6개: 큰 것부터, 같으면 이름 열쇠 순. 점마다 덩어리 번호, 선은 한 덩어리 안에 있다
    void graphComponents()
    {
        const QVector<QVector<int>>& vecComponents = m_graph.GetComponents();
        const QVector<KnowledgeGraphNode>& vecNodes = m_graph.GetNodes();
        QList<int> vecSizes;
        for (const QVector<int>& vecComponent : vecComponents)
            vecSizes << vecComponent.size();
        QCOMPARE(vecSizes, kComponentSizes);
        QStringList vecSingles;
        for (int iComponent = 0; iComponent < vecComponents.size(); ++iComponent)
        {
            const QVector<int>& vecComponent = vecComponents[iComponent];
            for (int iMember = 0; iMember < vecComponent.size(); ++iMember)
            {
                QCOMPARE(vecNodes[vecComponent[iMember]].iComponent, iComponent);
                if (iMember > 0)
                    QVERIFY2(vecComponent[iMember - 1] < vecComponent[iMember], "component members are in node order");
            }
            if (vecComponent.size() == 1)
                vecSingles << vecNodes[vecComponent[0]].strId;
        }
        QCOMPARE(vecSingles, kSingleComponents);
        QCOMPARE(vecNodes[m_graph.FindNode(DecodeUtf8("검출기"))].iComponent, 0);
        QCOMPARE(vecNodes[m_graph.FindNode(DecodeUtf8("결제-서비스"))].iComponent, 1);
        for (const KnowledgeGraphEdge& edge : m_graph.GetEdges())
            QCOMPARE(vecNodes[edge.iFrom].iComponent, vecNodes[edge.iTo].iComponent);
    }

    // 문서 6(문서 목록 순)과 문서 → 개념 연결(개념 파일이 있는 것만). 무게는 가리킨 횟수
    void graphDocumentsAndLinks()
    {
        const QVector<KnowledgeDocument> vecIndexDocuments = m_index.ListDocuments();
        const QVector<KnowledgeGraphDocument>& vecDocuments = m_graph.GetDocuments();
        QCOMPARE(vecDocuments.size(), vecIndexDocuments.size());
        QStringList vecTitles;
        for (int iDocument = 0; iDocument < vecDocuments.size(); ++iDocument)
        {
            QCOMPARE(vecDocuments[iDocument].strId, vecIndexDocuments[iDocument].strId);
            vecTitles << vecDocuments[iDocument].strTitle;
        }
        QCOMPARE(vecTitles, kDocumentTitles);

        const QVector<KnowledgeGraphNode>& vecNodes = m_graph.GetNodes();
        QMap<QString, int> mapLinks;
        QMap<QString, int> mapWeights;
        QSet<int> setLinkedNodes;
        QVector<int> vecDocumentsPerNode(vecNodes.size(), 0);
        const QVector<KnowledgeGraphLink>& vecLinks = m_graph.GetLinks();
        for (int iLink = 0; iLink < vecLinks.size(); ++iLink)
        {
            const KnowledgeGraphLink& link = vecLinks[iLink];
            QVERIFY(link.iDocument >= 0 && link.iDocument < vecDocuments.size());
            QVERIFY(link.iNode >= 0 && link.iNode < vecNodes.size());
            QVERIFY2(link.iWeight >= 1, qUtf8Printable(vecNodes[link.iNode].strId));
            if (iLink > 0)
            {
                const KnowledgeGraphLink& previous = vecLinks[iLink - 1];
                QVERIFY2(previous.iDocument < link.iDocument || (previous.iDocument == link.iDocument && previous.iNode < link.iNode),
                         "links are in (document, node) order without repeats");
            }
            const QString strTitle = vecDocuments[link.iDocument].strTitle;
            ++mapLinks[strTitle];
            mapWeights[strTitle] += link.iWeight;
            setLinkedNodes.insert(link.iNode);
            ++vecDocumentsPerNode[link.iNode];
        }
        for (const DocumentLinks& expected : kDocumentLinks)
        {
            const QString strTitle = DecodeUtf8(expected.pTitle);
            QCOMPARE(strTitle + QLatin1Char(' ') + QString::number(mapLinks.value(strTitle)), strTitle + QLatin1Char(' ') + QString::number(expected.iLinks));
            QCOMPARE(strTitle + QLatin1Char(' ') + QString::number(mapWeights.value(strTitle)),
                     strTitle + QLatin1Char(' ') + QString::number(expected.iWeightSum));
        }
        for (const LinkWeight& expected : kLinkWeights)
            QCOMPARE(FindLinkWeight(DecodeUtf8(expected.pTitle), DecodeUtf8(expected.pConcept)), expected.iWeight);
        QCOMPARE(setLinkedNodes.size(), kBipartiteConcepts);
        for (int iNode = 0; iNode < vecNodes.size(); ++iNode)
            QCOMPARE(vecNodes[iNode].strId + QLatin1Char(' ') + QString::number(vecDocumentsPerNode[iNode]),
                     vecNodes[iNode].strId + QLatin1Char(' ') + QString::number(vecNodes[iNode].iDocumentCount));
        for (const QString& strId : kConceptsWithoutDocuments)
            QVERIFY2(!setLinkedNodes.contains(m_graph.FindNode(strId)), qUtf8Printable(strId));
    }

    // 관계망: 점 31 · 선 36. 점 상자끼리 · 덩어리 상자끼리 겹치지 않고, 덩어리는 큰 것부터 왼쪽에서 오른쪽, 선 없는 점은 끝에 한 줄
    void networkShowsWholeGraph()
    {
        const GraphLayoutOutput& out = GetOutput(EGraphLayoutType::NETWORK);
        QCOMPARE(ListNodeIds(out), ListNodeIds(m_graph));
        const QVector<KnowledgeGraphEdge>& vecGraphEdges = m_graph.GetEdges();
        QCOMPARE(out.vecEdges.size(), kEdgeCount);
        for (int iEdge = 0; iEdge < out.vecEdges.size(); ++iEdge)
        {
            const LayoutEdge& edge = out.vecEdges[iEdge];
            const KnowledgeGraphEdge& graphEdge = vecGraphEdges[iEdge];
            QCOMPARE(edge.iGraphEdge, iEdge);
            QCOMPARE(edge.iFrom, graphEdge.iFrom);
            QCOMPARE(edge.iTo, graphEdge.iTo);
            QCOMPARE(edge.strKind, graphEdge.edge.strKind);
            QCOMPARE(edge.eStatus, graphEdge.edge.eStatus);
            QCOMPARE(edge.bDirected, graphEdge.bDirected);
        }
        const QStringList vecOverlaps = FindOverlaps(out);
        QVERIFY2(vecOverlaps.isEmpty(), qUtf8Printable(vecOverlaps.join(QStringLiteral(", "))));

        // 덩어리 상자: 서로 겹치지 않고 덩어리 순으로 왼쪽에서 오른쪽
        const QVector<QVector<int>>& vecComponents = m_graph.GetComponents();
        QVector<QRectF> vecBoxes;
        for (const QVector<int>& vecComponent : vecComponents)
        {
            QRectF rectBox;
            for (const int iNode : vecComponent)
                rectBox = rectBox.isNull() ? MakeBox(out.vecNodes[iNode]) : rectBox.united(MakeBox(out.vecNodes[iNode]));
            vecBoxes << rectBox;
        }
        QCOMPARE(vecBoxes.size(), kComponentSizes.size());
        for (int iFirst = 0; iFirst < vecBoxes.size(); ++iFirst)
        {
            for (int iSecond = iFirst + 1; iSecond < vecBoxes.size(); ++iSecond)
                QVERIFY2(!vecBoxes[iFirst].intersects(vecBoxes[iSecond]),
                         qUtf8Printable(QStringLiteral("components %1 and %2 overlap").arg(iFirst).arg(iSecond)));
            if (iFirst > 0)
                QVERIFY2(vecBoxes[iFirst - 1].right() < vecBoxes[iFirst].left(),
                         qUtf8Printable(QStringLiteral("component %1 is not right of component %2").arg(iFirst).arg(iFirst - 1)));
        }
        // 선 없는 점은 한 줄(같은 높이)
        double dSingleY = qQNaN();
        for (const QString& strId : kSingleComponents)
        {
            const QPointF ptCenter = GetCenter(out, strId);
            if (std::isnan(dSingleY))
                dSingleY = ptCenter.y();
            QVERIFY2(qFuzzyCompare(ptCenter.y(), dSingleY), qUtf8Printable(DescribePoint(strId, ptCenter)));
        }
    }

    // 계보도: improves 선과 그 끝(점 8 · 선 5), 갈래 3개. 갈래 안에서 옛것이 왼쪽이고 선마다 from(새것)이 to(옛것)보다 오른쪽
    void lineageFollowsImproves()
    {
        const GraphLayoutOutput& out = GetOutput(EGraphLayoutType::LINEAGE);
        QCOMPARE(out.vecNodes.size(), kLineageNodes);
        QCOMPARE(out.vecEdges.size(), kLineageEdges);
        QCOMPARE(ListLaneLabels(out), ListExpectedLaneLabels(kLineageLanes));
        for (int iLane = 0; iLane < kLineageLanes.size(); ++iLane)
        {
            const ExpectedLane& expected = kLineageLanes[iLane];
            const QString strDiff = DiffItems(DecodeUtf8(expected.pLabel), ListNodesInLane(out, out.vecLanes[iLane]), expected.vecIds);
            QVERIFY2(strDiff.isEmpty(), qUtf8Printable(strDiff));
            for (int iNode = 1; iNode < expected.vecIds.size(); ++iNode)
            {
                const QPointF ptOlder = GetCenter(out, expected.vecIds[iNode - 1]);
                const QPointF ptNewer = GetCenter(out, expected.vecIds[iNode]);
                QVERIFY2(ptOlder.x() < ptNewer.x(), qUtf8Printable(DescribePoint(expected.vecIds[iNode - 1], ptOlder) + QStringLiteral(" / ")
                                                                   + DescribePoint(expected.vecIds[iNode], ptNewer)));
            }
            if (iLane > 0)
                QVERIFY2(out.vecLanes[iLane - 1].dBottom < out.vecLanes[iLane].dTop, expected.pLabel);
        }
        for (const LayoutEdge& edge : out.vecEdges)
        {
            QCOMPARE(edge.strKind, QString("improves"));
            QVERIFY(edge.bDirected);
            QVERIFY2(out.vecNodes[edge.iFrom].center.x() > out.vecNodes[edge.iTo].center.x(), qUtf8Printable(DescribeLayoutEdge(out, edge)));
            QCOMPARE(m_graph.GetEdges()[edge.iGraphEdge].edge.strKind, QString("improves"));
        }
        const QStringList vecOverlaps = FindOverlaps(out);
        QVERIFY2(vecOverlaps.isEmpty(), qUtf8Printable(vecOverlaps.join(QStringLiteral(", "))));
    }

    // 분류 트리: isA 선과 그 끝(점 14 · 선 12), 나무 2개(큰 것부터 왼쪽). 자식은 부모 아래 한 깊이, 이름 열쇠 순, 부모는 첫 · 끝 자식 가운데
    void treeFollowsIsA()
    {
        const GraphLayoutOutput& out = GetOutput(EGraphLayoutType::TREE);
        QCOMPARE(out.vecNodes.size(), kTreeNodes);
        const QStringList vecIds = ListNodeIds(out);
        QCOMPARE(QSet<QString>(vecIds.cbegin(), vecIds.cend()).size(), kTreeNodes);   // 점마다 한 번
        QCOMPARE(out.vecEdges.size(), kTreeEdges);
        const double dLevelPitch = GraphLayoutSize::kNodeHeight + GraphLayoutSize::kTreeLevelGap;
        for (const LayoutEdge& edge : out.vecEdges)
        {
            QCOMPARE(edge.strKind, QString("isA"));
            const QPointF ptChild = out.vecNodes[edge.iFrom].center;
            const QPointF ptParent = out.vecNodes[edge.iTo].center;
            QVERIFY2(ptChild.y() > ptParent.y(), qUtf8Printable(DescribeLayoutEdge(out, edge)));
            QVERIFY2(qAbs(ptChild.y() - ptParent.y() - dLevelPitch) < kTolerance, qUtf8Printable(DescribeLayoutEdge(out, edge)));
        }
        for (const ExpectedChildren& expected : kTreeChildren)
        {
            const QString strParent = DecodeUtf8(expected.pParent);
            const int iParent = FindLayoutNode(out, strParent);
            QVERIFY2(iParent >= 0, expected.pParent);
            QMap<double, QString> mapChildrenByX;
            for (const LayoutEdge& edge : out.vecEdges)
                if (edge.iTo == iParent)
                    mapChildrenByX.insert(out.vecNodes[edge.iFrom].center.x(), out.vecNodes[edge.iFrom].strId);
            QCOMPARE(QStringList(mapChildrenByX.values()), expected.vecChildren);
            const double dMiddle = (GetCenter(out, expected.vecChildren.first()).x() + GetCenter(out, expected.vecChildren.last()).x()) / 2;
            QVERIFY2(qAbs(out.vecNodes[iParent].center.x() - dMiddle) < kTolerance, expected.pParent);
        }
        // 잎은 왼쪽부터 이어진 칸이다
        const double dSlot = GraphLayoutSize::kNodeWidth + GraphLayoutSize::kNodeGapX;
        for (int iLeaf = 1; iLeaf < kDetectorTreeLeaves.size(); ++iLeaf)
        {
            const double dStep = GetCenter(out, kDetectorTreeLeaves[iLeaf]).x() - GetCenter(out, kDetectorTreeLeaves[iLeaf - 1]).x();
            QVERIFY2(qAbs(dStep - dSlot) < kTolerance, qUtf8Printable(kDetectorTreeLeaves[iLeaf]));
        }
        // 나무: 뿌리가 맨 위이고, 큰 나무가 왼쪽에 있다
        const QStringList vecSmallTree { "결제-모듈", "결제-모듈-2판" };
        double dBigTreeRight = -qInf();
        double dSmallTreeLeft = qInf();
        int iBigTree = 0;
        for (const LayoutNode& node : out.vecNodes)
        {
            if (vecSmallTree.contains(node.strId))
            {
                dSmallTreeLeft = qMin(dSmallTreeLeft, MakeBox(node).left());
                continue;
            }
            ++iBigTree;
            dBigTreeRight = qMax(dBigTreeRight, MakeBox(node).right());
        }
        QCOMPARE(iBigTree, kTrees[0].iCount);
        QCOMPARE(out.vecNodes.size() - iBigTree, kTrees[1].iCount);
        QVERIFY2(dBigTreeRight < dSmallTreeLeft, "the bigger tree is on the left");
        for (const NameCount& tree : kTrees)
            QCOMPARE(GetCenter(out, DecodeUtf8(tree.pName)).y(), out.vecNodes[FindLayoutNode(out, DecodeUtf8(kTrees[0].pName))].center.y());
        const QStringList vecOverlaps = FindOverlaps(out);
        QVERIFY2(vecOverlaps.isEmpty(), qUtf8Printable(vecOverlaps.join(QStringLiteral(", "))));
    }

    // 타임라인: 연도 있는 점 16 · 없는 점 15(DPM 없음), 눈금은 있는 해마다, 갈래 3개. 같은 갈래에서 늦은 해가 오른쪽
    void timelineFollowsYears()
    {
        const GraphLayoutOutput& out = GetOutput(EGraphLayoutType::TIMELINE);
        QCOMPARE(out.vecNodes.size(), kTimelinePlaced);
        QCOMPARE(out.vecUnplaced.size(), kTimelineUnplaced);
        QStringList vecExpectedUnplaced;
        for (const KnowledgeGraphNode& node : m_graph.GetNodes())
            if (node.iYear < 0)
                vecExpectedUnplaced << node.strId;
        QCOMPARE(out.vecUnplaced, vecExpectedUnplaced);   // 이름 열쇠 순
        QCOMPARE(FindLayoutNode(out, DecodeUtf8(kHiddenConcept)), -1);

        // 눈금: 있는 해마다 하나, x 는 (해 − 첫 해) × 한 해 간격
        QStringList vecTickLabels;
        for (const LayoutTick& tick : out.vecTicks)
        {
            vecTickLabels << tick.strLabel;
            const double dExpected = out.vecTicks[0].dPosition + (tick.strLabel.toInt() - out.vecTicks[0].strLabel.toInt()) * GraphLayoutSize::kTimelineYearGap;
            QVERIFY2(qAbs(tick.dPosition - dExpected) < kTolerance, qUtf8Printable(tick.strLabel));
        }
        QCOMPARE(vecTickLabels, kTimelineYears);
        QHash<QString, double> hashTickByYear;
        for (const LayoutTick& tick : out.vecTicks)
            hashTickByYear.insert(tick.strLabel, tick.dPosition);
        for (const LayoutNode& node : out.vecNodes)
        {
            const KnowledgeGraphNode& graphNode = m_graph.GetNodes()[m_graph.FindNode(node.strId)];
            QVERIFY2(hashTickByYear.contains(graphNode.strYear), qUtf8Printable(node.strId));
            QVERIFY2(qAbs(node.center.x() - hashTickByYear.value(graphNode.strYear)) < kTolerance, qUtf8Printable(DescribePoint(node.strId, node.center)));
        }

        // 갈래: 이름 · 점, 같은 갈래 안에서 늦은 해가 오른쪽, 같은 해는 이름 열쇠 순으로 아래로 쌓는다
        QCOMPARE(ListLaneLabels(out), ListExpectedLaneLabels(kTimelineLanes));
        for (int iLane = 0; iLane < kTimelineLanes.size(); ++iLane)
        {
            const ExpectedLane& expected = kTimelineLanes[iLane];
            const QStringList vecMembers = ListNodesInLane(out, out.vecLanes[iLane]);
            const QString strDiff = DiffItems(DecodeUtf8(expected.pLabel), vecMembers, expected.vecIds);
            QVERIFY2(strDiff.isEmpty(), qUtf8Printable(strDiff));
            for (const QString& strFirst : vecMembers)
            {
                for (const QString& strSecond : vecMembers)
                {
                    const int iFirst = m_graph.FindNode(strFirst);
                    const int iSecond = m_graph.FindNode(strSecond);
                    const QPointF ptFirst = GetCenter(out, strFirst);
                    const QPointF ptSecond = GetCenter(out, strSecond);
                    const QString strPair = DescribePoint(strFirst, ptFirst) + QStringLiteral(" / ") + DescribePoint(strSecond, ptSecond);
                    if (m_graph.GetNodes()[iFirst].iYear < m_graph.GetNodes()[iSecond].iYear)
                        QVERIFY2(ptFirst.x() < ptSecond.x(), qUtf8Printable(strPair));
                    else if (m_graph.GetNodes()[iFirst].iYear == m_graph.GetNodes()[iSecond].iYear && iFirst < iSecond)
                        QVERIFY2(ptFirst.y() < ptSecond.y(), qUtf8Printable(strPair));
                }
            }
        }

        // 선: 놓인 두 점 사이의 isA · improves 선 전부
        QStringList vecExpectedEdges;
        for (const KnowledgeGraphEdge& edge : m_graph.GetEdges())
            if ((edge.edge.strKind == QLatin1String("isA") || edge.edge.strKind == QLatin1String("improves"))
                && m_graph.GetNodes()[edge.iFrom].iYear >= 0 && m_graph.GetNodes()[edge.iTo].iYear >= 0)
                vecExpectedEdges << DescribeGraphEdge(edge);
        QStringList vecEdges;
        for (const LayoutEdge& edge : out.vecEdges)
        {
            vecEdges << DescribeLayoutEdge(out, edge);
            QCOMPARE(DescribeLayoutEdge(out, edge), DescribeGraphEdge(m_graph.GetEdges()[edge.iGraphEdge]));
        }
        QCOMPARE(vecEdges, vecExpectedEdges);
        const QStringList vecOverlaps = FindOverlaps(out);
        QVERIFY2(vecOverlaps.isEmpty(), qUtf8Printable(vecOverlaps.join(QStringLiteral(", "))));
    }

    // 관계 행렬: 31 × 31, 채워진 칸 40. 종류가 둘인 칸은 (결제 모듈 2판, 결제 모듈) 하나, 방향 없는 선은 두 칸, 대각선은 빈다
    void matrixFollowsComponents()
    {
        const GraphLayoutOutput& out = GetOutput(EGraphLayoutType::MATRIX);
        QCOMPARE(out.vecMatrixOrder, kMatrixOrder);
        QCOMPARE(ListNodeIds(out), kMatrixOrder);
        QCOMPARE(out.vecCells.size(), kMatrixCells);
        const QStringList vecVocabularyKinds = KnowledgeVocabulary::Get().GetRelationCodes();
        QStringList vecSeveral;
        for (int iCell = 0; iCell < out.vecCells.size(); ++iCell)
        {
            const LayoutMatrixCell& cell = out.vecCells[iCell];
            QVERIFY(cell.iRow >= 0 && cell.iRow < kMatrixOrder.size() && cell.iColumn >= 0 && cell.iColumn < kMatrixOrder.size());
            QVERIFY2(cell.iRow != cell.iColumn, qUtf8Printable(kMatrixOrder[cell.iRow]));
            QVERIFY(!cell.vecKinds.isEmpty());
            for (int iKind = 1; iKind < cell.vecKinds.size(); ++iKind)
                QVERIFY2(vecVocabularyKinds.indexOf(cell.vecKinds[iKind - 1]) < vecVocabularyKinds.indexOf(cell.vecKinds[iKind]),
                         qUtf8Printable(cell.vecKinds.join(QLatin1Char(' '))));
            if (iCell > 0)
            {
                const LayoutMatrixCell& previous = out.vecCells[iCell - 1];
                QVERIFY2(previous.iRow < cell.iRow || (previous.iRow == cell.iRow && previous.iColumn < cell.iColumn), "cells are in (row, column) order");
            }
            if (cell.vecKinds.size() > 1)
                vecSeveral << kMatrixOrder[cell.iRow] + QLatin1Char(' ') + kMatrixOrder[cell.iColumn] + QStringLiteral(": ") + cell.vecKinds.join(QLatin1Char(' '));
        }
        QStringList vecExpectedSeveral;
        for (const ExpectedCell& expected : kCellsWithSeveralKinds)
            vecExpectedSeveral << DecodeUtf8(expected.pRow) + QLatin1Char(' ') + DecodeUtf8(expected.pColumn) + QStringLiteral(": ")
                                      + expected.vecKinds.join(QLatin1Char(' '));
        QCOMPARE(vecSeveral, vecExpectedSeveral);

        // 선마다 그 칸에 종류가 있다. 방향 없는 선은 마주 보는 두 칸 모두
        for (const KnowledgeGraphEdge& edge : m_graph.GetEdges())
        {
            const int iRow = kMatrixOrder.indexOf(edge.edge.strFromId);
            const int iColumn = kMatrixOrder.indexOf(edge.edge.strToId);
            const int iCell = FindCell(out, iRow, iColumn);
            QVERIFY2(iCell >= 0 && out.vecCells[iCell].vecKinds.contains(edge.edge.strKind), qUtf8Printable(DescribeGraphEdge(edge)));
            if (!edge.bDirected)
            {
                const int iMirror = FindCell(out, iColumn, iRow);
                QVERIFY2(iMirror >= 0 && out.vecCells[iMirror].vecKinds.contains(edge.edge.strKind), qUtf8Printable(DescribeGraphEdge(edge)));
            }
        }
        for (const LayoutMatrixCell& cell : out.vecCells)
        {
            if (!cell.vecKinds.contains(QStringLiteral("alternative")))
                continue;
            const int iMirror = FindCell(out, cell.iColumn, cell.iRow);
            QVERIFY2(iMirror >= 0 && out.vecCells[iMirror].vecKinds.contains(QStringLiteral("alternative")), qUtf8Printable(kMatrixOrder[cell.iRow]));
        }
    }

    // 문서 ↔ 개념: 왼쪽 열 문서 6(문서 순), 오른쪽 열 개념 28(무게 중심 순, 같으면 이름 열쇠 순), 문서마다 선 5 · 7 · 5 · 7 · 4 · 9
    void bipartiteLinksDocumentsToConcepts()
    {
        const GraphLayoutOutput& out = GetOutput(EGraphLayoutType::BIPARTITE);
        const int iDocuments = kDocumentTitles.size();
        QCOMPARE(out.vecNodes.size(), iDocuments + kBipartiteConcepts);
        QStringList vecTitles;
        for (int iNode = 0; iNode < out.vecNodes.size(); ++iNode)
        {
            const LayoutNode& node = out.vecNodes[iNode];
            const bool bDocument = iNode < iDocuments;
            QCOMPARE(node.bDocument, bDocument);
            if (bDocument)
            {
                vecTitles << node.strLabel;
                QCOMPARE(node.strId, m_graph.GetDocuments()[iNode].strId);
            }
            else
            {
                QVERIFY2(m_graph.FindNode(node.strId) >= 0, qUtf8Printable(node.strId));
            }
            // 같은 열은 같은 x, 위에서 아래로
            if (iNode > 0 && (iNode < iDocuments) == (iNode - 1 < iDocuments))
            {
                QCOMPARE(node.center.x(), out.vecNodes[iNode - 1].center.x());
                QVERIFY2(node.center.y() > out.vecNodes[iNode - 1].center.y(), qUtf8Printable(node.strId));
            }
        }
        QCOMPARE(vecTitles, kDocumentTitles);
        QVERIFY(out.vecNodes[0].center.x() < out.vecNodes[iDocuments].center.x());
        QCOMPARE(FindLayoutNode(out, DecodeUtf8(kMissingConcept)), -1);
        for (const QString& strId : kConceptsWithoutDocuments)
            QCOMPARE(FindLayoutNode(out, strId), -1);

        // 선: 연결마다 하나, 무게는 가리킨 횟수
        QMap<QString, int> mapLinks;
        QMap<QString, int> mapWeights;
        QVector<qint64> vecRowSum(out.vecNodes.size(), 0);
        QVector<qint64> vecRowCount(out.vecNodes.size(), 0);
        for (const LayoutEdge& edge : out.vecEdges)
        {
            QVERIFY(edge.iFrom >= 0 && edge.iFrom < iDocuments);
            QVERIFY(edge.iTo >= iDocuments && edge.iTo < out.vecNodes.size());
            QCOMPARE(edge.iGraphEdge, -1);
            QVERIFY(!edge.bDirected);
            const QString strTitle = out.vecNodes[edge.iFrom].strLabel;
            ++mapLinks[strTitle];
            mapWeights[strTitle] += edge.iWeight;
            vecRowSum[edge.iTo] += edge.iFrom;
            ++vecRowCount[edge.iTo];
        }
        for (const DocumentLinks& expected : kDocumentLinks)
        {
            const QString strTitle = DecodeUtf8(expected.pTitle);
            QCOMPARE(strTitle + QLatin1Char(' ') + QString::number(mapLinks.value(strTitle)), strTitle + QLatin1Char(' ') + QString::number(expected.iLinks));
            QCOMPARE(strTitle + QLatin1Char(' ') + QString::number(mapWeights.value(strTitle)),
                     strTitle + QLatin1Char(' ') + QString::number(expected.iWeightSum));
        }
        for (const LinkWeight& expected : kLinkWeights)
        {
            int iWeight = 0;
            for (const LayoutEdge& edge : out.vecEdges)
                if (out.vecNodes[edge.iFrom].strLabel == DecodeUtf8(expected.pTitle) && out.vecNodes[edge.iTo].strId == DecodeUtf8(expected.pConcept))
                    iWeight = edge.iWeight;
            QCOMPARE(DecodeUtf8(expected.pConcept) + QLatin1Char(' ') + QString::number(iWeight),
                     DecodeUtf8(expected.pConcept) + QLatin1Char(' ') + QString::number(expected.iWeight));
        }
        // 개념 순서: 가리키는 문서 행의 평균이 작은 것부터, 같으면 이름 열쇠 순(점 번호 순)
        for (int iNode = iDocuments + 1; iNode < out.vecNodes.size(); ++iNode)
        {
            QVERIFY(vecRowCount[iNode - 1] > 0 && vecRowCount[iNode] > 0);
            const qint64 llBefore = vecRowSum[iNode - 1] * vecRowCount[iNode];
            const qint64 llAfter = vecRowSum[iNode] * vecRowCount[iNode - 1];
            const bool bOrdered = llBefore < llAfter
                || (llBefore == llAfter && m_graph.FindNode(out.vecNodes[iNode - 1].strId) < m_graph.FindNode(out.vecNodes[iNode].strId));
            QVERIFY2(bOrdered, qUtf8Printable(out.vecNodes[iNode - 1].strId + QStringLiteral(" / ") + out.vecNodes[iNode].strId));
        }
        const QStringList vecOverlaps = FindOverlaps(out);
        QVERIFY2(vecOverlaps.isEmpty(), qUtf8Printable(vecOverlaps.join(QStringLiteral(", "))));
    }

    void layoutInvariants_data()
    {
        QTest::addColumn<int>("type");
        for (const EGraphLayoutType eType : kAllTypes)
            QTest::newRow(qPrintable(GetTypeName(eType))) << static_cast<int>(eType);
    }

    // 모든 배치: 값이 유한하고, 선은 있는 점을 가리키고, 경계가 점 상자(행렬은 점 가운데) · 갈래 · 눈금을 모두 담는다
    void layoutInvariants()
    {
        QFETCH(int, type);
        const EGraphLayoutType eType = static_cast<EGraphLayoutType>(type);
        const GraphLayoutOutput& out = GetOutput(eType);
        QVERIFY(!out.vecNodes.isEmpty());
        const QStringList vecNonFinite = FindNonFinite(out);
        QVERIFY2(vecNonFinite.isEmpty(), qUtf8Printable(vecNonFinite.join(QStringLiteral(", "))));
        for (const LayoutEdge& edge : out.vecEdges)
        {
            QVERIFY(edge.iFrom >= 0 && edge.iFrom < out.vecNodes.size());
            QVERIFY(edge.iTo >= 0 && edge.iTo < out.vecNodes.size());
            QVERIFY(edge.iGraphEdge >= -1 && edge.iGraphEdge < m_graph.GetEdges().size());
            QVERIFY(edge.iWeight >= 1);
        }
        QVERIFY(!out.bounds.isEmpty());
        for (const LayoutNode& node : out.vecNodes)
        {
            if (eType == EGraphLayoutType::MATRIX)
                QVERIFY2(ContainsWithTolerance(out.bounds, node.center), qUtf8Printable(DescribePoint(node.strId, node.center)));
            else
                QVERIFY2(ContainsWithTolerance(out.bounds, MakeBox(node)), qUtf8Printable(DescribePoint(node.strId, node.center)));
        }
        for (const LayoutLane& lane : out.vecLanes)
        {
            QVERIFY2(lane.dTop < lane.dBottom, qUtf8Printable(lane.strLabel));
            QVERIFY2(lane.dTop >= out.bounds.top() - kTolerance && lane.dBottom <= out.bounds.bottom() + kTolerance, qUtf8Printable(lane.strLabel));
        }
        for (const LayoutTick& tick : out.vecTicks)
            QVERIFY2(tick.dPosition >= out.bounds.left() - kTolerance && tick.dPosition <= out.bounds.right() + kTolerance, qUtf8Printable(tick.strLabel));
        QSet<QString> setIds;
        for (const LayoutNode& node : out.vecNodes)
        {
            QVERIFY2(!setIds.contains(node.strId), qUtf8Printable(node.strId));   // 점마다 한 번
            setIds.insert(node.strId);
            QVERIFY2(node.strId != DecodeUtf8(kHiddenConcept), kHiddenConcept);
        }
    }

    void layoutsAreDeterministic_data()
    {
        layoutInvariants_data();
    }

    // 같은 입력이면 같은 출력: 같은 그래프로 다시 돌려도, 색인을 다시 읽어 만든 그래프로 돌려도 비트까지 같다
    void layoutsAreDeterministic()
    {
        QFETCH(int, type);
        const EGraphLayoutType eType = static_cast<EGraphLayoutType>(type);
        KnowledgeIndex index;
        QVERIFY(index.Load(QStringLiteral(VISUALMARKDOWN_SHOWCASE_DIR)));
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromIndex(index);
        QCOMPARE(DescribeGraph(graph), DescribeGraph(m_graph));

        const QStringList vecExpected = DescribeLayout(GetOutput(eType));
        GraphLayoutOutput again;
        QVERIFY(RunLayout(m_graph, eType, &again));
        QCOMPARE(DescribeLayout(again), vecExpected);
        GraphLayoutOutput rebuilt;
        QVERIFY(RunLayout(graph, eType, &rebuilt));
        QCOMPARE(DescribeLayout(rebuilt), vecExpected);

        // 한 배치 객체를 다른 보기에 쓴 뒤 다시 써도, 앞의 출력을 지우고 같은 것을 낸다
        KnowledgeGraphLayout layout;
        GraphLayoutInput in;
        in.pGraph = &m_graph;
        GraphLayoutOutput reused;
        for (const EGraphLayoutType eOther : kAllTypes)
        {
            layout.SetStrategy(eOther);
            QVERIFY(layout.Execute(in, reused));
        }
        layout.SetStrategy(eType);
        QVERIFY(layout.Execute(in, reused));
        QCOMPARE(DescribeLayout(reused), vecExpected);
    }

    // 고른 전략이 배치한다. 그래프가 없으면 false 이고 출력을 비운다
    void strategySelection()
    {
        KnowledgeGraphLayout layout;
        QCOMPARE(layout.GetStrategy(), EGraphLayoutType::NETWORK);
        GraphLayoutInput in;
        in.pGraph = &m_graph;
        for (const EGraphLayoutType eType : kAllTypes)
        {
            layout.SetStrategy(eType);
            QCOMPARE(layout.GetStrategy(), eType);
            GraphLayoutOutput out;
            QVERIFY(layout.Execute(in, out));
            QCOMPARE(DescribeLayout(out), DescribeLayout(GetOutput(eType)));
        }

        GraphLayoutOutput out = GetOutput(EGraphLayoutType::NETWORK);
        QVERIFY(!layout.Execute(GraphLayoutInput(), out));
        QCOMPARE(DescribeLayout(out), DescribeLayout(GraphLayoutOutput()));
    }

    // 개념 · 선 · 문서로 만들 때: 같은 id 는 앞의 것, 점이 아닌 개념을 가리키는 선 · 연결은 뺀다, 점은 이름 열쇠 순
    void graphFromPartsSkipsUnknownConcepts()
    {
        KnowledgeDocument document;
        document.strId = QStringLiteral("doc");
        document.strTitle = DecodeUtf8("문서");
        KnowledgeCard card;
        card.vecConcepts = { MakeLink("a"), MakeLink("ghost"), MakeLink("a") };
        document.vecCards = { card };
        KnowledgeRelation relation;
        relation.fromConcept = MakeLink("a");
        relation.strKind = QStringLiteral("uses");
        relation.toConcept = MakeLink("b");
        document.vecRelations = { relation };
        KnowledgeTable table;
        table.vecConceptIds = QStringList { QStringLiteral("b"), QStringLiteral("ghost") };
        document.vecTables = { table };

        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts({ MakeConcept("b", "Beta"), MakeConcept("a", "alpha", "1999"), MakeConcept("a", "Again") },
                                                                    { MakeEdge("a", "uses", "b"), MakeEdge("a", "uses", "ghost"), MakeEdge("b", "alternative", "a") },
                                                                    { document });
        QCOMPARE(ListNodeIds(graph), (QStringList { "a", "b" }));
        QCOMPARE(graph.GetNodes()[0].strName, QString("alpha"));
        QCOMPARE(graph.GetNodes()[0].iYear, 1999);
        QCOMPARE(graph.GetNodes()[1].iYear, -1);
        QCOMPARE(graph.GetEdges().size(), 2);
        QCOMPARE(graph.GetEdges()[1].bDirected, false);
        QCOMPARE(graph.GetNodes()[0].iEdgeCount, 2);
        QCOMPARE(graph.GetComponents().size(), 1);
        QCOMPARE(graph.GetLinks().size(), 2);
        QCOMPARE(graph.GetLinks()[0].iNode, 0);
        QCOMPARE(graph.GetLinks()[0].iWeight, 3);   // 카드 2 · 관계 from 1
        QCOMPARE(graph.GetLinks()[1].iNode, 1);
        QCOMPARE(graph.GetLinks()[1].iWeight, 2);   // 관계 to 1 · 표 1
        QCOMPARE(graph.GetNodes()[0].iDocumentCount, 1);
        QCOMPARE(graph.GetNodes()[1].iDocumentCount, 1);
        QCOMPARE(graph.FindNode("ghost"), -1);
    }

    // 빈 그래프: 모든 배치가 돌고 아무것도 내지 않는다(경계는 빈 사각형)
    void emptyGraphGivesEmptyLayouts()
    {
        const KnowledgeGraph built = KnowledgeGraph::BuildFromParts({}, {}, {});
        const KnowledgeGraph defaulted;
        for (const KnowledgeGraph* pGraph : { &built, &defaulted })
        {
            for (const EGraphLayoutType eType : kAllTypes)
            {
                GraphLayoutOutput out;
                QVERIFY2(RunLayout(*pGraph, eType, &out), qUtf8Printable(GetTypeName(eType)));
                QCOMPARE(DescribeLayout(out), DescribeLayout(GraphLayoutOutput()));
                QVERIFY(out.bounds.isNull());
            }
        }
    }

    // 점 하나(선 · 문서 없음)
    void singleNodeGraph()
    {
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts({ MakeConcept("a", "A", "2020") }, {}, {});
        GraphLayoutOutput out;
        QVERIFY(RunLayout(graph, EGraphLayoutType::NETWORK, &out));
        QCOMPARE(ListNodeIds(out), QStringList { "a" });
        QVERIFY(out.vecEdges.isEmpty());
        QCOMPARE(out.bounds.size(), QSizeF(GraphLayoutSize::kNodeWidth, GraphLayoutSize::kNodeHeight));
        QVERIFY(ContainsWithTolerance(out.bounds, MakeBox(out.vecNodes[0])));

        for (const EGraphLayoutType eType : { EGraphLayoutType::LINEAGE, EGraphLayoutType::TREE, EGraphLayoutType::BIPARTITE })
        {
            QVERIFY(RunLayout(graph, eType, &out));
            QCOMPARE(DescribeLayout(out), DescribeLayout(GraphLayoutOutput()));
        }

        QVERIFY(RunLayout(graph, EGraphLayoutType::TIMELINE, &out));
        QCOMPARE(ListNodeIds(out), QStringList { "a" });
        QCOMPARE(out.vecTicks.size(), 1);
        QCOMPARE(out.vecTicks[0].strLabel, QString("2020"));
        QCOMPARE(ListLaneLabels(out), QStringList { DecodeUtf8("그 밖") });
        QVERIFY(out.vecUnplaced.isEmpty());
        QVERIFY(ContainsWithTolerance(out.bounds, MakeBox(out.vecNodes[0])));

        QVERIFY(RunLayout(graph, EGraphLayoutType::MATRIX, &out));
        QCOMPARE(out.vecMatrixOrder, QStringList { "a" });
        QVERIFY(out.vecCells.isEmpty());

        // 연도가 없거나 네 자리가 아니면 타임라인에 놓지 않는다. 연도 없는 점은 id 가 아니라 이름 열쇠 순이다
        const KnowledgeGraph undated = KnowledgeGraph::BuildFromParts({ MakeConcept("z", "Alpha"), MakeConcept("b", "B", "98"), MakeConcept("c", "C", "2020s") }, {}, {});
        QVERIFY(RunLayout(undated, EGraphLayoutType::TIMELINE, &out));
        QVERIFY(out.vecNodes.isEmpty());
        QVERIFY(out.vecTicks.isEmpty());
        QVERIFY(out.vecLanes.isEmpty());
        QCOMPARE(out.vecUnplaced, (QStringList { "z", "b", "c" }));
        QVERIFY(out.bounds.isNull());
    }

    // 덩어리: 큰 것부터, 같으면 id 가 아니라 이름 열쇠로 앞선 것부터
    void componentsOrderBySizeThenNameKey()
    {
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(
            { MakeConcept("a", "Zeta"), MakeConcept("b", "Alpha"), MakeConcept("c", "Mid"), MakeConcept("d", "Nu") },
            { MakeEdge("d", "uses", "c") }, {});
        QCOMPARE(ListNodeIds(graph), (QStringList { "b", "c", "d", "a" }));
        QStringList vecFirstIds;
        for (const QVector<int>& vecComponent : graph.GetComponents())
            vecFirstIds << graph.GetNodes()[vecComponent.first()].strId;
        QCOMPARE(vecFirstIds, (QStringList { "c", "b", "a" }));
        QCOMPARE(graph.GetNodes()[graph.FindNode("a")].iComponent, 2);

        // 관계망도 같은 순서로 왼쪽에서 오른쪽: 이어진 덩어리, 그다음 선 없는 점 b · a 한 줄
        GraphLayoutOutput out;
        QVERIFY(RunLayout(graph, EGraphLayoutType::NETWORK, &out));
        QVERIFY(qMax(GetCenter(out, "c").x(), GetCenter(out, "d").x()) < GetCenter(out, "b").x());
        QVERIFY(GetCenter(out, "b").x() < GetCenter(out, "a").x());
        QCOMPARE(GetCenter(out, "b").y(), GetCenter(out, "a").y());
    }

    // 계보도: 갈래는 큰 것부터(이름 열쇠보다 크기가 먼저), 열은 가장 긴 개선 길이(지름길 선이 있어도),
    // 아무것도 개선하지 않는 점이 여럿이면 연도가 이른 것이 가장 옛 점이다. 같은 열의 점은 이름 열쇠 순으로 아래로 쌓는다
    void lineageLanesAndColumns()
    {
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(
            { MakeConcept("a1", "Alpha 1"), MakeConcept("a2", "Alpha 2"),
              MakeConcept("z1", "Zeta 1"), MakeConcept("z2", "Zeta 2"), MakeConcept("z3", "Zeta 3"), MakeConcept("z4", "Zeta 4"),
              MakeConcept("m1", "Mu old", "1990"), MakeConcept("m2", "Mu new", "2000"), MakeConcept("m3", "Mu next", "2010") },
            { MakeEdge("a2", "improves", "a1"),
              MakeEdge("z2", "improves", "z1"), MakeEdge("z3", "improves", "z2"), MakeEdge("z4", "improves", "z3"), MakeEdge("z4", "improves", "z1"),
              MakeEdge("m3", "improves", "m1"), MakeEdge("m3", "improves", "m2") },
            {});
        GraphLayoutOutput out;
        QVERIFY(RunLayout(graph, EGraphLayoutType::LINEAGE, &out));
        QCOMPARE(ListLaneLabels(out), (QStringList { "Zeta 1", "Mu old", "Alpha 1" }));
        QCOMPARE(out.vecEdges.size(), 7);
        const double dColumnPitch = GraphLayoutSize::kNodeWidth + GraphLayoutSize::kLineageColumnGap;
        const QStringList vecChain { "z1", "z2", "z3", "z4" };
        for (int iColumn = 0; iColumn < vecChain.size(); ++iColumn)
            QVERIFY2(qAbs(GetCenter(out, vecChain[iColumn]).x() - GetCenter(out, "z1").x() - iColumn * dColumnPitch) < kTolerance,
                     qUtf8Printable(vecChain[iColumn]));
        QCOMPARE(GetCenter(out, "m1").x(), GetCenter(out, "m2").x());
        QVERIFY(GetCenter(out, "m2").y() < GetCenter(out, "m1").y());   // "Mu new" 가 이름 열쇠로 앞선다
        QVERIFY(GetCenter(out, "m3").x() > GetCenter(out, "m1").x());
        for (const LayoutEdge& edge : out.vecEdges)
            QVERIFY2(out.vecNodes[edge.iFrom].center.x() > out.vecNodes[edge.iTo].center.x(), qUtf8Printable(DescribeLayoutEdge(out, edge)));
        const QStringList vecOverlaps = FindOverlaps(out);
        QVERIFY2(vecOverlaps.isEmpty(), qUtf8Printable(vecOverlaps.join(QStringLiteral(", "))));
    }

    // 타임라인 갈래: 분류 나무의 뿌리가 개선 묶음보다 먼저, 둘 다 없으면 "그 밖". 점이 같으면 이름 열쇠 순
    void timelineLanePriority()
    {
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(
            { MakeConcept("r", "Root"), MakeConcept("x", "X", "2001"), MakeConcept("y", "Y", "2000"), MakeConcept("w", "W", "2002"),
              MakeConcept("o", "Other", "2003") },
            { MakeEdge("x", "isA", "r"), MakeEdge("x", "improves", "y"), MakeEdge("w", "improves", "y") },
            {});
        GraphLayoutOutput out;
        QVERIFY(RunLayout(graph, EGraphLayoutType::TIMELINE, &out));
        QCOMPARE(ListLaneLabels(out), (QStringList { "Y", "Root", DecodeUtf8("그 밖") }));
        QCOMPARE(ListNodesInLane(out, out.vecLanes[0]), (QStringList { "y", "w" }));
        QCOMPARE(ListNodesInLane(out, out.vecLanes[1]), QStringList { "x" });
        QCOMPARE(ListNodesInLane(out, out.vecLanes[2]), QStringList { "o" });
        QCOMPARE(out.vecUnplaced, QStringList { "r" });
        QCOMPARE(out.vecEdges.size(), 2);   // 놓인 두 점 사이의 개선 선(종류 선은 끝 r 에 연도가 없다)
    }

    // 분류 트리: 나무는 뿌리 이름보다 크기가 먼저다(큰 나무가 왼쪽)
    void treesOrderBySizeThenRootName()
    {
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(
            { MakeConcept("a", "A"), MakeConcept("a1", "A1"), MakeConcept("b", "B"), MakeConcept("b1", "B1"), MakeConcept("b2", "B2"), MakeConcept("b3", "B3") },
            { MakeEdge("a1", "isA", "a"), MakeEdge("b1", "isA", "b"), MakeEdge("b2", "isA", "b"), MakeEdge("b3", "isA", "b") },
            {});
        GraphLayoutOutput out;
        QVERIFY(RunLayout(graph, EGraphLayoutType::TREE, &out));
        QCOMPARE(out.vecNodes.size(), 6);
        QVERIFY(GetCenter(out, "b3").x() < GetCenter(out, "a1").x());
        QCOMPARE(GetCenter(out, "b").x(), GetCenter(out, "b2").x());   // 첫 자식 b1 과 끝 자식 b3 의 가운데
    }

    // 관계 행렬: 한 칸의 종류는 선 순서가 아니라 어휘 순, 어휘에 없는 종류는 뒤. 제 자신으로 가는 선은 칸이 없다
    void matrixKindsFollowVocabulary()
    {
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(
            { MakeConcept("x", "X"), MakeConcept("y", "Y") },
            { MakeEdge("x", "isA", "y"), MakeEdge("x", "zzz", "y"), MakeEdge("x", "improves", "y"), MakeEdge("x", "aaa", "y"), MakeEdge("x", "uses", "x") },
            {});
        GraphLayoutOutput out;
        QVERIFY(RunLayout(graph, EGraphLayoutType::MATRIX, &out));
        QCOMPARE(out.vecMatrixOrder, (QStringList { "x", "y" }));
        QCOMPARE(out.vecCells.size(), 1);
        QCOMPARE(out.vecCells[0].iRow, 0);
        QCOMPARE(out.vecCells[0].iColumn, 1);
        QCOMPARE(out.vecCells[0].vecKinds, (QStringList { "improves", "isA", "aaa", "zzz" }));
        QCOMPARE(out.vecEdges.size(), 5);
    }

    // 타임라인 갈래: "그 밖" 은 늘 마지막이다. 점이 다른 갈래보다 많아도, 점 수가 같고 이름이 앞서도 그렇다
    void timelineOtherLaneIsLast()
    {
        // 그 밖 3점 > Y 2점 > 하나 1점이어도 그 밖이 마지막
        const KnowledgeGraph crowded = KnowledgeGraph::BuildFromParts(
            { MakeConcept("y", "Y", "2000"), MakeConcept("w", "W", "2001"),
              MakeConcept("h", DecodeUtf8("하나"), "2002"), MakeConcept("h2", DecodeUtf8("하나 둘")),
              MakeConcept("a", "Alpha", "2003"), MakeConcept("b", "Beta", "2004"), MakeConcept("c", "Gamma", "2005") },
            { MakeEdge("w", "improves", "y"), MakeEdge("h2", "isA", "h") },
            {});
        GraphLayoutOutput out;
        QVERIFY(RunLayout(crowded, EGraphLayoutType::TIMELINE, &out));
        QCOMPARE(ListLaneLabels(out), (QStringList { "Y", DecodeUtf8("하나"), DecodeUtf8("그 밖") }));
        QCOMPARE(ListNodesInLane(out, out.vecLanes[0]), (QStringList { "y", "w" }));
        QCOMPARE(ListNodesInLane(out, out.vecLanes[1]), QStringList { "h" });
        QCOMPARE(ListNodesInLane(out, out.vecLanes[2]), (QStringList { "a", "b", "c" }));
        QCOMPARE(out.vecUnplaced, QStringList { "h2" });

        // 점 수가 같으면 이름 순이지만, 이름 열쇠로 "그 밖" 이 앞서는 "하나" 갈래보다도 그 밖이 뒤다
        const KnowledgeGraph tied = KnowledgeGraph::BuildFromParts(
            { MakeConcept("h", DecodeUtf8("하나"), "2002"), MakeConcept("h2", DecodeUtf8("하나 둘")), MakeConcept("a", "Alpha", "2003") },
            { MakeEdge("h2", "isA", "h") },
            {});
        QVERIFY(RunLayout(tied, EGraphLayoutType::TIMELINE, &out));
        QCOMPARE(ListLaneLabels(out), (QStringList { DecodeUtf8("하나"), DecodeUtf8("그 밖") }));
        QCOMPARE(ListNodesInLane(out, out.vecLanes[1]), QStringList { "a" });
    }

    // 빽빽한 그래프(잎이 많은 별 + 서로 다 이어진 점들)도 관계망에서 점 상자가 겹치지 않는다
    void denseNetworkHasNoOverlaps()
    {
        QVector<KnowledgeConcept> vecConcepts { MakeConcept("hub", "Hub") };
        QVector<KnowledgeEdge> vecEdges;
        for (int iLeaf = 0; iLeaf < kDenseLeafCount; ++iLeaf)
        {
            const QString strId = QStringLiteral("leaf-%1").arg(iLeaf, kLeafNumberWidth, kDecimalBase, QLatin1Char('0'));
            vecConcepts << MakeConcept(strId, strId);
            vecEdges << MakeEdge(strId, "isA", "hub");
        }
        for (int iFirst = 0; iFirst < kDenseCliqueSize; ++iFirst)
        {
            vecConcepts << MakeConcept(QStringLiteral("k%1").arg(iFirst), QStringLiteral("K%1").arg(iFirst));
            for (int iSecond = 0; iSecond < iFirst; ++iSecond)
                vecEdges << MakeEdge(QStringLiteral("k%1").arg(iFirst), "uses", QStringLiteral("k%1").arg(iSecond));
        }
        vecEdges << MakeEdge("k0", "uses", "hub");
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(vecConcepts, vecEdges, {});
        GraphLayoutOutput out;
        QVERIFY(RunLayout(graph, EGraphLayoutType::NETWORK, &out));
        QCOMPARE(out.vecNodes.size(), 48);   // 중심 1 · 잎 40 · 서로 다 이어진 점 7
        const QStringList vecOverlaps = FindOverlaps(out);
        QVERIFY2(vecOverlaps.isEmpty(), qUtf8Printable(vecOverlaps.join(QStringLiteral(", "))));
        const QStringList vecNonFinite = FindNonFinite(out);
        QVERIFY2(vecNonFinite.isEmpty(), qUtf8Printable(vecNonFinite.join(QStringLiteral(", "))));
        for (const LayoutNode& node : out.vecNodes)
            QVERIFY2(ContainsWithTolerance(out.bounds, MakeBox(node)), qUtf8Printable(DescribePoint(node.strId, node.center)));
    }

    // isA 고리(뿌리 없음)와 improves 고리: 멈추고, 점마다 한 번 놓고, 선을 모두 그린다
    void cyclesDoNotHang()
    {
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(
            { MakeConcept("a", "A", "2001"), MakeConcept("b", "B", "2002"), MakeConcept("c", "C", "2003"), MakeConcept("d", "D", "2004"),
              MakeConcept("e", "E", "2005") },
            { MakeEdge("a", "isA", "b"), MakeEdge("b", "isA", "c"), MakeEdge("c", "isA", "a"), MakeEdge("d", "isA", "a"),
              MakeEdge("d", "improves", "e"), MakeEdge("e", "improves", "d"), MakeEdge("e", "isA", "e") },
            {});
        for (const EGraphLayoutType eType : kAllTypes)
        {
            GraphLayoutOutput out;
            QVERIFY2(RunLayout(graph, eType, &out), qUtf8Printable(GetTypeName(eType)));
            const QStringList vecNonFinite = FindNonFinite(out);
            QVERIFY2(vecNonFinite.isEmpty(), qUtf8Printable(GetTypeName(eType) + QLatin1Char(' ') + vecNonFinite.join(QStringLiteral(", "))));
            const QStringList vecIds = ListNodeIds(out);
            QCOMPARE(QSet<QString>(vecIds.cbegin(), vecIds.cend()).size(), vecIds.size());
        }

        // 분류 트리: 뿌리 없는 고리 a → b → c → a 는 고리 안에서 이름 열쇠로 앞선 a 를 뿌리로 삼는다. e 의 제 자신 선도 그린다
        GraphLayoutOutput tree;
        QVERIFY(RunLayout(graph, EGraphLayoutType::TREE, &tree));
        QCOMPARE(tree.vecNodes.size(), 5);
        QCOMPARE(tree.vecEdges.size(), 5);
        for (const LayoutNode& node : tree.vecNodes)
            if (node.strId != QLatin1String("a") && node.strId != QLatin1String("e"))
                QVERIFY2(node.center.y() > GetCenter(tree, "a").y(), qUtf8Printable(DescribePoint(node.strId, node.center)));
        const QStringList vecTreeOverlaps = FindOverlaps(tree);
        QVERIFY2(vecTreeOverlaps.isEmpty(), qUtf8Printable(vecTreeOverlaps.join(QStringLiteral(", "))));

        // 계보도: d ⇄ e 고리도 갈래 하나
        GraphLayoutOutput lineage;
        QVERIFY(RunLayout(graph, EGraphLayoutType::LINEAGE, &lineage));
        QCOMPARE(lineage.vecNodes.size(), 2);
        QCOMPARE(lineage.vecEdges.size(), 2);
        QCOMPARE(lineage.vecLanes.size(), 1);

        // 타임라인: 고리 a · b · c · d 는 뿌리 a 의 갈래. e 는 isA 가 제 자신뿐이라 제 갈래다
        GraphLayoutOutput timeline;
        QVERIFY(RunLayout(graph, EGraphLayoutType::TIMELINE, &timeline));
        QCOMPARE(timeline.vecNodes.size(), 5);
        QCOMPARE(ListLaneLabels(timeline), (QStringList { "A", "E" }));
        QCOMPARE(ListNodesInLane(timeline, timeline.vecLanes[0]), (QStringList { "a", "b", "c", "d" }));
    }

    // 부모가 둘인 점: 이름 열쇠로 앞선 부모 아래에 한 번만 두고, 다른 부모로 가는 선도 그린다
    void nodeWithTwoParents()
    {
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(
            { MakeConcept("p2", "P2", "2000"), MakeConcept("p1", "P1", "2000"), MakeConcept("x", "X", "2010"), MakeConcept("y", "Y", "2010") },
            { MakeEdge("x", "isA", "p2"), MakeEdge("x", "isA", "p1"), MakeEdge("y", "isA", "p2") },
            {});
        GraphLayoutOutput tree;
        QVERIFY(RunLayout(graph, EGraphLayoutType::TREE, &tree));
        QCOMPARE(ListNodeIds(tree).size(), 4);
        QCOMPARE(tree.vecEdges.size(), 3);
        const double dLevelPitch = GraphLayoutSize::kNodeHeight + GraphLayoutSize::kTreeLevelGap;
        QCOMPARE(GetCenter(tree, "x").x(), GetCenter(tree, "p1").x());
        QVERIFY(qAbs(GetCenter(tree, "x").y() - GetCenter(tree, "p1").y() - dLevelPitch) < kTolerance);
        QCOMPARE(GetCenter(tree, "y").x(), GetCenter(tree, "p2").x());
        QVERIFY(GetCenter(tree, "p1").x() < GetCenter(tree, "p2").x());   // 크기가 같으면 뿌리 이름 열쇠 순
        QStringList vecEdges;
        for (const LayoutEdge& edge : tree.vecEdges)
            vecEdges << DescribeLayoutEdge(tree, edge);
        QCOMPARE(vecEdges, (QStringList { "x isA p2", "x isA p1", "y isA p2" }));
        const QStringList vecOverlaps = FindOverlaps(tree);
        QVERIFY2(vecOverlaps.isEmpty(), qUtf8Printable(vecOverlaps.join(QStringLiteral(", "))));

        // 타임라인의 갈래도 앞선 부모를 따라간다: x 는 P1 갈래
        GraphLayoutOutput timeline;
        QVERIFY(RunLayout(graph, EGraphLayoutType::TIMELINE, &timeline));
        QCOMPARE(ListLaneLabels(timeline), (QStringList { "P1", "P2" }));
        QCOMPARE(ListNodesInLane(timeline, timeline.vecLanes[0]), (QStringList { "p1", "x" }));
        QCOMPARE(ListNodesInLane(timeline, timeline.vecLanes[1]), (QStringList { "p2", "y" }));
    }

private:
    const GraphLayoutOutput& GetOutput(EGraphLayoutType type) const
    {
        return m_vecOutputs[static_cast<int>(type)];
    }

    // 문서 제목 · 개념 id 의 연결 무게(없으면 0)
    int FindLinkWeight(const QString& title, const QString& conceptId) const
    {
        for (const KnowledgeGraphLink& link : m_graph.GetLinks())
            if (m_graph.GetDocuments()[link.iDocument].strTitle == title && m_graph.GetNodes()[link.iNode].strId == conceptId)
                return link.iWeight;
        return 0;
    }

private:
    KnowledgeIndex m_index;                     // showcase wiki 를 읽은 색인
    KnowledgeGraph m_graph;                     // 그 색인으로 만든 그래프
    QVector<GraphLayoutOutput> m_vecOutputs;    // EGraphLayoutType 값 → 배치 결과
};

std::unique_ptr<QObject> CreateKnowledgeGraphLayoutTest() { return std::make_unique<KnowledgeGraphLayoutTest>(); }

#include "KnowledgeGraphLayoutTest.moc"
