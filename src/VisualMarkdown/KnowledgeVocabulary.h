#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

struct VocabularyTerm
{
    QString strCode;
    QString strLabel;
    QString strMeaning;
    QString strExamples;
    QString strDirection;
    bool bUndirected = false;
};

class KnowledgeVocabulary
{
public:
    static const KnowledgeVocabulary& Get();
    static KnowledgeVocabulary FromJson(const QByteArray& json);

public:
    QString ToConceptCode(const QString& labelOrCode) const;
    QString ToConceptLabel(const QString& code) const;
    QString ToRelationCode(const QString& labelOrCode) const;
    QString ToRelationLabel(const QString& code) const;
    bool IsUndirected(const QString& relationCode) const;
    QString ToStatusCode(const QString& labelOrCode) const;
    QString ToStatusLabel(const QString& code) const;

public:
    int GetFormat() const;
    int GetRules() const;
    const QVector<VocabularyTerm>& GetConceptKinds() const;
    const QVector<VocabularyTerm>& GetRelationKinds() const;
    QStringList GetConceptCodes() const;
    QStringList GetRelationCodes() const;

private:
    int m_iFormat = 0;
    int m_iRules = 0;
    QVector<VocabularyTerm> m_vecConceptKinds;
    QVector<VocabularyTerm> m_vecRelationKinds;
    QVector<VocabularyTerm> m_vecStatuses;
};
