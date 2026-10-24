#include "VisualMarkdown/KnowledgeIndex.h"

#include "VisualMarkdown/KnowledgeMarkdownReader.h"
#include "VisualMarkdown/KnowledgeText.h"
#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace
{
const QString kDocumentsFolder = QStringLiteral("documents");
const QString kConceptsFolder = QStringLiteral("concepts");
constexpr QChar kEdgeKeySeparator = QLatin1Char('\n');   // id · 종류 코드는 한 줄이다(링크 · 파일 이름에서 온다)

// 폴더 바로 아래의 *.md 파일 이름(이름 순). 하위 폴더는 보지 않는다.
QStringList ListMarkdownFiles(const QString& folder)
{
    return QDir(folder).entryList({ QStringLiteral("*.md") }, QDir::Files, QDir::Name);
}

bool ReadTextFile(const QString& path, QString* text, QString* error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        *error = file.errorString();
        return false;
    }
    *text = QString::fromUtf8(file.readAll());
    return true;
}

// 경고 글은 Ocean KnowledgeBase 의 로그와 같은 꼴이다(앞의 "KnowledgeBase: " 만 없다)
QString MakeSkipWarning(const QString& path, const QString& reason)
{
    return QStringLiteral("skipped ") + path + QStringLiteral(" - ") + reason;
}

QString MakeEdgeKey(const QString& fromId, const QString& kind, const QString& toId)
{
    return fromId + kEdgeKeySeparator + kind + kEdgeKeySeparator + toId;
}

// 문서가 가리키는 개념 id: 카드 `개념:` 의 링크, 관계의 from · to, 표의 concepts. 개념 파일이 없는 id 도 넣는다.
QSet<QString> CollectPointedConceptIds(const KnowledgeDocument& document)
{
    QSet<QString> setIds;
    for (const KnowledgeCard& card : document.vecCards)
        for (const ConceptLink& link : card.vecConcepts)
            setIds.insert(link.strConceptId);
    for (const KnowledgeRelation& relation : document.vecRelations)
    {
        setIds.insert(relation.fromConcept.strConceptId);
        setIds.insert(relation.toConcept.strConceptId);
    }
    for (const KnowledgeTable& table : document.vecTables)
        for (const QString& strId : table.vecConceptIds)
            setIds.insert(strId);
    return setIds;
}
}

bool KnowledgeIndex::Load(const QString& wikiRoot)
{
    Clear();
    // 빈 경로는 QDir 에서 지금 폴더가 되므로 따로 막는다
    const QDir root(wikiRoot);
    if (wikiRoot.isEmpty() || !root.exists())
    {
        m_vecWarnings << QStringLiteral("wiki folder does not exist: ") + wikiRoot;
        return false;
    }
    const QString strDocuments = root.filePath(kDocumentsFolder);
    const QString strConcepts = root.filePath(kConceptsFolder);
    if (!QDir(strDocuments).exists() && !QDir(strConcepts).exists())
    {
        m_vecWarnings << QStringLiteral("wiki folder has no documents or concepts folder: ") + wikiRoot;
        return false;
    }
    ReadDocuments(strDocuments);
    ReadConcepts(strConcepts);
    RebuildIndex();
    return true;
}

QVector<KnowledgeConcept> KnowledgeIndex::ListConcepts(bool includeHidden) const
{
    QVector<KnowledgeConcept> vecConcepts;
    for (const QString& strId : m_vecConceptOrder)
        if (includeHidden || m_setReferencedConceptIds.contains(strId))
            vecConcepts.push_back(m_hashConcepts.value(strId));
    return vecConcepts;
}

std::optional<KnowledgeConcept> KnowledgeIndex::FindConcept(const QString& conceptId) const
{
    const auto it = m_hashConcepts.constFind(conceptId);
    if (it == m_hashConcepts.constEnd())
        return std::nullopt;
    return it.value();
}

QString KnowledgeIndex::FindConceptIdByName(const QString& nameOrAlias) const
{
    return m_hashNameKeyToConceptId.value(KnowledgeText::MakeNameKey(nameOrAlias));
}

bool KnowledgeIndex::IsHidden(const QString& conceptId) const
{
    return m_hashConcepts.contains(conceptId) && !m_setReferencedConceptIds.contains(conceptId);
}

QVector<KnowledgeEdge> KnowledgeIndex::ListEdges() const
{
    return m_vecEdges;
}

QVector<KnowledgeDroppedRelation> KnowledgeIndex::ListDroppedRelations() const
{
    return m_vecDroppedRelations;
}

QVector<KnowledgeDocument> KnowledgeIndex::ListDocuments() const
{
    QVector<KnowledgeDocument> vecDocuments;
    for (const QString& strId : m_vecDocumentOrder)
        vecDocuments.push_back(m_hashDocuments.value(strId));
    return vecDocuments;
}

