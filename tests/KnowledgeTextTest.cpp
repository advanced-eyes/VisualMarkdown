#include "VisualMarkdown/KnowledgeText.h"

#include <QRegularExpression>
#include <QSet>
#include <QTest>

#include <memory>

class KnowledgeTextTest : public QObject
{
    Q_OBJECT

private slots:
    void slugDropsPercent()
    {
        QCOMPARE(KnowledgeText::MakeSlug("50% rule", "concept"), QString("50-rule"));
        QCOMPARE(KnowledgeText::MakeSlug("a%41b", "concept"), QString("a41b"));
    }

    void slugFollowsSpecRules()
    {
        QCOMPARE(KnowledgeText::MakeSlug("Faster R-CNN", "concept"), QString("faster-r-cnn"));
        QCOMPARE(KnowledgeText::MakeSlug("1단계 검출기", "concept"), QString("1단계-검출기"));
        QCOMPARE(KnowledgeText::MakeSlug("A/B: test? [x] (y) #z", "concept"), QString("ab-test-x-y-z"));
        QCOMPARE(KnowledgeText::MakeSlug("  --Region  Proposal -- Network--  ", "concept"), QString("region-proposal-network"));
        QCOMPARE(KnowledgeText::MakeSlug("///", "concept"), QString("concept"));
        QCOMPARE(KnowledgeText::MakeSlug("CON", "concept"), QString("con-1"));
        QCOMPARE(KnowledgeText::MakeSlug("Com7", "document"), QString("com7-1"));
        QCOMPARE(KnowledgeText::MakeSlug(QString(70, 'a') + " b", "concept"), QString(60, 'a'));   // 60자로 자르고 끝 '-' 를 뺀다
        QCOMPARE(KnowledgeText::MakeSlug(QString("검출기").normalized(QString::NormalizationForm_D), "concept"), QString("검출기"));
        QCOMPARE(KnowledgeText::MakeSlug("YOLO9000: Better, Faster, Stronger", "document"), QString("yolo9000-better-faster-stronger"));
        QCOMPARE(KnowledgeText::MakeUniqueId("yolo", { "yolo", "yolo-2" }), QString("yolo-3"));
        QCOMPARE(KnowledgeText::MakeUniqueId("yolo", {}), QString("yolo"));
    }

    void nameKeyIgnoresCaseSpacesAndHyphens()
    {
        QCOMPARE(KnowledgeText::MakeNameKey("Faster R-CNN"), QString("fasterrcnn"));
        QCOMPARE(KnowledgeText::MakeNameKey("faster rcnn"), KnowledgeText::MakeNameKey("Faster R-CNN"));
        QVERIFY(KnowledgeText::MakeNameKey("RPN") != KnowledgeText::MakeNameKey("Region Proposal Network"));
    }

    void quantityTextFollowsSpec()
    {
        for (const QString& strCell : { "63.4", " 1,234 ", "-0.3", "+2", "1e-3", "0.02" })
            QVERIFY2(KnowledgeText::IsQuantityText(strCell), qPrintable(strCell));
        for (const QString& strCell : { "63.4*", "63.4±0.2", "n/a", "", "1,23,4", "12%" })
            QVERIFY2(!KnowledgeText::IsQuantityText(strCell), qPrintable(strCell));
        QCOMPARE(KnowledgeText::ExtractNumbers("mAP 63.4 → 76.8, 1,234개, 2017,"), QStringList({ "63.4", "76.8", "1234", "2017" }));
    }

    void cardIdIsEightLowerHexAndUnique()
    {
        const QString strId = KnowledgeText::MakeCardId({});
        QVERIFY(QRegularExpression("^[0-9a-f]{8}$").match(strId).hasMatch());
        QSet<QString> setTaken;
        for (int iCard = 0; iCard < 200; ++iCard)
            setTaken.insert(KnowledgeText::MakeCardId(setTaken));
        QCOMPARE(setTaken.size(), 200);
    }
};

std::unique_ptr<QObject> CreateKnowledgeTextTest() { return std::make_unique<KnowledgeTextTest>(); }

#include "KnowledgeTextTest.moc"
