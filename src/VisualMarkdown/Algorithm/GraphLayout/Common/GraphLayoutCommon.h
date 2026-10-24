#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"

#include <QPointF>
#include <QRectF>
#include <QVector>

// 배치 전략들이 함께 쓰는 것: 관계 종류, isA 숲, 개선 묶음, 점 상자 · 경계.
// 동점은 모두 점 번호(= 이름 열쇠 순)로 가른다. 해시 순서에 기대지 않는다.
namespace GraphLayoutCommon
{
inline constexpr const char* kImprovesKind = "improves";
inline constexpr const char* kIsAKind = "isA";

bool IsKind(const KnowledgeGraphEdge& edge, const char* kind);

// isA 선(from = 하위, to = 상위)으로 만든 숲. 부모가 여럿이면 이름 열쇠로 앞선 부모 하나만 나무의 부모로 본다(제 자신으로 가는 선은 뺀다).
// 뿌리는 나가는 isA 가 없는 점이다. 어느 뿌리에서도 닿지 않는 점은 고리에 걸려 있으므로, 부모를 따라 올라가 만난 고리 안에서
// 이름 열쇠로 앞선 점을 뿌리로 삼는다(그 점의 부모 선은 나무 선이 아니게 된다).
struct IsAForest
{
    QVector<bool> vecInForest;           // 점 번호 → isA 선의 끝인지
    QVector<int> vecParent;              // 점 번호 → 나무의 부모(뿌리 · 숲 밖은 -1)
    QVector<QVector<int>> vecChildren;   // 점 번호 → 나무의 자식(이름 열쇠 순)
    QVector<int> vecRootOf;              // 점 번호 → 든 나무의 뿌리(숲 밖은 -1)
    QVector<int> vecRoots;               // 나무 뿌리: 큰 나무부터, 같으면 뿌리 이름 열쇠 순
    QVector<int> vecTreeSizes;           // vecRoots 와 같은 순서의 나무 점 수
};
IsAForest BuildIsAForest(const KnowledgeGraph& graph);

// improves 선(from = 새것, to = 옛것)으로 이어진 묶음(방향을 보지 않는다)과 점마다의 개선 깊이.
// 깊이는 아무것도 개선하지 않는 점까지의 가장 긴 개선 길이다(고리를 닫는 선은 보지 않는다).
// 가장 옛 점은 깊이 0 인 점 가운데 연도가 가장 이른 것(연도 없는 점은 뒤), 같으면 이름 열쇠 순이다.
struct ImprovesGroups
{
    QVector<QVector<int>> vecGroups;     // 묶음의 점(점 번호 순): 큰 묶음부터, 같으면 가장 옛 점의 이름 열쇠 순
    QVector<int> vecOldest;              // vecGroups 와 같은 순서의 가장 옛 점
    QVector<int> vecGroupOf;             // 점 번호 → 묶음 자리(묶음 밖은 -1)
    QVector<int> vecDepth;               // 점 번호 → 개선 깊이(묶음 밖은 -1)
};
ImprovesGroups BuildImprovesGroups(const KnowledgeGraph& graph);

QRectF MakeNodeBox(const QPointF& center);
// 점 상자를 모두 담는 사각형. 점이 없으면 빈 사각형
QRectF ComputeNodeBounds(const QVector<LayoutNode>& nodes);
// 점 상자와 갈래 띠의 세로 범위를 모두 담는 사각형. 점이 없으면 빈 사각형
QRectF ComputeLaneBounds(const QVector<LayoutNode>& nodes, const QVector<LayoutLane>& lanes);
LayoutNode MakeLayoutNode(const KnowledgeGraphNode& node, const QPointF& center);
// 그래프 선 하나를 배치 선으로. from · to 는 vecNodes 자리다
LayoutEdge MakeLayoutEdge(const KnowledgeGraph& graph, int graphEdge, int from, int to);
}
