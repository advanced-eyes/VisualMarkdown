#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/TreeLayoutCPU.h"

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutCommon.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"

bool TreeLayoutCPU::Execute(const GraphLayoutInput& in, GraphLayoutOutput& out)
{
    using namespace GraphLayoutSize;
    out = GraphLayoutOutput();
    if (in.pGraph == nullptr)
        return false;
    const KnowledgeGraph& graph = *in.pGraph;
    const QVector<KnowledgeGraphNode>& vecNodes = graph.GetNodes();
    const GraphLayoutCommon::IsAForest forest = GraphLayoutCommon::BuildIsAForest(graph);

    const double dSlotPitch = kNodeWidth + kNodeGapX;
    const double dLevelPitch = kNodeHeight + kTreeLevelGap;
    QVector<int> vecLayoutIndex(vecNodes.size(), -1);
    QVector<int> vecDepth(vecNodes.size(), 0);
    QVector<double> vecX(vecNodes.size(), 0);
    double dNextLeaf = 0;   // 다음 잎 상자의 왼쪽
    for (const int iRoot : forest.vecRoots)
    {
        // 앞차례(부모 다음 자식, 자식은 이름 열쇠 순). 되부름 대신 쌓기를 쓴다
        QVector<int> vecOrder;
        QVector<int> vecStack { iRoot };
        vecDepth[iRoot] = 0;
        while (!vecStack.isEmpty())
        {
            const int iNode = vecStack.takeLast();
            vecOrder.push_back(iNode);
            // 첫 자식이 먼저 나오도록 거꾸로 쌓는다
            const QVector<int>& vecChildren = forest.vecChildren[iNode];
            for (int iChild = vecChildren.size() - 1; iChild >= 0; --iChild)
            {
                vecDepth[vecChildren[iChild]] = vecDepth[iNode] + 1;
                vecStack.push_back(vecChildren[iChild]);
            }
        }
        // 잎은 앞차례대로 이어진 칸, 부모는 첫 자식과 마지막 자식의 가운데(뒤에서부터 돌면 자식이 먼저 정해진다)
        for (const int iNode : vecOrder)
        {
            if (!forest.vecChildren[iNode].isEmpty())
                continue;
            vecX[iNode] = dNextLeaf + kNodeWidth / 2;
            dNextLeaf += dSlotPitch;
        }
        for (int iOrder = vecOrder.size() - 1; iOrder >= 0; --iOrder)
        {
            const QVector<int>& vecChildren = forest.vecChildren[vecOrder[iOrder]];
            if (!vecChildren.isEmpty())
                vecX[vecOrder[iOrder]] = (vecX[vecChildren.first()] + vecX[vecChildren.last()]) / 2;
        }
        for (const int iNode : vecOrder)
        {
            vecLayoutIndex[iNode] = out.vecNodes.size();
            out.vecNodes.push_back(GraphLayoutCommon::MakeLayoutNode(vecNodes[iNode], QPointF(vecX[iNode], kNodeHeight / 2 + vecDepth[iNode] * dLevelPitch)));
        }
        dNextLeaf += kTreeGap;
    }

    // 선: isA 선 전부(나무 선이 아닌 둘째 부모 선 · 고리를 닫는 선도 그린다)
    const QVector<KnowledgeGraphEdge>& vecEdges = graph.GetEdges();
    for (int iEdge = 0; iEdge < vecEdges.size(); ++iEdge)
        if (GraphLayoutCommon::IsKind(vecEdges[iEdge], GraphLayoutCommon::kIsAKind))
            out.vecEdges.push_back(GraphLayoutCommon::MakeLayoutEdge(graph, iEdge, vecLayoutIndex[vecEdges[iEdge].iFrom], vecLayoutIndex[vecEdges[iEdge].iTo]));
    out.bounds = GraphLayoutCommon::ComputeNodeBounds(out.vecNodes);
    return true;
}

EGraphLayoutType TreeLayoutCPU::GetType() const
{
    return EGraphLayoutType::TREE;
}
