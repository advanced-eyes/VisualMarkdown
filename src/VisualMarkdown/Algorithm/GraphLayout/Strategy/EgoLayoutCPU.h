#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"

// 이웃(design/MoreViews.md §6.1): 초점 개념 둘레의 점을 동심원에 놓는다(Eades 방사형 나무).
// - 초점: 받은 id(strFocusId)가 보이는 점이면 그 점, 아니면 기본 초점(KnowledgeGraphLayout::FindDefaultFocus). 점이 없으면 빈 배치다.
// - 고리: 방향을 보지 않고 선을 따라 걸은 걸음 수. 초점은 가운데(0, 0), k 걸음 점은 k 번째 고리이고 깊이(iDepth, 1 아래는 1)까지만 넣는다.
//   선은 넣은 점 사이의 그래프 선 전부(그래프 선 순서)다.
// - 1 걸음 점의 순서: 초점과 잇는 선의 관계 종류(어휘 순, 어휘에 없는 종류는 뒤에 코드 순. 선이 둘 이상이면 앞의 종류),
//   같으면 초점에서 나가는 선 → 들어오는 선 → 방향 없는 선, 같으면 점 번호 순.
// - 2 걸음부터의 점은 앞 고리에서 이어진 점 가운데 그 고리 순서가 가장 앞인 점을 부모로 둔다. 한 부모의 자식은 점 번호 순이다.
// - 쐐기: 1 걸음 점마다 잎 수(자식이 없으면 1, 있으면 자식들의 잎 수 합)에 비례한 각. 첫 1 걸음 점의 쐐기 가운데가 12시(화면 좌표 −90°)이고
//   시계 방향(각이 늘어나는 쪽)으로 잇는다. 자식들은 부모 쐐기를 잎 수에 비례해 나누고, 점은 자기 쐐기의 가운데 각에 둔다.
// - 반지름: 고리마다 하나. 앞 고리 반지름 + kEgoRingGap 에서 시작해, 같은 고리의 점 상자끼리와 안쪽 고리(초점 포함)의 점 상자와
//   틈(kEgoNodeGapX · kEgoNodeGapY)을 두고 겹치지 않는 가장 작은 값이다.
// - 출력 점: 초점(bFocus) → 1 걸음(순서대로) → 2 걸음(각 순) … . 갈래 · 눈금은 없다.
// - 나무 선과 둘레 선: 점과 그 부모(1 걸음 점은 초점)를 잇는 선은 방향과 상관없이 모두 나무 선이고, 그 밖의 선(같은 고리 점끼리,
//   부모가 아닌 앞 고리 점과의 선, 제 자신으로 가는 선)은 둘레 선(LayoutEdge::bContext)이다. 위젯은 둘레 선을 흐리게 그린다.
class EgoLayoutCPU : public IGraphLayoutStrategy
{
public:
    bool Execute(const GraphLayoutInput& in, GraphLayoutOutput& out) override;

public:
    EGraphLayoutType GetType() const override;
};
