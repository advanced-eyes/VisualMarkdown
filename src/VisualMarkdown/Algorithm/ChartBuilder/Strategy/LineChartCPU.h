#pragma once

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"

// 선: x 는 범주(행 순서), 계열마다 선 하나. 빈 값에서 선을 끊는다. y 축은 값 범위에 여유를 둔다(여유는 0 을 넘지 않는다).
class LineChartCPU : public IChartBuilderStrategy
{
public:
    bool Execute(const ChartBuildInput& in, ChartSpec& out) override;

public:
    EChartType GetType() const override;
};
