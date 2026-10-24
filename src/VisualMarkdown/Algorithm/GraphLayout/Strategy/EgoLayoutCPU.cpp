#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/EgoLayoutCPU.h"

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutCommon.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <QPair>

#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace
{
using namespace GraphLayoutSize;

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwelveOClock = -kPi / 2;   // 화면 좌표(y 가 아래)의 12시. 각이 늘어나면 시계 방향이다
constexpr double kMinComponent = 1e-12;      // 방향 성분이 이보다 작으면 그 축으로는 반지름이 커져도 떨어지지 않는다고 본다
constexpr double kInfinity = std::numeric_limits<double>::infinity();

// 1 걸음 점의 순서 열쇠(작을수록 앞): 관계 종류 자리(어휘 순, 어휘에 없는 종류는 뒤에 코드 순) → 방향(나가는 선 · 들어오는 선 · 방향 없는 선)
enum class EDirection { OUTGOING, INCOMING, UNDIRECTED };

struct RingOneKey
{
    int iKindRank = 0;
    QString strKind;
    EDirection eDirection = EDirection::OUTGOING;
};

bool IsBefore(const RingOneKey& a, const RingOneKey& b)
{
    return std::make_tuple(a.iKindRank, a.strKind, static_cast<int>(a.eDirection)) < std::make_tuple(b.iKindRank, b.strKind, static_cast<int>(b.eDirection));
}

// 같은 반지름 r 에 놓인 두 방향(길이 1) a · b 의 상자가 (틈을 둔 크기 width × height 로) 겹치지 않는 가장 작은 r.
// 가로로 width 만큼 떨어지거나 세로로 height 만큼 떨어지면 된다
double FindSeparatingRadius(const QPointF& a, const QPointF& b, double width, double height)
{
    const double dX = std::abs(a.x() - b.x());
    const double dY = std::abs(a.y() - b.y());
    const double dByX = dX < kMinComponent ? kInfinity : width / dX;
    const double dByY = dY < kMinComponent ? kInfinity : height / dY;
    return std::min(dByX, dByY);
}

// 방향 direction(길이 1)의 반지름 r 자리 상자가 center 에 놓인 상자와 (틈을 둔 크기로) 겹치는 r 의 열린 구간. 겹치는 r 이 없으면 빈 구간(앞 ≥ 뒤)
QPair<double, double> FindOverlapInterval(const QPointF& direction, const QPointF& center, double width, double height)
{
    double dLow = -kInfinity;
    double dHigh = kInfinity;
    // 한 축에서 |r × 성분 − 자리| < 크기 인 r 의 구간을 겹쳐 간다
    const auto funcAxis = [&dLow, &dHigh](double component, double position, double extent)
    {
        if (std::abs(component) < kMinComponent)
        {
            if (std::abs(position) >= extent)
            {
                dLow = kInfinity;   // 이 축으로 늘 떨어져 있다
                dHigh = -kInfinity;
            }
            return;
        }
        const double dFirst = (position - extent) / component;
        const double dSecond = (position + extent) / component;
        dLow = std::max(dLow, std::min(dFirst, dSecond));
        dHigh = std::min(dHigh, std::max(dFirst, dSecond));
    };
    funcAxis(direction.x(), center.x(), width);
    funcAxis(direction.y(), center.y(), height);
    return qMakePair(dLow, dHigh);
}

// 고리 하나의 반지름: start 부터, 같은 고리 상자끼리 · 안쪽 상자(placed)와 겹치지 않는 가장 작은 값
double FindRingRadius(const QVector<QPointF>& directions, const QVector<QPointF>& placed, double start)
{
    const double dWidth = kNodeWidth + kEgoNodeGapX;
    const double dHeight = kNodeHeight + kEgoNodeGapY;
    double dRadius = start;
    // 같은 고리: 반지름이 커질수록 멀어지므로 쌍마다 떨어지는 가장 작은 반지름 이상이면 된다
    for (int iFirst = 0; iFirst < directions.size(); ++iFirst)
        for (int iSecond = iFirst + 1; iSecond < directions.size(); ++iSecond)
            dRadius = std::max(dRadius, FindSeparatingRadius(directions[iFirst], directions[iSecond], dWidth, dHeight));
    // 안쪽 상자: 겹치는 반지름 구간들을 피한다. 구간을 앞 끝 순으로 보며, 지금 반지름을 담는 구간이면 그 뒤 끝으로 옮긴다
    QVector<QPair<double, double>> vecIntervals;
    for (const QPointF& ptDirection : directions)
    {
        for (const QPointF& ptPlaced : placed)
        {
            const QPair<double, double> pairInterval = FindOverlapInterval(ptDirection, ptPlaced, dWidth, dHeight);
            if (pairInterval.first < pairInterval.second)
                vecIntervals << pairInterval;
        }
    }
    std::sort(vecIntervals.begin(), vecIntervals.end());
    for (const QPair<double, double>& pairInterval : vecIntervals)
    {
        if (pairInterval.first >= dRadius)
            break;   // 뒤의 구간은 모두 지금 반지름보다 뒤에서 시작한다
        dRadius = std::max(dRadius, pairInterval.second);
    }
    return dRadius;
}
}

