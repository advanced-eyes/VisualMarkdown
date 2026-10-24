#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"

// 관계망: 보이는 점 전부와 선 전부. 덩어리마다 Fruchterman–Reingold 힘 배치를 정한 횟수만큼 돌린다
// (처음 자리는 id 순으로 원 위, 움직일 수 있는 거리는 반복마다 고르게 줄인다. 앞 반은 모든 쌍을, 뒤 반은 가까운 쌍만 밀고,
// 덩어리 가운데로 약하게 당긴다). 그다음 겹친 점 상자를 밀어 떼고, 덩어리를 큰 것부터 왼쪽에서 오른쪽으로 놓는다.
// 선 없는 점(크기 1 덩어리)은 그 뒤에 한 줄로 둔다.
class NetworkLayoutCPU : public IGraphLayoutStrategy
{
public:
    bool Execute(const GraphLayoutInput& in, GraphLayoutOutput& out) override;

public:
    EGraphLayoutType GetType() const override;
};
