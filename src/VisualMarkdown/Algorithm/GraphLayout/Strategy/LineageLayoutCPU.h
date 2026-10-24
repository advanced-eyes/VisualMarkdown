#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"

// 계보도: improves 선과 그 양 끝. 개선 선으로 이어진 묶음(방향을 보지 않는다) 하나가 갈래 하나다.
// 열은 가장 옛 점까지의 가장 긴 개선 길이다(옛것이 0 열, 왼쪽). 한 갈래에서 같은 열의 점은 이름 열쇠 순으로 세로로 쌓는다.
// 갈래는 큰 것부터, 같으면 가장 옛 점의 이름 열쇠 순이고, 이름은 가장 옛 점의 이름이다. 선은 새것 → 옛것이다.
class LineageLayoutCPU : public IGraphLayoutStrategy
{
public:
    bool Execute(const GraphLayoutInput& in, GraphLayoutOutput& out) override;

public:
    EGraphLayoutType GetType() const override;
};
