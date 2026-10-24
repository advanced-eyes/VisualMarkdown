#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/Algorithm/GraphLayout/KnowledgeGraphLayout.h"
#include "VisualMarkdown/KnowledgeIndex.h"
#include "VisualMarkdown/KnowledgeTypes.h"
#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <QHash>
#include <QLineF>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QTest>
#include <QtMath>

#include <algorithm>
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
constexpr int kResearchComponent = 0;   // 연구 덩어리(19점, 검출기가 든 것)
constexpr int kPlanningComponent = 1;   // 기획서 덩어리(8점, 결제 서비스가 든 것)
const QVector<NameCount> kEdgeCounts { { "1단계-검출기", 6 }, { "검출기", 3 }, { "faster-r-cnn", 6 }, { "coco", 0 } };
const QStringList kDocumentTitles { kPaymentTitle, kFocalLossTitle, kTwoStageTitle, kYolov3Title, kYolo9000Title, kYoloTitle };   // learned 늦은 것부터
constexpr const char* kHiddenConcept = "dpm";          // 아무도 가리키지 않아 숨는다(연도 2008)
constexpr const char* kMissingConcept = "darknet-53";  // YOLOv3 문서가 가리키지만 개념 파일이 없다

// 관계망 배치 범위. 고치기 전(049ba54: 덩어리를 큰 것부터 한 줄로 늘어놓고 선 없는 점을 끝에 한 줄로 두던 때) 잰 값은
// 2140.056878 × 981.601564 = 넓이 2100683.18(가로:세로 2.18)이었다. 덩어리를 세로 열로 쌓아 넓이를 그 75% 이하,
// 가로를 세로의 1.7배 이하로 줄인다(design/Views.md §5 관계망 줄).
constexpr double kNetworkAreaBefore = 2100683.18;
constexpr double kNetworkMaxAreaShare = 0.75;
constexpr double kNetworkMaxAspect = 1.7;

// 문서 ↔ 개념: 문서마다 연결 수(개념 파일이 없는 Darknet-53 은 뺀다)와 무게 합(카드 링크 + 관계 끝 + 표 concepts)
struct DocumentLinks
{
    const char* pTitle;
    int iLinks;
    int iWeightSum;
};
const QVector<DocumentLinks> kDocumentLinks {
    { kYoloTitle, 5, 13 }, { kYolo9000Title, 7, 20 }, { kYolov3Title, 5, 17 },
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
    { kYolov3Title, "yolov3", 8 },         // 카드 2 · 관계 끝 3(빠진 Darknet-53 관계의 from 도 센다) · 표 3
    { kYolov3Title, "retinanet", 3 },      // 카드 1 · 관계 끝 1 · 표 1(검출기별 COCO 정확도)
    { kYolov3Title, "yolov2", 3 },         // 관계 끝 2 · 표 1(검출기별 COCO 정확도)
    { kYolov3Title, "coco", 2 },           // 표 2
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
    EGraphLayoutType::EGO,
};

// 이웃(design/MoreViews.md §6 · §7.3): 기본 초점은 Faster R-CNN(선 6 · 문서 3. 선 6 인 1단계 검출기 · YOLOv2 보다 문서가 많다).
// 깊이 2 는 점 12(초점 1 · 1 걸음 6 · 2 걸음 5) · 선 18, 깊이 1 은 점 7 · 선 9
constexpr const char* kEgoDefaultFocus = "faster-r-cnn";
constexpr int kEgoNodes = 12;
constexpr int kEgoEdges = 18;
constexpr int kEgoDepthOneNodes = 7;
constexpr int kEgoDepthOneEdges = 9;
constexpr int kEgoDefaultDepth = 2;
// 고리의 점(출력 순서)과 쐐기 가운데 각(도. 12시 = −90, 시계 방향으로 는다). 1 걸음 점은 잎 수도 적는다
struct ExpectedEgoNode
{
    const char* pId;
    double dAngle;
    int iLeaves;
};
const QVector<ExpectedEgoNode> kEgoRingOne {
    { "fast-r-cnn", -90, 2 }, { "rpn", -22.5, 1 }, { "2단계-검출기", 22.5, 1 }, { "객체-검출", 67.5, 1 }, { "detr", 112.5, 1 }, { "yolo", 180, 2 },
};
const QVector<ExpectedEgoNode> kEgoRingTwo {
    { "r-cnn", -112.5, 1 }, { "roi-풀링", -67.5, 1 }, { "검출기", 22.5, 1 }, { "1단계-검출기", 157.5, 1 }, { "yolov2", 202.5, 1 },
};
constexpr double kEgoDegreesPerLeaf = 45;   // 잎 8 이 360° 를 나눈다
constexpr double kEgoDepthOneStep = 60;     // 깊이 1: 1 걸음 6점이 모두 잎이라 60° 씩
struct ExpectedParent
{
    const char* pChild;
    const char* pParent;
};
const QVector<ExpectedParent> kEgoParents {
    { "r-cnn", "fast-r-cnn" }, { "roi-풀링", "fast-r-cnn" }, { "검출기", "2단계-검출기" }, { "1단계-검출기", "yolo" }, { "yolov2", "yolo" },
};
// 넣는 선 18(from 종류 to, 방향 없는 선은 id 글자 순). 앞 9개가 깊이 1 의 선(초점의 선 6 · 1 걸음 점끼리 3)이다
const QStringList kEgoEdgeList {
    "faster-r-cnn improves fast-r-cnn", "faster-r-cnn uses rpn", "faster-r-cnn isA 2단계-검출기", "faster-r-cnn solves 객체-검출",
    "detr alternative faster-r-cnn", "faster-r-cnn alternative yolo",
    "fast-r-cnn isA 2단계-검출기", "detr solves 객체-검출", "yolo solves 객체-검출",
    "fast-r-cnn improves r-cnn", "fast-r-cnn uses roi-풀링", "r-cnn isA 2단계-검출기", "2단계-검출기 isA 검출기", "detr isA 검출기",
    "yolo isA 1단계-검출기", "yolov2 improves yolo",
    "yolov2 isA 1단계-검출기", "1단계-검출기 isA 검출기",
};
// 나무 선 11(초점의 선 6 · 부모와 자식을 잇는 선 5)과 둘레 선 7(1 걸음 점끼리 3 · 부모가 아닌 앞 고리 점과의 선 2 · 2 걸음 점끼리 2)
const QStringList kEgoTreeEdgeList {
    "faster-r-cnn improves fast-r-cnn", "faster-r-cnn uses rpn", "faster-r-cnn isA 2단계-검출기", "faster-r-cnn solves 객체-검출",
    "detr alternative faster-r-cnn", "faster-r-cnn alternative yolo",
    "fast-r-cnn improves r-cnn", "fast-r-cnn uses roi-풀링", "2단계-검출기 isA 검출기", "yolo isA 1단계-검출기", "yolov2 improves yolo",
};
const QStringList kEgoContextEdgeList {
    "fast-r-cnn isA 2단계-검출기", "detr solves 객체-검출", "yolo solves 객체-검출",
    "r-cnn isA 2단계-검출기", "detr isA 검출기",
    "yolov2 isA 1단계-검출기", "1단계-검출기 isA 검출기",
};
// 허브 초점(1단계 검출기, 선 6) 깊이 2: 1 걸음 6 · 2 걸음 7, 나무 선 13 · 둘레 선 8.
// 2 걸음의 부모: 2단계 검출기 · DETR ← 검출기, Focal Loss ← RetinaNet, Faster R-CNN · 객체 검출 ← YOLO, Darknet-19 · 배치 정규화 ← YOLOv2
constexpr const char* kHubFocus = "1단계-검출기";
const QStringList kHubTreeEdgeList {
    "1단계-검출기 isA 검출기", "retinanet isA 1단계-검출기", "ssd isA 1단계-검출기", "yolo isA 1단계-검출기", "yolov2 isA 1단계-검출기",
    "yolov3 isA 1단계-검출기",
    "2단계-검출기 isA 검출기", "detr isA 검출기", "retinanet uses focal-loss", "faster-r-cnn alternative yolo", "yolo solves 객체-검출",
    "yolov2 uses darknet-19", "yolov2 uses 배치-정규화",
};
const QStringList kHubContextEdgeList {
    "yolov2 improves yolo", "ssd alternative yolov2", "yolov3 improves yolov2", "retinanet alternative yolov3",
    "faster-r-cnn isA 2단계-검출기", "faster-r-cnn solves 객체-검출", "detr solves 객체-검출", "detr alternative faster-r-cnn",
};
constexpr const char* kEdgelessConcept = "coco";   // 선 없는 점: 초점이면 점 1 · 선 0
constexpr double kAngleTolerance = 1e-6;           // 각 견줌의 여유(도)
constexpr int kEgoMaxDepth = 3;                    // 모든 점을 초점으로 돌려 보는 가장 깊은 깊이
constexpr double kFullTurn = 360;
constexpr double kHalfTurn = 180;
constexpr double kTwelveOClock = -90;              // 화면 좌표(y 가 아래)에서 12시 각(도). 시계 방향으로 는다

constexpr double kTolerance = 1e-6;   // 경계 · 자리 견줌의 여유(배치 단위)
constexpr int kRoundTripDigits = 17;  // FormatNumber 의 자리 수

// 빽빽한 그래프(denseNetworkHasNoOverlaps): 중심 하나에 isA 로 닿는 잎들과 서로 다 이어진 점들
constexpr int kDenseLeafCount = 40;
constexpr int kDenseCliqueSize = 7;
constexpr int kLeafNumberWidth = 2;   // 잎 id 의 번호 자리 수(leaf-00)
constexpr int kDecimalBase = 10;

// 열 쌓기 합성 그래프: 별(중심 하나에 잎 6)이 가장 큰 덩어리, 그 뒤에 두 점 덩어리 7개 또는 두 점 덩어리 하나와 선 없는 점 14개
constexpr int kStarLeafCount = 6;
constexpr int kSmallPairCount = 7;
constexpr int kOverflowSingleCount = 14;
// 열 높이 상한 합성 그래프: 서로 다 이어진 다섯 점(A1 …)과 점 수가 같은 사슬(B1 – … – B5, 더 높다), 그 뒤에 두 점 덩어리 6개
constexpr int kLimitGroupSize = 5;
constexpr int kLimitPairCount = 6;

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
    case EGraphLayoutType::EGO: return QStringLiteral("EGO");
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

