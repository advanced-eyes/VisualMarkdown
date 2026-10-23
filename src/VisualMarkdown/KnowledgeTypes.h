#pragma once

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

enum class ERelationStatus { CONFIRMED, PROPOSED };
enum class EColumnKind { QUANTITY, ORDINAL, NOMINAL };

struct ConceptLink
{
    QString strName;
    QString strConceptId;
};

inline bool operator==(const ConceptLink& a, const ConceptLink& b)
{
    return a.strName == b.strName
        && a.strConceptId == b.strConceptId;
}

inline bool operator!=(const ConceptLink& a, const ConceptLink& b) { return !(a == b); }

struct KnowledgeOutlineEntry
{
    QString strNumber;
    QString strTitle;
    QVector<KnowledgeOutlineEntry> vecChildren;
};

inline bool operator==(const KnowledgeOutlineEntry& a, const KnowledgeOutlineEntry& b)
{
    return a.strNumber == b.strNumber
        && a.strTitle == b.strTitle
        && a.vecChildren == b.vecChildren;
}

inline bool operator!=(const KnowledgeOutlineEntry& a, const KnowledgeOutlineEntry& b) { return !(a == b); }

struct KnowledgeKeyNumber
{
    QString strValue;
    QString strUnit;
    QString strLabel;
};

inline bool operator==(const KnowledgeKeyNumber& a, const KnowledgeKeyNumber& b)
{
    return a.strValue == b.strValue
        && a.strUnit == b.strUnit
        && a.strLabel == b.strLabel;
}

inline bool operator!=(const KnowledgeKeyNumber& a, const KnowledgeKeyNumber& b) { return !(a == b); }

struct KnowledgeCard
{
    QString strId;
    QString strClaim;
    QString strEvidence;
    QString strSection;
    QVector<ConceptLink> vecConcepts;
    QStringList vecExtraLines;
};

inline bool operator==(const KnowledgeCard& a, const KnowledgeCard& b)
{
    return a.strId == b.strId
        && a.strClaim == b.strClaim
        && a.strEvidence == b.strEvidence
        && a.strSection == b.strSection
        && a.vecConcepts == b.vecConcepts
        && a.vecExtraLines == b.vecExtraLines;
}

inline bool operator!=(const KnowledgeCard& a, const KnowledgeCard& b) { return !(a == b); }

struct KnowledgeRelation
{
    ConceptLink fromConcept;
    QString strKind;
    ConceptLink toConcept;
    QString strNote;
    QString strWhere;
    QString strQuote;
    ERelationStatus eStatus = ERelationStatus::CONFIRMED;
    QStringList vecExtraLines;
};

inline bool operator==(const KnowledgeRelation& a, const KnowledgeRelation& b)
{
    return a.fromConcept == b.fromConcept
        && a.strKind == b.strKind
        && a.toConcept == b.toConcept
        && a.strNote == b.strNote
        && a.strWhere == b.strWhere
        && a.strQuote == b.strQuote
        && a.eStatus == b.eStatus
        && a.vecExtraLines == b.vecExtraLines;
}

inline bool operator!=(const KnowledgeRelation& a, const KnowledgeRelation& b) { return !(a == b); }

struct KnowledgeColumn
{
    QString strName;
    QString strUnit;
};

inline bool operator==(const KnowledgeColumn& a, const KnowledgeColumn& b)
{
    return a.strName == b.strName
        && a.strUnit == b.strUnit;
}

inline bool operator!=(const KnowledgeColumn& a, const KnowledgeColumn& b) { return !(a == b); }

struct KnowledgeTable
{
    QString strTitle;
    QString strX;
    QStringList vecY;
    QString strSeries;
    QStringList vecOrdered;
    bool bCumulative = false;
    QStringList vecConceptIds;
    QString strCondition;
    QString strWhere;
    QVector<KnowledgeColumn> vecColumns;
    QVector<QStringList> vecRows;
};

inline bool operator==(const KnowledgeTable& a, const KnowledgeTable& b)
{
    return a.strTitle == b.strTitle
        && a.strX == b.strX
        && a.vecY == b.vecY
        && a.strSeries == b.strSeries
        && a.vecOrdered == b.vecOrdered
        && a.bCumulative == b.bCumulative
        && a.vecConceptIds == b.vecConceptIds
        && a.strCondition == b.strCondition
        && a.strWhere == b.strWhere
        && a.vecColumns == b.vecColumns
        && a.vecRows == b.vecRows;
}

inline bool operator!=(const KnowledgeTable& a, const KnowledgeTable& b) { return !(a == b); }

struct KnowledgeDocument
{
    int iFormat = 1;
    QString strId, strSource, strFileInfo, strMeta, strLearned, strAnalysisId;
    int iRules = 0;
    bool bEdited = false;
    QString strDigest;
    QString strTitle, strSummary;
    QVector<KnowledgeOutlineEntry> vecOutline;
    QVector<KnowledgeKeyNumber> vecKeyNumbers;
    QVector<KnowledgeCard> vecCards;
    QVector<KnowledgeRelation> vecRelations;
    QVector<KnowledgeTable> vecTables;
};

inline bool operator==(const KnowledgeDocument& a, const KnowledgeDocument& b)
{
    return a.iFormat == b.iFormat
        && a.strId == b.strId
        && a.strSource == b.strSource
        && a.strFileInfo == b.strFileInfo
        && a.strMeta == b.strMeta
        && a.strLearned == b.strLearned
        && a.strAnalysisId == b.strAnalysisId
        && a.iRules == b.iRules
        && a.bEdited == b.bEdited
        && a.strDigest == b.strDigest
        && a.strTitle == b.strTitle
        && a.strSummary == b.strSummary
        && a.vecOutline == b.vecOutline
        && a.vecKeyNumbers == b.vecKeyNumbers
        && a.vecCards == b.vecCards
        && a.vecRelations == b.vecRelations
        && a.vecTables == b.vecTables;
}

inline bool operator!=(const KnowledgeDocument& a, const KnowledgeDocument& b) { return !(a == b); }

struct KnowledgeConcept
{
    int iFormat = 1;
    QString strId, strKind;
    QStringList vecAliases;
    QString strYear;
    QString strName, strSummary;
    QVector<KnowledgeRelation> vecRelations;
};

inline bool operator==(const KnowledgeConcept& a, const KnowledgeConcept& b)
{
    return a.iFormat == b.iFormat
        && a.strId == b.strId
        && a.strKind == b.strKind
        && a.vecAliases == b.vecAliases
        && a.strYear == b.strYear
        && a.strName == b.strName
        && a.strSummary == b.strSummary
        && a.vecRelations == b.vecRelations;
}

inline bool operator!=(const KnowledgeConcept& a, const KnowledgeConcept& b) { return !(a == b); }

struct DocumentReadResult
{
    bool bOk = false;
    QString strError;
    KnowledgeDocument document;
    QHash<QString, QStringList> hashUnreadLines;
};

struct ConceptReadResult
{
    bool bOk = false;
    QString strError;
    KnowledgeConcept concept;
    QHash<QString, QStringList> hashUnreadLines;
};
