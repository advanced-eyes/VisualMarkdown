#pragma once

#include "VisualMarkdown/KnowledgeTypes.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

// 합친 선 하나. (from · 종류 · to) 가 같은 관계 줄을 모은 것이다(format/Visualization.md §2).
struct KnowledgeEdge
{
    QString strFromId;
    QString strKind;
    QString strToId;                                        // 방향 없는 종류는 두 id 를 글자 순으로 둔다
    ERelationStatus eStatus = ERelationStatus::PROPOSED;   // 하나라도 확인됨이면 확인됨
    QStringList vecDocumentIds;                             // 그 선을 말한 문서(id 순)
    bool bFromConceptFile = false;
    QStringList vecNotes;                                   // 출처마다의 설명(빈 것 · 겹친 것은 뺀다)
};

// 개념 파일이 없어 빠진 관계. from · to 는 적힌 그대로다.
struct KnowledgeDroppedRelation
{
    QString strFromId;
    QString strKind;
    QString strToId;
    QString strSource;   // 문서 id. 개념 파일이면 그 개념 id
};

// wiki 폴더를 읽어 만든 읽기 전용 색인(design/Views.md §4). 읽기 규칙은 Ocean KnowledgeBase::Reload · RebuildIndex 와 같다.
class KnowledgeIndex
{
public:
    // documents/ · concepts/ 바로 아래의 *.md 를 읽는다. 읽지 못한 파일은 건너뛰고 경고로 남긴다.
    // 앞서 읽은 것은 지운다. wiki 폴더가 없거나 두 폴더가 다 없으면 false 다.
    bool Load(const QString& wikiRoot);

    QVector<KnowledgeConcept> ListConcepts(bool includeHidden = false) const;       // 이름 열쇠 순(같으면 id 순)
    std::optional<KnowledgeConcept> FindConcept(const QString& conceptId) const;     // 숨은 개념도 찾는다
    QString FindConceptIdByName(const QString& nameOrAlias) const;                   // 이름 열쇠로 찾는다. 없으면 빈 글
    bool IsHidden(const QString& conceptId) const;                                   // 개념 파일이 있고 아무도 가리키지 않는다
    QVector<KnowledgeEdge> ListEdges() const;                                        // 처음 나온 순서(문서 id 순, 그다음 개념 파일 id 순)
    QVector<KnowledgeDroppedRelation> ListDroppedRelations() const;                  // 빠진 관계 줄마다 하나, ListEdges 와 같은 순서
    QVector<KnowledgeDocument> ListDocuments() const;                                // learned 늦은 것부터(같으면 id 순)
    std::optional<KnowledgeDocument> FindDocument(const QString& documentId) const;  // 머리글 id 로 찾는다
    QStringList ListDocumentIdsPointingTo(const QString& conceptId) const;          // 그 개념을 가리키는 문서(ListDocuments 순)

public:
    QStringList GetWarnings() const;

private:
    void Clear();
    void ReadDocuments(const QString& folder);
    void ReadConcepts(const QString& folder);
    void RebuildIndex();

private:
    QHash<QString, KnowledgeDocument> m_hashDocuments;           // 머리글 id → 문서
    QHash<QString, KnowledgeConcept> m_hashConcepts;             // 파일 이름 → 개념
    QHash<QString, QString> m_hashNameKeyToConceptId;
    QSet<QString> m_setReferencedConceptIds;                     // 문서가 가리키거나 개념 파일 관계에 나오는 id
    QHash<QString, QStringList> m_hashDocumentIdsByConceptId;    // 개념 id → 가리키는 문서(ListDocuments 순)
    QStringList m_vecConceptOrder;                               // 모든 개념 id, 이름 열쇠 순
    QStringList m_vecDocumentOrder;                              // 모든 문서 id, learned 늦은 것부터
    QVector<KnowledgeEdge> m_vecEdges;
    QVector<KnowledgeDroppedRelation> m_vecDroppedRelations;
    QStringList m_vecWarnings;
};
