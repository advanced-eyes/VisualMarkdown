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
constexpr double kColumnSlack = 1e-6;      // 열 높이 상한과 견줄 때의 여유(딱 맞는 상자가 부동소수 오차로 밀려나지 않게)

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

    // 이어진 덩어리(점 2개 이상)마다 힘 배치 · 상자 밀기를 하고 덩어리 상자(점 상자를 모두 담는 사각형, 덩어리 안 자리 기준)를 잰다
    QVector<QVector<QPointF>> vecLocalPositions(vecComponents.size());
    QVector<QRectF> vecLocalBoxes(vecComponents.size());
    QVector<int> vecSingles;   // 선 없는 점(크기 1 덩어리)의 점 번호, 덩어리 순(= 이름 열쇠 순)
    double dColumnLimit = 0;   // 열 높이 상한 H
    for (int iComponent = 0; iComponent < vecComponents.size(); ++iComponent)
    {
        const QVector<int>& vecComponent = vecComponents[iComponent];
        if (vecComponent.size() < 2)
        {
            vecSingles.push_back(vecComponent.first());
            continue;
        }
        QVector<QPointF> vecPositions = RunForceLayout(graph, vecComponent, vecPairs[iComponent]);
        SeparateBoxes(vecPositions);
        double dLeft = std::numeric_limits<double>::max();
        double dTop = std::numeric_limits<double>::max();
        double dRight = std::numeric_limits<double>::lowest();
        double dBottom = std::numeric_limits<double>::lowest();
        for (const QPointF& ptPosition : vecPositions)
        {
            dLeft = std::min(dLeft, ptPosition.x() - kNodeWidth / 2);
            dTop = std::min(dTop, ptPosition.y() - kNodeHeight / 2);
            dRight = std::max(dRight, ptPosition.x() + kNodeWidth / 2);
            dBottom = std::max(dBottom, ptPosition.y() + kNodeHeight / 2);
        }
        vecLocalPositions[iComponent] = vecPositions;
        vecLocalBoxes[iComponent] = QRectF(QPointF(dLeft, dTop), QPointF(dRight, dBottom));
        dColumnLimit = std::max(dColumnLimit, dBottom - dTop);
    }

    // 덩어리 상자를 세로 열로 쌓는다(design/Views.md §5 관계망). 정한 규칙:
    // - 열 높이 상한 H 는 이어진 덩어리 상자 높이의 가장 큰 값이다. 이어진 덩어리가 없으면(점이 모두 선 없는 점) 줄 수 r = ⌈√점 수⌉ 인
    //   격자 높이 r × kNodeHeight + (r − 1) × kNodeGapY 다.
    // - 이어진 덩어리는 덩어리 순(큰 것부터, 같으면 이름 열쇠 순)으로 놓는다. 지금 열의 아래(틈 kComponentGap)에 넣어 열 높이가 H 를
    //   넘지 않으면 거기에 두고, 넘으면 오른쪽에 새 열을 열어 그 맨 위에 둔다. 새 열의 x 는 지금까지 놓은 상자의 오른쪽 끝 + kComponentGap 이다.
    //   상자 왼쪽은 열 왼쪽에 맞추고, 열 너비는 그 열 상자 너비의 가장 큰 값이다. 첫 열은 x = 0, 열 맨 위는 y = 0 이다.
    // - 선 없는 점은 그 뒤에 덩어리 순으로 격자 줄에 놓는다(점 사이 kNodeGapX · kNodeGapY, 줄 안은 왼쪽 → 오른쪽, 줄은 위 → 아래).
    //   지금 열에 덩어리가 있으면 그 아래(틈 kComponentGap)부터, 한 줄에 열 너비에 드는 만큼(최소 1)씩 줄 바닥이 H 를 넘지 않는 동안 채운다.
    //   남은 점(또는 지금 열이 비었을 때 모든 점)은 오른쪽 새 열(x 는 위와 같다)의 맨 위부터 둔다. 새 열의 줄 수 R 은 H 에 드는 줄 수(최소 1),
    //   한 줄의 점 수는 ⌈남은 점 / R⌉ 이다. 그래서 선 없는 점만 9개면 3 × 3 격자다.
    // 덩어리 안 점끼리의 상대 자리는 바꾸지 않는다(덩어리마다 평행 이동만). 정한 값과 정한 순서만 쓰므로 같은 입력이면 같은 출력이다.
    const double dSlotPitch = kNodeWidth + kNodeGapX;
    const double dRowPitch = kNodeHeight + kNodeGapY;
    if (vecSingles.size() == vecComponents.size() && !vecSingles.isEmpty())
    {
        int iRows = 1;
        while (iRows * iRows < vecSingles.size())
            ++iRows;
        dColumnLimit = iRows * kNodeHeight + (iRows - 1) * kNodeGapY;
    }

    QVector<QPointF> vecCenters(vecNodes.size(), QPointF(0, 0));
    bool bColumnEmpty = true;   // 지금 열에 아직 아무것도 없다(이어진 덩어리가 하나도 없을 때만 끝까지 비었다)
    double dColumnLeft = 0;
    double dColumnWidth = 0;
    double dColumnBottom = 0;
    double dRightEnd = 0;       // 놓은 상자들의 오른쪽 끝
    for (int iComponent = 0; iComponent < vecComponents.size(); ++iComponent)
    {
        const QVector<int>& vecComponent = vecComponents[iComponent];
        if (vecComponent.size() < 2)
            continue;
        const QRectF& rectLocal = vecLocalBoxes[iComponent];
        double dTop = 0;
        if (!bColumnEmpty && dColumnBottom + kComponentGap + rectLocal.height() <= dColumnLimit + kColumnSlack)
        {
            dTop = dColumnBottom + kComponentGap;
        }
        else if (!bColumnEmpty)
        {
            dColumnLeft = dRightEnd + kComponentGap;
            dColumnWidth = 0;
        }
        const QPointF ptShift(dColumnLeft - rectLocal.left(), dTop - rectLocal.top());
        for (int iLocal = 0; iLocal < vecComponent.size(); ++iLocal)
            vecCenters[vecComponent[iLocal]] = vecLocalPositions[iComponent][iLocal] + ptShift;
        bColumnEmpty = false;
        dColumnBottom = dTop + rectLocal.height();
        dColumnWidth = std::max(dColumnWidth, rectLocal.width());
        dRightEnd = std::max(dRightEnd, dColumnLeft + rectLocal.width());
    }

    int iNextSingle = 0;
    if (!bColumnEmpty)
    {
        // 지금 열의 남은 높이: 열 너비에 드는 만큼씩 줄을 채운다
        const int iPerRow = std::max(1, static_cast<int>(std::floor((dColumnWidth + kNodeGapX) / dSlotPitch)));
        double dRowTop = dColumnBottom + kComponentGap;
        while (iNextSingle < vecSingles.size() && dRowTop + kNodeHeight <= dColumnLimit + kColumnSlack)
        {
            for (int iSlot = 0; iSlot < iPerRow && iNextSingle < vecSingles.size(); ++iSlot, ++iNextSingle)
            {
                const double dLeft = dColumnLeft + iSlot * dSlotPitch;
                vecCenters[vecSingles[iNextSingle]] = QPointF(dLeft + kNodeWidth / 2, dRowTop + kNodeHeight / 2);
                dRightEnd = std::max(dRightEnd, dLeft + kNodeWidth);
            }
            dRowTop += dRowPitch;
        }
    }
    if (iNextSingle < vecSingles.size())
    {
        // 남은 점: 오른쪽 새 열. 줄 수는 H 에 드는 만큼, 한 줄은 남은 점을 그 줄 수로 고르게 나눈 만큼
        const double dLeft = bColumnEmpty ? 0 : dRightEnd + kComponentGap;
        int iRows = 1;
        while (iRows * dRowPitch + kNodeHeight <= dColumnLimit + kColumnSlack)
            ++iRows;
        const int iRemaining = vecSingles.size() - iNextSingle;
        const int iPerRow = (iRemaining + iRows - 1) / iRows;
        for (int iIndex = 0; iIndex < iRemaining; ++iIndex)
        {
            const double dX = dLeft + (iIndex % iPerRow) * dSlotPitch + kNodeWidth / 2;
            const double dY = (iIndex / iPerRow) * dRowPitch + kNodeHeight / 2;
            vecCenters[vecSingles[iNextSingle + iIndex]] = QPointF(dX, dY);
        }
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
