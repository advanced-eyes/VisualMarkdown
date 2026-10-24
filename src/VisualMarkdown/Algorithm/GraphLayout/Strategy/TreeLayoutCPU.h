#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"

// 분류 트리: isA 선(from = 하위, to = 상위)과 그 양 끝. 뿌리(나가는 isA 가 없는 점)를 위에 두고 깊이마다 아래로 내린다.
// 자식은 이름 열쇠 순이고, 잎은 그 순서로 이어진 가로 칸을 받는다. 부모는 첫 자식과 마지막 자식의 가운데다.
// 나무가 여럿이면 큰 것부터(같으면 뿌리 이름 열쇠 순) 옆으로 놓는다. 부모가 둘 이상인 점은 이름 열쇠로 앞선 부모 아래에 두고
// 다른 선도 그린다. 고리가 있으면 이미 놓은 점에서 멈춘다(뿌리가 없는 고리는 그 안에서 이름 열쇠로 앞선 점을 뿌리로 삼는다).
class TreeLayoutCPU : public IGraphLayoutStrategy
{
public:
    bool Execute(const GraphLayoutInput& in, GraphLayoutOutput& out) override;

public:
    EGraphLayoutType GetType() const override;
};
