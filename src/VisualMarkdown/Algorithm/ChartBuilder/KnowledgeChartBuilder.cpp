#include "VisualMarkdown/Algorithm/ChartBuilder/KnowledgeChartBuilder.h"

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartBuilderCommon.h"
#include "VisualMarkdown/Algorithm/ChartBuilder/Strategy/BarChartCPU.h"
#include "VisualMarkdown/Algorithm/ChartBuilder/Strategy/HeatmapChartCPU.h"
#include "VisualMarkdown/Algorithm/ChartBuilder/Strategy/LineChartCPU.h"
#include "VisualMarkdown/Algorithm/ChartBuilder/Strategy/ScatterChartCPU.h"
#include "VisualMarkdown/Algorithm/ChartBuilder/Strategy/StepBarChartCPU.h"
#include "VisualMarkdown/KnowledgeText.h"

#include <utility>

namespace
{
constexpr int kHeatmapMinSeries = 4;   // 범주 x 에 계열이 이만큼 이상이면 히트맵(design/MoreViews.md §4.2)
}

KnowledgeChartBuilder::KnowledgeChartBuilder()
    : m_hashStrategy()
    , m_optStrategy()
{
    // 그리는 종류마다 전략 하나(Patterns.md §1.2). NONE 은 전략 없이 까닭만 남긴다
    std::unique_ptr<IChartBuilderStrategy> arrStrategies[] = {
        std::make_unique<BarChartCPU>(), std::make_unique<StepBarChartCPU>(), std::make_unique<LineChartCPU>(), std::make_unique<ScatterChartCPU>(),
        std::make_unique<HeatmapChartCPU>(),
    };
    for (std::unique_ptr<IChartBuilderStrategy>& upStrategy : arrStrategies)
    {
        const EChartType eType = upStrategy->GetType();
        m_hashStrategy.emplace(eType, std::move(upStrategy));
    }
}

KnowledgeChartBuilder::~KnowledgeChartBuilder() = default;

EChartType KnowledgeChartBuilder::Recommend(const KnowledgeTable& table)
{
    ChartBuilderCommon::ChartColumns columns;
    QStringList vecWarnings;
    if (!ChartBuilderCommon::ResolveColumns(table, &columns, &vecWarnings))
        return EChartType::NONE;
    switch (KnowledgeText::ClassifyColumn(table, columns.iX))
    {
    case EColumnKind::NOMINAL:
        // 계열(수치인 y 열 수 × series 값 수)이 넷부터는 나란한 막대의 값 글이 겹치고 색만으로 가르기 어렵다: 칸마다 값을 쓰는 히트맵
        return ChartBuilderCommon::CountSeries(table, columns) >= kHeatmapMinSeries ? EChartType::HEATMAP : EChartType::BAR;
    case EColumnKind::ORDINAL:
        return table.bCumulative ? EChartType::STEP_BAR : EChartType::LINE;
    case EColumnKind::QUANTITY:
        return EChartType::SCATTER;
    }
    return EChartType::BAR;
}

void KnowledgeChartBuilder::SetStrategy(EChartType type)
{
    if (type == EChartType::NONE || m_hashStrategy.count(type))
        m_optStrategy = type;
}

void KnowledgeChartBuilder::ResetStrategy()
{
    m_optStrategy.reset();
}

bool KnowledgeChartBuilder::Execute(const ChartBuildInput& in, ChartSpec& out)
{
    out = ChartSpec();
    if (in.pTable == nullptr)
        return false;
    const EChartType eType = m_optStrategy ? *m_optStrategy : Recommend(*in.pTable);
    const auto it = m_hashStrategy.find(eType);
    if (it == m_hashStrategy.end())
    {
        // NONE: 제목 · 밑글과 그리지 않는 까닭만 남긴다
        ChartBuilderCommon::ChartColumns columns;
        ChartBuilderCommon::BeginChart(*in.pTable, EChartType::NONE, &out, &columns);
        return true;
    }
    return it->second->Execute(in, out);
}

std::optional<EChartType> KnowledgeChartBuilder::GetStrategy() const
{
    return m_optStrategy;
}
