#include "VisualMarkdown/KnowledgeMarkdownReader.h"
#include "VisualMarkdown/KnowledgeMarkdownWriter.h"
#include "VisualMarkdown/KnowledgeText.h"

#include <QTest>

#include <memory>

namespace
{
// Ocean 검사기 테스트의 Table() 이 만드는 표 기록과 같다(응답 JSON 을 기록으로 읽은 모양). 첫 열만 ordered 다.
KnowledgeTable MakeTable(const QString& title, const QStringList& concepts, const QStringList& colNames,
                         const QStringList& units, const QVector<QStringList>& rows, const QString& x,
                         const QStringList& y, const QString& series = QString())
{
    KnowledgeTable table;
    table.strTitle = title;
    table.vecConceptIds = concepts;
    table.strCondition = QStringLiteral("VOC");
    table.strWhere = QStringLiteral("표 2");
    for (int iColumn = 0; iColumn < colNames.size(); ++iColumn)
    {
        table.vecColumns.push_back({ colNames[iColumn], units.value(iColumn) });
        if (iColumn == 0)
            table.vecOrdered << colNames[iColumn];
    }
    table.vecRows = rows;
    table.strX = x;
    table.vecY = y;
    table.strSeries = series;
    table.bCumulative = true;
    return table;
}

// 표 하나만 있는 문서 기록. 다시 읽은 기록과 견주려고 digest 를 쓸 때와 같게 계산해 둔다.
KnowledgeDocument MakeDocument(const KnowledgeTable& table)
{
    KnowledgeDocument doc;
    doc.strId = QStringLiteral("x");
    doc.strTitle = QStringLiteral("t");
    doc.vecTables = { table };
    doc.strDigest = KnowledgeMarkdownWriter::ComputeDigest(doc);
    return doc;
}

// 표를 문서 파일로 쓰고 다시 읽으면 같은 기록인지 본다
bool SurvivesTheFile(const KnowledgeTable& table)
{
    const KnowledgeDocument doc = MakeDocument(table);
    const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(KnowledgeMarkdownWriter::WriteDocument(doc));
    return read.bOk && read.hashUnreadLines.isEmpty() && read.document == doc;
}
}

// 기록을 파일 문법에 맞추는 이름 정리(README §5.1 의 1+층)를 본다.
// Ocean KnowledgeExtractionValidatorTest 의 이름 정리 시험을 옮겨, Validate() 대신 이 함수들을 바로 부른다. 입력과 기대 값은 그대로다.
class KnowledgeTextCleanTest : public QObject
{
    Q_OBJECT

private slots:
    void namesWithCommasAreSanitised()
    {
        QCOMPARE(KnowledgeText::CleanName("AP, small"), QString::fromUtf8("AP · small"));
        QCOMPARE(KnowledgeText::CleanNames({ "a ,b", "YOLO9000" }), QStringList({ QString::fromUtf8("a · b"), "YOLO9000" }));
        QCOMPARE(KnowledgeText::CleanName("YOLO,v2"), QString::fromUtf8("YOLO · v2"));
        QCOMPARE(KnowledgeText::CleanName("B"), QString("B"));
        QCOMPARE(KnowledgeText::CleanName(" a , b "), QString::fromUtf8("a · b"));   // 쉼표 양쪽과 앞뒤의 공백

        KnowledgeTable table = MakeTable("표", {}, { "Size, px", "AP, small", "모델,  판" }, { "", "%", "" },
                                         { { "320", "63.4", "m" } }, "Size, px", { "AP, small" }, "모델,  판");
        KnowledgeText::CleanColumns(&table);
        QCOMPARE(table.vecColumns[0].strName, QString::fromUtf8("Size · px"));
        QCOMPARE(table.vecColumns[1].strName, QString::fromUtf8("AP · small"));
        QCOMPARE(table.vecColumns[2].strName, QString::fromUtf8("모델 · 판"));
        QCOMPARE(table.strX, QString::fromUtf8("Size · px"));
        QCOMPARE(table.vecY, QStringList({ QString::fromUtf8("AP · small") }));
        QCOMPARE(table.strSeries, QString::fromUtf8("모델 · 판"));
        QCOMPARE(table.vecOrdered, QStringList({ QString::fromUtf8("Size · px") }));
        QVERIFY(SurvivesTheFile(table));
    }

    // 비게 되는 이름은 뺀다(검사기가 별칭에 하는 것과 같다)
    void namesThatCleanToEmptyAreDropped()
    {
        QCOMPARE(KnowledgeText::CleanName(" \t "), QString());
        QCOMPARE(KnowledgeText::CleanNames({ "", "  ", "YOLO, v2", "\t" }), QStringList({ QString::fromUtf8("YOLO · v2") }));
        QCOMPARE(KnowledgeText::CleanNames({}), QStringList());
    }

