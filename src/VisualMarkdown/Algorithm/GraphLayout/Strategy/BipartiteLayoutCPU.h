#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"

// 문서 ↔ 개념: 왼쪽 열에 문서 전부(그래프의 문서 순), 오른쪽 열에 문서가 가리키는 개념(개념 파일이 있는 것).
// 개념은 가리키는 문서 행의 평균(무게 중심) 순, 같으면 이름 열쇠 순이다. 연결마다 선 하나를 두고 무게는 가리킨 횟수다.
// 두 열은 같은 높이에 고르게 편다.
class BipartiteLayoutCPU : public IGraphLayoutStrategy
{
public:
    bool Execute(const GraphLayoutInput& in, GraphLayoutOutput& out) override;

public:
    EGraphLayoutType GetType() const override;
};
