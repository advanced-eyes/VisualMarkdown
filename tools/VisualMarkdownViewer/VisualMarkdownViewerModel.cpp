#include "VisualMarkdownViewer/VisualMarkdownViewerModel.h"

#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <utility>

namespace
{
const QString kJoin = QStringLiteral(" · ");
const QString kNoYear = QStringLiteral("연도 없음");
const QString kNoKind = QStringLiteral("종류 없음");
const QString kDirectedArrow = QStringLiteral(" → ");
const QString kUndirectedArrow = QStringLiteral(" ↔ ");
}

VisualMarkdownViewerModel::VisualMarkdownViewerModel()
    : m_index()
    , m_graph()
    , m_vecDocuments()
    , m_mapLayouts()
    , m_mapChosenCharts()
    , m_graphLayout()
    , m_chartBuilder()
    , m_strFocusId()
    , m_strWikiRoot()
    , m_strLastError()
    , m_bLoaded(false)
{
}

VisualMarkdownViewerModel::~VisualMarkdownViewerModel() = default;

bool VisualMarkdownViewerModel::Load(const QString& wikiRoot)
{
    // 새 색인에 읽어 보고, 읽었을 때만 바꾼다(KnowledgeIndex::Load 는 앞서 읽은 것을 지운다)
    KnowledgeIndex index;
    if (!index.Load(wikiRoot))
    {
        const QStringList vecReasons = index.GetWarnings();
        m_strLastError = QStringLiteral("wiki 를 읽지 못했다: %1").arg(wikiRoot);
        if (!vecReasons.isEmpty())
            m_strLastError += QStringLiteral(" (") + vecReasons.join(QStringLiteral("; ")) + QLatin1Char(')');
        return false;
    }
    m_index = std::move(index);
    m_graph = KnowledgeGraph::BuildFromIndex(m_index);
    m_vecDocuments = m_index.ListDocuments();
    m_mapLayouts.clear();
    m_mapChosenCharts.clear();
    m_strFocusId.clear();
    m_strWikiRoot = wikiRoot;
    m_strLastError.clear();
    m_bLoaded = true;
    return true;
}

QVector<EGraphLayoutType> VisualMarkdownViewerModel::ListViews()
{
    return { EGraphLayoutType::NETWORK, EGraphLayoutType::EGO, EGraphLayoutType::LINEAGE, EGraphLayoutType::TREE,
             EGraphLayoutType::TIMELINE, EGraphLayoutType::MATRIX, EGraphLayoutType::BIPARTITE };
}

QString VisualMarkdownViewerModel::GetViewLabel(EGraphLayoutType type)
{
    switch (type)
    {
    case EGraphLayoutType::NETWORK: return QStringLiteral("관계망");
    case EGraphLayoutType::LINEAGE: return QStringLiteral("계보도");
    case EGraphLayoutType::TREE: return QStringLiteral("분류 트리");
    case EGraphLayoutType::TIMELINE: return QStringLiteral("타임라인");
    case EGraphLayoutType::MATRIX: return QStringLiteral("관계 행렬");
    case EGraphLayoutType::BIPARTITE: return QStringLiteral("문서 ↔ 개념");
    case EGraphLayoutType::EGO: return QStringLiteral("이웃");
    }
    return QString();
}

QString VisualMarkdownViewerModel::GetViewKey(EGraphLayoutType type)
{
    switch (type)
    {
    case EGraphLayoutType::NETWORK: return QStringLiteral("network");
    case EGraphLayoutType::LINEAGE: return QStringLiteral("lineage");
    case EGraphLayoutType::TREE: return QStringLiteral("tree");
    case EGraphLayoutType::TIMELINE: return QStringLiteral("timeline");
    case EGraphLayoutType::MATRIX: return QStringLiteral("matrix");
    case EGraphLayoutType::BIPARTITE: return QStringLiteral("bipartite");
    case EGraphLayoutType::EGO: return QStringLiteral("ego");
    }
    return QString();
}

QString VisualMarkdownViewerModel::GetViewTitle(EGraphLayoutType type) const
{
    const QString strLabel = GetViewLabel(type);
    if (type != EGraphLayoutType::EGO)
        return strLabel;
    const QString strFocus = GetFocusConcept();
    const int iFocus = m_graph.FindNode(strFocus);
    if (iFocus < 0)
        return strLabel;
    const QString strName = m_graph.GetNodes()[iFocus].strName;
    return strLabel + kJoin + (strName.isEmpty() ? strFocus : strName);
}

