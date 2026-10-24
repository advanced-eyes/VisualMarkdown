#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/MatrixLayoutCPU.h"

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutCommon.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <QMap>
#include <QPair>

#include <algorithm>
#include <limits>

bool MatrixLayoutCPU::Execute(const GraphLayoutInput& in, GraphLayoutOutput& out)
{
    using namespace GraphLayoutSize;
    out = GraphLayoutOutput();
    if (in.pGraph == nullptr)
        return false;
    const KnowledgeGraph& graph = *in.pGraph;
    const QVector<KnowledgeGraphNode>& vecNodes = graph.GetNodes();

    // 행 · 열 순서: 덩어리 순, 덩어리 안에서는 점 번호(이름 열쇠) 순. 점은 대각선 칸 가운데에 둔다
    QVector<int> vecPosition(vecNodes.size(), -1);
    for (const QVector<int>& vecComponent : graph.GetComponents())
    {
        for (const int iNode : vecComponent)
        {
            const int iPosition = out.vecNodes.size();
            const double dCenter = (iPosition + 0.5) * kMatrixCellSize;
            vecPosition[iNode] = iPosition;
            out.vecMatrixOrder << vecNodes[iNode].strId;
            out.vecNodes.push_back(GraphLayoutCommon::MakeLayoutNode(vecNodes[iNode], QPointF(dCenter, dCenter)));
        }
    }

    // 칸: 방향 있는 선은 (from 행, to 열), 방향 없는 선은 두 칸. 대각선(제 자신으로 가는 선)은 비운다
    QMap<QPair<int, int>, QStringList> mapCells;   // (행, 열) → 종류(행 · 열 순으로 돈다)
    const auto funcAddKind = [&mapCells](int row, int column, const QString& kind)
    {
        if (row == column)
            return;
        QStringList& vecKinds = mapCells[qMakePair(row, column)];
        if (!vecKinds.contains(kind))
            vecKinds << kind;
    };
    const QVector<KnowledgeGraphEdge>& vecEdges = graph.GetEdges();
    for (int iEdge = 0; iEdge < vecEdges.size(); ++iEdge)
    {
        const KnowledgeGraphEdge& edge = vecEdges[iEdge];
        const int iRow = vecPosition[edge.iFrom];
        const int iColumn = vecPosition[edge.iTo];
        funcAddKind(iRow, iColumn, edge.edge.strKind);
        if (!edge.bDirected)
            funcAddKind(iColumn, iRow, edge.edge.strKind);
        out.vecEdges.push_back(GraphLayoutCommon::MakeLayoutEdge(graph, iEdge, iRow, iColumn));
    }

    // 한 칸의 종류는 어휘 순, 어휘에 없는 종류는 그 뒤에 코드 순
    const QStringList vecVocabularyKinds = KnowledgeVocabulary::Get().GetRelationCodes();
    const auto funcRank = [&vecVocabularyKinds](const QString& kind)
    {
        const int iRank = vecVocabularyKinds.indexOf(kind);
        return iRank >= 0 ? iRank : std::numeric_limits<int>::max();
    };
    for (auto it = mapCells.cbegin(); it != mapCells.cend(); ++it)
    {
        QStringList vecKinds = it.value();
        std::sort(vecKinds.begin(), vecKinds.end(), [&funcRank](const QString& a, const QString& b)
        {
            const int iRankA = funcRank(a);
            const int iRankB = funcRank(b);
            if (iRankA != iRankB)
                return iRankA < iRankB;
            return a < b;
        });
        out.vecCells.push_back({ it.key().first, it.key().second, vecKinds });
    }
    if (!out.vecNodes.isEmpty())
        out.bounds = QRectF(0, 0, out.vecNodes.size() * kMatrixCellSize, out.vecNodes.size() * kMatrixCellSize);
    return true;
}

EGraphLayoutType MatrixLayoutCPU::GetType() const
{
    return EGraphLayoutType::MATRIX;
}
