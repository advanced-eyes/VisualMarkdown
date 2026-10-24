#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/BipartiteLayoutCPU.h"

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutCommon.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"

#include <algorithm>

bool BipartiteLayoutCPU::Execute(const GraphLayoutInput& in, GraphLayoutOutput& out)
{
    using namespace GraphLayoutSize;
    out = GraphLayoutOutput();
    if (in.pGraph == nullptr)
        return false;
    const KnowledgeGraph& graph = *in.pGraph;
    const QVector<KnowledgeGraphNode>& vecNodes = graph.GetNodes();
    const QVector<KnowledgeGraphDocument>& vecDocuments = graph.GetDocuments();
    const QVector<KnowledgeGraphLink>& vecLinks = graph.GetLinks();
    if (vecDocuments.isEmpty())
        return true;

    // 개념마다 가리키는 문서 행의 합과 수. 무게 중심(합 / 수)은 나누지 않고 엇곱으로 견주어 정확히 가른다
    QVector<qint64> vecRowSum(vecNodes.size(), 0);
    QVector<qint64> vecRowCount(vecNodes.size(), 0);
    for (const KnowledgeGraphLink& link : vecLinks)
    {
        vecRowSum[link.iNode] += link.iDocument;
        ++vecRowCount[link.iNode];
    }
    QVector<int> vecConcepts;
    for (int iNode = 0; iNode < vecNodes.size(); ++iNode)
        if (vecRowCount[iNode] > 0)
            vecConcepts.push_back(iNode);
    std::sort(vecConcepts.begin(), vecConcepts.end(), [&vecRowSum, &vecRowCount](int a, int b)
    {
        const qint64 llLeft = vecRowSum[a] * vecRowCount[b];
        const qint64 llRight = vecRowSum[b] * vecRowCount[a];
        if (llLeft != llRight)
            return llLeft < llRight;
        return a < b;
    });

    // 두 열을 같은 높이에 고르게 편다(행 간격은 점 상자 + 틈보다 좁아지지 않는다)
    const int iDocumentCount = vecDocuments.size();
    const int iConceptCount = vecConcepts.size();
    const double dSpan = std::max(iDocumentCount, iConceptCount) * (kNodeHeight + kNodeGapY);
    const double dDocumentPitch = dSpan / iDocumentCount;
    const double dConceptPitch = iConceptCount > 0 ? dSpan / iConceptCount : 0;
    const double dDocumentX = kNodeWidth / 2;
    const double dConceptX = dDocumentX + kNodeWidth + kBipartiteColumnGap;
    for (int iDocument = 0; iDocument < iDocumentCount; ++iDocument)
    {
        LayoutNode node;
        node.strId = vecDocuments[iDocument].strId;
        node.strLabel = vecDocuments[iDocument].strTitle;
        node.center = QPointF(dDocumentX, (iDocument + 0.5) * dDocumentPitch);
        node.bDocument = true;
        out.vecNodes.push_back(node);
    }
    QVector<int> vecLayoutIndex(vecNodes.size(), -1);
    for (int iRank = 0; iRank < iConceptCount; ++iRank)
    {
        const int iNode = vecConcepts[iRank];
        vecLayoutIndex[iNode] = out.vecNodes.size();
        out.vecNodes.push_back(GraphLayoutCommon::MakeLayoutNode(vecNodes[iNode], QPointF(dConceptX, (iRank + 0.5) * dConceptPitch)));
    }

    // 연결마다 선 하나(방향 없음), 무게는 가리킨 횟수
    for (const KnowledgeGraphLink& link : vecLinks)
    {
        LayoutEdge edge;
        edge.iFrom = link.iDocument;
        edge.iTo = vecLayoutIndex[link.iNode];
        edge.bDirected = false;
        edge.iWeight = link.iWeight;
        out.vecEdges.push_back(edge);
    }
    out.bounds = GraphLayoutCommon::ComputeNodeBounds(out.vecNodes);
    return true;
}

EGraphLayoutType BipartiteLayoutCPU::GetType() const
{
    return EGraphLayoutType::BIPARTITE;
}
