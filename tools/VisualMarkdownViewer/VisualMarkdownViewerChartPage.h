#pragma once

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"
#include "VisualMarkdown/KnowledgeTypes.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"

#include <QWidget>

#include <optional>

class KnowledgeChartView;
class QComboBox;
class QLabel;
class QTableWidget;

// 지식 안 표 하나의 쪽: 위는 차트(KnowledgeChartView), 아래는 원래 표(읽기 전용, 머리 줄은 열 이름과 단위).
// 차트 종류 상자: 추천(그 종류 이름) · 막대 · 단계 막대 · 선 · 산점도 · 히트맵 · 도넛. 고르면 ChartTypeChosen 을 낸다(차트는 쓰는 쪽이 다시 만들어 넣는다).
// 차트로 그리지 않는 표(NONE)는 차트 대신 그 까닭을 한 줄로 보이고 표를 크게 보인다(design/Views.md §6 규칙 1).
// 창과 스냅샷이 같은 쪽을 쓴다.
class VisualMarkdownViewerChartPage : public QWidget
{
    Q_OBJECT

public:
    explicit VisualMarkdownViewerChartPage(QWidget* parent = nullptr);
    ~VisualMarkdownViewerChartPage() override;

public:
    // 표 · 추천 종류 · 고른 종류(비면 추천) · 차트 설정을 보인다
    void SetTable(const KnowledgeTable& table, EChartType recommended, std::optional<EChartType> chosen, const ChartSpec& spec);
    void SetTheme(const VisualMarkdownTheme& theme);

public:
    KnowledgeChartView* GetChartView() const;
    QTableWidget* GetTableWidget() const;
    QComboBox* GetChartTypeBox() const;
    QLabel* GetTitleLabel() const;     // NONE 의 표 제목(차트가 없으니 따로 보인다). 차트를 그리면 숨긴다
    QLabel* GetMessageLabel() const;   // NONE 의 까닭 · 차트 경고 한 줄. 없으면 숨긴다

public:
    static constexpr int kRecommendedChoice = -1;   // 상자 항목의 data: 추천은 -1, 그 밖은 EChartType 값

signals:
    void ChartTypeChosen(int choice);   // kRecommendedChoice 또는 EChartType 값

private:
    void FillTable(const KnowledgeTable& table, bool fillPage);
    int ComputeTableWidth() const;
    int ComputeTableHeight() const;

private slots:
    void HandleCurrentIndexChanged(int index);   // 상자에서 고른 항목의 data 로 ChartTypeChosen 을 낸다

private:
    QComboBox* m_pChartTypeBox;
    QLabel* m_pTitleLabel;
    QLabel* m_pMessageLabel;
    KnowledgeChartView* m_pChartView;
    QTableWidget* m_pTableWidget;
};
