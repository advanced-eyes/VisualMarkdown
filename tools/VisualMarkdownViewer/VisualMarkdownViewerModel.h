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
// wiki 를 읽어 지식 사이 보기 7개의 배치와 지식 안 표마다의 차트 설정을 낸다.
// - 배치는 그 보기를 처음 달라고 할 때 계산하고, wiki 를 다시 읽을 때까지 둔다. 이웃 배치는 초점이 바뀌면 버린다.
// - 이웃 보기의 초점 id 를 갖는다(design/MoreViews.md §6.2). 처음 · 다시 읽은 뒤에는 비었다(기본 초점).
// - 차트 설정은 달라고 할 때(표를 고를 때) 만든다. 고른 차트 종류는 표마다 기억하고, 다시 읽으면 지운다.
// - 읽지 못하면 앞 상태(색인 · 그래프 · 배치 · 초점 · 고른 차트)를 그대로 둔다.
class VisualMarkdownViewerModel
{
public:
    VisualMarkdownViewerModel();
    ~VisualMarkdownViewerModel();

public:
    // wiki 폴더를 읽는다. 읽지 못하면 false 이고 앞 상태를 그대로 둔다(까닭은 GetLastError)
    bool Load(const QString& wikiRoot);

    // 지식 사이: 보기 7개(나무 · 스냅샷 순서). 이웃은 관계망 다음이다
    static QVector<EGraphLayoutType> ListViews();
    static QString GetViewLabel(EGraphLayoutType type);    // 관계망 · 이웃 · 계보도 · 분류 트리 · 타임라인 · 관계 행렬 · 문서 ↔ 개념
    static QString GetViewKey(EGraphLayoutType type);      // network · ego · lineage · tree · timeline · matrix · bipartite
    QString GetViewTitle(EGraphLayoutType type) const;     // 나무 글: 보기 이름. 이웃은 "이웃 · {초점 이름}"(점이 없으면 이름만)
    const GraphLayoutOutput& GetLayout(EGraphLayoutType type);   // 처음 부를 때 계산한다. 이웃은 지금 초점 · 깊이 kEgoDepth 로
    bool HasLayout(EGraphLayoutType type) const;                 // 이미 계산했는지
    const GraphLayoutOutput* FindLayout(EGraphLayoutType type) const;   // 계산한 배치. 아직 계산하지 않았으면 nullptr(계산하지 않는다)

    // 이웃 보기의 초점 개념 id 를 둔다. 실제 초점(GetFocusConcept)이 바뀌면 이웃 배치를 버리고 true 다
    bool SetFocusConcept(const QString& conceptId);
    // 실제 초점: 둔 id 가 보이는 점이면 그것, 아니면 기본 초점(KnowledgeGraphLayout::FindDefaultFocus). 점이 없으면 빈 글
    QString GetFocusConcept() const;

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

    static QString GetChartTypeKey(EChartType type);     // EChartType 이름: BAR · STEP_BAR · LINE · SCATTER · HEATMAP · DONUT · NONE
    static QString GetChartTypeLabel(EChartType type);   // 막대 · 단계 막대 · 선 · 산점도 · 히트맵 · 도넛 · 그리지 않음

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

public:
    static constexpr int kEgoDepth = 2;   // 이웃 보기의 깊이(보기 앱에는 고르는 칸이 없다)

private:
    KnowledgeIndex m_index;
    KnowledgeGraph m_graph;
    QVector<KnowledgeDocument> m_vecDocuments;
    std::map<EGraphLayoutType, GraphLayoutOutput> m_mapLayouts;          // 계산한 배치
    std::map<std::pair<int, int>, EChartType> m_mapChosenCharts;         // (문서, 표) → 고른 종류
    KnowledgeGraphLayout m_graphLayout;
    KnowledgeChartBuilder m_chartBuilder;
    QString m_strFocusId;                                                // 이웃 보기의 초점(비었으면 기본 초점)
    QString m_strWikiRoot;
    QString m_strLastError;
    bool m_bLoaded;
};
