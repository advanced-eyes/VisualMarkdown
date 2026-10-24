#pragma once

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"

// 도넛(design/MoreViews.md §5.1): 행 놓기 · 계열은 막대와 같고, 조각은 첫 계열의 점이다(범주 자리 = 조각 자리, 행 순서).
// 계열이 둘 이상이면 첫 계열만 두고 나머지를 빼며 경고를 단다. vecYAxes 는 하나다: 첫 계열의 y 열 이름 · 단위, 0 부터 0 보다 큰 값의 합(전체)까지, 눈금 없음.
// 값이 없거나 0 보다 작은 점은 조각이 되지 않는다(점은 남기고 행마다 경고를 단다). 단위가 % 인데 합이 100 ± 1 밖이거나 조각이 6개를 넘으면 경고를 단다.
// 단위 셋째부터의 경고(규칙 4)는 없다(축이 하나라서). 추천 규칙(몫 표)은 ChartBuilderCommon::IsShareTable 이다.
class DonutChartCPU : public IChartBuilderStrategy
{
public:
    bool Execute(const ChartBuildInput& in, ChartSpec& out) override;

public:
    EChartType GetType() const override;
};
