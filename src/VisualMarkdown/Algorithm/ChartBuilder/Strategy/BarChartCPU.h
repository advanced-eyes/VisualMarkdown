#pragma once

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"

// 막대: x 는 범주(행 순서), 계열마다 막대를 나란히 둔다. 막대는 0 부터 값까지이고 y 축은 0 을 넣는다.
class BarChartCPU : public IChartBuilderStrategy
{
public:
    bool Execute(const ChartBuildInput& in, ChartSpec& out) override;

public:
    EChartType GetType() const override;
};
