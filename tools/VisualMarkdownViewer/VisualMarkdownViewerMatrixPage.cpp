#include "VisualMarkdownViewer/VisualMarkdownViewerMatrixPage.h"

#include "VisualMarkdown/UI/KnowledgeMatrixView.h"

VisualMarkdownViewerMatrixPage::VisualMarkdownViewerMatrixPage(QWidget* parent)
    : QScrollArea(parent)
    , m_pMatrixView(new KnowledgeMatrixView)
{
    // 행렬 위젯을 쪽 크기로 늘리되 크기 힌트(가장 작은 칸)보다 작게 줄이지 않는다. 그보다 작은 쪽이면 스크롤 막대가 나온다
    setFrameShape(QFrame::NoFrame);
    setWidgetResizable(true);
    setWidget(m_pMatrixView);
}

VisualMarkdownViewerMatrixPage::~VisualMarkdownViewerMatrixPage() = default;

KnowledgeMatrixView* VisualMarkdownViewerMatrixPage::GetMatrixView() const
{
    return m_pMatrixView;
}