const GraphLayoutOutput& VisualMarkdownViewerModel::GetLayout(EGraphLayoutType type)
{
    const auto it = m_mapLayouts.find(type);
    if (it != m_mapLayouts.end())
        return it->second;
    GraphLayoutInput in;
    in.pGraph = &m_graph;
    if (type == EGraphLayoutType::EGO)
    {
        in.strFocusId = GetFocusConcept();
        in.iDepth = kEgoDepth;
    }
    GraphLayoutOutput out;
    m_graphLayout.SetStrategy(type);
    m_graphLayout.Execute(in, out);
    return m_mapLayouts.emplace(type, std::move(out)).first->second;
}

bool VisualMarkdownViewerModel::HasLayout(EGraphLayoutType type) const
{
    return m_mapLayouts.count(type) > 0;
}

const GraphLayoutOutput* VisualMarkdownViewerModel::FindLayout(EGraphLayoutType type) const
{
    const auto it = m_mapLayouts.find(type);
    return it == m_mapLayouts.end() ? nullptr : &it->second;
}

bool VisualMarkdownViewerModel::SetFocusConcept(const QString& conceptId)
{
    const QString strBefore = GetFocusConcept();
    m_strFocusId = conceptId;
    if (GetFocusConcept() == strBefore)
        return false;
    m_mapLayouts.erase(EGraphLayoutType::EGO);
    return true;
}

QString VisualMarkdownViewerModel::GetFocusConcept() const
{
    if (m_graph.FindNode(m_strFocusId) >= 0)
        return m_strFocusId;
    return KnowledgeGraphLayout::FindDefaultFocus(m_graph);
}

int VisualMarkdownViewerModel::GetDocumentCount() const
{
    return m_vecDocuments.size();
}

const KnowledgeDocument* VisualMarkdownViewerModel::FindDocument(int document) const
{
    return document >= 0 && document < m_vecDocuments.size() ? &m_vecDocuments[document] : nullptr;
}

int VisualMarkdownViewerModel::FindDocumentIndex(const QString& documentId) const
{
    for (int iDocument = 0; iDocument < m_vecDocuments.size(); ++iDocument)
        if (m_vecDocuments[iDocument].strId == documentId)
            return iDocument;
    return -1;
}

int VisualMarkdownViewerModel::GetTableCount(int document) const
{
    const KnowledgeDocument* pDocument = FindDocument(document);
    return pDocument != nullptr ? pDocument->vecTables.size() : 0;
}

const KnowledgeTable* VisualMarkdownViewerModel::FindTable(int document, int table) const
{
    const KnowledgeDocument* pDocument = FindDocument(document);
    if (pDocument == nullptr || table < 0 || table >= pDocument->vecTables.size())
        return nullptr;
    return &pDocument->vecTables[table];
}

EChartType VisualMarkdownViewerModel::GetRecommendedChart(int document, int table) const
{
    const KnowledgeTable* pTable = FindTable(document, table);
    return pTable != nullptr ? KnowledgeChartBuilder::Recommend(*pTable) : EChartType::NONE;
}

std::optional<EChartType> VisualMarkdownViewerModel::GetChosenChart(int document, int table) const
{
    const auto it = m_mapChosenCharts.find(std::make_pair(document, table));
    if (it == m_mapChosenCharts.end())
        return std::nullopt;
    return it->second;
}

EChartType VisualMarkdownViewerModel::GetChartType(int document, int table) const
{
    const std::optional<EChartType> optChosen = GetChosenChart(document, table);
    return optChosen ? *optChosen : GetRecommendedChart(document, table);
}

void VisualMarkdownViewerModel::ChooseChart(int document, int table, std::optional<EChartType> type)
{
    if (FindTable(document, table) == nullptr)
        return;
    if (type)
        m_mapChosenCharts[std::make_pair(document, table)] = *type;
    else
        m_mapChosenCharts.erase(std::make_pair(document, table));
}

ChartSpec VisualMarkdownViewerModel::BuildChart(int document, int table)
{
    ChartSpec spec;
    const KnowledgeTable* pTable = FindTable(document, table);
    if (pTable == nullptr)
        return spec;
    const std::optional<EChartType> optChosen = GetChosenChart(document, table);
    if (optChosen)
        m_chartBuilder.SetStrategy(*optChosen);
    else
        m_chartBuilder.ResetStrategy();
    ChartBuildInput in;
    in.pTable = pTable;
    m_chartBuilder.Execute(in, spec);
    return spec;
}

