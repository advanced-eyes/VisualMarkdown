#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutCommon.h"

#include <QLatin1String>

#include <algorithm>
#include <limits>

namespace GraphLayoutCommon
{
namespace
{
// 뿌리에서 나무의 자식을 따라 닿는 점에 그 뿌리를 적는다(이미 적힌 점에서 멈춘다)
void MarkTree(IsAForest& forest, int root)
{
    forest.vecRootOf[root] = root;
    QVector<int> vecStack { root };
    while (!vecStack.isEmpty())
    {
        const int iNode = vecStack.takeLast();
        for (const int iChild : forest.vecChildren[iNode])
        {
            if (forest.vecRootOf[iChild] >= 0)
                continue;
            forest.vecRootOf[iChild] = root;
            vecStack.push_back(iChild);
        }
    }
}

// 고리에 걸린 점 start 에서 부모를 따라 올라가, 처음 다시 만난 점부터의 고리에서 점 번호가 가장 작은 점.
// 고리를 못 찾으면(이미 나무에 든 점을 만나면) start 다
int FindCycleRoot(const IsAForest& forest, int start)
{
    QVector<int> vecWalk;
    QVector<bool> vecOnWalk(forest.vecParent.size(), false);
    int iNode = start;
    while (iNode >= 0 && forest.vecRootOf[iNode] < 0 && !vecOnWalk[iNode])
    {
        vecOnWalk[iNode] = true;
        vecWalk.push_back(iNode);
        iNode = forest.vecParent[iNode];
    }
    if (iNode < 0 || forest.vecRootOf[iNode] >= 0)
        return start;
    return *std::min_element(vecWalk.cbegin() + vecWalk.indexOf(iNode), vecWalk.cend());
}

// a 가 b 보다 옛것인지: 연도가 이른 것이 먼저, 연도 없는 점은 뒤. 같으면 false(점 번호 순을 그대로 둔다)
bool IsOlder(const KnowledgeGraphNode& a, const KnowledgeGraphNode& b)
{
    if (a.iYear < 0)
        return false;
    return b.iYear < 0 || a.iYear < b.iYear;
}
}

bool IsKind(const KnowledgeGraphEdge& edge, const char* kind)
{
    return edge.edge.strKind == QLatin1String(kind);
}

IsAForest BuildIsAForest(const KnowledgeGraph& graph)
{
    const int iNodeCount = graph.GetNodes().size();
    IsAForest forest;
    forest.vecInForest.fill(false, iNodeCount);
    forest.vecParent.fill(-1, iNodeCount);
    forest.vecChildren.resize(iNodeCount);
    forest.vecRootOf.fill(-1, iNodeCount);

    // 나무의 부모: 이름 열쇠로 앞선 부모(점 번호가 작은 것)
    for (const KnowledgeGraphEdge& edge : graph.GetEdges())
    {
        if (!IsKind(edge, kIsAKind))
            continue;
        forest.vecInForest[edge.iFrom] = true;
        forest.vecInForest[edge.iTo] = true;
        if (edge.iFrom == edge.iTo)
            continue;
        int& iParent = forest.vecParent[edge.iFrom];
        iParent = iParent < 0 ? edge.iTo : std::min(iParent, edge.iTo);
    }
    // 자식은 점 번호 순으로 모인다
    for (int iNode = 0; iNode < iNodeCount; ++iNode)
        if (forest.vecParent[iNode] >= 0)
            forest.vecChildren[forest.vecParent[iNode]].push_back(iNode);

    QVector<int> vecRoots;
    for (int iNode = 0; iNode < iNodeCount; ++iNode)
    {
        if (!forest.vecInForest[iNode] || forest.vecParent[iNode] >= 0)
            continue;
        vecRoots.push_back(iNode);
        MarkTree(forest, iNode);
    }
    // 뿌리에서 닿지 않은 점: 고리 안에서 뿌리를 골라 그 부모 선을 끊는다. 끊은 뒤에는 그 뿌리 아래가 나무가 된다
    for (int iNode = 0; iNode < iNodeCount; ++iNode)
    {
        if (!forest.vecInForest[iNode] || forest.vecRootOf[iNode] >= 0)
            continue;
        const int iRoot = FindCycleRoot(forest, iNode);
        const int iCutParent = forest.vecParent[iRoot];
        if (iCutParent >= 0)
        {
            forest.vecChildren[iCutParent].removeOne(iRoot);
            forest.vecParent[iRoot] = -1;
        }
        vecRoots.push_back(iRoot);
        MarkTree(forest, iRoot);
    }

    // 큰 나무부터, 같으면 뿌리 이름 열쇠 순
    QVector<int> vecSizeOfRoot(iNodeCount, 0);
    for (int iNode = 0; iNode < iNodeCount; ++iNode)
        if (forest.vecRootOf[iNode] >= 0)
            ++vecSizeOfRoot[forest.vecRootOf[iNode]];
    std::sort(vecRoots.begin(), vecRoots.end(), [&vecSizeOfRoot](int a, int b)
    {
        if (vecSizeOfRoot[a] != vecSizeOfRoot[b])
            return vecSizeOfRoot[a] > vecSizeOfRoot[b];
        return a < b;
    });
    forest.vecRoots = vecRoots;
    for (const int iRoot : vecRoots)
        forest.vecTreeSizes.push_back(vecSizeOfRoot[iRoot]);
    return forest;
}

ImprovesGroups BuildImprovesGroups(const KnowledgeGraph& graph)
{
    const QVector<KnowledgeGraphNode>& vecNodes = graph.GetNodes();
    const int iNodeCount = vecNodes.size();
    ImprovesGroups groups;
    groups.vecGroupOf.fill(-1, iNodeCount);
    groups.vecDepth.fill(-1, iNodeCount);

    QVector<bool> vecInGroup(iNodeCount, false);
    QVector<QVector<int>> vecNeighbors(iNodeCount);   // 방향을 보지 않는 이웃
    QVector<QVector<int>> vecImproved(iNodeCount);    // 점 → 그 점이 개선한 옛것(제 자신은 뺀다)
    for (const KnowledgeGraphEdge& edge : graph.GetEdges())
    {
        if (!IsKind(edge, kImprovesKind))
            continue;
        vecInGroup[edge.iFrom] = true;
        vecInGroup[edge.iTo] = true;
        if (edge.iFrom == edge.iTo)
            continue;
        vecNeighbors[edge.iFrom].push_back(edge.iTo);
        vecNeighbors[edge.iTo].push_back(edge.iFrom);
        vecImproved[edge.iFrom].push_back(edge.iTo);
    }
    for (QVector<int>& vecTargets : vecImproved)
    {
        std::sort(vecTargets.begin(), vecTargets.end());
        vecTargets.erase(std::unique(vecTargets.begin(), vecTargets.end()), vecTargets.end());
    }

    // 개선 깊이: 깊이 우선으로 돌아 끝난 점에서 정한다. 돌고 있는 점으로 가는 선(고리를 닫는 선)은 보지 않는다.
    // 점이 많아도 넘치지 않게 되부름 대신 쌓기를 쓴다
    enum class EVisit { NEW, ACTIVE, DONE };
    struct Frame
    {
        int iNode;
        int iNext;   // 다음에 볼 vecImproved 자리
    };
    QVector<EVisit> vecVisit(iNodeCount, EVisit::NEW);
    for (int iStart = 0; iStart < iNodeCount; ++iStart)
    {
        if (!vecInGroup[iStart] || vecVisit[iStart] != EVisit::NEW)
            continue;
        QVector<Frame> vecStack { { iStart, 0 } };
        vecVisit[iStart] = EVisit::ACTIVE;
        groups.vecDepth[iStart] = 0;
        while (!vecStack.isEmpty())
        {
            const int iNode = vecStack.last().iNode;
            const QVector<int>& vecTargets = vecImproved[iNode];
            if (vecStack.last().iNext < vecTargets.size())
            {
                const int iTarget = vecTargets[vecStack.last().iNext++];
                if (vecVisit[iTarget] == EVisit::NEW)
                {
                    vecVisit[iTarget] = EVisit::ACTIVE;
                    groups.vecDepth[iTarget] = 0;
                    vecStack.push_back({ iTarget, 0 });
                }
                else if (vecVisit[iTarget] == EVisit::DONE)
                {
                    groups.vecDepth[iNode] = std::max(groups.vecDepth[iNode], groups.vecDepth[iTarget] + 1);
                }
                continue;
            }
            vecVisit[iNode] = EVisit::DONE;
            vecStack.removeLast();
            if (!vecStack.isEmpty())
            {
                const int iCaller = vecStack.last().iNode;
                groups.vecDepth[iCaller] = std::max(groups.vecDepth[iCaller], groups.vecDepth[iNode] + 1);
            }
        }
    }

    // 묶음: 방향을 보지 않고 이어진 점들. 가장 옛 점은 깊이 0 인 점 가운데 고른다(묶음마다 적어도 하나 있다)
    QVector<QVector<int>> vecGroups;
    QVector<int> vecOldest;
    QVector<bool> vecSeen(iNodeCount, false);
    for (int iStart = 0; iStart < iNodeCount; ++iStart)
    {
        if (!vecInGroup[iStart] || vecSeen[iStart])
            continue;
        vecSeen[iStart] = true;
        QVector<int> vecMembers { iStart };
        for (int iNext = 0; iNext < vecMembers.size(); ++iNext)
        {
            for (const int iNeighbor : vecNeighbors[vecMembers[iNext]])
            {
                if (vecSeen[iNeighbor])
                    continue;
                vecSeen[iNeighbor] = true;
                vecMembers.push_back(iNeighbor);
            }
        }
        std::sort(vecMembers.begin(), vecMembers.end());
        int iOldest = -1;
        for (const int iMember : vecMembers)
            if (groups.vecDepth[iMember] == 0 && (iOldest < 0 || IsOlder(vecNodes[iMember], vecNodes[iOldest])))
                iOldest = iMember;
        vecGroups.push_back(vecMembers);
        vecOldest.push_back(iOldest >= 0 ? iOldest : vecMembers.first());
    }

    // 큰 묶음부터, 같으면 가장 옛 점의 이름 열쇠 순
    QVector<int> vecOrder(vecGroups.size());
    for (int iGroup = 0; iGroup < vecOrder.size(); ++iGroup)
        vecOrder[iGroup] = iGroup;
    std::sort(vecOrder.begin(), vecOrder.end(), [&vecGroups, &vecOldest](int a, int b)
    {
        if (vecGroups[a].size() != vecGroups[b].size())
            return vecGroups[a].size() > vecGroups[b].size();
        return vecOldest[a] < vecOldest[b];
    });
    for (const int iGroup : vecOrder)
    {
        for (const int iMember : vecGroups[iGroup])
            groups.vecGroupOf[iMember] = groups.vecGroups.size();
        groups.vecGroups.push_back(vecGroups[iGroup]);
        groups.vecOldest.push_back(vecOldest[iGroup]);
    }
    return groups;
}

int FindDefaultFocus(const KnowledgeGraph& graph)
{
    // 앞에서부터 보며 더 나은 점만 고르므로, 같으면 점 번호가 앞인 점이 남는다
    const QVector<KnowledgeGraphNode>& vecNodes = graph.GetNodes();
    int iBest = -1;
    for (int iNode = 0; iNode < vecNodes.size(); ++iNode)
    {
        if (iBest >= 0 && vecNodes[iNode].iEdgeCount < vecNodes[iBest].iEdgeCount)
            continue;
        if (iBest >= 0 && vecNodes[iNode].iEdgeCount == vecNodes[iBest].iEdgeCount && vecNodes[iNode].iDocumentCount <= vecNodes[iBest].iDocumentCount)
            continue;
        iBest = iNode;
    }
    return iBest;
}

QRectF MakeNodeBox(const QPointF& center)
{
    return QRectF(center.x() - GraphLayoutSize::kNodeWidth / 2, center.y() - GraphLayoutSize::kNodeHeight / 2,
                  GraphLayoutSize::kNodeWidth, GraphLayoutSize::kNodeHeight);
}

QRectF ComputeNodeBounds(const QVector<LayoutNode>& nodes)
{
    return ComputeLaneBounds(nodes, QVector<LayoutLane>());
}

QRectF ComputeLaneBounds(const QVector<LayoutNode>& nodes, const QVector<LayoutLane>& lanes)
{
    if (nodes.isEmpty())
        return QRectF();
    double dLeft = std::numeric_limits<double>::max();
    double dTop = std::numeric_limits<double>::max();
    double dRight = std::numeric_limits<double>::lowest();
    double dBottom = std::numeric_limits<double>::lowest();
    for (const LayoutNode& node : nodes)
    {
        const QRectF rectBox = MakeNodeBox(node.center);
        dLeft = std::min(dLeft, rectBox.left());
        dTop = std::min(dTop, rectBox.top());
        dRight = std::max(dRight, rectBox.right());
        dBottom = std::max(dBottom, rectBox.bottom());
    }
    for (const LayoutLane& lane : lanes)
    {
        dTop = std::min(dTop, lane.dTop);
        dBottom = std::max(dBottom, lane.dBottom);
    }
    return QRectF(QPointF(dLeft, dTop), QPointF(dRight, dBottom));
}

LayoutNode MakeLayoutNode(const KnowledgeGraphNode& node, const QPointF& center)
{
    LayoutNode layoutNode;
    layoutNode.strId = node.strId;
    layoutNode.strLabel = node.strName;
    layoutNode.strKind = node.strKind;
    layoutNode.center = center;
    return layoutNode;
}

LayoutEdge MakeLayoutEdge(const KnowledgeGraph& graph, int graphEdge, int from, int to)
{
    const KnowledgeGraphEdge& edge = graph.GetEdges()[graphEdge];
    LayoutEdge layoutEdge;
    layoutEdge.iFrom = from;
    layoutEdge.iTo = to;
    layoutEdge.strKind = edge.edge.strKind;
    layoutEdge.eStatus = edge.edge.eStatus;
    layoutEdge.bDirected = edge.bDirected;
    layoutEdge.iGraphEdge = graphEdge;
    return layoutEdge;
}
}