bool EgoLayoutCPU::Execute(const GraphLayoutInput& in, GraphLayoutOutput& out)
{
    out = GraphLayoutOutput();
    if (in.pGraph == nullptr)
        return false;
    const KnowledgeGraph& graph = *in.pGraph;
    const QVector<KnowledgeGraphNode>& vecNodes = graph.GetNodes();
    const QVector<KnowledgeGraphEdge>& vecEdges = graph.GetEdges();
    const int iNodeCount = vecNodes.size();
    if (iNodeCount == 0)
        return true;
    int iFocus = graph.FindNode(in.strFocusId);
    if (iFocus < 0)
        iFocus = GraphLayoutCommon::FindDefaultFocus(graph);
    const int iDepth = std::max(1, in.iDepth);

    // 걸음 수: 방향을 보지 않는 이웃(제 자신으로 가는 선은 뺀다)으로 깊이까지 넓게 걷는다
    QVector<QVector<int>> vecNeighbors(iNodeCount);
    for (const KnowledgeGraphEdge& edge : vecEdges)
    {
        if (edge.iFrom == edge.iTo)
            continue;
        vecNeighbors[edge.iFrom].push_back(edge.iTo);
        vecNeighbors[edge.iTo].push_back(edge.iFrom);
    }
    QVector<int> vecHop(iNodeCount, -1);
    vecHop[iFocus] = 0;
    QVector<int> vecQueue { iFocus };
    for (int iNext = 0; iNext < vecQueue.size(); ++iNext)
    {
        const int iNode = vecQueue[iNext];
        if (vecHop[iNode] >= iDepth)
            continue;
        for (const int iNeighbor : vecNeighbors[iNode])
        {
            if (vecHop[iNeighbor] >= 0)
                continue;
            vecHop[iNeighbor] = vecHop[iNode] + 1;
            vecQueue.push_back(iNeighbor);
        }
    }

    // 1 걸음 점의 순서: 초점과 잇는 선마다 열쇠를 만들어 가장 앞선 것을 그 점의 열쇠로 둔다
    const QStringList vecVocabularyKinds = KnowledgeVocabulary::Get().GetRelationCodes();
    QVector<RingOneKey> vecKeys(iNodeCount);
    QVector<bool> vecHasKey(iNodeCount, false);
    for (const KnowledgeGraphEdge& edge : vecEdges)
    {
        if (edge.iFrom == edge.iTo || (edge.iFrom != iFocus && edge.iTo != iFocus))
            continue;
        const int iOther = edge.iFrom == iFocus ? edge.iTo : edge.iFrom;
        RingOneKey key;
        const int iRank = vecVocabularyKinds.indexOf(edge.edge.strKind);
        key.iKindRank = iRank >= 0 ? iRank : vecVocabularyKinds.size();
        key.strKind = edge.edge.strKind;
        if (!edge.bDirected)
            key.eDirection = EDirection::UNDIRECTED;
        else
            key.eDirection = edge.iFrom == iFocus ? EDirection::OUTGOING : EDirection::INCOMING;
        if (!vecHasKey[iOther] || IsBefore(key, vecKeys[iOther]))
        {
            vecKeys[iOther] = key;
            vecHasKey[iOther] = true;
        }
    }

    // 고리: 1 걸음은 열쇠 순(같으면 점 번호 순), 2 걸음부터는 앞 고리 순서대로 부모마다 자식(점 번호 순)
    QVector<QVector<int>> vecRings { { iFocus } };
    QVector<int> vecRingOne;
    for (int iNode = 0; iNode < iNodeCount; ++iNode)
        if (vecHop[iNode] == 1)
            vecRingOne.push_back(iNode);
    std::stable_sort(vecRingOne.begin(), vecRingOne.end(), [&vecKeys](int a, int b) { return IsBefore(vecKeys[a], vecKeys[b]); });
    QVector<QVector<int>> vecChildren(iNodeCount);
    QVector<int> vecParent(iNodeCount, -1);         // 점 → 나무의 부모(1 걸음 점은 초점, 초점 · 넣지 않은 점은 -1)
    QVector<int> vecRingPosition(iNodeCount, -1);   // 점 → 제 고리 안 자리
    for (const int iNode : vecRingOne)
        vecParent[iNode] = iFocus;
    if (!vecRingOne.isEmpty())
        vecRings.push_back(vecRingOne);
    for (int iRing = 2; iRing <= iDepth && vecRings.size() == iRing; ++iRing)
    {
        const QVector<int>& vecPrevious = vecRings[iRing - 1];
        for (int iPosition = 0; iPosition < vecPrevious.size(); ++iPosition)
            vecRingPosition[vecPrevious[iPosition]] = iPosition;
        for (int iNode = 0; iNode < iNodeCount; ++iNode)
        {
            if (vecHop[iNode] != iRing)
                continue;
            int iParent = -1;
            for (const int iNeighbor : vecNeighbors[iNode])
                if (vecHop[iNeighbor] == iRing - 1 && (iParent < 0 || vecRingPosition[iNeighbor] < vecRingPosition[iParent]))
                    iParent = iNeighbor;
            vecParent[iNode] = iParent;
            vecChildren[iParent].push_back(iNode);   // 점 번호 순으로 돌아 자식은 점 번호 순이다
        }
        QVector<int> vecRing;
        for (const int iParent : vecPrevious)
            vecRing += vecChildren[iParent];
        if (!vecRing.isEmpty())
            vecRings.push_back(vecRing);
    }

    // 쐐기: 잎 수(깊은 고리부터), 그다음 잎 자리(1 걸음부터 차례로, 자식은 부모 자리 안에서 차례로). 잎 하나는 2π / 잎 수다
    QVector<int> vecLeaves(iNodeCount, 0);
    QVector<int> vecLeafStart(iNodeCount, 0);
    for (int iRing = vecRings.size() - 1; iRing >= 1; --iRing)
    {
        for (const int iNode : vecRings[iRing])
        {
            int iLeaves = 0;
            for (const int iChild : vecChildren[iNode])
                iLeaves += vecLeaves[iChild];
            vecLeaves[iNode] = std::max(1, iLeaves);
        }
    }
    int iTotalLeaves = 0;
    if (vecRings.size() > 1)
    {
        for (const int iNode : vecRings[1])
        {
            vecLeafStart[iNode] = iTotalLeaves;
            iTotalLeaves += vecLeaves[iNode];
        }
        for (int iRing = 1; iRing < vecRings.size(); ++iRing)
        {
            for (const int iNode : vecRings[iRing])
            {
                int iStart = vecLeafStart[iNode];
                for (const int iChild : vecChildren[iNode])
                {
                    vecLeafStart[iChild] = iStart;
                    iStart += vecLeaves[iChild];
                }
            }
        }
    }
    // 점의 방향(길이 1): 쐐기 가운데 각. 첫 1 걸음 점의 쐐기 가운데가 12시다
    QVector<QPointF> vecDirections(iNodeCount, QPointF(0, 0));
    if (iTotalLeaves > 0)
    {
        const double dFirstCenter = vecLeaves[vecRings[1].first()] / 2.0;
        for (int iRing = 1; iRing < vecRings.size(); ++iRing)
        {
            for (const int iNode : vecRings[iRing])
            {
                const double dLeafCenter = vecLeafStart[iNode] + vecLeaves[iNode] / 2.0;
                const double dAngle = kTwelveOClock + 2 * kPi * (dLeafCenter - dFirstCenter) / iTotalLeaves;
                vecDirections[iNode] = QPointF(std::cos(dAngle), std::sin(dAngle));
            }
        }
    }

    // 반지름: 안쪽 고리부터. 앞 고리 + kEgoRingGap 에서 시작해 겹치지 않는 가장 작은 값
    QVector<QPointF> vecCenters(iNodeCount, QPointF(0, 0));
    QVector<QPointF> vecPlaced { QPointF(0, 0) };
    double dRadius = 0;
    for (int iRing = 1; iRing < vecRings.size(); ++iRing)
    {
        QVector<QPointF> vecRingDirections;
        for (const int iNode : vecRings[iRing])
            vecRingDirections.push_back(vecDirections[iNode]);
        dRadius = FindRingRadius(vecRingDirections, vecPlaced, dRadius + kEgoRingGap);
        for (const int iNode : vecRings[iRing])
        {
            vecCenters[iNode] = vecDirections[iNode] * dRadius;
            vecPlaced.push_back(vecCenters[iNode]);
        }
    }

    // 출력: 초점 → 고리 순서대로. 선은 넣은 점 사이의 그래프 선 전부(그래프 선 순서).
    // 점과 그 부모를 잇는 선(방향은 보지 않는다. 둘 이상이면 모두)은 걸음을 이루는 나무 선이고, 그 밖은 둘레 선(bContext)이다
    QVector<int> vecLayoutIndex(iNodeCount, -1);
    for (const QVector<int>& vecRing : vecRings)
    {
        for (const int iNode : vecRing)
        {
            vecLayoutIndex[iNode] = out.vecNodes.size();
            LayoutNode node = GraphLayoutCommon::MakeLayoutNode(vecNodes[iNode], vecCenters[iNode]);
            node.bFocus = iNode == iFocus;
            out.vecNodes.push_back(node);
        }
    }
    for (int iEdge = 0; iEdge < vecEdges.size(); ++iEdge)
    {
        const KnowledgeGraphEdge& edge = vecEdges[iEdge];
        if (vecLayoutIndex[edge.iFrom] < 0 || vecLayoutIndex[edge.iTo] < 0)
            continue;
        LayoutEdge layoutEdge = GraphLayoutCommon::MakeLayoutEdge(graph, iEdge, vecLayoutIndex[edge.iFrom], vecLayoutIndex[edge.iTo]);
        const bool bTree = edge.iFrom != edge.iTo && (vecParent[edge.iFrom] == edge.iTo || vecParent[edge.iTo] == edge.iFrom);
        layoutEdge.bContext = !bTree;
        out.vecEdges.push_back(layoutEdge);
    }
    out.bounds = GraphLayoutCommon::ComputeNodeBounds(out.vecNodes);
    return true;
}

EGraphLayoutType EgoLayoutCPU::GetType() const
{
    return EGraphLayoutType::EGO;
}
