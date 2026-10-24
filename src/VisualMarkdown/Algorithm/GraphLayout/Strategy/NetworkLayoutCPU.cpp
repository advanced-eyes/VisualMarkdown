#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/NetworkLayoutCPU.h"

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutCommon.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"

#include <QPair>

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
using namespace GraphLayoutSize;

// 힘 배치 값. showcase 로 견주어 골랐다(끝까지 모든 쌍을 미는 원래 FR 은 잎이 멀리 밀려 그림이 1.7배 넓고,
// 처음부터 가까운 쌍만 밀면 엉킴이 덜 풀려 교차가 는다).
constexpr double kNetworkEdgeLength = 160;                          // k: 바라는 선 길이
constexpr int kNetworkIterations = 300;                             // 반복 횟수(정해 둔다)
constexpr int kNetworkGlobalIterations = kNetworkIterations / 2;    // 처음 이만큼은 모든 쌍을 밀어 엉킨 것을 푼다
constexpr double kNetworkRepulsionRange = 2 * kNetworkEdgeLength;   // 그 뒤에는 이 거리 안의 쌍만 민다(FR 의 격자 변형)
constexpr double kNetworkGravity = 0.1;                             // 덩어리 가운데로 당기는 힘(거리 1 당)
constexpr double kNetworkStartStep = kNetworkEdgeLength;            // 첫 반복에서 한 점이 움직일 수 있는 거리. 반복마다 0 까지 고르게 줄인다
constexpr int kNetworkSeparationPasses = 500;                       // 겹친 상자 밀기를 되풀이할 최대 횟수

constexpr double kPi = 3.14159265358979323846;
constexpr double kMinDistance = 0.01;      // 두 점이 이보다 가까우면 정한 방향(x)으로 민다(0 으로 나누지 않게)
constexpr double kSeparationSlack = 0.5;   // 겹친 상자를 뗄 때 더 떼는 거리(부동소수 오차로 다시 겹치지 않게)

using NodePair = QPair<int, int>;          // 덩어리 안 자리(작은 것, 큰 것)

// 덩어리 하나를 Fruchterman–Reingold 로 놓는다. positions 는 덩어리 안 자리 순이다.
// 처음 자리는 id 순으로 원 위에 고르게(이웃한 두 점 사이가 k), 밀어내기는 k²/d, 끌어당기기는 이어진 쌍에 d²/k,
// 덩어리 가운데로 kNetworkGravity × 거리. 앞 반은 모든 쌍을, 뒤 반은 2k 안의 쌍만 민다.
// 한 번에 움직일 수 있는 거리는 kNetworkStartStep 에서 0 까지 반복마다 고르게 줄인다. 난수를 쓰지 않아 결과가 늘 같다.
QVector<QPointF> RunForceLayout(const KnowledgeGraph& graph, const QVector<int>& members, const QVector<NodePair>& pairs)
{
    const int iCount = members.size();
    QVector<QPointF> vecPositions(iCount, QPointF(0, 0));
    if (iCount < 2)
        return vecPositions;

    QVector<int> vecById(iCount);
    for (int iLocal = 0; iLocal < iCount; ++iLocal)
        vecById[iLocal] = iLocal;
    const QVector<KnowledgeGraphNode>& vecNodes = graph.GetNodes();
    std::sort(vecById.begin(), vecById.end(), [&vecNodes, &members](int a, int b)
    {
        return vecNodes[members[a]].strId < vecNodes[members[b]].strId;
    });
    const double dRadius = kNetworkEdgeLength / (2 * std::sin(kPi / iCount));
    for (int iRank = 0; iRank < iCount; ++iRank)
    {
        const double dAngle = 2 * kPi * iRank / iCount;
        vecPositions[vecById[iRank]] = QPointF(dRadius * std::cos(dAngle), dRadius * std::sin(dAngle));
    }

    const double dLengthSquared = kNetworkEdgeLength * kNetworkEdgeLength;
    for (int iIteration = 0; iIteration < kNetworkIterations; ++iIteration)
    {
        const bool bNearOnly = iIteration >= kNetworkGlobalIterations;
        QVector<QPointF> vecShift(iCount, QPointF(0, 0));
        for (int iFirst = 0; iFirst < iCount; ++iFirst)
        {
            for (int iSecond = iFirst + 1; iSecond < iCount; ++iSecond)
            {
                QPointF ptDelta = vecPositions[iFirst] - vecPositions[iSecond];
                double dDistance = std::hypot(ptDelta.x(), ptDelta.y());
                if (dDistance < kMinDistance)
                {
                    ptDelta = QPointF(kMinDistance, 0);
                    dDistance = kMinDistance;
                }
                if (bNearOnly && dDistance > kNetworkRepulsionRange)
                    continue;
                const QPointF ptForce = ptDelta * (dLengthSquared / (dDistance * dDistance));   // 방향(delta/d) × k²/d
                vecShift[iFirst] += ptForce;
                vecShift[iSecond] -= ptForce;
            }
        }
        for (const NodePair& pair : pairs)
        {
            const QPointF ptDelta = vecPositions[pair.first] - vecPositions[pair.second];
            const double dDistance = std::hypot(ptDelta.x(), ptDelta.y());
            if (dDistance < kMinDistance)
                continue;
            const QPointF ptForce = ptDelta * (dDistance / kNetworkEdgeLength);   // 방향(delta/d) × d²/k
            vecShift[pair.first] -= ptForce;
            vecShift[pair.second] += ptForce;
        }
        QPointF ptCentroid(0, 0);
        for (const QPointF& ptPosition : vecPositions)
            ptCentroid += ptPosition;
        ptCentroid /= iCount;
        for (int iLocal = 0; iLocal < iCount; ++iLocal)
            vecShift[iLocal] -= (vecPositions[iLocal] - ptCentroid) * kNetworkGravity;

        const double dStep = kNetworkStartStep * (1.0 - static_cast<double>(iIteration) / kNetworkIterations);
        for (int iLocal = 0; iLocal < iCount; ++iLocal)
        {
            const double dLength = std::hypot(vecShift[iLocal].x(), vecShift[iLocal].y());
            if (dLength > 0)
                vecPositions[iLocal] += vecShift[iLocal] * (std::min(dLength, dStep) / dLength);
        }
    }
    return vecPositions;
}

