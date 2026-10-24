#pragma once

#include "VisualMarkdown/KnowledgeIndex.h"
#include "VisualMarkdown/KnowledgeTypes.h"

#include <QHash>
#include <QString>
#include <QVector>

// 그래프의 점: 보이는 개념 하나
struct KnowledgeGraphNode
{
    QString strId;
    QString strName;
    QString strNameKey;          // 이름 열쇠(KnowledgeText::MakeNameKey). 점은 이 순서로 놓는다
    QString strKind;
    QString strYear;             // 개념 파일의 year 그대로
    int iYear = -1;              // year 가 네 자리 숫자면 그 해, 아니면 -1
    QString strDefinition;       // 개념의 정의(KnowledgeConcept::strSummary). 위젯의 마우스 올림 설명에 쓴다
    int iDocumentCount = 0;      // 이 개념을 가리키는 문서 수(한 문서는 한 번 센다)
    int iEdgeCount = 0;          // 이 점에 닿는 선 수
    int iComponent = -1;         // 덩어리 자리(KnowledgeGraph::GetComponents)
};

// 그래프의 선: 색인의 합친 선 하나와 두 끝의 점 번호
struct KnowledgeGraphEdge
{
    int iFrom = -1;
    int iTo = -1;
    bool bDirected = true;       // 방향 없는 종류(어휘의 undirected)면 false
    KnowledgeEdge edge;          // 색인의 선 그대로(종류 · 상태 · 출처 · 설명)
};

struct KnowledgeGraphDocument
{
    QString strId;
    QString strTitle;
};

// 문서 → 개념 연결. 무게는 그 문서가 그 개념을 가리킨 횟수다(카드 `개념:` 링크 · 관계 from · to · 표 concepts)
struct KnowledgeGraphLink
{
    int iDocument = -1;          // KnowledgeGraph::GetDocuments 자리
    int iNode = -1;              // KnowledgeGraph::GetNodes 자리
    int iWeight = 0;
};

// 색인에서 만든 지식 사이 그래프(design/Views.md §5): 점 · 선 · 덩어리 · 문서 ↔ 개념 연결.
// 점은 이름 열쇠 순(같으면 id 순)이라 점 번호 순이 곧 이름 열쇠 순이다. 배치는 이 순서로 동점을 가른다.
class KnowledgeGraph
{
public:
    // 보이는 개념(ListConcepts) · 선(ListEdges) · 문서(ListDocuments)로 만든다
    static KnowledgeGraph BuildFromIndex(const KnowledgeIndex& index);
    // 점이 될 개념 · 선 · 문서로 만든다. 같은 id 의 개념은 앞의 것을 쓴다.
    // 점이 아닌 개념을 가리키는 선과 문서 연결(개념 파일이 없는 것)은 뺀다. 문서는 받은 순서 그대로 둔다.
    static KnowledgeGraph BuildFromParts(const QVector<KnowledgeConcept>& concepts, const QVector<KnowledgeEdge>& edges,
                                         const QVector<KnowledgeDocument>& documents);

public:
    int FindNode(const QString& conceptId) const;   // 점 번호. 없으면 -1

public:
    const QVector<KnowledgeGraphNode>& GetNodes() const;
    const QVector<KnowledgeGraphEdge>& GetEdges() const;           // 받은 선 순서(색인이면 처음 나온 순서)
    const QVector<QVector<int>>& GetComponents() const;            // 방향을 보지 않고 이어진 점 번호들(점 번호 순). 큰 것부터, 같으면 이름 열쇠 순
    const QVector<KnowledgeGraphDocument>& GetDocuments() const;   // 받은 문서 순서(색인이면 learned 늦은 것부터)
    const QVector<KnowledgeGraphLink>& GetLinks() const;           // 문서 순, 한 문서 안에서는 점 번호 순

private:
    QVector<KnowledgeGraphNode> m_vecNodes;
    QVector<KnowledgeGraphEdge> m_vecEdges;
    QVector<QVector<int>> m_vecComponents;
    QVector<KnowledgeGraphDocument> m_vecDocuments;
    QVector<KnowledgeGraphLink> m_vecLinks;
    QHash<QString, int> m_hashNodeIndex;   // 개념 id → 점 번호(찾기에만 쓴다. 순서를 매기는 데 쓰지 않는다)
};
