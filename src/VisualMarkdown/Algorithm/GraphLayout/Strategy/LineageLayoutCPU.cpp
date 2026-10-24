#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/LineageLayoutCPU.h"

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutCommon.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"

#include <QMap>

#include <algorithm>

namespace
{
// 갈래 안 점 하나의 자리: 개선 깊이 열과 그 열에서 쌓인 칸
struct LanePlacement
{
    int iNode;
    int iColumn;
    int iStack;
};
}

bool LineageLayoutCPU::Execute(const GraphLayoutInput& in, GraphLayoutOutput& out)
{
    using namespace GraphLayoutSize;
    out = GraphLayoutOutput();
    if (in.pGraph == nullptr)
        return false;
    const KnowledgeGraph& graph = *in.pGraph;
    const QVector<KnowledgeGraphNode>& vecNodes = graph.GetNodes();
    const GraphLayoutCommon::ImprovesGroups groups = GraphLayoutCommon::BuildImprovesGroups(graph);

    const double dColumnPitch = kNodeWidth + kLineageColumnGap;
    const double dRowPitch = kNodeHeight + kNodeGapY;
    QVector<int> vecLayoutIndex(vecNodes.size(), -1);
    double dLaneTop = 0;
    for (int iGroup = 0; iGroup < groups.vecGroups.size(); ++iGroup)
    {
        // 열은 개선 깊이(옛것이 0). 같은 열의 점은 이름 열쇠 순(점 번호 순)으로 아래로 쌓는다
        QMap<int, int> mapStackByColumn;
        QVector<LanePlacement> vecPlacements;
        int iMaxStack = 0;
        for (const int iNode : groups.vecGroups[iGroup])
        {
            const int iColumn = groups.vecDepth[iNode];
            const int iStack = mapStackByColumn[iColumn]++;
            iMaxStack = std::max(iMaxStack, iStack + 1);
            vecPlacements.push_back({ iNode, iColumn, iStack });
        }
        std::sort(vecPlacements.begin(), vecPlacements.end(), [](const LanePlacement& a, const LanePlacement& b)
        {
            if (a.iColumn != b.iColumn)
                return a.iColumn < b.iColumn;
            return a.iStack < b.iStack;
        });
        for (const LanePlacement& placement : vecPlacements)
        {
            const QPointF ptCenter(kNodeWidth / 2 + placement.iColumn * dColumnPitch,
                                   dLaneTop + kLanePadding + placement.iStack * dRowPitch + kNodeHeight / 2);
            vecLayoutIndex[placement.iNode] = out.vecNodes.size();
            out.vecNodes.push_back(GraphLayoutCommon::MakeLayoutNode(vecNodes[placement.iNode], ptCenter));
        }
        const double dLaneBottom = dLaneTop + 2 * kLanePadding + iMaxStack * kNodeHeight + (iMaxStack - 1) * kNodeGapY;
        out.vecLanes.push_back({ vecNodes[groups.vecOldest[iGroup]].strName, dLaneTop, dLaneBottom });
        dLaneTop = dLaneBottom + kLaneGap;
    }

    // 선은 새것(from) → 옛것(to), 그래프의 선 순서
    const QVector<KnowledgeGraphEdge>& vecEdges = graph.GetEdges();
    for (int iEdge = 0; iEdge < vecEdges.size(); ++iEdge)
        if (GraphLayoutCommon::IsKind(vecEdges[iEdge], GraphLayoutCommon::kImprovesKind))
            out.vecEdges.push_back(GraphLayoutCommon::MakeLayoutEdge(graph, iEdge, vecLayoutIndex[vecEdges[iEdge].iFrom], vecLayoutIndex[vecEdges[iEdge].iTo]));
    out.bounds = GraphLayoutCommon::ComputeLaneBounds(out.vecNodes, out.vecLanes);
    return true;
}

EGraphLayoutType LineageLayoutCPU::GetType() const
{
    return EGraphLayoutType::LINEAGE;
}
