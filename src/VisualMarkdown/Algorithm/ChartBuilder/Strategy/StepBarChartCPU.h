#pragma once

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"

// 단계 막대(누적 ablation): x 는 범주(행 순서). 첫 막대는 0 부터, 그다음 막대는 앞 값에서 이 값까지다(dBase).
// 앞 값은 그 계열에서 값이 있는 앞 점의 값이다. y 축은 0 을 넣는다.
class StepBarChartCPU : public IChartBuilderStrategy
{
public:
    bool Execute(const ChartBuildInput& in, ChartSpec& out) override;

public:
    EChartType GetType() const override;
};