    void columnUnitInNameMovesToUnit()
    {
        KnowledgeTable table = MakeTable("표", {}, { "Backbone (depth)", "mAP", "f(x)", "Size (px)", "Gap ()" },
                                         { "", "%", "", "pixels", "" },
                                         { { "19", "63.4", "1", "2", "3" } }, "Backbone (depth)", { "mAP", "Gap ()" },
                                         "f(x)");
        KnowledgeText::CleanColumns(&table);
        QCOMPARE(table.vecColumns[0], KnowledgeColumn({ "Backbone", "depth" }));
        QCOMPARE(table.vecColumns[1], KnowledgeColumn({ "mAP", "%" }));
        QCOMPARE(table.vecColumns[2], KnowledgeColumn({ "f(x)", "" }));          // 괄호 앞에 공백이 없으면 이름 그대로
        QCOMPARE(table.vecColumns[3], KnowledgeColumn({ "Size (px)", "pixels" })); // 단위가 있으면 이름 그대로
        QCOMPARE(table.vecColumns[4], KnowledgeColumn({ "Gap", "" }));
        QCOMPARE(table.strX, QString("Backbone"));
        QCOMPARE(table.vecOrdered, QStringList({ "Backbone" }));
        QCOMPARE(table.vecY, QStringList({ "mAP", "Gap" }));
        QCOMPARE(table.strSeries, QString("f(x)"));
        QVERIFY(SurvivesTheFile(table));
    }

    void unitParenthesesAreRemoved()
    {
        KnowledgeTable table = MakeTable("표", {}, { "단계", "mAP", "Latency (ms)", "FPS" }, { "", "(%)", "()", "((f/s))" },
                                         { { "a", "63.4", "5", "40" } }, "단계", { "mAP", "Latency (ms)", "FPS" });
        KnowledgeText::CleanColumns(&table);
        QCOMPARE(table.vecColumns[1], KnowledgeColumn({ "mAP", "%" }));
        QCOMPARE(table.vecColumns[2], KnowledgeColumn({ "Latency", "ms" }));
        QCOMPARE(table.vecColumns[3], KnowledgeColumn({ "FPS", "f/s" }));
        QCOMPARE(table.vecY, QStringList({ "mAP", "Latency", "FPS" }));
        QVERIFY(SurvivesTheFile(table));
    }

    void sanitisedNamesSurviveTheFile()
    {
        KnowledgeTable table = MakeTable("표", { "c1" }, { "Size, px (in)", "AP, small", "Backbone (depth)", "Latency" },
                                         { "", "(%)", "", "((ms))" },
                                         { { "320", "63.4", "19", "5" }, { "416", "65.8", "53", "7" } },
                                         "Size, px (in)", { "AP, small", "Latency" }, "Backbone (depth)");
        KnowledgeText::CleanColumns(&table);

        // 정리한 표를 파일로 쓰고 다시 읽으면 같은 기록이다
        const KnowledgeDocument doc = MakeDocument(table);
        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(KnowledgeMarkdownWriter::WriteDocument(doc));
        QVERIFY2(read.bOk, qPrintable(read.strError));
        QVERIFY(read.hashUnreadLines.isEmpty());
        QVERIFY(read.document == doc);

        // 개념 이름 · 별칭도
        KnowledgeConcept concept;
        concept.strId = QStringLiteral("c");
        concept.strName = KnowledgeText::CleanName("AP, small");
        concept.vecAliases = KnowledgeText::CleanNames({ "a, b", "*c" });
        QCOMPARE(concept.vecAliases.size(), 2);
        const ConceptReadResult conceptRead = KnowledgeMarkdownReader::ReadConcept(KnowledgeMarkdownWriter::WriteConcept(concept));
        QVERIFY(conceptRead.bOk);
        QVERIFY(conceptRead.concept == concept);
    }

    void cleanedColumnNamesStayUnique()
    {
        KnowledgeTable table = MakeTable("표", {}, { "Size", "AP, small", "AP · small" }, { "", "%", "%" },
                                         { { "320", "63.4", "50.1" } }, "Size", { "AP, small", "AP · small" }, "AP · small");
        table.vecOrdered << QString::fromUtf8("AP · small");   // 셋째 열도 ordered
        KnowledgeText::CleanColumns(&table);
        QCOMPARE(table.vecColumns[1].strName, QString::fromUtf8("AP · small"));
        QCOMPARE(table.vecColumns[2].strName, QString::fromUtf8("AP · small 2"));
        QCOMPARE(table.vecY, QStringList({ QString::fromUtf8("AP · small"), QString::fromUtf8("AP · small 2") }));
        QCOMPARE(table.strSeries, QString::fromUtf8("AP · small 2"));
        QCOMPARE(table.vecOrdered, QStringList({ "Size", QString::fromUtf8("AP · small 2") }));
        QVERIFY(SurvivesTheFile(table));
    }

    // 겹치는 이름이 여럿이면 ` 2` · ` 3` … 이 차례로 붙고, 이미 있는 이름은 건너뛴다. x · y 는 처음 그 이름이던 열을 따라간다.
    void duplicateSuffixesCountUp()
    {
        KnowledgeTable table = MakeTable("표", {}, { "A", "A 2", "A", "A" }, { "", "", "", "" },
                                         { { "1", "2", "3", "4" } }, "A", { "A 2" });
        KnowledgeText::CleanColumns(&table);
        QCOMPARE(table.vecColumns[0].strName, QString("A"));
        QCOMPARE(table.vecColumns[1].strName, QString("A 2"));
        QCOMPARE(table.vecColumns[2].strName, QString("A 3"));
        QCOMPARE(table.vecColumns[3].strName, QString("A 4"));
        QCOMPARE(table.strX, QString("A"));
        QCOMPARE(table.vecY, QStringList({ "A 2" }));
        QCOMPARE(table.vecOrdered, QStringList({ "A" }));
        QVERIFY(SurvivesTheFile(table));
    }
};

std::unique_ptr<QObject> CreateKnowledgeTextCleanTest() { return std::make_unique<KnowledgeTextCleanTest>(); }

#include "KnowledgeTextCleanTest.moc"
