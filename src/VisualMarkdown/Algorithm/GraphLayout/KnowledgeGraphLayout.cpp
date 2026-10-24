#include "VisualMarkdown/Algorithm/GraphLayout/KnowledgeGraphLayout.h"

#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/BipartiteLayoutCPU.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/LineageLayoutCPU.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/MatrixLayoutCPU.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/NetworkLayoutCPU.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/TimelineLayoutCPU.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Strategy/TreeLayoutCPU.h"

#include <utility>

KnowledgeGraphLayout::KnowledgeGraphLayout()
    : m_hashStrategy()
    , m_eStrategy(EGraphLayoutType::NETWORK)
{
    // 보기마다 전략 하나(Patterns.md §1.2). 모두 가볍고 상태가 없어 처음에 다 만든다
    std::unique_ptr<IGraphLayoutStrategy> arrStrategies[] = {
        std::make_unique<NetworkLayoutCPU>(), std::make_unique<LineageLayoutCPU>(), std::make_unique<TreeLayoutCPU>(),
        std::make_unique<TimelineLayoutCPU>(), std::make_unique<MatrixLayoutCPU>(), std::make_unique<BipartiteLayoutCPU>(),
    };
    for (std::unique_ptr<IGraphLayoutStrategy>& upStrategy : arrStrategies)
    {
        const EGraphLayoutType eType = upStrategy->GetType();
        m_hashStrategy.emplace(eType, std::move(upStrategy));
    }
}

KnowledgeGraphLayout::~KnowledgeGraphLayout() = default;

void KnowledgeGraphLayout::SetStrategy(EGraphLayoutType type)
{
    if (m_hashStrategy.count(type))
        m_eStrategy = type;
}

bool KnowledgeGraphLayout::Execute(const GraphLayoutInput& in, GraphLayoutOutput& out)
{
    out = GraphLayoutOutput();
    const auto it = m_hashStrategy.find(m_eStrategy);
    if (in.pGraph == nullptr || it == m_hashStrategy.end())
        return false;
    return it->second->Execute(in, out);
}

EGraphLayoutType KnowledgeGraphLayout::GetStrategy() const
{
    return m_eStrategy;
}
