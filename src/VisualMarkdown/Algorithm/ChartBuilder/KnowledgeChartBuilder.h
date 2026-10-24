#pragma once

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"
#include "VisualMarkdown/Interface/IAlgorithm.h"

#include <memory>
#include <optional>
#include <unordered_map>

// 지식 안 차트 설정(design/Views.md §6). 차트 종류마다 전략 하나를 갖는다.
// 종류를 고르지 않으면 Recommend 가 고른 종류로, SetStrategy 로 고르면 그 종류로 그린다(보기 앱에서 사용자가 바꿀 수 있다).
class KnowledgeChartBuilder : public VisualMarkdown::IAlgorithm<ChartBuildInput, ChartSpec, EChartType>
{
public:
    KnowledgeChartBuilder();
    ~KnowledgeChartBuilder() override;

public:
    // 규칙 1 · 2: 수치인 y 가 없거나 모르는 열이 있으면 NONE. 그 밖에는 x 열 종류로 고른다
    // (범주 → BAR, 범주 + 계열 넷 이상 → HEATMAP, 순서 + cumulative → STEP_BAR, 순서 → LINE, 수치 → SCATTER).
    // 계열 수는 수치인 y 열 수 × series 값(빈 칸이 아닌 것) 수다(series 열이 없으면 × 1)
    static EChartType Recommend(const KnowledgeTable& table);

    // 차트 종류를 정한다. NONE 이면 그리지 않는다
    void SetStrategy(EChartType type) override;
    // 정한 종류를 지우고 다시 추천 종류로 그린다
    void ResetStrategy();
    // out 을 비우고 차트 설정을 채운다. 그릴 수 없으면 NONE 과 그 까닭(vecWarnings)을 남긴다. 표가 없으면 false 다
    bool Execute(const ChartBuildInput& in, ChartSpec& out) override;

public:
    std::optional<EChartType> GetStrategy() const;   // 정한 종류. 정하지 않았으면 비었다(추천 종류를 쓴다)

private:
    std::unordered_map<EChartType, std::unique_ptr<IChartBuilderStrategy>> m_hashStrategy;
    std::optional<EChartType> m_optStrategy;
};
