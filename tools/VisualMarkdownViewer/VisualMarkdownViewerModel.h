#pragma once

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"
#include "VisualMarkdown/Algorithm/ChartBuilder/KnowledgeChartBuilder.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/Algorithm/GraphLayout/KnowledgeGraphLayout.h"
#include "VisualMarkdown/KnowledgeIndex.h"
#include "VisualMarkdown/KnowledgeTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <map>
#include <optional>
#include <utility>

// 보기 앱의 화면 로직(design/Views.md §8). QtCore 만 쓴다(창 없이 시험한다). 창 · 스냅샷이 이것을 쓴다.
// wiki 를 읽어 지식 사이 보기 6개의 배치와 지식 안 표마다의 차트 설정을 낸다.
// - 배치는 그 보기를 처음 달라고 할 때 계산하고, wiki 를 다시 읽을 때까지 둔다.
// - 차트 설정은 달라고 할 때(표를 고를 때) 만든다. 고른 차트 종류는 표마다 기억하고, 다시 읽으면 지운다.
// - 읽지 못하면 앞 상태(색인 · 그래프 · 배치 · 고른 차트)를 그대로 둔다.
class VisualMarkdownViewerModel
{
public:
    VisualMarkdownViewerModel();
    ~VisualMarkdownViewerModel();

public:
    // wiki 폴더를 읽는다. 읽지 못하면 false 이고 앞 상태를 그대로 둔다(까닭은 GetLastError)
    bool Load(const QString& wikiRoot);

    // 지식 사이: 보기 6개(나무 · 스냅샷 순서)
    static QVector<EGraphLayoutType> ListViews();
    static QString GetViewLabel(EGraphLayoutType type);    // 관계망 · 계보도 · 분류 트리 · 타임라인 · 관계 행렬 · 문서 ↔ 개념
    static QString GetViewKey(EGraphLayoutType type);      // network · lineage · tree · timeline · matrix · bipartite
    const GraphLayoutOutput& GetLayout(EGraphLayoutType type);   // 처음 부를 때 계산한다
    bool HasLayout(EGraphLayoutType type) const;                 // 이미 계산했는지
    const GraphLayoutOutput* FindLayout(EGraphLayoutType type) const;   // 계산한 배치. 아직 계산하지 않았으면 nullptr(계산하지 않는다)

    // 지식 안: 문서(문서 목록 순)와 그 안의 표
    int GetDocumentCount() const;
    const KnowledgeDocument* FindDocument(int document) const;     // 없는 자리면 nullptr
    int FindDocumentIndex(const QString& documentId) const;        // 없으면 -1
    int GetTableCount(int document) const;
    const KnowledgeTable* FindTable(int document, int table) const;   // 없는 자리면 nullptr
    EChartType GetRecommendedChart(int document, int table) const;    // KnowledgeChartBuilder::Recommend
    std::optional<EChartType> GetChosenChart(int document, int table) const;   // 고르지 않았으면 비었다
    EChartType GetChartType(int document, int table) const;           // 고른 종류, 없으면 추천 종류
    // 차트 종류를 고른다. 비우면 추천으로 되돌린다
    void ChooseChart(int document, int table, std::optional<EChartType> type);
    // 그 표의 차트 설정을 만든다: 고른 종류면 SetStrategy, 아니면 ResetStrategy 로 추천 종류
    ChartSpec BuildChart(int document, int table);

    static QString GetChartTypeKey(EChartType type);     // EChartType 이름: BAR · STEP_BAR · LINE · SCATTER · NONE
    static QString GetChartTypeLabel(EChartType type);   // 막대 · 단계 막대 · 선 · 산점도 · 그리지 않음

    // 상태 줄 글
    QString DescribeConcept(const QString& conceptId) const;   // 이름 · 종류 · 연도(없으면 "연도 없음"). 모르는 id 는 그대로
    QStringList DescribeDroppedRelations() const;              // 빠진 관계마다 "from 종류 → to (출처)"

public:
    bool IsLoaded() const;
    QString GetWikiRoot() const;
    QString GetLastError() const;
    const KnowledgeIndex& GetIndex() const;
    const KnowledgeGraph& GetGraph() const;
    int GetNodeCount() const;
    int GetEdgeCount() const;
    int GetDroppedRelationCount() const;
    QStringList GetWarnings() const;

private:
    KnowledgeIndex m_index;
    KnowledgeGraph m_graph;
    QVector<KnowledgeDocument> m_vecDocuments;
    std::map<EGraphLayoutType, GraphLayoutOutput> m_mapLayouts;          // 계산한 배치
    std::map<std::pair<int, int>, EChartType> m_mapChosenCharts;         // (문서, 표) → 고른 종류
    KnowledgeGraphLayout m_graphLayout;
    KnowledgeChartBuilder m_chartBuilder;
    QString m_strWikiRoot;
    QString m_strLastError;
    bool m_bLoaded;
};
