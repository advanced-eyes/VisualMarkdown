#pragma once

#include <QScrollArea>

class KnowledgeMatrixView;

// 관계 행렬 쪽: KnowledgeMatrixView 를 스크롤 영역에 넣는다. 창이 행렬보다 크면 칸을 키워 채우고,
// 가장 작은 칸에서도 창보다 크면(KnowledgeMatrixView::minimumSizeHint) 스크롤로 본다. 창과 스냅샷이 같은 쪽을 쓴다.
class VisualMarkdownViewerMatrixPage : public QScrollArea
{
    Q_OBJECT

public:
    explicit VisualMarkdownViewerMatrixPage(QWidget* parent = nullptr);
    ~VisualMarkdownViewerMatrixPage() override;

public:
    KnowledgeMatrixView* GetMatrixView() const;

private:
    KnowledgeMatrixView* m_pMatrixView;
};
