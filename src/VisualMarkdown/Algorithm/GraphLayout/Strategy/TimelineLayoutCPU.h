#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"

// 타임라인: 네 자리 연도가 있는 보이는 점과 그 사이의 isA · improves 선. x 는 (연도 − 첫 연도) × 한 해 간격이고, 눈금은 있는 해마다 하나다.
// 갈래는 점이 속한 분류 나무의 뿌리(isA 를 위로 따라간다. 부모가 여럿이면 이름 열쇠로 앞선 부모), 없으면 개선 묶음의 가장 옛 점,
// 그것도 없으면 "그 밖" 이다. 갈래는 점이 많은 것부터, 같으면 이름 순이고, "그 밖" 은 늘 마지막이다.
// 같은 갈래 · 같은 해의 점은 이름 열쇠 순으로 세로로 쌓는다.
// 연도 없는 점은 vecUnplaced 에 이름 열쇠 순으로 둔다.
class TimelineLayoutCPU : public IGraphLayoutStrategy
{
public:
    bool Execute(const GraphLayoutInput& in, GraphLayoutOutput& out) override;

public:
    EGraphLayoutType GetType() const override;
};
