#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"

// 관계 행렬: 보이는 점 전부. 행 · 열 순서(vecMatrixOrder)는 덩어리 순(KnowledgeGraph 순) 안에서 이름 열쇠 순이다.
// 칸: 방향 있는 선은 (from 행, to 열), 방향 없는 선은 두 칸 모두. 한 칸에 떨어지는 종류는 모두 어휘 순으로 담는다. 대각선은 비운다.
// 점은 대각선 칸 가운데에 두고, 선은 그래프의 선마다 하나(행렬 자리로)를 둔다(위젯이 칸의 선을 찾을 때 쓴다).
class MatrixLayoutCPU : public IGraphLayoutStrategy
{
public:
    bool Execute(const GraphLayoutInput& in, GraphLayoutOutput& out) override;

public:
    EGraphLayoutType GetType() const override;
};