std::optional<KnowledgeDocument> KnowledgeIndex::FindDocument(const QString& documentId) const
{
    const auto it = m_hashDocuments.constFind(documentId);
    if (it == m_hashDocuments.constEnd())
        return std::nullopt;
    return it.value();
}

QStringList KnowledgeIndex::ListDocumentIdsPointingTo(const QString& conceptId) const
{
    return m_hashDocumentIdsByConceptId.value(conceptId);
}

QStringList KnowledgeIndex::GetWarnings() const
{
    return m_vecWarnings;
}

void KnowledgeIndex::Clear()
{
    m_hashDocuments.clear();
    m_hashConcepts.clear();
    m_hashNameKeyToConceptId.clear();
    m_setReferencedConceptIds.clear();
    m_hashDocumentIdsByConceptId.clear();
    m_vecConceptOrder.clear();
    m_vecDocumentOrder.clear();
    m_vecEdges.clear();
    m_vecDroppedRelations.clear();
    m_vecWarnings.clear();
}

// 문서는 머리글 id 로 모은다. id 가 겹치면 파일 이름 순으로 앞선 것을 쓴다(Ocean KnowledgeBase::Reload 와 같다).
void KnowledgeIndex::ReadDocuments(const QString& folder)
{
    const QDir dir(folder);
    for (const QString& strFileName : ListMarkdownFiles(folder))
    {
        const QString strPath = dir.filePath(strFileName);
        QString strText, strError;
        if (!ReadTextFile(strPath, &strText, &strError))
        {
            m_vecWarnings << MakeSkipWarning(strPath, strError);
            continue;
        }
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        if (!result.bOk || result.document.strId.isEmpty())
        {
            m_vecWarnings << MakeSkipWarning(strPath, result.bOk ? QStringLiteral("missing id") : result.strError);
            continue;
        }
        if (m_hashDocuments.contains(result.document.strId))
        {
            m_vecWarnings << MakeSkipWarning(strPath, QStringLiteral("duplicate id ") + result.document.strId);
            continue;
        }
        m_hashDocuments.insert(result.document.strId, result.document);
    }
}

// 개념 id 는 파일 이름이다. 링크가 가리키는 것이 파일 이름이라, 머리글 id 가 다르면 경고를 남기고 파일 이름을 쓴다.
void KnowledgeIndex::ReadConcepts(const QString& folder)
{
    const QDir dir(folder);
    for (const QString& strFileName : ListMarkdownFiles(folder))
    {
        const QString strPath = dir.filePath(strFileName);
        QString strText, strError;
        if (!ReadTextFile(strPath, &strText, &strError))
        {
            m_vecWarnings << MakeSkipWarning(strPath, strError);
            continue;
        }
        const ConceptReadResult result = KnowledgeMarkdownReader::ReadConcept(strText);
        if (!result.bOk || result.concept.strId.isEmpty())
        {
            m_vecWarnings << MakeSkipWarning(strPath, result.bOk ? QStringLiteral("missing id") : result.strError);
            continue;
        }
        KnowledgeConcept concept = result.concept;
        const QString strBaseName = QFileInfo(strFileName).completeBaseName();
        if (concept.strId != strBaseName)
        {
            m_vecWarnings << QStringLiteral("concept id differs from file name %1 - header id %2 - file name %3 (using file name)")
                                 .arg(strPath, concept.strId, strBaseName);
            concept.strId = strBaseName;
        }
        // 대소문자만 다른 파일 이름(x.md · x.MD)은 같은 id 가 된다
        if (m_hashConcepts.contains(concept.strId))
        {
            m_vecWarnings << MakeSkipWarning(strPath, QStringLiteral("duplicate id ") + concept.strId);
            continue;
        }
        m_hashConcepts.insert(concept.strId, concept);
    }
}