// 초점 id 와 깊이를 준 배치(이웃 보기가 보는 칸. 다른 전략은 보지 않는다)
bool RunLayoutWithFocus(const KnowledgeGraph& graph, EGraphLayoutType type, const QString& focus, int depth, GraphLayoutOutput* out)
{
    KnowledgeGraphLayout layout;
    layout.SetStrategy(type);
    GraphLayoutInput in;
    in.pGraph = &graph;
    in.strFocusId = focus;
    in.iDepth = depth;
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
                                  FormatNumber(node.center.y()), FormatBool(node.bDocument), FormatBool(node.bFocus) }.join(QLatin1Char('|'));
    for (const LayoutEdge& edge : out.vecEdges)
        vecLines << QStringList { QStringLiteral("edge"), QString::number(edge.iFrom), QString::number(edge.iTo), edge.strKind,
                                  QString::number(static_cast<int>(edge.eStatus)), FormatBool(edge.bDirected),
                                  QString::number(edge.iGraphEdge), QString::number(edge.iWeight), FormatBool(edge.bContext) }.join(QLatin1Char('|'));
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

QString DescribeBox(const QRectF& box)
{
    return QStringLiteral("(%1, %2 %3 x %4)").arg(box.x()).arg(box.y()).arg(box.width()).arg(box.height());
}

bool IsNear(double first, double second)
{
    return qAbs(first - second) < kTolerance;
}

// 관계망: 덩어리마다 점 상자를 모두 담는 사각형(GetComponents 순). 관계망 출력의 점 자리는 그래프 점 번호와 같다
QVector<QRectF> ListComponentBoxes(const KnowledgeGraph& graph, const GraphLayoutOutput& out)
{
    QVector<QRectF> vecBoxes;
    for (const QVector<int>& vecComponent : graph.GetComponents())
    {
        QRectF rectBox;
        for (const int iNode : vecComponent)
            rectBox = rectBox.isNull() ? MakeBox(out.vecNodes[iNode]) : rectBox.united(MakeBox(out.vecNodes[iNode]));
        vecBoxes << rectBox;
    }
    return vecBoxes;
}

// 관계망 열 높이 상한 H: 이어진 덩어리(점 2개 이상) 상자 높이의 가장 큰 값. 이어진 덩어리가 없으면 0
double GetColumnLimit(const KnowledgeGraph& graph, const GraphLayoutOutput& out)
{
    const QVector<QRectF> vecBoxes = ListComponentBoxes(graph, out);
    double dLimit = 0;
    for (int iComponent = 0; iComponent < vecBoxes.size(); ++iComponent)
        if (graph.GetComponents()[iComponent].size() > 1)
            dLimit = qMax(dLimit, vecBoxes[iComponent].height());
    return dLimit;
}

// 관계망: 이어진 덩어리가 세로 열로 쌓였는지 본다(design/Views.md §5). 맞으면 빈 글, 아니면 처음 어긋난 덩어리.
// 덩어리 순서대로 첫 덩어리는 배치 범위의 왼쪽 위. 다음 덩어리는 지금 열 아래(틈 kComponentGap)에 넣어 열 높이가 H 를
// 넘지 않으면 거기(상자 왼쪽을 열 왼쪽에 맞춘다), 넘으면 오른쪽 새 열(앞 상자들의 오른쪽 끝 + kComponentGap)의 맨 위다
QString CheckComponentColumns(const KnowledgeGraph& graph, const GraphLayoutOutput& out)
{
    const QVector<QVector<int>>& vecComponents = graph.GetComponents();
    const QVector<QRectF> vecBoxes = ListComponentBoxes(graph, out);
    const double dLimit = GetColumnLimit(graph, out);
    const double dTop = out.bounds.top();
    bool bFirst = true;
    double dColumnLeft = 0;
    double dColumnBottom = 0;
    double dRightEnd = 0;   // 놓은 상자들의 오른쪽 끝
    for (int iComponent = 0; iComponent < vecComponents.size(); ++iComponent)
    {
        if (vecComponents[iComponent].size() < 2)
            continue;
        const QRectF& rectBox = vecBoxes[iComponent];
        const QString strWhere = QStringLiteral("component %1 %2").arg(iComponent).arg(DescribeBox(rectBox));
        if (bFirst)
        {
            if (!IsNear(rectBox.left(), out.bounds.left()) || !IsNear(rectBox.top(), dTop))
                return strWhere + QStringLiteral(" is not at the top left of ") + DescribeBox(out.bounds);
            dColumnLeft = rectBox.left();
            dRightEnd = rectBox.right();
            bFirst = false;
        }
        else if (dColumnBottom + GraphLayoutSize::kComponentGap + rectBox.height() <= dTop + dLimit + kTolerance)
        {
            if (!IsNear(rectBox.left(), dColumnLeft) || !IsNear(rectBox.top(), dColumnBottom + GraphLayoutSize::kComponentGap))
                return strWhere + QStringLiteral(" fits below its column (bottom %1) but is not there").arg(dColumnBottom);
        }
        else
        {
            if (!IsNear(rectBox.left(), dRightEnd + GraphLayoutSize::kComponentGap) || !IsNear(rectBox.top(), dTop))
                return strWhere + QStringLiteral(" does not fit below its column but is not at the top of a new column (right end %1)").arg(dRightEnd);
            dColumnLeft = rectBox.left();
        }
        if (rectBox.bottom() > dTop + dLimit + kTolerance)
            return strWhere + QStringLiteral(" goes below the column limit %1").arg(dLimit);
        dColumnBottom = rectBox.bottom();
        dRightEnd = qMax(dRightEnd, rectBox.right());
    }
    return QString();
}

