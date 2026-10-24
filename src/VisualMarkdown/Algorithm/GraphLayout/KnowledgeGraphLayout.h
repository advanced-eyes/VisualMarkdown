#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/Interface/IAlgorithm.h"

#include <memory>
#include <unordered_map>

// 지식 사이 배치(design/Views.md §5). 보기마다 전략 하나를 갖고, SetStrategy 로 고른 전략으로 배치한다.
// 배치는 결정적이다: 같은 그래프면 같은 출력이 나온다(난수 · 해시 순서를 쓰지 않는다).
class KnowledgeGraphLayout : public VisualMarkdown::IAlgorithm<GraphLayoutInput, GraphLayoutOutput, EGraphLayoutType>
{
public:
    KnowledgeGraphLayout();
    ~KnowledgeGraphLayout() override;

public:
    void SetStrategy(EGraphLayoutType type) override;
    // out 을 비우고 고른 전략으로 채운다. 그래프가 없으면 false 다(out 은 빈 채로 둔다)
    bool Execute(const GraphLayoutInput& in, GraphLayoutOutput& out) override;

public:
    EGraphLayoutType GetStrategy() const;

private:
    std::unordered_map<EGraphLayoutType, std::unique_ptr<IGraphLayoutStrategy>> m_hashStrategy;
    EGraphLayoutType m_eStrategy;
};
