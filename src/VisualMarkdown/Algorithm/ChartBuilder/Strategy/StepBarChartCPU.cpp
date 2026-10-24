#include "VisualMarkdown/Algorithm/ChartBuilder/Strategy/StepBarChartCPU.h"

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartBuilderCommon.h"

bool StepBarChartCPU::Execute(const ChartBuildInput& in, ChartSpec& out)
{
    out = ChartSpec();
    if (in.pTable == nullptr)
        return false;
    const KnowledgeTable& table = *in.pTable;
    ChartBuilderCommon::ChartColumns columns;
    if (!ChartBuilderCommon::BeginChart(table, EChartType::STEP_BAR, &out, &columns))
        return true;
    const QVector<ChartBuilderCommon::ChartRow> vecRows = ChartBuilderCommon::PlaceOnCategories(table, columns, &out);
    ChartBuilderCommon::BuildSeries(table, columns, vecRows, &out);

    // 첫 막대는 0 부터, 그다음은 그 계열에서 값이 있는 앞 점의 값부터. 값 < 시작이면 줄어든 단계다
    for (ChartSeries& series : out.vecSeries)
    {
        double dPrevious = 0;
        for (ChartPoint& point : series.vecPoints)
        {
            point.dBase = dPrevious;
            if (point.optY)
                dPrevious = *point.optY;
        }
    }
    // 시작 값은 0 이거나 앞 값이라 0 을 넣은 값 범위 안에 든다
    ChartBuilderCommon::ScaleYAxes(&out, true);
    return true;
}

EChartType StepBarChartCPU::GetType() const
{
    return EChartType::STEP_BAR;
}