// 관계망: kComponentGap 보다 가까운 덩어리 상자 쌍(없으면 빈 목록). 선 없는 점끼리는 점 틈만큼 떼므로 보지 않는다
QStringList FindCloseComponents(const KnowledgeGraph& graph, const GraphLayoutOutput& out)
{
    const QVector<QVector<int>>& vecComponents = graph.GetComponents();
    const QVector<QRectF> vecBoxes = ListComponentBoxes(graph, out);
    QStringList vecPairs;
    for (int iFirst = 0; iFirst < vecBoxes.size(); ++iFirst)
    {
        for (int iSecond = iFirst + 1; iSecond < vecBoxes.size(); ++iSecond)
        {
            if (vecComponents[iFirst].size() == 1 && vecComponents[iSecond].size() == 1)
                continue;
            const QRectF& rectFirst = vecBoxes[iFirst];
            const QRectF& rectSecond = vecBoxes[iSecond];
            const double dGapX = qMax(rectSecond.left() - rectFirst.right(), rectFirst.left() - rectSecond.right());
            const double dGapY = qMax(rectSecond.top() - rectFirst.bottom(), rectFirst.top() - rectSecond.bottom());
            if (qMax(dGapX, dGapY) < GraphLayoutSize::kComponentGap - kTolerance)
                vecPairs << QStringLiteral("%1 %2 / %3 %4").arg(iFirst).arg(DescribeBox(rectFirst)).arg(iSecond).arg(DescribeBox(rectSecond));
        }
    }
    return vecPairs;
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

// 열 쌓기 합성 그래프의 별: 중심 hub(A Hub)에 잎 leaf1 … 이 isA 로 닿는다. 이름이 A 로 시작해 다른 점보다 이름 열쇠가 앞선다
void AddStar(QVector<KnowledgeConcept>* concepts, QVector<KnowledgeEdge>* edges)
{
    *concepts << MakeConcept("hub", "A Hub");
    for (int iLeaf = 1; iLeaf <= kStarLeafCount; ++iLeaf)
    {
        const QString strId = QStringLiteral("leaf%1").arg(iLeaf);
        *concepts << MakeConcept(strId, QStringLiteral("A Leaf %1").arg(iLeaf));
        *edges << MakeEdge(strId, "isA", "hub");
    }
}

// 두 점 덩어리: p<n>a uses p<n>b(이름 P<n> A · P<n> B)
void AddPair(int number, QVector<KnowledgeConcept>* concepts, QVector<KnowledgeEdge>* edges)
{
    const QString strFirst = QStringLiteral("p%1a").arg(number);
    const QString strSecond = QStringLiteral("p%1b").arg(number);
    *concepts << MakeConcept(strFirst, QStringLiteral("P%1 A").arg(number)) << MakeConcept(strSecond, QStringLiteral("P%1 B").arg(number));
    *edges << MakeEdge(strFirst, "uses", strSecond);
}

// 선 없는 점: s01 …(이름 S01 …)
void AddSingles(int count, QVector<KnowledgeConcept>* concepts)
{
    for (int iSingle = 1; iSingle <= count; ++iSingle)
    {
        const QString strId = QStringLiteral("s%1").arg(iSingle, kLeafNumberWidth, kDecimalBase, QLatin1Char('0'));
        *concepts << MakeConcept(strId, strId.toUpper());
    }
}

// 관계망에서 이어진 덩어리마다, 그 덩어리의 개념 · 선만으로 따로 배치한 것과 점끼리의 상대 자리가 같은지 본다. 같으면 빈 글
QString CompareComponentShapes(const KnowledgeGraph& graph, const QVector<KnowledgeConcept>& concepts, const QVector<KnowledgeEdge>& edges)
{
    GraphLayoutOutput out;
    if (!RunLayout(graph, EGraphLayoutType::NETWORK, &out))
        return QStringLiteral("layout failed");
    int iCompared = 0;
    for (const QVector<int>& vecComponent : graph.GetComponents())
    {
        if (vecComponent.size() < 2)
            continue;
        QSet<QString> setIds;
        for (const int iNode : vecComponent)
            setIds.insert(graph.GetNodes()[iNode].strId);
        QVector<KnowledgeConcept> vecPartConcepts;
        for (const KnowledgeConcept& concept : concepts)
            if (setIds.contains(concept.strId))
                vecPartConcepts << concept;
        QVector<KnowledgeEdge> vecPartEdges;
        for (const KnowledgeEdge& edge : edges)
            if (setIds.contains(edge.strFromId) && setIds.contains(edge.strToId))
                vecPartEdges << edge;
        const QString strAnchor = graph.GetNodes()[vecComponent.first()].strId;
        const KnowledgeGraph part = KnowledgeGraph::BuildFromParts(vecPartConcepts, vecPartEdges, {});
        GraphLayoutOutput partOut;
        if (part.GetNodes().size() != vecComponent.size() || part.GetComponents().size() != 1 || !RunLayout(part, EGraphLayoutType::NETWORK, &partOut))
            return QStringLiteral("could not lay out the component of ") + strAnchor + QStringLiteral(" alone");
        const QPointF ptAnchor = out.vecNodes[vecComponent.first()].center;
        const QPointF ptPartAnchor = GetCenter(partOut, strAnchor);
        for (const int iNode : vecComponent)
        {
            const QString strId = graph.GetNodes()[iNode].strId;
            const QPointF ptOffset = out.vecNodes[iNode].center - ptAnchor;
            const QPointF ptPartOffset = GetCenter(partOut, strId) - ptPartAnchor;
            if (!IsNear(ptOffset.x(), ptPartOffset.x()) || !IsNear(ptOffset.y(), ptPartOffset.y()))
                return DescribePoint(strId + QStringLiteral(" - ") + strAnchor, ptOffset) + QStringLiteral(" / alone ") + DescribePoint(strId, ptPartOffset);
        }
        ++iCompared;
    }
    return iCompared > 0 ? QString() : QStringLiteral("no component with an edge");
}

// 이웃 배치: 가운데(초점)에서 본 점의 각(도, −180–180). 화면 좌표(y 가 아래)라 12시가 −90 이고 시계 방향으로 는다
double GetAngleDegrees(const QPointF& center, const QPointF& point)
{
    return qRadiansToDegrees(std::atan2(point.y() - center.y(), point.x() - center.x()));
}

// 두 각의 차(도)를 −180–180 으로
double GetAngleDifference(double first, double second)
{
    double dDifference = std::fmod(first - second, kFullTurn);
    if (dDifference > kHalfTurn)
        dDifference -= kFullTurn;
    if (dDifference < -kHalfTurn)
        dDifference += kFullTurn;
    return dDifference;
}

bool IsSameAngle(double first, double second)
{
    return std::abs(GetAngleDifference(first, second)) < kAngleTolerance;
}

// 방향을 보지 않고 선을 따라 focus 에서 걸은 걸음 수(제 자신으로 가는 선은 보지 않는다). 닿지 않는 점은 -1
QVector<int> ComputeHops(const KnowledgeGraph& graph, int focus)
{
    const int iNodeCount = graph.GetNodes().size();
    QVector<QVector<int>> vecNeighbors(iNodeCount);
    for (const KnowledgeGraphEdge& edge : graph.GetEdges())
    {
        if (edge.iFrom == edge.iTo)
            continue;
        vecNeighbors[edge.iFrom] << edge.iTo;
        vecNeighbors[edge.iTo] << edge.iFrom;
    }
    QVector<int> vecHops(iNodeCount, -1);
    if (focus < 0 || focus >= iNodeCount)
        return vecHops;
    vecHops[focus] = 0;
    QVector<int> vecQueue { focus };
    for (int iNext = 0; iNext < vecQueue.size(); ++iNext)
    {
        for (const int iNeighbor : vecNeighbors[vecQueue[iNext]])
        {
            if (vecHops[iNeighbor] >= 0)
                continue;
            vecHops[iNeighbor] = vecHops[vecQueue[iNext]] + 1;
            vecQueue << iNeighbor;
        }
    }
    return vecHops;
}

// 이웃 배치가 지킬 조건(design/MoreViews.md §6.1). 맞으면 빈 글, 아니면 처음 어긋난 것.
// 점: 깊이 안의 점 전부를 한 번씩, 첫 점은 초점(bFocus 는 초점만), 그다음 걸음 수 순. 고리마다 반지름이 같고 바깥 고리가 더 크다.
// 한 고리의 점은 시계 방향으로 한 바퀴 안에 차례로 놓이고, 첫 1 걸음 점은 12시다. 선: 넣은 점 사이의 그래프 선 전부(그래프 선 순서).
// 점 상자가 겹치지 않고, 경계가 점 상자를 모두 담고, 값이 유한하다
QString CheckEgoLayout(const KnowledgeGraph& graph, const GraphLayoutOutput& out, const QString& focus, int depth)
{
    const QString strWhere = focus + QStringLiteral(" depth %1: ").arg(depth);
    const int iFocus = graph.FindNode(focus);
    if (iFocus < 0)
        return strWhere + QStringLiteral("the focus is not a node");
    const QVector<int> vecHops = ComputeHops(graph, iFocus);
    const auto funcIncluded = [&vecHops, depth](int node) { return vecHops[node] >= 0 && vecHops[node] <= depth; };
    QStringList vecExpectedIds;
    for (int iNode = 0; iNode < graph.GetNodes().size(); ++iNode)
        if (funcIncluded(iNode))
            vecExpectedIds << graph.GetNodes()[iNode].strId;
    const QStringList vecIds = ListNodeIds(out);
    const QString strNodeDiff = DiffItems(QStringLiteral("nodes"), vecIds, vecExpectedIds);
    if (!strNodeDiff.isEmpty())
        return strWhere + strNodeDiff;
    if (QSet<QString>(vecIds.cbegin(), vecIds.cend()).size() != vecIds.size())
        return strWhere + QStringLiteral("a node appears twice");
    if (out.vecNodes.isEmpty() || out.vecNodes[0].strId != focus)
        return strWhere + QStringLiteral("the first node is not the focus");

    const QPointF ptCenter = out.vecNodes[0].center;
    QVector<double> vecRingRadius(depth + 1, -1);
    vecRingRadius[0] = 0;
    QVector<double> vecLastAngle(depth + 1, qQNaN());
    QVector<double> vecTurned(depth + 1, 0);
    int iPreviousHop = 0;
    for (int iNode = 0; iNode < out.vecNodes.size(); ++iNode)
    {
        const LayoutNode& node = out.vecNodes[iNode];
        const QString strNode = DescribePoint(node.strId, node.center);
        if (node.bFocus != (iNode == 0))
            return strWhere + strNode + QStringLiteral(" bFocus ") + FormatBool(node.bFocus);
        if (node.bDocument)
            return strWhere + strNode + QStringLiteral(" is a document");
        const int iHop = vecHops[graph.FindNode(node.strId)];
        if (iHop < iPreviousHop)
            return strWhere + strNode + QStringLiteral(" (ring %1) comes after ring %2").arg(iHop).arg(iPreviousHop);
        iPreviousHop = iHop;
        if (iHop == 0)
            continue;
        const double dRadius = QLineF(ptCenter, node.center).length();
        if (vecRingRadius[iHop] < 0)
        {
            if (vecRingRadius[iHop - 1] < 0 || dRadius <= vecRingRadius[iHop - 1] + kTolerance)
                return strWhere + strNode + QStringLiteral(" ring %1 radius %2 does not grow from %3").arg(iHop).arg(dRadius).arg(vecRingRadius[iHop - 1]);
            vecRingRadius[iHop] = dRadius;
        }
        else if (!IsNear(dRadius, vecRingRadius[iHop]))
        {
            return strWhere + strNode + QStringLiteral(" radius %1 differs from its ring %2").arg(dRadius).arg(vecRingRadius[iHop]);
        }
        const double dAngle = GetAngleDegrees(ptCenter, node.center);
        if (std::isnan(vecLastAngle[iHop]))
        {
            if (iHop == 1 && !IsSameAngle(dAngle, kTwelveOClock))
                return strWhere + strNode + QStringLiteral(" is the first ring-1 node but not at 12 o'clock (%1)").arg(dAngle);
        }
        else
        {
            const double dStep = std::fmod(dAngle - vecLastAngle[iHop] + 2 * kFullTurn, kFullTurn);
            vecTurned[iHop] += dStep;
            if (dStep <= kAngleTolerance || vecTurned[iHop] >= kFullTurn - kAngleTolerance)
                return strWhere + strNode + QStringLiteral(" is not clockwise after the previous node of ring %1").arg(iHop);
        }
        vecLastAngle[iHop] = dAngle;
    }

    QStringList vecExpectedEdges;
    for (const KnowledgeGraphEdge& edge : graph.GetEdges())
        if (funcIncluded(edge.iFrom) && funcIncluded(edge.iTo))
            vecExpectedEdges << DescribeGraphEdge(edge);
    QStringList vecEdges;
    int iPreviousGraphEdge = -1;
    for (const LayoutEdge& edge : out.vecEdges)
    {
        if (edge.iFrom < 0 || edge.iFrom >= out.vecNodes.size() || edge.iTo < 0 || edge.iTo >= out.vecNodes.size())
            return strWhere + QStringLiteral("an edge points outside the nodes");
        if (edge.iGraphEdge <= iPreviousGraphEdge || edge.iGraphEdge >= graph.GetEdges().size())
            return strWhere + QStringLiteral("edges are not in graph edge order");
        iPreviousGraphEdge = edge.iGraphEdge;
        const KnowledgeGraphEdge& graphEdge = graph.GetEdges()[edge.iGraphEdge];
        if (DescribeLayoutEdge(out, edge) != DescribeGraphEdge(graphEdge) || edge.eStatus != graphEdge.edge.eStatus || edge.bDirected != graphEdge.bDirected)
            return strWhere + DescribeLayoutEdge(out, edge) + QStringLiteral(" does not match its graph edge");
        vecEdges << DescribeLayoutEdge(out, edge);
    }
    if (vecEdges != vecExpectedEdges)
        return strWhere + QStringLiteral("edges [") + vecEdges.join(QStringLiteral(", ")) + QStringLiteral("] expected [")
               + vecExpectedEdges.join(QStringLiteral(", ")) + QLatin1Char(']');

    // 나무 선 · 둘레 선: 출력 순서가 고리 순서라, k 걸음 점의 부모는 앞 고리에서 이어진 점 가운데 출력에서 가장 앞인 점이다(1 걸음 점의 부모는 초점).
    // 점과 그 부모를 잇는 선은 모두 나무 선이고, 그 밖(같은 고리 점끼리 · 부모가 아닌 앞 고리 점과 · 제 자신으로 가는 선)은 둘레 선이다
    QHash<QString, int> hashPosition;
    for (int iNode = 0; iNode < out.vecNodes.size(); ++iNode)
        hashPosition.insert(out.vecNodes[iNode].strId, iNode);
    const QVector<KnowledgeGraphNode>& vecGraphNodes = graph.GetNodes();
    QVector<int> vecParent(vecGraphNodes.size(), -1);   // 그래프 점 번호 → 부모 점 번호
    for (const KnowledgeGraphEdge& edge : graph.GetEdges())
    {
        if (edge.iFrom == edge.iTo)
            continue;
        for (const QPair<int, int>& pairEnds : { qMakePair(edge.iFrom, edge.iTo), qMakePair(edge.iTo, edge.iFrom) })
        {
            const int iChild = pairEnds.first;
            const int iCandidate = pairEnds.second;
            if (!funcIncluded(iChild) || vecHops[iChild] == 0 || vecHops[iCandidate] != vecHops[iChild] - 1)
                continue;
            int& iParent = vecParent[iChild];
            if (iParent < 0 || hashPosition.value(vecGraphNodes[iCandidate].strId) < hashPosition.value(vecGraphNodes[iParent].strId))
                iParent = iCandidate;
        }
    }
    for (const LayoutEdge& edge : out.vecEdges)
    {
        const KnowledgeGraphEdge& graphEdge = graph.GetEdges()[edge.iGraphEdge];
        const bool bTree = graphEdge.iFrom != graphEdge.iTo && (vecParent[graphEdge.iFrom] == graphEdge.iTo || vecParent[graphEdge.iTo] == graphEdge.iFrom);
        if (edge.bContext == bTree)
            return strWhere + DescribeLayoutEdge(out, edge) + (bTree ? QStringLiteral(" joins a node and its parent but is a context edge")
                                                                    : QStringLiteral(" is not a tree edge but is not marked as context"));
    }

    const QStringList vecOverlaps = FindOverlaps(out);
    if (!vecOverlaps.isEmpty())
        return strWhere + QStringLiteral("overlaps ") + vecOverlaps.join(QStringLiteral(", "));
    const QStringList vecNonFinite = FindNonFinite(out);
    if (!vecNonFinite.isEmpty())
        return strWhere + QStringLiteral("not finite ") + vecNonFinite.join(QStringLiteral(", "));
    for (const LayoutNode& node : out.vecNodes)
        if (!ContainsWithTolerance(out.bounds, MakeBox(node)))
            return strWhere + DescribePoint(node.strId, node.center) + QStringLiteral(" is outside the bounds ") + DescribeBox(out.bounds);
    if (!out.vecTicks.isEmpty() || !out.vecLanes.isEmpty() || !out.vecMatrixOrder.isEmpty() || !out.vecCells.isEmpty() || !out.vecUnplaced.isEmpty())
        return strWhere + QStringLiteral("has ticks, lanes, matrix cells or unplaced nodes");
    return QString();
}

// 문서 하나: 카드 하나가 conceptIds 를 가리킨다(다루는 문서 수를 늘릴 때)
KnowledgeDocument MakeDocumentPointingTo(const QString& id, const QStringList& conceptIds)
{
    KnowledgeDocument document;
    document.strId = id;
    document.strTitle = id;
    KnowledgeCard card;
    for (const QString& strConceptId : conceptIds)
        card.vecConcepts << MakeLink(strConceptId);
    document.vecCards = { card };
    return document;
}
}

