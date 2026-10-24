#pragma once

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"

// 히트맵(design/MoreViews.md §4.1): 행은 x 범주(행 순서), 열은 계열(y, series 가 있으면 그 값마다). 칸 값은 그 계열에서 그 범주 점의 값이다.
// 색 눈금은 y 열마다 하나(vecYAxes 는 y 순서, 계열의 iAxis 는 그 y 열 자리)이고, 범위는 그 y 열 값의 가장 작은 값 · 가장 큰 값 그대로다(여유 · 0 · 눈금 없음).
// 한 계열에 같은 칸(같은 x 글 · 같은 series 값)이 둘 이상이면 첫 점만 두고 경고를 단다. 단위 셋째부터의 경고(규칙 4)는 없다.
class HeatmapChartCPU : public IChartBuilderStrategy
{
public:
    bool Execute(const ChartBuildInput& in, ChartSpec& out) override;

public:
    EChartType GetType() const override;
};
