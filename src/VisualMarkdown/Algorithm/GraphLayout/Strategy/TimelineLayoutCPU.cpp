#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/TimelineLayoutCPU.h"

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutCommon.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/KnowledgeText.h"

#include <QMap>

#include <algorithm>
#include <limits>

namespace
{
const QString kOtherLaneLabel = QStringLiteral("그 밖");   // 분류 나무에도 개선 묶음에도 들지 않는 점의 갈래
constexpr int kOtherLaneAnchor = -1;
constexpr int kYearDigits = 4;
constexpr int kDecimalBase = 10;   // 눈금 연도 글의 진법

struct TimelineLane
{
    int iAnchor = kOtherLaneAnchor;   // 갈래의 기준 점(분류 뿌리 · 가장 옛 점). "그 밖" 은 -1
    QString strLabel;
    QString strLabelKey;              // 이름 열쇠로 견준다
    QVector<int> vecMembers;          // 점 번호 순
};

// 같은 갈래 · 같은 해의 점 하나가 쌓인 칸
struct TimelinePlacement
{
    int iNode;
    int iYear;
    int iStack;
};
}

bool TimelineLayoutCPU::Execute(const GraphLayoutInput& in, GraphLayoutOutput& out)
{
    using namespace GraphLayoutSize;
    out = GraphLayoutOutput();
    if (in.pGraph == nullptr)
        return false;
    const KnowledgeGraph& graph = *in.pGraph;
    const QVector<KnowledgeGraphNode>& vecNodes = graph.GetNodes();

    // 연도 없는 점은 이름 열쇠 순으로 따로 둔다
    int iFirstYear = std::numeric_limits<int>::max();
    for (const KnowledgeGraphNode& node : vecNodes)
    {
        if (node.iYear < 0)
            out.vecUnplaced << node.strId;
        else
            iFirstYear = std::min(iFirstYear, node.iYear);
    }
    if (out.vecUnplaced.size() == vecNodes.size())
        return true;

    // 갈래: 분류 나무의 뿌리 → 개선 묶음의 가장 옛 점 → "그 밖"
    const GraphLayoutCommon::IsAForest forest = GraphLayoutCommon::BuildIsAForest(graph);
    const GraphLayoutCommon::ImprovesGroups groups = GraphLayoutCommon::BuildImprovesGroups(graph);
    QMap<int, TimelineLane> mapLanes;   // 기준 점 → 갈래
    for (int iNode = 0; iNode < vecNodes.size(); ++iNode)
    {
        if (vecNodes[iNode].iYear < 0)
            continue;
        int iAnchor = kOtherLaneAnchor;
        if (forest.vecInForest[iNode])
            iAnchor = forest.vecRootOf[iNode];
        else if (groups.vecGroupOf[iNode] >= 0)
            iAnchor = groups.vecOldest[groups.vecGroupOf[iNode]];
        TimelineLane& lane = mapLanes[iAnchor];
        lane.iAnchor = iAnchor;
        lane.vecMembers.push_back(iNode);
    }
    QVector<TimelineLane> vecLanes;
    for (TimelineLane lane : mapLanes)
    {
        lane.strLabel = lane.iAnchor == kOtherLaneAnchor ? kOtherLaneLabel : vecNodes[lane.iAnchor].strName;
        lane.strLabelKey = KnowledgeText::MakeNameKey(lane.strLabel);
        vecLanes.push_back(lane);
    }
    // "그 밖" 은 점 수 · 이름과 상관없이 늘 마지막이다(design/Views.md §5).
    // 나머지는 점이 많은 갈래부터, 같으면 이름(이름 열쇠, 그다음 글자) 순, 그다음 기준 점 순
    std::sort(vecLanes.begin(), vecLanes.end(), [](const TimelineLane& a, const TimelineLane& b)
    {
        const bool bOtherA = a.iAnchor == kOtherLaneAnchor;
        const bool bOtherB = b.iAnchor == kOtherLaneAnchor;
        if (bOtherA != bOtherB)
            return bOtherB;
        if (a.vecMembers.size() != b.vecMembers.size())
            return a.vecMembers.size() > b.vecMembers.size();
        if (a.strLabelKey != b.strLabelKey)
            return a.strLabelKey < b.strLabelKey;
        if (a.strLabel != b.strLabel)
            return a.strLabel < b.strLabel;
        return a.iAnchor < b.iAnchor;
    });

    // x 는 (연도 − 첫 연도) × 한 해 간격. 같은 갈래 · 같은 해의 점은 이름 열쇠 순으로 아래로 쌓는다
    const double dRowPitch = kNodeHeight + kNodeGapY;
    const auto funcYearX = [iFirstYear](int year)
    {
        return kNodeWidth / 2 + (year - iFirstYear) * kTimelineYearGap;
    };
    QVector<int> vecLayoutIndex(vecNodes.size(), -1);
    QMap<int, bool> mapYears;   // 있는 해(작은 것부터)
    double dLaneTop = 0;
    for (const TimelineLane& lane : vecLanes)
    {
        QMap<int, int> mapStackByYear;
        QVector<TimelinePlacement> vecPlacements;
        int iMaxStack = 0;
        for (const int iNode : lane.vecMembers)
        {
            const int iYear = vecNodes[iNode].iYear;
            const int iStack = mapStackByYear[iYear]++;
            iMaxStack = std::max(iMaxStack, iStack + 1);
            vecPlacements.push_back({ iNode, iYear, iStack });
            mapYears.insert(iYear, true);
        }
        std::sort(vecPlacements.begin(), vecPlacements.end(), [](const TimelinePlacement& a, const TimelinePlacement& b)
        {
            if (a.iYear != b.iYear)
                return a.iYear < b.iYear;
            return a.iStack < b.iStack;
        });
        for (const TimelinePlacement& placement : vecPlacements)
        {
            const QPointF ptCenter(funcYearX(placement.iYear), dLaneTop + kLanePadding + placement.iStack * dRowPitch + kNodeHeight / 2);
            vecLayoutIndex[placement.iNode] = out.vecNodes.size();
            out.vecNodes.push_back(GraphLayoutCommon::MakeLayoutNode(vecNodes[placement.iNode], ptCenter));
        }
        const double dLaneBottom = dLaneTop + 2 * kLanePadding + iMaxStack * kNodeHeight + (iMaxStack - 1) * kNodeGapY;
        out.vecLanes.push_back({ lane.strLabel, dLaneTop, dLaneBottom });
        dLaneTop = dLaneBottom + kLaneGap;
    }
    for (auto it = mapYears.cbegin(); it != mapYears.cend(); ++it)
        out.vecTicks.push_back({ funcYearX(it.key()), QStringLiteral("%1").arg(it.key(), kYearDigits, kDecimalBase, QLatin1Char('0')) });

    // 선: 놓인 두 점 사이의 isA · improves 선(그래프의 선 순서)
    const QVector<KnowledgeGraphEdge>& vecEdges = graph.GetEdges();
    for (int iEdge = 0; iEdge < vecEdges.size(); ++iEdge)
    {
        const KnowledgeGraphEdge& edge = vecEdges[iEdge];
        if (!GraphLayoutCommon::IsKind(edge, GraphLayoutCommon::kIsAKind) && !GraphLayoutCommon::IsKind(edge, GraphLayoutCommon::kImprovesKind))
            continue;
        if (vecLayoutIndex[edge.iFrom] < 0 || vecLayoutIndex[edge.iTo] < 0)
            continue;
        out.vecEdges.push_back(GraphLayoutCommon::MakeLayoutEdge(graph, iEdge, vecLayoutIndex[edge.iFrom], vecLayoutIndex[edge.iTo]));
    }
    out.bounds = GraphLayoutCommon::ComputeLaneBounds(out.vecNodes, out.vecLanes);
    return true;
}

EGraphLayoutType TimelineLayoutCPU::GetType() const
{
    return EGraphLayoutType::TIMELINE;
}