// 그래프 모델(KnowledgeGraph)과 배치 7가지(KnowledgeGraphLayout, design/Views.md §5 · design/MoreViews.md §6)를 showcase 와 작은 합성 그래프로 시험한다.
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

    // 관계망: 점 31 · 선 36. 점 상자끼리 · 덩어리 상자끼리 겹치지 않고, 덩어리는 큰 것부터 차례로 앞 덩어리의 오른쪽이나 아래(열로 쌓는다)
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

        // 덩어리 상자: 서로 겹치지 않고, 덩어리 순으로 다음 덩어리는 앞 덩어리의 오른쪽이나 아래에 있다
        // (예전에는 "왼쪽에서 오른쪽 한 줄 · 선 없는 점은 끝에 한 줄" 을 봤다. 열로 쌓는 배치로 바꾸며 이 두 검사를 고쳤다)
        const QVector<QRectF> vecBoxes = ListComponentBoxes(m_graph, out);
        QCOMPARE(vecBoxes.size(), kComponentSizes.size());
        for (int iFirst = 0; iFirst < vecBoxes.size(); ++iFirst)
        {
            for (int iSecond = iFirst + 1; iSecond < vecBoxes.size(); ++iSecond)
                QVERIFY2(!vecBoxes[iFirst].intersects(vecBoxes[iSecond]),
                         qUtf8Printable(QStringLiteral("components %1 and %2 overlap").arg(iFirst).arg(iSecond)));
            if (iFirst > 0)
                QVERIFY2(vecBoxes[iFirst - 1].right() < vecBoxes[iFirst].left() || vecBoxes[iFirst - 1].bottom() < vecBoxes[iFirst].top(),
                         qUtf8Printable(QStringLiteral("component %1 is neither right of nor below component %2").arg(iFirst).arg(iFirst - 1)));
        }
        // 덩어리 상자끼리 kComponentGap 이상 떨어진다(선 없는 점끼리는 점 틈만큼)
        const QStringList vecClose = FindCloseComponents(m_graph, out);
        QVERIFY2(vecClose.isEmpty(), qUtf8Printable(vecClose.join(QStringLiteral(", "))));
    }

    // 관계망은 덩어리 상자를 세로 열로 쌓는다(design/Views.md §5 관계망 줄). showcase: 연구 덩어리 오른쪽 열의 맨 위에 기획서 덩어리,
    // 그 아래 같은 열에 선 없는 점 4개가 격자로(한 줄이 아니다). 배치 범위는 가로 ≤ 1.7 × 세로, 넓이는 고치기 전의 75% 이하다
    void networkStacksComponentsInColumns()
    {
        const GraphLayoutOutput& out = GetOutput(EGraphLayoutType::NETWORK);
        const QString strBounds = DescribeBox(out.bounds);
        QVERIFY2(out.bounds.width() <= kNetworkMaxAspect * out.bounds.height(), qUtf8Printable(strBounds));
        QVERIFY2(out.bounds.width() * out.bounds.height() <= kNetworkMaxAreaShare * kNetworkAreaBefore,
                 qUtf8Printable(strBounds + QStringLiteral(" area %1").arg(out.bounds.width() * out.bounds.height())));
        const QString strColumns = CheckComponentColumns(m_graph, out);
        QVERIFY2(strColumns.isEmpty(), qUtf8Printable(strColumns));

        const QVector<QRectF> vecBoxes = ListComponentBoxes(m_graph, out);
        const QRectF& rectResearch = vecBoxes[kResearchComponent];
        const QRectF& rectPlanning = vecBoxes[kPlanningComponent];
        QCOMPARE(m_graph.GetNodes()[m_graph.FindNode(DecodeUtf8("검출기"))].iComponent, kResearchComponent);
        QCOMPARE(m_graph.GetNodes()[m_graph.FindNode(DecodeUtf8("결제-서비스"))].iComponent, kPlanningComponent);
        const QString strPair = DescribeBox(rectResearch) + QStringLiteral(" / ") + DescribeBox(rectPlanning);
        QVERIFY2(rectPlanning.left() >= rectResearch.right() + GraphLayoutSize::kComponentGap - kTolerance, qUtf8Printable(strPair));
        QVERIFY2(IsNear(rectPlanning.top(), rectResearch.top()), qUtf8Printable(strPair));

        QSet<double> setRowTops;
        for (const QString& strId : kSingleComponents)
        {
            const int iNode = FindLayoutNode(out, strId);
            QVERIFY2(iNode >= 0, qUtf8Printable(strId));
            const QRectF rectSingle = MakeBox(out.vecNodes[iNode]);
            const QString strSingle = strId + QLatin1Char(' ') + DescribeBox(rectSingle) + QStringLiteral(" / planning ") + DescribeBox(rectPlanning);
            QVERIFY2(rectSingle.top() >= rectPlanning.bottom() + GraphLayoutSize::kComponentGap - kTolerance, qUtf8Printable(strSingle));
            QVERIFY2(rectSingle.left() >= rectPlanning.left() - kTolerance && rectSingle.right() <= rectPlanning.right() + kTolerance,
                     qUtf8Printable(strSingle));
            setRowTops.insert(rectSingle.top());
        }
        QVERIFY2(setRowTops.size() > 1, "single nodes are not in one row");
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
            QVERIFY2(!edge.bContext || eType == EGraphLayoutType::EGO, qUtf8Printable(DescribeLayoutEdge(out, edge)));   // 둘레 선은 이웃만 쓴다
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
        for (int iNode = 0; iNode < out.vecNodes.size(); ++iNode)
        {
            const LayoutNode& node = out.vecNodes[iNode];
            QVERIFY2(!setIds.contains(node.strId), qUtf8Printable(node.strId));   // 점마다 한 번
            setIds.insert(node.strId);
            QVERIFY2(node.strId != DecodeUtf8(kHiddenConcept), kHiddenConcept);
            // 초점 점은 이웃 보기의 첫 점 하나뿐이다
            QVERIFY2(node.bFocus == (eType == EGraphLayoutType::EGO && iNode == 0), qUtf8Printable(node.strId));
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
            // 이웃: 초점을 주어도 점이 없으면 빈 배치, 기본 초점은 빈 글
            GraphLayoutOutput out;
            QVERIFY(RunLayoutWithFocus(*pGraph, EGraphLayoutType::EGO, QStringLiteral("a"), kEgoDefaultDepth, &out));
            QCOMPARE(DescribeLayout(out), DescribeLayout(GraphLayoutOutput()));
            QCOMPARE(KnowledgeGraphLayout::FindDefaultFocus(*pGraph), QString());
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

        // 이웃: 기본 초점이 그 점이고, 배치는 그 점 하나(가운데, 초점)
        QCOMPARE(KnowledgeGraphLayout::FindDefaultFocus(graph), QString("a"));
        QVERIFY(RunLayout(graph, EGraphLayoutType::EGO, &out));
        QCOMPARE(ListNodeIds(out), QStringList { "a" });
        QVERIFY(out.vecNodes[0].bFocus);
        QVERIFY(out.vecEdges.isEmpty());
        QCOMPARE(out.bounds.size(), QSizeF(GraphLayoutSize::kNodeWidth, GraphLayoutSize::kNodeHeight));
        QVERIFY(ContainsWithTolerance(out.bounds, MakeBox(out.vecNodes[0])));

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

    void networkGridsOnlySingleNodes_data()
    {
        QTest::addColumn<int>("count");
        QTest::addColumn<int>("perRow");
        QTest::addColumn<int>("rows");
        QTest::newRow("9 singles") << 9 << 3 << 3;
        QTest::newRow("10 singles") << 10 << 3 << 4;   // 줄 수는 ⌈√10⌉ = 4, 한 줄은 ⌈10 / 4⌉ = 3
        QTest::newRow("2 singles") << 2 << 1 << 2;
        QTest::newRow("1 single") << 1 << 1 << 1;
    }

    // 관계망: 선 없는 점만 있으면 줄 수 ⌈√점 수⌉ 의 격자다(9개면 3 × 3). 이름 열쇠 순으로 줄마다 왼쪽에서 오른쪽, 점 사이는 kNodeGapX · kNodeGapY
    void networkGridsOnlySingleNodes()
    {
        QFETCH(int, count);
        QFETCH(int, perRow);
        QFETCH(int, rows);
        QVector<KnowledgeConcept> vecConcepts;
        AddSingles(count, &vecConcepts);
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(vecConcepts, {}, {});
        GraphLayoutOutput out;
        QVERIFY(RunLayout(graph, EGraphLayoutType::NETWORK, &out));
        QCOMPARE(out.vecNodes.size(), count);
        const double dSlotPitch = GraphLayoutSize::kNodeWidth + GraphLayoutSize::kNodeGapX;
        const double dRowPitch = GraphLayoutSize::kNodeHeight + GraphLayoutSize::kNodeGapY;
        for (int iNode = 0; iNode < count; ++iNode)
        {
            const QPointF ptExpected = out.vecNodes[0].center + QPointF((iNode % perRow) * dSlotPitch, (iNode / perRow) * dRowPitch);
            const QPointF ptCenter = out.vecNodes[iNode].center;
            QVERIFY2(IsNear(ptCenter.x(), ptExpected.x()) && IsNear(ptCenter.y(), ptExpected.y()),
                     qUtf8Printable(DescribePoint(out.vecNodes[iNode].strId, ptCenter) + QStringLiteral(" / expected ") + DescribePoint(QString(), ptExpected)));
        }
        QVERIFY2(IsNear(out.bounds.width(), perRow * GraphLayoutSize::kNodeWidth + (perRow - 1) * GraphLayoutSize::kNodeGapX)
                     && IsNear(out.bounds.height(), rows * GraphLayoutSize::kNodeHeight + (rows - 1) * GraphLayoutSize::kNodeGapY),
                 qUtf8Printable(DescribeBox(out.bounds)));
        const QStringList vecOverlaps = FindOverlaps(out);
        QVERIFY2(vecOverlaps.isEmpty(), qUtf8Printable(vecOverlaps.join(QStringLiteral(", "))));
    }

    // 관계망: 큰 덩어리 하나와 작은 덩어리(두 점) 여럿. 작은 것들은 큰 것 오른쪽 열에 위 → 아래로(틈 kComponentGap) 쌓이고,
    // 열 높이가 큰 덩어리 상자 높이를 넘으면 오른쪽 다음 열의 맨 위로 간다. 같은 입력이면 같은 출력이다
    void networkStacksSmallComponentsInColumns()
    {
        QVector<KnowledgeConcept> vecConcepts;
        QVector<KnowledgeEdge> vecEdges;
        AddStar(&vecConcepts, &vecEdges);
        for (int iPair = 1; iPair <= kSmallPairCount; ++iPair)
            AddPair(iPair, &vecConcepts, &vecEdges);
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(vecConcepts, vecEdges, {});
        QCOMPARE(graph.GetComponents().size(), 1 + kSmallPairCount);
        QCOMPARE(graph.GetComponents()[0].size(), 1 + kStarLeafCount);
        GraphLayoutOutput out;
        QVERIFY(RunLayout(graph, EGraphLayoutType::NETWORK, &out));
        const QString strColumns = CheckComponentColumns(graph, out);
        QVERIFY2(strColumns.isEmpty(), qUtf8Printable(strColumns));

        const QVector<QRectF> vecBoxes = ListComponentBoxes(graph, out);
        const QRectF& rectBig = vecBoxes[0];
        QVERIFY2(IsNear(rectBig.left(), out.bounds.left()) && IsNear(rectBig.top(), out.bounds.top()), qUtf8Printable(DescribeBox(rectBig)));
        QVERIFY2(IsNear(out.bounds.height(), rectBig.height()), qUtf8Printable(DescribeBox(out.bounds)));   // 열 높이 상한은 큰 덩어리 상자 높이
        QVERIFY2(IsNear(vecBoxes[1].left(), rectBig.right() + GraphLayoutSize::kComponentGap) && IsNear(vecBoxes[1].top(), rectBig.top()),
                 qUtf8Printable(DescribeBox(vecBoxes[1])));
        int iColumns = 0;
        for (int iPair = 1; iPair <= kSmallPairCount; ++iPair)
        {
            QVERIFY2(vecBoxes[iPair].left() >= rectBig.right() + GraphLayoutSize::kComponentGap - kTolerance, qUtf8Printable(DescribeBox(vecBoxes[iPair])));
            if (IsNear(vecBoxes[iPair].top(), rectBig.top()))
                ++iColumns;
            else
                QVERIFY2(vecBoxes[iPair].top() > vecBoxes[iPair - 1].bottom(), qUtf8Printable(DescribeBox(vecBoxes[iPair])));   // 앞 것 아래
        }
        QVERIFY2(iColumns >= 2, qUtf8Printable(QStringLiteral("small components use %1 column(s)").arg(iColumns)));   // 넘쳐서 다음 열로 갔다
        QVERIFY2(iColumns < kSmallPairCount, "small components are stacked, not one per column");
        const QStringList vecOverlaps = FindOverlaps(out);
        QVERIFY2(vecOverlaps.isEmpty(), qUtf8Printable(vecOverlaps.join(QStringLiteral(", "))));
        const QStringList vecClose = FindCloseComponents(graph, out);
        QVERIFY2(vecClose.isEmpty(), qUtf8Printable(vecClose.join(QStringLiteral(", "))));

        GraphLayoutOutput again;
        QVERIFY(RunLayout(KnowledgeGraph::BuildFromParts(vecConcepts, vecEdges, {}), EGraphLayoutType::NETWORK, &again));
        QCOMPARE(DescribeLayout(again), DescribeLayout(out));
    }

    // 관계망: 열 높이 상한 H 는 첫 덩어리가 아니라 가장 높은 덩어리 상자의 높이다. 점 수가 같아 이름 열쇠 순으로 뒤에 놓이는 사슬(B)이
    // 서로 다 이어진 다섯 점(A)보다 높으면, 그 뒤의 두 점 덩어리들은 사슬 오른쪽 열에 사슬 높이까지 쌓인다
    void networkColumnLimitIsTallestComponent()
    {
        QVector<KnowledgeConcept> vecConcepts;
        QVector<KnowledgeEdge> vecEdges;
        for (int iFirst = 1; iFirst <= kLimitGroupSize; ++iFirst)
        {
            vecConcepts << MakeConcept(QStringLiteral("a%1").arg(iFirst), QStringLiteral("A%1").arg(iFirst));
            for (int iSecond = 1; iSecond < iFirst; ++iSecond)
                vecEdges << MakeEdge(QStringLiteral("a%1").arg(iFirst), "uses", QStringLiteral("a%1").arg(iSecond));
        }
        for (int iLink = 1; iLink <= kLimitGroupSize; ++iLink)
        {
            vecConcepts << MakeConcept(QStringLiteral("b%1").arg(iLink), QStringLiteral("B%1").arg(iLink));
            if (iLink > 1)
                vecEdges << MakeEdge(QStringLiteral("b%1").arg(iLink), "improves", QStringLiteral("b%1").arg(iLink - 1));
        }
        for (int iPair = 1; iPair <= kLimitPairCount; ++iPair)
            AddPair(iPair, &vecConcepts, &vecEdges);
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(vecConcepts, vecEdges, {});
        QCOMPARE(graph.GetComponents().size(), 2 + kLimitPairCount);
        QCOMPARE(graph.GetNodes()[graph.GetComponents()[0].first()].strId, QString("a1"));   // 점 수가 같으면 이름 열쇠 순: A 가 먼저
        GraphLayoutOutput out;
        QVERIFY(RunLayout(graph, EGraphLayoutType::NETWORK, &out));
        const QString strColumns = CheckComponentColumns(graph, out);
        QVERIFY2(strColumns.isEmpty(), qUtf8Printable(strColumns));

        const QVector<QRectF> vecBoxes = ListComponentBoxes(graph, out);
        const double dPairHeight = vecBoxes[2].height();
        const double dPairPitch = dPairHeight + GraphLayoutSize::kComponentGap;
        const int iPerColumn = static_cast<int>((vecBoxes[1].height() + GraphLayoutSize::kComponentGap) / dPairPitch);
        const int iPerColumnIfFirst = static_cast<int>((vecBoxes[0].height() + GraphLayoutSize::kComponentGap) / dPairPitch);
        QVERIFY2(iPerColumn > iPerColumnIfFirst, qUtf8Printable(DescribeBox(vecBoxes[0]) + QStringLiteral(" / ") + DescribeBox(vecBoxes[1])));   // 이 입력의 전제
        QVERIFY2(IsNear(out.bounds.height(), vecBoxes[1].height()), qUtf8Printable(DescribeBox(out.bounds)));
        int iInFirstPairColumn = 0;
        for (int iComponent = 2; iComponent < vecBoxes.size(); ++iComponent)
            if (IsNear(vecBoxes[iComponent].left(), vecBoxes[2].left()))
                ++iInFirstPairColumn;
        QCOMPARE(iInFirstPairColumn, qMin(kLimitPairCount, iPerColumn));
    }

    // 관계망: 선 없는 점은 덩어리 뒤에 격자로 둔다. 지금 열의 남은 높이에 열 너비에 드는 만큼씩(최소 1) 줄을 위 → 아래로 채우고,
    // 모자라면 오른쪽 새 열로 넘긴다. 새 열에서는 열 높이 상한 H 에 드는 줄 수로 남은 점을 고르게 나눈다(한 줄 ⌈남은 점 / 줄 수⌉)
    void networkSinglesFillColumnThenOverflow()
    {
        QVector<KnowledgeConcept> vecConcepts;
        QVector<KnowledgeEdge> vecEdges;
        AddStar(&vecConcepts, &vecEdges);
        AddPair(1, &vecConcepts, &vecEdges);
        AddSingles(kOverflowSingleCount, &vecConcepts);
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(vecConcepts, vecEdges, {});
        QCOMPARE(graph.GetComponents().size(), 2 + kOverflowSingleCount);
        GraphLayoutOutput out;
        QVERIFY(RunLayout(graph, EGraphLayoutType::NETWORK, &out));
        const QString strColumns = CheckComponentColumns(graph, out);
        QVERIFY2(strColumns.isEmpty(), qUtf8Printable(strColumns));

        const QVector<QRectF> vecBoxes = ListComponentBoxes(graph, out);
        const QRectF& rectBig = vecBoxes[0];
        const QRectF& rectPair = vecBoxes[1];
        const double dLimit = rectBig.height();
        const double dSlotPitch = GraphLayoutSize::kNodeWidth + GraphLayoutSize::kNodeGapX;
        const double dRowPitch = GraphLayoutSize::kNodeHeight + GraphLayoutSize::kNodeGapY;
        QVERIFY2(IsNear(rectPair.left(), rectBig.right() + GraphLayoutSize::kComponentGap) && IsNear(rectPair.top(), rectBig.top()),
                 qUtf8Printable(DescribeBox(rectPair)));

        // 두 점 덩어리의 열: 한 줄은 열 너비에 드는 만큼, 줄은 열 높이 상한까지
        const int iPerRow = qMax(1, static_cast<int>(std::floor((rectPair.width() + GraphLayoutSize::kNodeGapX) / dSlotPitch)));
        const double dFirstRowTop = rectPair.bottom() + GraphLayoutSize::kComponentGap;
        int iRows = 0;
        while (dFirstRowTop + iRows * dRowPitch + GraphLayoutSize::kNodeHeight <= rectBig.top() + dLimit + kTolerance)
            ++iRows;
        const int iInColumn = qMin(kOverflowSingleCount, iPerRow * iRows);
        QVERIFY2(iRows >= 1 && iInColumn < kOverflowSingleCount, qUtf8Printable(QStringLiteral("%1 per row, %2 rows").arg(iPerRow).arg(iRows)));   // 두 경우를 다 지난다
        double dColumnRight = rectPair.right();
        for (int iSingle = 0; iSingle < kOverflowSingleCount; ++iSingle)
        {
            const QRectF& rectSingle = vecBoxes[2 + iSingle];
            QPointF ptExpected;
            if (iSingle < iInColumn)
            {
                ptExpected = QPointF(rectPair.left() + (iSingle % iPerRow) * dSlotPitch, dFirstRowTop + (iSingle / iPerRow) * dRowPitch);
                dColumnRight = qMax(dColumnRight, rectSingle.right());
            }
            else
            {
                // 새 열: 맨 위부터, 줄 수는 H 에 드는 만큼
                int iNewRows = 1;
                while (iNewRows * dRowPitch + GraphLayoutSize::kNodeHeight <= dLimit + kTolerance)
                    ++iNewRows;
                const int iRemaining = kOverflowSingleCount - iInColumn;
                const int iNewPerRow = (iRemaining + iNewRows - 1) / iNewRows;
                const int iIndex = iSingle - iInColumn;
                ptExpected = QPointF(dColumnRight + GraphLayoutSize::kComponentGap + (iIndex % iNewPerRow) * dSlotPitch, rectBig.top() + (iIndex / iNewPerRow) * dRowPitch);
            }
            QVERIFY2(IsNear(rectSingle.left(), ptExpected.x()) && IsNear(rectSingle.top(), ptExpected.y()),
                     qUtf8Printable(QStringLiteral("single %1 %2 / expected top left (%3, %4)").arg(iSingle).arg(DescribeBox(rectSingle)).arg(ptExpected.x()).arg(ptExpected.y())));
            QVERIFY2(rectSingle.bottom() <= rectBig.top() + dLimit + kTolerance, qUtf8Printable(DescribeBox(rectSingle)));
        }
        const QStringList vecOverlaps = FindOverlaps(out);
        QVERIFY2(vecOverlaps.isEmpty(), qUtf8Printable(vecOverlaps.join(QStringLiteral(", "))));
        const QStringList vecClose = FindCloseComponents(graph, out);
        QVERIFY2(vecClose.isEmpty(), qUtf8Printable(vecClose.join(QStringLiteral(", "))));
    }

    // 관계망은 덩어리마다 평행 이동만 한다: 덩어리 안 점끼리의 상대 자리는 그 덩어리만 따로 배치한 것과 같다(합성 그래프 · showcase)
    void networkMovesEachComponentWhole()
    {
        QVector<KnowledgeConcept> vecConcepts;
        QVector<KnowledgeEdge> vecEdges;
        AddStar(&vecConcepts, &vecEdges);
        vecConcepts << MakeConcept("t1", "T1") << MakeConcept("t2", "T2") << MakeConcept("t3", "T3");   // 세모
        vecEdges << MakeEdge("t1", "uses", "t2") << MakeEdge("t2", "uses", "t3") << MakeEdge("t3", "alternative", "t1");
        vecConcepts << MakeConcept("c1", "C1") << MakeConcept("c2", "C2") << MakeConcept("c3", "C3") << MakeConcept("c4", "C4");   // 사슬
        vecEdges << MakeEdge("c2", "improves", "c1") << MakeEdge("c3", "improves", "c2") << MakeEdge("c4", "improves", "c3");
        AddPair(1, &vecConcepts, &vecEdges);
        AddSingles(2, &vecConcepts);
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(vecConcepts, vecEdges, {});
        QCOMPARE(graph.GetComponents().size(), 6);   // 별 · 세모 · 사슬 · 두 점 덩어리 · 선 없는 점 2
        const QString strSynthetic = CompareComponentShapes(graph, vecConcepts, vecEdges);
        QVERIFY2(strSynthetic.isEmpty(), qUtf8Printable(strSynthetic));

        const QString strShowcase = CompareComponentShapes(m_graph, m_index.ListConcepts(), m_index.ListEdges());
        QVERIFY2(strShowcase.isEmpty(), qUtf8Printable(strShowcase));
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

    // 이웃의 기본 초점: 선이 가장 많은 점, 같으면 다루는 문서가 많은 점, 같으면 점 번호(id 가 아니라 이름 열쇠) 순으로 앞의 것. 점이 없으면 빈 글
    void egoDefaultFocus()
    {
        QCOMPARE(KnowledgeGraphLayout::FindDefaultFocus(m_graph), DecodeUtf8(kEgoDefaultFocus));

        // 선 수가 먼저다: 이름 열쇠로 앞선 Alpha 가 아니라 선 2 인 Beta
        const KnowledgeGraph byEdges = KnowledgeGraph::BuildFromParts({ MakeConcept("a", "Alpha"), MakeConcept("b", "Beta"), MakeConcept("c", "Gamma") },
                                                                      { MakeEdge("b", "uses", "a"), MakeEdge("b", "uses", "c") }, {});
        QCOMPARE(KnowledgeGraphLayout::FindDefaultFocus(byEdges), QString("b"));

        // 선 수가 같으면 다루는 문서가 많은 점: Xi 가 아니라 문서 하나가 가리키는 Ypsilon
        const KnowledgeGraph byDocuments = KnowledgeGraph::BuildFromParts({ MakeConcept("x", "Xi"), MakeConcept("y", "Ypsilon") },
                                                                          { MakeEdge("x", "uses", "y") },
                                                                          { MakeDocumentPointingTo("doc", { "y" }) });
        QCOMPARE(byDocuments.GetNodes()[byDocuments.FindNode("y")].iDocumentCount, 1);
        QCOMPARE(KnowledgeGraphLayout::FindDefaultFocus(byDocuments), QString("y"));

        // 선 수 · 문서 수가 같으면 점 번호 순: id 는 뒤지만 이름 열쇠가 앞선 Alpha(id z-id)
        const KnowledgeGraph byNameKey = KnowledgeGraph::BuildFromParts({ MakeConcept("a-id", "Beta"), MakeConcept("z-id", "Alpha") },
                                                                        { MakeEdge("a-id", "uses", "z-id") }, {});
        QCOMPARE(KnowledgeGraphLayout::FindDefaultFocus(byNameKey), QString("z-id"));

        // 선이 하나도 없으면 문서 수로 고른다
        const KnowledgeGraph edgeless = KnowledgeGraph::BuildFromParts({ MakeConcept("p", "P"), MakeConcept("q", "Q") }, {},
                                                                       { MakeDocumentPointingTo("doc", { "q" }) });
        QCOMPARE(KnowledgeGraphLayout::FindDefaultFocus(edgeless), QString("q"));
        QCOMPARE(KnowledgeGraphLayout::FindDefaultFocus(KnowledgeGraph()), QString());
    }

    // 이웃(깊이 2, 기본 초점 Faster R-CNN): 점 12 · 선 18(design/MoreViews.md §7.3). 1 걸음 6점은 12시에서 시계 방향으로 잇는 선의 관계 종류 순,
    // 2 걸음 5점은 각 순이다. 쐐기의 가운데 각, 잎 수에 비례한 쐐기, 부모 쐐기 안의 자식, 넣는 선 18개, 고리마다 같은 반지름, 겹침 없음
    void egoShowcaseDepthTwo()
    {
        const GraphLayoutOutput& out = GetOutput(EGraphLayoutType::EGO);
        QCOMPARE(out.vecNodes.size(), kEgoNodes);
        QCOMPARE(out.vecEdges.size(), kEgoEdges);
        QStringList vecExpectedIds { DecodeUtf8(kEgoDefaultFocus) };
        for (const QVector<ExpectedEgoNode>* pRing : { &kEgoRingOne, &kEgoRingTwo })
            for (const ExpectedEgoNode& expected : *pRing)
                vecExpectedIds << DecodeUtf8(expected.pId);
        QCOMPARE(ListNodeIds(out), vecExpectedIds);
        QVERIFY(out.vecNodes[0].bFocus);
        QCOMPARE(out.vecNodes[0].center, QPointF(0, 0));

        // 가운데 각
        const QPointF ptCenter = out.vecNodes[0].center;
        QHash<QString, double> hashAngles;
        for (const QVector<ExpectedEgoNode>* pRing : { &kEgoRingOne, &kEgoRingTwo })
        {
            for (const ExpectedEgoNode& expected : *pRing)
            {
                const QString strId = DecodeUtf8(expected.pId);
                const double dAngle = GetAngleDegrees(ptCenter, GetCenter(out, strId));
                QVERIFY2(IsSameAngle(dAngle, expected.dAngle), qUtf8Printable(strId + QStringLiteral(" %1 / expected %2").arg(dAngle).arg(expected.dAngle)));
                hashAngles.insert(strId, dAngle);
            }
        }
        // 쐐기는 잎 수에 비례한다: 이웃한 1 걸음 두 점의 각 차는 (앞 잎 + 뒤 잎) / 2 × 45°
        int iLeaves = 0;
        for (int iNode = 0; iNode < kEgoRingOne.size(); ++iNode)
        {
            iLeaves += kEgoRingOne[iNode].iLeaves;
            const ExpectedEgoNode& current = kEgoRingOne[iNode];
            const ExpectedEgoNode& next = kEgoRingOne[(iNode + 1) % kEgoRingOne.size()];
            const double dStep = std::fmod(hashAngles.value(DecodeUtf8(next.pId)) - hashAngles.value(DecodeUtf8(current.pId)) + 2 * kFullTurn, kFullTurn);
            QVERIFY2(std::abs(dStep - (current.iLeaves + next.iLeaves) / 2.0 * kEgoDegreesPerLeaf) < kAngleTolerance, current.pId);
        }
        QCOMPARE(iLeaves * kEgoDegreesPerLeaf, kFullTurn);

        // 부모: 자식과 선으로 이어져 있고, 자식은 부모 쐐기(부모 각 ± 잎 수 × 45° / 2) 안에 있다
        for (const ExpectedParent& expected : kEgoParents)
        {
            const QString strChild = DecodeUtf8(expected.pChild);
            const QString strParent = DecodeUtf8(expected.pParent);
            const int iChild = FindLayoutNode(out, strChild);
            const int iParent = FindLayoutNode(out, strParent);
            QVERIFY(iChild >= 0 && iParent >= 0);
            const bool bLinked = std::any_of(out.vecEdges.cbegin(), out.vecEdges.cend(), [iChild, iParent](const LayoutEdge& edge)
            {
                return (edge.iFrom == iChild && edge.iTo == iParent) || (edge.iFrom == iParent && edge.iTo == iChild);
            });
            QVERIFY2(bLinked, expected.pChild);
            int iParentLeaves = 0;
            for (const ExpectedEgoNode& ringOne : kEgoRingOne)
                if (DecodeUtf8(ringOne.pId) == strParent)
                    iParentLeaves = ringOne.iLeaves;
            QVERIFY2(std::abs(GetAngleDifference(hashAngles.value(strChild), hashAngles.value(strParent))) < iParentLeaves * kEgoDegreesPerLeaf / 2,
                     expected.pChild);
        }

        // 선 18개(그래프 선 순서, 끝은 출력 점 자리)
        QStringList vecEdges;
        for (const LayoutEdge& edge : out.vecEdges)
            vecEdges << DescribeLayoutEdge(out, edge);
        const QString strDiff = DiffItems(QStringLiteral("edges"), vecEdges, kEgoEdgeList);
        QVERIFY2(strDiff.isEmpty(), qUtf8Printable(strDiff));
        const QString strCheck = CheckEgoLayout(m_graph, out, DecodeUtf8(kEgoDefaultFocus), kEgoDefaultDepth);
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));
    }

    // 이웃 깊이 1: 점 7 · 선 9(초점의 선 6 · 1 걸음 점끼리 3). 1 걸음 점은 모두 잎이라 60° 씩이고 첫 점이 12시다.
    // 1 아래 깊이는 1 이고, 깊이를 주지 않으면 2 다
    void egoShowcaseDepthOne()
    {
        GraphLayoutOutput out;
        QVERIFY(RunLayoutWithFocus(m_graph, EGraphLayoutType::EGO, QString(), 1, &out));
        QCOMPARE(out.vecNodes.size(), kEgoDepthOneNodes);
        QCOMPARE(out.vecEdges.size(), kEgoDepthOneEdges);
        QStringList vecExpectedIds { DecodeUtf8(kEgoDefaultFocus) };
        for (const ExpectedEgoNode& expected : kEgoRingOne)
            vecExpectedIds << DecodeUtf8(expected.pId);
        QCOMPARE(ListNodeIds(out), vecExpectedIds);
        for (int iNode = 0; iNode < kEgoRingOne.size(); ++iNode)
        {
            const double dAngle = GetAngleDegrees(out.vecNodes[0].center, out.vecNodes[iNode + 1].center);
            QVERIFY2(IsSameAngle(dAngle, kTwelveOClock + iNode * kEgoDepthOneStep),
                     qUtf8Printable(out.vecNodes[iNode + 1].strId + QStringLiteral(" %1").arg(dAngle)));
        }
        QStringList vecEdges;
        for (const LayoutEdge& edge : out.vecEdges)
            vecEdges << DescribeLayoutEdge(out, edge);
        const QString strDiff = DiffItems(QStringLiteral("edges"), vecEdges, kEgoEdgeList.mid(0, kEgoDepthOneEdges));
        QVERIFY2(strDiff.isEmpty(), qUtf8Printable(strDiff));
        const QString strCheck = CheckEgoLayout(m_graph, out, DecodeUtf8(kEgoDefaultFocus), 1);
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));

        for (const int iDepth : { 0, -3 })
        {
            GraphLayoutOutput shallow;
            QVERIFY(RunLayoutWithFocus(m_graph, EGraphLayoutType::EGO, QString(), iDepth, &shallow));
            QCOMPARE(DescribeLayout(shallow), DescribeLayout(out));
        }
        QCOMPARE(GraphLayoutInput().iDepth, kEgoDefaultDepth);
        QCOMPARE(GraphLayoutInput().strFocusId, QString());
    }

    // 이웃의 초점: 받은 id 가 보이는 점이면 그 점. 비었거나 없는 id · 숨은 개념 · 개념 파일이 없는 id 는 기본 초점과 같은 배치다.
    // 선 없는 점(COCO)이 초점이면 그 점 하나다
    void egoFocusChoice()
    {
        const QStringList vecDefault = DescribeLayout(GetOutput(EGraphLayoutType::EGO));
        for (const QString& strFocus : { QString(), QStringLiteral("no-such-concept"), DecodeUtf8(kHiddenConcept), DecodeUtf8(kMissingConcept),
                                         DecodeUtf8(kEgoDefaultFocus) })
        {
            GraphLayoutOutput out;
            QVERIFY(RunLayoutWithFocus(m_graph, EGraphLayoutType::EGO, strFocus, kEgoDefaultDepth, &out));
            QVERIFY2(DescribeLayout(out) == vecDefault, qUtf8Printable(strFocus));
        }

        GraphLayoutOutput edgeless;
        QVERIFY(RunLayoutWithFocus(m_graph, EGraphLayoutType::EGO, DecodeUtf8(kEdgelessConcept), kEgoDefaultDepth, &edgeless));
        QCOMPARE(ListNodeIds(edgeless), QStringList { DecodeUtf8(kEdgelessConcept) });
        QVERIFY(edgeless.vecNodes[0].bFocus);
        QVERIFY(edgeless.vecEdges.isEmpty());
        QCOMPARE(edgeless.bounds.size(), QSizeF(GraphLayoutSize::kNodeWidth, GraphLayoutSize::kNodeHeight));

        GraphLayoutOutput yolo;
        QVERIFY(RunLayoutWithFocus(m_graph, EGraphLayoutType::EGO, QStringLiteral("yolo"), kEgoDefaultDepth, &yolo));
        QCOMPARE(yolo.vecNodes[0].strId, QString("yolo"));
        const QString strCheck = CheckEgoLayout(m_graph, yolo, QStringLiteral("yolo"), kEgoDefaultDepth);
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));
    }

    // showcase 의 모든 점을 초점으로 깊이 1–3: 깊이 안의 점 · 선 전부, 걸음 수 순, 고리마다 같은 반지름(바깥이 더 크다),
    // 고리 안은 시계 방향, 겹침 없음(허브 1단계 검출기 · 잎 RoI 풀링 · 선 없는 점도). 같은 입력이면 같은 출력이다
    void egoEveryFocusKeepsRules()
    {
        for (const KnowledgeGraphNode& node : m_graph.GetNodes())
        {
            for (int iDepth = 1; iDepth <= kEgoMaxDepth; ++iDepth)
            {
                GraphLayoutOutput out;
                QVERIFY(RunLayoutWithFocus(m_graph, EGraphLayoutType::EGO, node.strId, iDepth, &out));
                const QString strCheck = CheckEgoLayout(m_graph, out, node.strId, iDepth);
                QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));
                GraphLayoutOutput again;
                QVERIFY(RunLayoutWithFocus(m_graph, EGraphLayoutType::EGO, node.strId, iDepth, &again));
                QCOMPARE(DescribeLayout(again), DescribeLayout(out));
            }
        }
    }

    // 1 걸음 점의 순서: 초점과 잇는 선의 관계 종류(어휘 순: 개선 · 사용 · 종류 · 해결 · 대안. 선이 둘 이상이면 앞의 종류),
    // 같으면 초점에서 나가는 선 → 들어오는 선 → 방향 없는 선, 같으면 점 번호(id 가 아니라 이름 열쇠) 순. 어휘에 없는 종류는 뒤다.
    // 모두 잎이라 쐐기가 같다(점 8 → 45° 씩)
    void egoRingOneOrder()
    {
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(
            { MakeConcept("f", "Focus"), MakeConcept("a", "Alpha"), MakeConcept("b", "Beta"), MakeConcept("c", "Gamma"), MakeConcept("d", "Delta"),
              MakeConcept("e", "Epsilon"), MakeConcept("g", "Eta"), MakeConcept("h", "Theta"), MakeConcept("a-u", "Upsilon") },
            { MakeEdge("f", "alternative", "a"),                                 // 방향 없는 대안
              MakeEdge("b", "improves", "f"),                                    // 들어오는 개선
              MakeEdge("f", "improves", "c"),                                    // 나가는 개선
              MakeEdge("f", "uses", "d"),                                        // 나가는 사용
              MakeEdge("e", "uses", "f"),                                        // 들어오는 사용
              MakeEdge("g", "isA", "f"), MakeEdge("f", "alternative", "g"),      // 들어오는 종류 · 대안 → 앞의 종류(종류)
              MakeEdge("f", "zzz", "h"),                                         // 어휘에 없는 종류
              MakeEdge("f", "solves", "a-u"), MakeEdge("a-u", "improves", "f") },  // 해결 · 들어오는 개선 → 개선. b 와 같아 이름 열쇠 순(Beta < Upsilon)
            {});
        GraphLayoutOutput out;
        QVERIFY(RunLayoutWithFocus(graph, EGraphLayoutType::EGO, QStringLiteral("f"), kEgoDefaultDepth, &out));
        const QStringList vecExpected { "f", "c", "b", "a-u", "d", "e", "g", "a", "h" };
        QCOMPARE(ListNodeIds(out), vecExpected);
        const double dStep = kFullTurn / (vecExpected.size() - 1);
        for (int iNode = 1; iNode < out.vecNodes.size(); ++iNode)
        {
            const double dAngle = GetAngleDegrees(out.vecNodes[0].center, out.vecNodes[iNode].center);
            QVERIFY2(IsSameAngle(dAngle, kTwelveOClock + (iNode - 1) * dStep), qUtf8Printable(out.vecNodes[iNode].strId + QStringLiteral(" %1").arg(dAngle)));
        }
        const QString strCheck = CheckEgoLayout(graph, out, QStringLiteral("f"), kEgoDefaultDepth);
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));
    }

    // 2 걸음부터: 부모는 앞 고리에서 이어진 점 가운데 그 고리 순서가 가장 앞인 점이다(점 번호가 앞인 A1 이 아니라 개선으로 이어 앞에 온 Z1).
    // 한 부모의 자식은 점 번호 순(W → X)이고, 쐐기는 잎 수에 비례한다: Z1(잎 2) 240° · A1(잎 1) 120°
    void egoParentsAndWedges()
    {
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(
            { MakeConcept("f", "F"), MakeConcept("z1", "Z1"), MakeConcept("a1", "A1"), MakeConcept("x", "X"), MakeConcept("w", "W") },
            { MakeEdge("f", "improves", "z1"), MakeEdge("f", "uses", "a1"), MakeEdge("x", "isA", "a1"), MakeEdge("x", "isA", "z1"),
              MakeEdge("w", "uses", "z1") },
            {});
        GraphLayoutOutput out;
        QVERIFY(RunLayoutWithFocus(graph, EGraphLayoutType::EGO, QStringLiteral("f"), kEgoDefaultDepth, &out));
        QCOMPARE(ListNodeIds(out), (QStringList { "f", "z1", "a1", "w", "x" }));
        const QVector<QPair<QString, double>> vecAngles { { "z1", -90 }, { "a1", 90 }, { "w", -150 }, { "x", -30 } };
        for (const QPair<QString, double>& pairAngle : vecAngles)
        {
            const double dAngle = GetAngleDegrees(out.vecNodes[0].center, GetCenter(out, pairAngle.first));
            QVERIFY2(IsSameAngle(dAngle, pairAngle.second), qUtf8Printable(pairAngle.first + QStringLiteral(" %1").arg(dAngle)));
        }
        QCOMPARE(out.vecEdges.size(), graph.GetEdges().size());
        const QString strCheck = CheckEgoLayout(graph, out, QStringLiteral("f"), kEgoDefaultDepth);
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));
    }

    // 사슬 f – a – b – c – d 를 깊이 3 으로: 점마다 쐐기가 한 바퀴라 모두 12시이고 d 는 넣지 않는다.
    // 별(잎 40 이 중심에 닿는다): 한 고리에 다 놓여도 겹치지 않는다
    void egoChainAndStar()
    {
        const KnowledgeGraph chain = KnowledgeGraph::BuildFromParts(
            { MakeConcept("f", "F"), MakeConcept("a", "A"), MakeConcept("b", "B"), MakeConcept("c", "C"), MakeConcept("d", "D") },
            { MakeEdge("a", "improves", "f"), MakeEdge("b", "improves", "a"), MakeEdge("c", "improves", "b"), MakeEdge("d", "improves", "c") },
            {});
        GraphLayoutOutput out;
        QVERIFY(RunLayoutWithFocus(chain, EGraphLayoutType::EGO, QStringLiteral("f"), 3, &out));
        QCOMPARE(ListNodeIds(out), (QStringList { "f", "a", "b", "c" }));
        for (int iNode = 1; iNode < out.vecNodes.size(); ++iNode)
            QVERIFY2(IsSameAngle(GetAngleDegrees(out.vecNodes[0].center, out.vecNodes[iNode].center), kTwelveOClock),
                     qUtf8Printable(out.vecNodes[iNode].strId));
        QCOMPARE(out.vecEdges.size(), 3);
        QString strCheck = CheckEgoLayout(chain, out, QStringLiteral("f"), 3);
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));

        QVector<KnowledgeConcept> vecConcepts { MakeConcept("hub", "Hub") };
        QVector<KnowledgeEdge> vecEdges;
        for (int iLeaf = 0; iLeaf < kDenseLeafCount; ++iLeaf)
        {
            const QString strId = QStringLiteral("leaf-%1").arg(iLeaf, kLeafNumberWidth, kDecimalBase, QLatin1Char('0'));
            vecConcepts << MakeConcept(strId, strId);
            vecEdges << MakeEdge(strId, "isA", "hub");
        }
        const KnowledgeGraph star = KnowledgeGraph::BuildFromParts(vecConcepts, vecEdges, {});
        QCOMPARE(KnowledgeGraphLayout::FindDefaultFocus(star), QString("hub"));
        QVERIFY(RunLayout(star, EGraphLayoutType::EGO, &out));
        QCOMPARE(out.vecNodes.size(), 1 + kDenseLeafCount);
        strCheck = CheckEgoLayout(star, out, QStringLiteral("hub"), kEgoDefaultDepth);
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));
    }

    // 나무 선과 둘레 선(design/MoreViews.md §6.1 · §7.3): 초점의 선과 점–부모를 잇는 선(방향은 보지 않는다)은 나무 선, 그 밖은 둘레 선(bContext)이다.
    // 기본 초점은 나무 선 11 · 둘레 선 7, 깊이 1 은 초점의 선 6 · 1 걸음 점끼리 3, 허브(1단계 검출기) 깊이 2 는 13 · 8 이다.
    // 부모와 자식을 잇는 선이 둘이면 둘 다 나무 선이다. 다른 보기에는 둘레 선이 없다
    void egoTreeAndContextEdges()
    {
        const auto funcSplit = [](const GraphLayoutOutput& out, QStringList* tree, QStringList* context)
        {
            for (const LayoutEdge& edge : out.vecEdges)
                (edge.bContext ? context : tree)->append(DescribeLayoutEdge(out, edge));
        };
        const auto funcCompare = [](const QString& what, const QStringList& actual, const QStringList& expected)
        {
            const QString strDiff = DiffItems(what, actual, expected);
            return strDiff.isEmpty() && actual.size() == expected.size() ? QString() : what + QStringLiteral(" %1/%2 ").arg(actual.size()).arg(expected.size()) + strDiff;
        };

        QStringList vecTree;
        QStringList vecContext;
        funcSplit(GetOutput(EGraphLayoutType::EGO), &vecTree, &vecContext);
        QCOMPARE(kEgoTreeEdgeList.size() + kEgoContextEdgeList.size(), kEgoEdges);
        QString strCheck = funcCompare(QStringLiteral("tree"), vecTree, kEgoTreeEdgeList) + funcCompare(QStringLiteral("context"), vecContext, kEgoContextEdgeList);
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));

        // 깊이 1: 초점의 선 6 은 나무 선, 1 걸음 점끼리 3 은 둘레 선
        GraphLayoutOutput shallow;
        QVERIFY(RunLayoutWithFocus(m_graph, EGraphLayoutType::EGO, QString(), 1, &shallow));
        vecTree.clear();
        vecContext.clear();
        funcSplit(shallow, &vecTree, &vecContext);
        constexpr int kFocusEdges = 6;
        strCheck = funcCompare(QStringLiteral("tree"), vecTree, kEgoEdgeList.mid(0, kFocusEdges))
                   + funcCompare(QStringLiteral("context"), vecContext, kEgoEdgeList.mid(kFocusEdges, kEgoDepthOneEdges - kFocusEdges));
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));

        // 허브 초점 깊이 2
        GraphLayoutOutput hub;
        QVERIFY(RunLayoutWithFocus(m_graph, EGraphLayoutType::EGO, DecodeUtf8(kHubFocus), kEgoDefaultDepth, &hub));
        vecTree.clear();
        vecContext.clear();
        funcSplit(hub, &vecTree, &vecContext);
        strCheck = funcCompare(QStringLiteral("tree"), vecTree, kHubTreeEdgeList) + funcCompare(QStringLiteral("context"), vecContext, kHubContextEdgeList);
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));
        strCheck = CheckEgoLayout(m_graph, hub, DecodeUtf8(kHubFocus), kEgoDefaultDepth);
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));

        // 합성: x 의 부모는 Z1(A1 과의 선은 둘레 선). w–z1 사이 선 둘은 둘 다 나무 선, 1 걸음 점끼리(a1 – z1) · 제 자신으로 가는 선은 둘레 선
        const KnowledgeGraph graph = KnowledgeGraph::BuildFromParts(
            { MakeConcept("f", "F"), MakeConcept("z1", "Z1"), MakeConcept("a1", "A1"), MakeConcept("x", "X"), MakeConcept("w", "W") },
            { MakeEdge("f", "improves", "z1"), MakeEdge("f", "uses", "a1"), MakeEdge("x", "isA", "a1"), MakeEdge("x", "isA", "z1"),
              MakeEdge("w", "uses", "z1"), MakeEdge("z1", "alternative", "w"), MakeEdge("a1", "uses", "z1"), MakeEdge("w", "isA", "w") },
            {});
        GraphLayoutOutput synthetic;
        QVERIFY(RunLayoutWithFocus(graph, EGraphLayoutType::EGO, QStringLiteral("f"), kEgoDefaultDepth, &synthetic));
        vecTree.clear();
        vecContext.clear();
        funcSplit(synthetic, &vecTree, &vecContext);
        strCheck = funcCompare(QStringLiteral("tree"), vecTree, { "f improves z1", "f uses a1", "x isA z1", "w uses z1", "z1 alternative w" })
                   + funcCompare(QStringLiteral("context"), vecContext, { "x isA a1", "a1 uses z1", "w isA w" });
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));
        strCheck = CheckEgoLayout(graph, synthetic, QStringLiteral("f"), kEgoDefaultDepth);
        QVERIFY2(strCheck.isEmpty(), qUtf8Printable(strCheck));

        // 다른 보기는 둘레 선을 쓰지 않는다
        for (const EGraphLayoutType eType : kAllTypes)
            if (eType != EGraphLayoutType::EGO)
                for (const LayoutEdge& edge : GetOutput(eType).vecEdges)
                    QVERIFY2(!edge.bContext, qUtf8Printable(GetTypeName(eType)));
    }

    // 다른 전략은 초점 · 깊이 칸을 보지 않는다: 그 칸을 채워도 출력이 비트까지 같다
    void otherLayoutsIgnoreFocusInput()
    {
        for (const EGraphLayoutType eType : kAllTypes)
        {
            if (eType == EGraphLayoutType::EGO)
                continue;
            for (const QPair<QString, int>& pairInput : QVector<QPair<QString, int>> { { QStringLiteral("yolo"), 5 }, { DecodeUtf8(kEdgelessConcept), 0 } })
            {
                GraphLayoutOutput out;
                QVERIFY(RunLayoutWithFocus(m_graph, eType, pairInput.first, pairInput.second, &out));
                QVERIFY2(DescribeLayout(out) == DescribeLayout(GetOutput(eType)), qUtf8Printable(GetTypeName(eType)));
            }
        }
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
