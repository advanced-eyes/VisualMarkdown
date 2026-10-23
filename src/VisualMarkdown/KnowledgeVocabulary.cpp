#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace
{
QVector<VocabularyTerm> ReadTerms(const QJsonArray& array)
{
    QVector<VocabularyTerm> vecTerms;
    for (const QJsonValue& value : array)
    {
        const QJsonObject object = value.toObject();
        VocabularyTerm term;
        term.strCode = object.value("code").toString();
        term.strLabel = object.value("label").toString();
        term.strMeaning = object.value("meaning").toString();
        term.strExamples = object.value("examples").toString();
        term.strDirection = object.value("direction").toString();
        term.bUndirected = object.value("undirected").toBool();
        vecTerms.push_back(term);
    }
    return vecTerms;
}

QStringList ListCodes(const QVector<VocabularyTerm>& terms)
{
    QStringList vecCodes;
    for (const VocabularyTerm& term : terms)
        vecCodes << term.strCode;
    return vecCodes;
}

// 이름이나 코드를 코드로 바꾼다. 어휘에 없으면 받은 값을 그대로 돌려준다(모르는 코드는 그대로 보관한다).
QString ConvertToCode(const QVector<VocabularyTerm>& terms, const QString& labelOrCode)
{
    for (const VocabularyTerm& term : terms)
        if (term.strCode == labelOrCode || term.strLabel == labelOrCode)
            return term.strCode;
    return labelOrCode;
}

// 코드를 이름으로 바꾼다. 어휘에 없으면 코드를 그대로 돌려준다.
QString ConvertToLabel(const QVector<VocabularyTerm>& terms, const QString& code)
{
    for (const VocabularyTerm& term : terms)
        if (term.strCode == code)
            return term.strLabel;
    return code;
}
}

const KnowledgeVocabulary& KnowledgeVocabulary::Get()
{
    static const KnowledgeVocabulary vocabulary = []
    {
        // 정적 라이브러리의 qrc 는 저절로 붙지 않는다. 파일을 열기 전에 등록한다.
        Q_INIT_RESOURCE(VisualMarkdown);
        QFile file(QStringLiteral(":/visualmarkdown/vocabulary.json"));
        const QByteArray baJson = file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        return FromJson(baJson);
    }();
    return vocabulary;
}

KnowledgeVocabulary KnowledgeVocabulary::FromJson(const QByteArray& json)
{
    KnowledgeVocabulary vocabulary;
    const QJsonDocument jsonDocument = QJsonDocument::fromJson(json);
    if (!jsonDocument.isObject())
        qWarning() << "KnowledgeVocabulary: vocabulary JSON is missing or broken";
    const QJsonObject root = jsonDocument.object();
    vocabulary.m_iFormat = root.value("format").toInt();
    vocabulary.m_iRules = root.value("rules").toInt();
    vocabulary.m_vecConceptKinds = ReadTerms(root.value("conceptKinds").toArray());
    vocabulary.m_vecRelationKinds = ReadTerms(root.value("relationKinds").toArray());
    vocabulary.m_vecStatuses = ReadTerms(root.value("statuses").toArray());
    return vocabulary;
}

QString KnowledgeVocabulary::ToConceptCode(const QString& labelOrCode) const { return ConvertToCode(m_vecConceptKinds, labelOrCode); }
QString KnowledgeVocabulary::ToConceptLabel(const QString& code) const { return ConvertToLabel(m_vecConceptKinds, code); }
QString KnowledgeVocabulary::ToRelationCode(const QString& labelOrCode) const { return ConvertToCode(m_vecRelationKinds, labelOrCode); }
QString KnowledgeVocabulary::ToRelationLabel(const QString& code) const { return ConvertToLabel(m_vecRelationKinds, code); }

bool KnowledgeVocabulary::IsUndirected(const QString& relationCode) const
{
    for (const VocabularyTerm& term : m_vecRelationKinds)
        if (term.strCode == relationCode)
            return term.bUndirected;
    return false;
}

QString KnowledgeVocabulary::ToStatusCode(const QString& labelOrCode) const { return ConvertToCode(m_vecStatuses, labelOrCode); }
QString KnowledgeVocabulary::ToStatusLabel(const QString& code) const { return ConvertToLabel(m_vecStatuses, code); }

int KnowledgeVocabulary::GetFormat() const { return m_iFormat; }
int KnowledgeVocabulary::GetRules() const { return m_iRules; }
const QVector<VocabularyTerm>& KnowledgeVocabulary::GetConceptKinds() const { return m_vecConceptKinds; }
const QVector<VocabularyTerm>& KnowledgeVocabulary::GetRelationKinds() const { return m_vecRelationKinds; }
QStringList KnowledgeVocabulary::GetConceptCodes() const { return ListCodes(m_vecConceptKinds); }
QStringList KnowledgeVocabulary::GetRelationCodes() const { return ListCodes(m_vecRelationKinds); }