bool IsOverlapping(const QPointF& a, const QPointF& b, double width, double height)
{
    return std::abs(a.x() - b.x()) < width && std::abs(a.y() - b.y()) < height;
}

// 점 상자(틈 포함)가 겹치지 않을 때까지 민다. 겹친 쌍은 덜 밀어도 되는 쪽(가로 · 세로)으로 반씩 떼고, 겹침이 없을 때까지 되풀이한다.
// 정한 횟수 안에 끝나지 않으면 점 순서대로 앞의 점과 겹칠 때 그 아래로 내린다(아래로만 가므로 반드시 끝난다).
void SeparateBoxes(QVector<QPointF>& positions)
{
    const double dWidth = kNodeWidth + kNodeGapX;
    const double dHeight = kNodeHeight + kNodeGapY;
    const int iCount = positions.size();
    for (int iPass = 0; iPass < kNetworkSeparationPasses; ++iPass)
    {
        bool bMoved = false;
        for (int iFirst = 0; iFirst < iCount; ++iFirst)
        {
            for (int iSecond = iFirst + 1; iSecond < iCount; ++iSecond)
            {
                QPointF& ptFirst = positions[iFirst];
                QPointF& ptSecond = positions[iSecond];
                if (!IsOverlapping(ptFirst, ptSecond, dWidth, dHeight))
                    continue;
                bMoved = true;
                const double dX = ptSecond.x() - ptFirst.x();
                const double dY = ptSecond.y() - ptFirst.y();
                const double dOverlapX = dWidth - std::abs(dX);
                const double dOverlapY = dHeight - std::abs(dY);
                if (dOverlapY <= dOverlapX)
                {
                    const double dPush = (dOverlapY + kSeparationSlack) / 2 * (dY >= 0 ? 1 : -1);
                    ptFirst.ry() -= dPush;
                    ptSecond.ry() += dPush;
                }
                else
                {
                    const double dPush = (dOverlapX + kSeparationSlack) / 2 * (dX >= 0 ? 1 : -1);
                    ptFirst.rx() -= dPush;
                    ptSecond.rx() += dPush;
                }
            }
        }
        if (!bMoved)
            return;
    }
    for (int iMoving = 1; iMoving < iCount; ++iMoving)
    {
        bool bOverlapping = true;
        while (bOverlapping)
        {
            bOverlapping = false;
            for (int iPlaced = 0; iPlaced < iMoving; ++iPlaced)
            {
                if (!IsOverlapping(positions[iMoving], positions[iPlaced], dWidth, dHeight))
                    continue;
                positions[iMoving].ry() = positions[iPlaced].y() + dHeight + kSeparationSlack;
                bOverlapping = true;
                break;
            }
        }
    }
}
}