QString VisualMarkdownViewerModel::GetChartTypeKey(EChartType type)
{
    switch (type)
    {
    case EChartType::NONE: return QStringLiteral("NONE");
    case EChartType::BAR: return QStringLiteral("BAR");
    case EChartType::STEP_BAR: return QStringLiteral("STEP_BAR");
    case EChartType::LINE: return QStringLiteral("LINE");
    case EChartType::SCATTER: return QStringLiteral("SCATTER");
    case EChartType::HEATMAP: return QStringLiteral("HEATMAP");
    case EChartType::DONUT: return QStringLiteral("DONUT");
    }
    return QString();
}

QString VisualMarkdownViewerModel::GetChartTypeLabel(EChartType type)
{
    switch (type)
    {
    case EChartType::NONE: return QStringLiteral("그리지 않음");
    case EChartType::BAR: return QStringLiteral("막대");
    case EChartType::STEP_BAR: return QStringLiteral("단계 막대");
    case EChartType::LINE: return QStringLiteral("선");
    case EChartType::SCATTER: return QStringLiteral("산점도");
    case EChartType::HEATMAP: return QStringLiteral("히트맵");
    case EChartType::DONUT: return QStringLiteral("도넛");
    }
    return QString();
}

QString VisualMarkdownViewerModel::DescribeConcept(const QString& conceptId) const
{
    const std::optional<KnowledgeConcept> optConcept = m_index.FindConcept(conceptId);
    if (!optConcept)
        return conceptId;
    const QString strName = optConcept->strName.isEmpty() ? conceptId : optConcept->strName;
    const QString strKind = optConcept->strKind.isEmpty() ? kNoKind : KnowledgeVocabulary::Get().ToConceptLabel(optConcept->strKind);
    const QString strYear = optConcept->strYear.isEmpty() ? kNoYear : optConcept->strYear;
    return QStringList { strName, strKind, strYear }.join(kJoin);
}

QStringList VisualMarkdownViewerModel::DescribeDroppedRelations() const
{
    const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
    const auto funcName = [this](const QString& conceptId)
    {
        const std::optional<KnowledgeConcept> optConcept = m_index.FindConcept(conceptId);
        return optConcept && !optConcept->strName.isEmpty() ? optConcept->strName : conceptId;
    };
    QStringList vecLines;
    for (const KnowledgeDroppedRelation& dropped : m_index.ListDroppedRelations())
    {
        // 출처: 문서면 그 제목, 개념 파일이면 그 개념 이름
        const KnowledgeDocument* pDocument = FindDocument(FindDocumentIndex(dropped.strSource));
        const QString strSource = pDocument != nullptr ? pDocument->strTitle : QStringLiteral("개념 파일 ") + funcName(dropped.strSource);
        const QString strArrow = vocabulary.IsUndirected(dropped.strKind) ? kUndirectedArrow : kDirectedArrow;
        vecLines << QStringLiteral("%1 %2%3%4 (%5)")
                        .arg(funcName(dropped.strFromId), vocabulary.ToRelationLabel(dropped.strKind), strArrow, funcName(dropped.strToId), strSource);
    }
    return vecLines;
}

bool VisualMarkdownViewerModel::IsLoaded() const
{
    return m_bLoaded;
}

QString VisualMarkdownViewerModel::GetWikiRoot() const
{
    return m_strWikiRoot;
}

QString VisualMarkdownViewerModel::GetLastError() const
{
    return m_strLastError;
}

const KnowledgeIndex& VisualMarkdownViewerModel::GetIndex() const
{
    return m_index;
}

const KnowledgeGraph& VisualMarkdownViewerModel::GetGraph() const
{
    return m_graph;
}

int VisualMarkdownViewerModel::GetNodeCount() const
{
    return m_graph.GetNodes().size();
}

int VisualMarkdownViewerModel::GetEdgeCount() const
{
    return m_graph.GetEdges().size();
}

int VisualMarkdownViewerModel::GetDroppedRelationCount() const
{
    return m_index.ListDroppedRelations().size();
}

QStringList VisualMarkdownViewerModel::GetWarnings() const
{
    return m_index.GetWarnings();
}
