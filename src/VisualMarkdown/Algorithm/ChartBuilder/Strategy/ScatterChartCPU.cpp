#include "VisualMarkdown/Algorithm/ChartBuilder/Strategy/ScatterChartCPU.h"

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartBuilderCommon.h"

bool ScatterChartCPU::Execute(const ChartBuildInput& in, ChartSpec& out)
{
    out = ChartSpec();
    if (in.pTable == nullptr)
        return false;
    const KnowledgeTable& table = *in.pTable;
    ChartBuilderCommon::ChartColumns columns;
    if (!ChartBuilderCommon::BeginChart(table, EChartType::SCATTER, &out, &columns))
        return true;
    const QVector<ChartBuilderCommon::ChartRow> vecRows = ChartBuilderCommon::PlaceOnNumbers(table, columns, &out);
    ChartBuilderCommon::BuildSeries(table, columns, vecRows, &out);
    ChartBuilderCommon::ScaleYAxes(&out, false);
    return true;
}

EChartType ScatterChartCPU::GetType() const
{
    return EChartType::SCATTER;
}