bool NetworkLayoutCPU::Execute(const GraphLayoutInput& in, GraphLayoutOutput& out)
{
    out = GraphLayoutOutput();
    if (in.pGraph == nullptr)
        return false;
    const KnowledgeGraph& graph = *in.pGraph;
    const QVector<KnowledgeGraphNode>& vecNodes = graph.GetNodes();
    const QVector<KnowledgeGraphEdge>& vecEdges = graph.GetEdges();
    const QVector<QVector<int>>& vecComponents = graph.GetComponents();

    // 점 번호 → 덩어리 안 자리, 덩어리마다 이어진 쌍(선이 여럿이어도 한 번, 제 자신으로 가는 선은 뺀다)
    QVector<int> vecLocal(vecNodes.size(), -1);
    for (const QVector<int>& vecComponent : vecComponents)
        for (int iLocal = 0; iLocal < vecComponent.size(); ++iLocal)
            vecLocal[vecComponent[iLocal]] = iLocal;
    QVector<QVector<NodePair>> vecPairs(vecComponents.size());
    for (const KnowledgeGraphEdge& edge : vecEdges)
    {
        if (edge.iFrom == edge.iTo)
            continue;
        const int iFirst = vecLocal[edge.iFrom];
        const int iSecond = vecLocal[edge.iTo];
        vecPairs[vecNodes[edge.iFrom].iComponent].push_back({ std::min(iFirst, iSecond), std::max(iFirst, iSecond) });
    }
    for (QVector<NodePair>& vecComponentPairs : vecPairs)
    {
        std::sort(vecComponentPairs.begin(), vecComponentPairs.end());
        vecComponentPairs.erase(std::unique(vecComponentPairs.begin(), vecComponentPairs.end()), vecComponentPairs.end());
    }

    // 덩어리를 큰 것부터 왼쪽에서 오른쪽으로(상자 위를 0 에 맞춘다). 선 없는 점(크기 1)은 그 뒤에 한 줄로
    QVector<QPointF> vecCenters(vecNodes.size(), QPointF(0, 0));
    double dCursor = 0;
    for (int iComponent = 0; iComponent < vecComponents.size(); ++iComponent)
    {
        const QVector<int>& vecComponent = vecComponents[iComponent];
        if (vecComponent.size() < 2)
            continue;
        QVector<QPointF> vecPositions = RunForceLayout(graph, vecComponent, vecPairs[iComponent]);
        SeparateBoxes(vecPositions);
        double dLeft = std::numeric_limits<double>::max();
        double dTop = std::numeric_limits<double>::max();
        double dRight = std::numeric_limits<double>::lowest();
        for (const QPointF& ptPosition : vecPositions)
        {
            dLeft = std::min(dLeft, ptPosition.x() - kNodeWidth / 2);
            dTop = std::min(dTop, ptPosition.y() - kNodeHeight / 2);
            dRight = std::max(dRight, ptPosition.x() + kNodeWidth / 2);
        }
        const QPointF ptShift(dCursor - dLeft, -dTop);
        for (int iLocal = 0; iLocal < vecComponent.size(); ++iLocal)
            vecCenters[vecComponent[iLocal]] = vecPositions[iLocal] + ptShift;
        dCursor += dRight - dLeft + kComponentGap;
    }
    for (const QVector<int>& vecComponent : vecComponents)
    {
        if (vecComponent.size() != 1)
            continue;
        vecCenters[vecComponent.first()] = QPointF(dCursor + kNodeWidth / 2, kNodeHeight / 2);
        dCursor += kNodeWidth + kNodeGapX;
    }

    for (int iNode = 0; iNode < vecNodes.size(); ++iNode)
        out.vecNodes.push_back(GraphLayoutCommon::MakeLayoutNode(vecNodes[iNode], vecCenters[iNode]));
    for (int iEdge = 0; iEdge < vecEdges.size(); ++iEdge)
        out.vecEdges.push_back(GraphLayoutCommon::MakeLayoutEdge(graph, iEdge, vecEdges[iEdge].iFrom, vecEdges[iEdge].iTo));
    out.bounds = GraphLayoutCommon::ComputeNodeBounds(out.vecNodes);
    return true;
}

EGraphLayoutType NetworkLayoutCPU::GetType() const
{
    return EGraphLayoutType::NETWORK;
}
