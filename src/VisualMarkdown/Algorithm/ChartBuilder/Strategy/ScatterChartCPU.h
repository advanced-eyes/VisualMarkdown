#pragma once

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"

// 산점도: x 는 수치(x 값), 계열마다 색을 나눈다. x 가 비었거나 숫자가 아닌 행은 뺀다.
// 두 축 모두 값 범위에 여유를 둔다(여유는 0 을 넘지 않는다).
class ScatterChartCPU : public IChartBuilderStrategy
{
public:
    bool Execute(const ChartBuildInput& in, ChartSpec& out) override;

public:
    EChartType GetType() const override;
};
