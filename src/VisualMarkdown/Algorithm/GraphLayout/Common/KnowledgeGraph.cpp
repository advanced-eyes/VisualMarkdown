#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"

#include "VisualMarkdown/KnowledgeText.h"
#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <QMap>
#include <QSet>

#include <algorithm>

namespace
{
constexpr int kYearDigits = 4;   // 타임라인에 놓는 연도는 네 자리 숫자다

// year 가 네 자리 숫자면 그 해, 아니면 -1. QChar::isDigit 은 다른 문자의 숫자도 받으므로 0–9 만 본다
int ParseYear(const QString& year)
{
    if (year.size() != kYearDigits)
        return -1;
    for (const QChar chDigit : year)
        if (chDigit < QLatin1Char('0') || chDigit > QLatin1Char('9'))
            return -1;
    return year.toInt();
}
}

KnowledgeGraph KnowledgeGraph::BuildFromIndex(const KnowledgeIndex& index)
{
    return BuildFromParts(index.ListConcepts(), index.ListEdges(), index.ListDocuments());
}

KnowledgeGraph KnowledgeGraph::BuildFromParts(const QVector<KnowledgeConcept>& concepts, const QVector<KnowledgeEdge>& edges,
                                              const QVector<KnowledgeDocument>& documents)
{
    KnowledgeGraph graph;

    // 점: 같은 id 는 앞의 것. 이름 열쇠 순(같으면 id 순)으로 놓아 점 번호 순이 곧 이름 열쇠 순이 되게 한다
    QSet<QString> setSeenIds;
    for (const KnowledgeConcept& concept : concepts)
    {
        if (concept.strId.isEmpty() || setSeenIds.contains(concept.strId))
            continue;
        setSeenIds.insert(concept.strId);
        KnowledgeGraphNode node;
        node.strId = concept.strId;
        node.strName = concept.strName;
        node.strNameKey = KnowledgeText::MakeNameKey(concept.strName);
        node.strKind = concept.strKind;
        node.strYear = concept.strYear;
        node.iYear = ParseYear(concept.strYear);
        node.strDefinition = concept.strSummary;
        graph.m_vecNodes.push_back(node);
    }
    std::sort(graph.m_vecNodes.begin(), graph.m_vecNodes.end(), [](const KnowledgeGraphNode& a, const KnowledgeGraphNode& b)
    {
        if (a.strNameKey != b.strNameKey)
            return a.strNameKey < b.strNameKey;
        return a.strId < b.strId;
    });
    for (int iNode = 0; iNode < graph.m_vecNodes.size(); ++iNode)
        graph.m_hashNodeIndex.insert(graph.m_vecNodes[iNode].strId, iNode);

    // 선: 두 끝이 모두 점인 것만. 받은 순서를 지킨다
    const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
    for (const KnowledgeEdge& edge : edges)
    {
        const int iFrom = graph.FindNode(edge.strFromId);
        const int iTo = graph.FindNode(edge.strToId);
        if (iFrom < 0 || iTo < 0)
            continue;
        KnowledgeGraphEdge graphEdge;
        graphEdge.iFrom = iFrom;
        graphEdge.iTo = iTo;
        graphEdge.bDirected = !vocabulary.IsUndirected(edge.strKind);
        graphEdge.edge = edge;
        graph.m_vecEdges.push_back(graphEdge);
        ++graph.m_vecNodes[iFrom].iEdgeCount;
        if (iTo != iFrom)
            ++graph.m_vecNodes[iTo].iEdgeCount;
    }

    // 문서와 문서 → 개념 연결. 가리킨 횟수: 카드 `개념:` 링크 · 관계 from · to · 표 concepts(색인과 같은 곳을 센다)
    for (int iDocument = 0; iDocument < documents.size(); ++iDocument)
    {
        const KnowledgeDocument& document = documents[iDocument];
        graph.m_vecDocuments.push_back({ document.strId, document.strTitle });
        QMap<int, int> mapWeightByNode;   // 점 번호 → 가리킨 횟수(점 번호 순으로 돈다)
        const auto funcCount = [&graph, &mapWeightByNode](const QString& conceptId)
        {
            const int iNode = graph.FindNode(conceptId);
            if (iNode >= 0)
                ++mapWeightByNode[iNode];
        };
        for (const KnowledgeCard& card : document.vecCards)
            for (const ConceptLink& link : card.vecConcepts)
                funcCount(link.strConceptId);
        for (const KnowledgeRelation& relation : document.vecRelations)
        {
            funcCount(relation.fromConcept.strConceptId);
            funcCount(relation.toConcept.strConceptId);
        }
        for (const KnowledgeTable& table : document.vecTables)
            for (const QString& strConceptId : table.vecConceptIds)
                funcCount(strConceptId);
        for (auto it = mapWeightByNode.cbegin(); it != mapWeightByNode.cend(); ++it)
        {
            graph.m_vecLinks.push_back({ iDocument, it.key(), it.value() });
            ++graph.m_vecNodes[it.key()].iDocumentCount;
        }
    }

    // 덩어리: 방향을 보지 않고 이어진 점들. 이웃은 점 번호 순으로 훑는다
    const int iNodeCount = graph.m_vecNodes.size();
    QVector<QVector<int>> vecNeighbors(iNodeCount);
    for (const KnowledgeGraphEdge& edge : graph.m_vecEdges)
    {
        vecNeighbors[edge.iFrom].push_back(edge.iTo);
        vecNeighbors[edge.iTo].push_back(edge.iFrom);
    }
    QVector<bool> vecSeen(iNodeCount, false);
    for (int iStart = 0; iStart < iNodeCount; ++iStart)
    {
        if (vecSeen[iStart])
            continue;
        vecSeen[iStart] = true;
        QVector<int> vecComponent { iStart };
        for (int iNext = 0; iNext < vecComponent.size(); ++iNext)
        {
            for (const int iNeighbor : vecNeighbors[vecComponent[iNext]])
            {
                if (vecSeen[iNeighbor])
                    continue;
                vecSeen[iNeighbor] = true;
                vecComponent.push_back(iNeighbor);
            }
        }
        std::sort(vecComponent.begin(), vecComponent.end());
        graph.m_vecComponents.push_back(vecComponent);
    }
    // 큰 것부터, 같으면 가장 앞선 이름 열쇠(첫 점 번호) 순
    std::sort(graph.m_vecComponents.begin(), graph.m_vecComponents.end(), [](const QVector<int>& a, const QVector<int>& b)
    {
        if (a.size() != b.size())
            return a.size() > b.size();
        return a.first() < b.first();
    });
    for (int iComponent = 0; iComponent < graph.m_vecComponents.size(); ++iComponent)
        for (const int iNode : graph.m_vecComponents[iComponent])
            graph.m_vecNodes[iNode].iComponent = iComponent;
    return graph;
}

int KnowledgeGraph::FindNode(const QString& conceptId) const
{
    return m_hashNodeIndex.value(conceptId, -1);
}

const QVector<KnowledgeGraphNode>& KnowledgeGraph::GetNodes() const
{
    return m_vecNodes;
}

const QVector<KnowledgeGraphEdge>& KnowledgeGraph::GetEdges() const
{
    return m_vecEdges;
}

const QVector<QVector<int>>& KnowledgeGraph::GetComponents() const
{
    return m_vecComponents;
}

const QVector<KnowledgeGraphDocument>& KnowledgeGraph::GetDocuments() const
{
    return m_vecDocuments;
}

const QVector<KnowledgeGraphLink>& KnowledgeGraph::GetLinks() const
{
    return m_vecLinks;
}
