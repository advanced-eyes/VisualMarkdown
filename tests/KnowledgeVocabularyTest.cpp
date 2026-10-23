#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <QRegularExpression>
#include <QTest>

#include <memory>

class KnowledgeVocabularyTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsSpecVocabulary()
    {
        const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
        QCOMPARE(vocabulary.GetFormat(), 1);
        QCOMPARE(vocabulary.GetRules(), 1);
        QCOMPARE(vocabulary.GetConceptCodes(), QStringList({ "method", "component", "task", "data", "metric", "system", "idea" }));
        QCOMPARE(vocabulary.GetRelationCodes(), QStringList({ "improves", "uses", "isA", "solves", "alternative" }));
        QCOMPARE(vocabulary.ToRelationCode("개선"), QString("improves"));
        QCOMPARE(vocabulary.ToRelationCode("uses"), QString("uses"));
        QCOMPARE(vocabulary.ToRelationCode("반박"), QString("반박"));       // 모르는 것은 그대로
        QCOMPARE(vocabulary.ToRelationLabel("isA"), QString("종류"));
        QCOMPARE(vocabulary.ToConceptLabel("idea"), QString("원리"));
        QCOMPARE(vocabulary.ToConceptCode("데이터"), QString("data"));
        QVERIFY(vocabulary.IsUndirected("alternative"));
        QVERIFY(!vocabulary.IsUndirected("improves"));
        QCOMPARE(vocabulary.ToStatusCode("AI 제안"), QString("proposed"));
        QCOMPARE(vocabulary.ToStatusLabel("confirmed"), QString("확인됨"));
    }

    void brokenVocabularyWarns()
    {
        QTest::ignoreMessage(QtWarningMsg, QRegularExpression("vocabulary"));
        const KnowledgeVocabulary vocabulary = KnowledgeVocabulary::FromJson("");
        QCOMPARE(vocabulary.GetFormat(), 0);
    }

    void fromJsonReadsTermFields()
    {
        const KnowledgeVocabulary vocabulary = KnowledgeVocabulary::FromJson(
            R"({"format":2,"rules":3,"conceptKinds":[{"code":"a","label":"에이","meaning":"m","examples":"e"}],)"
            R"("relationKinds":[{"code":"r","label":"알","direction":"d","undirected":true}]})");
        QCOMPARE(vocabulary.GetFormat(), 2);
        QCOMPARE(vocabulary.GetRules(), 3);
        QCOMPARE(vocabulary.GetConceptKinds().size(), 1);
        QCOMPARE(vocabulary.GetConceptKinds().first().strMeaning, QString("m"));
        QCOMPARE(vocabulary.GetRelationKinds().first().strDirection, QString("d"));
        QVERIFY(vocabulary.GetRelationKinds().first().bUndirected);
        QCOMPARE(vocabulary.ToConceptLabel("zzz"), QString("zzz"));
        QCOMPARE(vocabulary.ToStatusCode("x"), QString("x"));
    }
};

std::unique_ptr<QObject> CreateKnowledgeVocabularyTest() { return std::make_unique<KnowledgeVocabularyTest>(); }

#include "KnowledgeVocabularyTest.moc"
