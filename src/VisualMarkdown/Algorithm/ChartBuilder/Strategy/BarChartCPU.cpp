#include "VisualMarkdown/Algorithm/ChartBuilder/Strategy/BarChartCPU.h"

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartBuilderCommon.h"

bool BarChartCPU::Execute(const ChartBuildInput& in, ChartSpec& out)
{
    out = ChartSpec();
    if (in.pTable == nullptr)
        return false;
    const KnowledgeTable& table = *in.pTable;
    ChartBuilderCommon::ChartColumns columns;
    if (!ChartBuilderCommon::BeginChart(table, EChartType::BAR, &out, &columns))
        return true;
    // 막대는 0 부터 값까지(dBase 기본값 0)
    const QVector<ChartBuilderCommon::ChartRow> vecRows = ChartBuilderCommon::PlaceOnCategories(table, columns, &out);
    ChartBuilderCommon::BuildSeries(table, columns, vecRows, &out);
    ChartBuilderCommon::ScaleYAxes(&out, true);
    return true;
}

EChartType BarChartCPU::GetType() const
{
    return EChartType::BAR;
}
