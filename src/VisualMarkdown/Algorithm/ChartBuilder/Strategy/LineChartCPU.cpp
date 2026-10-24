#include "VisualMarkdown/Algorithm/ChartBuilder/Strategy/LineChartCPU.h"

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartBuilderCommon.h"

bool LineChartCPU::Execute(const ChartBuildInput& in, ChartSpec& out)
{
    out = ChartSpec();
    if (in.pTable == nullptr)
        return false;
    const KnowledgeTable& table = *in.pTable;
    ChartBuilderCommon::ChartColumns columns;
    if (!ChartBuilderCommon::BeginChart(table, EChartType::LINE, &out, &columns))
        return true;
    const QVector<ChartBuilderCommon::ChartRow> vecRows = ChartBuilderCommon::PlaceOnCategories(table, columns, &out);
    ChartBuilderCommon::BuildSeries(table, columns, vecRows, &out);
    ChartBuilderCommon::ScaleYAxes(&out, false);
    return true;
}

EChartType LineChartCPU::GetType() const
{
    return EChartType::LINE;
}
