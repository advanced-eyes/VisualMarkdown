#include "VisualMarkdown/Algorithm/ChartBuilder/Strategy/HeatmapChartCPU.h"

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartBuilderCommon.h"

#include <QPair>
#include <QSet>

#include <algorithm>

namespace
{
using ChartBuilderCommon::ChartColumns;
using ChartBuilderCommon::ChartRow;

// 한 계열에 같은 칸(같은 범주 · 같은 series 값)이 다시 나온 행은 빼고, 그 행이 들어갈 계열마다(y 마다) 경고를 단다.
// series 열이 없으면 행마다 범주 하나라 겹치지 않는다
QVector<ChartRow> DropDuplicateCells(const KnowledgeTable& table, const ChartColumns& columns, const QVector<ChartRow>& rows, ChartSpec* spec)
{
    if (columns.iSeries < 0)
        return rows;
    QVector<ChartRow> vecKept;
    QSet<QPair<int, QString>> setCells;   // (범주 자리, series 값)
    for (const ChartRow& row : rows)
    {
        const QString strGroup = table.vecRows[row.iRow].value(columns.iSeries).trimmed();
        const QPair<int, QString> pairCell(static_cast<int>(row.dX), strGroup);
        if (!setCells.contains(pairCell))
        {
            setCells.insert(pairCell);
            vecKept << row;
            continue;
        }
        for (const int iY : columns.vecY)
            spec->vecWarnings << QStringLiteral("%1행: 같은 칸(x \"%2\" · 계열 \"%3\")이 겹쳐 이 행을 뺀다")
                                     .arg(QString::number(row.iRow + 1), row.strLabel, ChartBuilderCommon::MakeSeriesName(table, columns, iY, strGroup));
    }
    return vecKept;
}

// 색 눈금: y 열(축)마다 그 축 계열 값의 가장 작은 값 · 가장 큰 값 그대로(여유 · 0 · 눈금 없음). 값이 없으면 0 · 0
void ScaleColorAxes(ChartSpec* spec)
{
    for (int iAxis = 0; iAxis < spec->vecYAxes.size(); ++iAxis)
    {
        bool bAnyValue = false;
        double dMin = 0;
        double dMax = 0;
        for (const ChartSeries& series : spec->vecSeries)
        {
            if (series.iAxis != iAxis)
                continue;
            for (const ChartPoint& point : series.vecPoints)
            {
                if (!point.optY)
                    continue;
                dMin = bAnyValue ? std::min(dMin, *point.optY) : *point.optY;
                dMax = bAnyValue ? std::max(dMax, *point.optY) : *point.optY;
                bAnyValue = true;
            }
        }
        ChartAxis& axis = spec->vecYAxes[iAxis];
        axis.dMin = dMin;
        axis.dMax = dMax;
        axis.vecTicks.clear();
    }
}
}

bool HeatmapChartCPU::Execute(const ChartBuildInput& in, ChartSpec& out)
{
    out = ChartSpec();
    if (in.pTable == nullptr)
        return false;
    const KnowledgeTable& table = *in.pTable;
    ChartColumns columns;
    if (!ChartBuilderCommon::BeginChart(table, EChartType::HEATMAP, &out, &columns))
        return true;
    // 행 놓기 · 계열은 막대와 같다(행 = x 범주). 축만 y 열마다다
    const QVector<ChartRow> vecPlaced = ChartBuilderCommon::PlaceOnCategories(table, columns, &out);
    const QVector<ChartRow> vecRows = DropDuplicateCells(table, columns, vecPlaced, &out);
    ChartBuilderCommon::BuildSeries(table, columns, vecRows, &out, ChartBuilderCommon::EYAxisGrouping::BY_COLUMN);
    ScaleColorAxes(&out);
    return true;
}

EChartType HeatmapChartCPU::GetType() const
{
    return EChartType::HEATMAP;
}