// format/Visualization.md §2 의 색인. 순서는 Ocean KnowledgeBase::RebuildIndex 와 같다:
// 이름 열쇠는 개념 id 순, 선은 문서를 id 순으로 훑은 뒤 개념 파일을 id 순으로 훑어 처음 나온 순서로 둔다.
void KnowledgeIndex::RebuildIndex()
{
    const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();

    // 이름 열쇠: 이름과 별칭. 열쇠가 겹치면 id 순으로 앞선 개념이 갖는다(ConceptFile.md §4).
    QStringList vecConceptIds = m_hashConcepts.keys();
    std::sort(vecConceptIds.begin(), vecConceptIds.end());
    QHash<QString, QString> hashNameKeyById;   // 개념 id → 이름의 열쇠(ListConcepts 정렬용)
    for (const QString& strId : vecConceptIds)
    {
        const KnowledgeConcept& concept = m_hashConcepts.constFind(strId).value();
        hashNameKeyById.insert(strId, KnowledgeText::MakeNameKey(concept.strName));
        QStringList vecNames = concept.vecAliases;
        vecNames.prepend(concept.strName);
        for (const QString& strName : vecNames)
        {
            const QString strKey = KnowledgeText::MakeNameKey(strName);
            if (!strKey.isEmpty() && !m_hashNameKeyToConceptId.contains(strKey))
                m_hashNameKeyToConceptId.insert(strKey, strId);
        }
    }
    m_vecConceptOrder = vecConceptIds;
    std::sort(m_vecConceptOrder.begin(), m_vecConceptOrder.end(), [&hashNameKeyById](const QString& a, const QString& b)
    {
        const QString strKeyA = hashNameKeyById.value(a);
        const QString strKeyB = hashNameKeyById.value(b);
        if (strKeyA != strKeyB)
            return strKeyA < strKeyB;
        return a < b;
    });

    // 관계 줄 하나를 선에 더한다. source 는 문서 id, 개념 파일이면 그 개념 id 다.
    // from 이나 to 의 개념 파일이 없으면 선에 넣지 않고 빠진 관계로 남긴다. 방향 없는 종류는 작은 id 를 from 에 둔다.
    QHash<QString, int> hashEdgeIndex;   // MakeEdgeKey → m_vecEdges 자리
    const auto funcAddRelation = [&](const KnowledgeRelation& relation, const QString& source, bool fromConceptFile)
    {
        QString strFrom = relation.fromConcept.strConceptId;
        QString strTo = relation.toConcept.strConceptId;
        if (!m_hashConcepts.contains(strFrom) || !m_hashConcepts.contains(strTo))
        {
            m_vecDroppedRelations.push_back({ strFrom, relation.strKind, strTo, source });
            return;
        }
        if (vocabulary.IsUndirected(relation.strKind) && strFrom > strTo)
            std::swap(strFrom, strTo);
        const QString strKey = MakeEdgeKey(strFrom, relation.strKind, strTo);
        auto it = hashEdgeIndex.find(strKey);
        if (it == hashEdgeIndex.end())
        {
            KnowledgeEdge newEdge;
            newEdge.strFromId = strFrom;
            newEdge.strKind = relation.strKind;
            newEdge.strToId = strTo;
            newEdge.eStatus = relation.eStatus;
            it = hashEdgeIndex.insert(strKey, m_vecEdges.size());
            m_vecEdges.push_back(newEdge);
        }
        KnowledgeEdge& edge = m_vecEdges[it.value()];
        if (relation.eStatus == ERelationStatus::CONFIRMED)
            edge.eStatus = ERelationStatus::CONFIRMED;
        if (fromConceptFile)
            edge.bFromConceptFile = true;
        else if (!edge.vecDocumentIds.contains(source))
            edge.vecDocumentIds.push_back(source);
        if (!relation.strNote.isEmpty() && !edge.vecNotes.contains(relation.strNote))
            edge.vecNotes.push_back(relation.strNote);
    };

    // 문서가 가리키는 개념은 보인다. 한 문서는 한 개념을 한 번 센다.
    QStringList vecDocumentIds = m_hashDocuments.keys();
    std::sort(vecDocumentIds.begin(), vecDocumentIds.end());
    QHash<QString, QSet<QString>> hashPointedByDocumentId;
    for (const QString& strDocumentId : vecDocumentIds)
    {
        const KnowledgeDocument& document = m_hashDocuments.constFind(strDocumentId).value();
        for (const KnowledgeRelation& relation : document.vecRelations)
            funcAddRelation(relation, strDocumentId, false);
        const QSet<QString> setPointed = CollectPointedConceptIds(document);
        m_setReferencedConceptIds += setPointed;
        hashPointedByDocumentId.insert(strDocumentId, setPointed);
    }
    // 개념 파일 관계의 양 끝도 보인다(관계가 빠져도 그렇다. Ocean 과 같다).
    for (const QString& strConceptId : vecConceptIds)
    {
        for (const KnowledgeRelation& relation : m_hashConcepts.constFind(strConceptId)->vecRelations)
        {
            m_setReferencedConceptIds.insert(relation.fromConcept.strConceptId);
            m_setReferencedConceptIds.insert(relation.toConcept.strConceptId);
            funcAddRelation(relation, strConceptId, true);
        }
    }

    // 문서 순서: learned 가 늦은 것부터, 같으면 id 순. learned 는 글자로 견준다(Ocean KnowledgeBase::ListDocuments 와 같다).
    m_vecDocumentOrder = vecDocumentIds;
    std::sort(m_vecDocumentOrder.begin(), m_vecDocumentOrder.end(), [this](const QString& a, const QString& b)
    {
        const QString& strLearnedA = m_hashDocuments.constFind(a)->strLearned;
        const QString& strLearnedB = m_hashDocuments.constFind(b)->strLearned;
        if (strLearnedA != strLearnedB)
            return strLearnedA > strLearnedB;
        return a < b;
    });
    for (const QString& strDocumentId : m_vecDocumentOrder)
        for (const QString& strConceptId : hashPointedByDocumentId.value(strDocumentId))
            m_hashDocumentIdsByConceptId[strConceptId].push_back(strDocumentId);
}
