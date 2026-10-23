#include "VisualMarkdown/KnowledgeMarkdownLayout.h"
#include "VisualMarkdown/KnowledgeMarkdownReader.h"
#include "VisualMarkdown/KnowledgeMarkdownWriter.h"
#include "VisualMarkdown/KnowledgeText.h"
#include "KnowledgeTestData.h"

#include <QRegularExpression>
#include <QTest>

#include <memory>

namespace
{
QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

QString MakeConceptWithRelations(const QString& lines)
{
    return KnowledgeTestData::CanonicalConcept() + QStringLiteral("\n## 관계\n\n") + lines;
}
}

class KnowledgeMarkdownReaderTest : public QObject
{
    Q_OBJECT

private slots:
    void canonicalTextIsPureLf()
    {
        const QString strText = KnowledgeTestData::CanonicalDocument();
        QVERIFY(!strText.contains(QLatin1Char('\r')));
        QVERIFY(strText.endsWith(QLatin1Char('\n')));
        QVERIFY(!strText.endsWith(QStringLiteral("\n\n")));
    }

    void readsCanonicalDocument()
    {
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(KnowledgeTestData::CanonicalDocument());
        QVERIFY2(result.bOk, qPrintable(result.strError));
        const KnowledgeDocument& doc = result.document;
        QCOMPARE(doc.iFormat, 1);
        QCOMPARE(doc.strId, QString("3f1c2a9e-5b7d-4c8e-9a21-6d0e4b8f7c31"));
        QCOMPARE(doc.strSource, QString("yolo9000.pdf"));
        QCOMPARE(doc.strFileInfo, DecodeUtf8("PDF 9쪽"));
        QCOMPARE(doc.strMeta, DecodeUtf8("Redmon · Farhadi · CVPR 2017"));
        QCOMPARE(doc.strLearned, QString("2026-10-22T10:00:00+09:00"));
        QCOMPARE(doc.strAnalysisId, QString("8b2e6c1d-0a4f-4e7b-9c3a-1f5d2e8b7a60"));
        QCOMPARE(doc.iRules, 1);
        QCOMPARE(doc.bEdited, false);
        QCOMPARE(doc.strDigest, KnowledgeTestData::kDigestSlot);
        QCOMPARE(doc.strTitle, QString("YOLO9000: Better, Faster, Stronger"));
        QCOMPARE(doc.strSummary, DecodeUtf8("YOLO 에 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올리고, 9000개 넘는 클래스를 검출하게 했다."));

        QCOMPARE(doc.vecOutline.size(), 3);
        QCOMPARE(doc.vecOutline[0].strNumber, QString("1"));
        QCOMPARE(doc.vecOutline[0].strTitle, DecodeUtf8("소개"));
        QCOMPARE(doc.vecOutline[1].strTitle, DecodeUtf8("더 좋게"));
        QCOMPARE(doc.vecOutline[1].vecChildren.size(), 1);
        QCOMPARE(doc.vecOutline[1].vecChildren[0].strNumber, QString("2.1"));
        QCOMPARE(doc.vecOutline[1].vecChildren[0].strTitle, DecodeUtf8("배치 정규화"));
        QCOMPARE(doc.vecOutline[2].strNumber, QString("3"));

        QCOMPARE(doc.vecKeyNumbers.size(), 1);
        QCOMPARE(doc.vecKeyNumbers[0].strValue, QString("76.8"));
        QCOMPARE(doc.vecKeyNumbers[0].strUnit, QString("mAP"));
        QCOMPARE(doc.vecKeyNumbers[0].strLabel, QString("VOC 2007, 67 fps"));

        QCOMPARE(doc.vecCards.size(), 1);
        const KnowledgeCard& card = doc.vecCards[0];
        QCOMPARE(card.strId, QString("7f3a2c1e"));
        QCOMPARE(card.strClaim, DecodeUtf8("작은 개선을 쌓아 속도를 잃지 않고 정확도를 올렸다"));
        QCOMPARE(card.strEvidence, DecodeUtf8("VOC 2007 mAP 63.4% → 76.8%, 67 fps"));
        QCOMPARE(card.strSection, QString("2"));
        QCOMPARE(card.vecConcepts.size(), 1);
        QCOMPARE(card.vecConcepts[0].strName, QString("YOLOv2"));
        QCOMPARE(card.vecConcepts[0].strConceptId, QString("yolov2"));
        QVERIFY(card.vecExtraLines.isEmpty());

        QCOMPARE(doc.vecRelations.size(), 1);
        const KnowledgeRelation& relation = doc.vecRelations[0];
        QCOMPARE(relation.fromConcept.strName, QString("YOLOv2"));
        QCOMPARE(relation.fromConcept.strConceptId, QString("yolov2"));
        QCOMPARE(relation.strKind, QString("improves"));
        QCOMPARE(relation.toConcept.strName, QString("YOLO"));
        QCOMPARE(relation.toConcept.strConceptId, QString("yolo"));
        QCOMPARE(relation.strNote, DecodeUtf8("배치 정규화 · anchor · 고해상도 학습으로 mAP 63.4 → 76.8, 속도 유지"));
        QCOMPARE(relation.strWhere, DecodeUtf8("2절"));
        QCOMPARE(relation.strQuote, QString("At 67 FPS, YOLOv2 gets 76.8 mAP on VOC 2007."));
        QCOMPARE(relation.eStatus, ERelationStatus::PROPOSED);

        QCOMPARE(doc.vecTables.size(), 1);
        const KnowledgeTable& table = doc.vecTables[0];
        QCOMPARE(table.strTitle, DecodeUtf8("YOLO 에서 YOLOv2 까지 — 하나씩 더한 개선"));
        QCOMPARE(table.strX, DecodeUtf8("단계"));
        QCOMPARE(table.vecY, QStringList({ "mAP" }));
        QCOMPARE(table.vecOrdered, QStringList({ DecodeUtf8("단계") }));
        QCOMPARE(table.bCumulative, true);
        QCOMPARE(table.vecConceptIds, QStringList({ "yolov2", "yolo" }));
        QCOMPARE(table.strCondition, QString("VOC 2007 test"));
        QCOMPARE(table.strWhere, DecodeUtf8("표 2"));
        QCOMPARE(table.vecColumns.size(), 2);
        QCOMPARE(table.vecColumns[0].strName, DecodeUtf8("단계"));
        QCOMPARE(table.vecColumns[0].strUnit, QString());
        QCOMPARE(table.vecColumns[1].strName, QString("mAP"));
        QCOMPARE(table.vecColumns[1].strUnit, QString("%"));
        QCOMPARE(table.vecRows.size(), 2);
        QCOMPARE(table.vecRows[0], QStringList({ DecodeUtf8("YOLO (기준)"), "63.4" }));
        QCOMPARE(table.vecRows[1], QStringList({ DecodeUtf8("+ 배치 정규화"), "65.8" }));

        QVERIFY(result.hashUnreadLines.isEmpty());
    }

    void relationKindsByLabelOrCode()
    {
        const QString strLines = DecodeUtf8(
            "- [A](a.md) **uses** → [B](b.md)\n"
            "- [A](a.md) **반박** → [B](b.md)\n"
            "- [A](a.md) **대안** ↔ [B](b.md)\n"
            "- [A](a.md) **사용** → [C](c.md)\n"
            "  - 상태: 보류\n"
            "- [A](a.md) **사용** → [D](d.md)\n"
            "  - 상태: 확인됨\n"
            "- [A](a.md) **사용** → [E](e.md)\n"
            "  - 상태: confirmed\n");
        const ConceptReadResult result = KnowledgeMarkdownReader::ReadConcept(MakeConceptWithRelations(strLines));
        QVERIFY2(result.bOk, qPrintable(result.strError));
        const QVector<KnowledgeRelation>& vecRelations = result.concept.vecRelations;
        QCOMPARE(vecRelations.size(), 6);
        QCOMPARE(vecRelations[0].strKind, QString("uses"));
        QCOMPARE(vecRelations[0].eStatus, ERelationStatus::CONFIRMED);
        QCOMPARE(vecRelations[1].strKind, DecodeUtf8("반박"));
        QCOMPARE(vecRelations[2].strKind, QString("alternative"));
        QCOMPARE(vecRelations[3].strKind, QString("uses"));
        QCOMPARE(vecRelations[3].eStatus, ERelationStatus::PROPOSED);
        QCOMPARE(vecRelations[4].eStatus, ERelationStatus::CONFIRMED);
        QCOMPARE(vecRelations[5].eStatus, ERelationStatus::CONFIRMED);
        QVERIFY(result.hashUnreadLines.isEmpty());

        const DocumentReadResult docResult = KnowledgeMarkdownReader::ReadDocument(
            KnowledgeTestData::CanonicalDocument().replace(DecodeUtf8("  - 상태: AI 제안\n"), QString()));
        QVERIFY(docResult.bOk);
        QCOMPARE(docResult.document.vecRelations[0].eStatus, ERelationStatus::CONFIRMED);
    }

    void unknownSubItemsAreKept()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText.replace(DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n"),
                        DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n  - 메모: 다시 볼 것\n"));
        strText.replace(DecodeUtf8("  - 상태: AI 제안\n"), DecodeUtf8("  - 상태: AI 제안\n  - 출처: 표 3\n"));
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(result.bOk);
        QCOMPARE(result.document.vecCards[0].vecExtraLines, QStringList({ DecodeUtf8("메모: 다시 볼 것") }));
        QCOMPARE(result.document.vecRelations[0].vecExtraLines, QStringList({ DecodeUtf8("출처: 표 3") }));
        QVERIFY(result.hashUnreadLines.isEmpty());
    }

    void unreadableLinesAreCollected()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText.replace(DecodeUtf8("## 데이터"), DecodeUtf8("그냥 문장\n\n## 데이터"));
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(result.bOk);
        QCOMPARE(result.document.vecRelations.size(), 1);
        QCOMPARE(result.hashUnreadLines.value(DecodeUtf8("관계")), QStringList({ DecodeUtf8("그냥 문장") }));
        QCOMPARE(result.hashUnreadLines.size(), 1);
    }

    void raggedTableRowIsUnread()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText += DecodeUtf8("| 셋 | 칸 | 이다 |\n| + 앵커 | 69.5 |\n");
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(result.bOk);
        QCOMPARE(result.document.vecTables[0].vecRows.size(), 3);
        QCOMPARE(result.document.vecTables[0].vecRows[2], QStringList({ DecodeUtf8("+ 앵커"), "69.5" }));
        QCOMPARE(result.hashUnreadLines.value(DecodeUtf8("데이터")), QStringList({ DecodeUtf8("| 셋 | 칸 | 이다 |") }));
    }

    void tableWithoutOuterPipesIsRead()
    {
        const KnowledgeDocument canonical = KnowledgeMarkdownReader::ReadDocument(KnowledgeTestData::CanonicalDocument()).document;
        const QString strBare = DecodeUtf8("단계 | mAP (%)\n--- | --:\nYOLO (기준) | 63.4\n+ 배치 정규화 | 65.8\n");
        const QString strOuter = DecodeUtf8("| 단계 | mAP (%) |\n| --- | --: |\n| YOLO (기준) | 63.4 |\n| + 배치 정규화 | 65.8 |\n");
        QVERIFY(KnowledgeTestData::CanonicalDocument().contains(strOuter));

        const DocumentReadResult bare = KnowledgeMarkdownReader::ReadDocument(
            KnowledgeTestData::CanonicalDocument().replace(strOuter, strBare));
        QVERIFY(bare.bOk);
        QVERIFY(bare.document == canonical);
        QVERIFY(bare.hashUnreadLines.isEmpty());

        for (int iSpaces = 1; iSpaces <= 3; ++iSpaces)
        {
            QString strIndented = strBare;
            strIndented.replace(QStringLiteral("\n"), QStringLiteral("\n") + QString(iSpaces, QLatin1Char(' ')));
            strIndented.prepend(QString(iSpaces, QLatin1Char(' ')));
            strIndented.chop(iSpaces);
            const DocumentReadResult indented = KnowledgeMarkdownReader::ReadDocument(
                KnowledgeTestData::CanonicalDocument().replace(strOuter, strIndented));
            QVERIFY(indented.document == canonical);
            QVERIFY(indented.hashUnreadLines.isEmpty());
        }

        const DocumentReadResult deep = KnowledgeMarkdownReader::ReadDocument(
            KnowledgeTestData::CanonicalDocument().replace(strOuter, DecodeUtf8("    ") + strOuter));
        QCOMPARE(deep.document.vecTables[0].vecRows.size(), 0);
    }

    void escapedPipeIsNotASeparator()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText.replace(DecodeUtf8("| + 배치 정규화 | 65.8 |"), DecodeUtf8("+ 배치 \\| 정규화 | 65.8"));
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QCOMPARE(result.document.vecTables[0].vecRows.size(), 2);
        QCOMPARE(result.document.vecTables[0].vecRows[1], QStringList({ DecodeUtf8("+ 배치 | 정규화"), "65.8" }));
        QVERIFY(result.hashUnreadLines.isEmpty());
    }

    void headingWithoutTableIsKeptAsText()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText += DecodeUtf8("\n### 메모\n\n그냥 글\n");
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(result.bOk);
        QCOMPARE(result.document.vecTables.size(), 1);
        QCOMPARE(result.hashUnreadLines.value(DecodeUtf8("데이터")), QStringList({ DecodeUtf8("### 메모"), DecodeUtf8("그냥 글") }));
    }

    void headerWithoutDelimiterRowIsNotATable()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText += DecodeUtf8("\n### 표 아님\n\n| 가 | 나 |\n| 1 | 2 |\n");
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QCOMPARE(result.document.vecTables.size(), 1);
        QCOMPARE(result.hashUnreadLines.value(DecodeUtf8("데이터")),
                 QStringList({ DecodeUtf8("### 표 아님"), DecodeUtf8("| 가 | 나 |"), DecodeUtf8("| 1 | 2 |") }));
    }

    void unknownSectionsAndFencesAreIgnored()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText += DecodeUtf8("\n## 내 메모\n\n그냥 적은 글\n\n```\n## 관계\n\n- [X](x.md) **uses** → [Y](y.md)\n```\n\n~~~\n# 제목 아님\n~~~\n");
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(result.bOk);
        QCOMPARE(result.document.vecRelations.size(), 1);
        QCOMPARE(result.document.vecTables.size(), 1);
        QCOMPARE(result.document.vecTables[0].vecRows.size(), 2);
        QVERIFY(result.hashUnreadLines.isEmpty());

        const KnowledgeLayout layout = KnowledgeMarkdownLayout::Scan(strText);
        QStringList vecNames;
        for (const KnowledgeSectionRange& section : layout.vecSections)
            vecNames << section.strName;
        QCOMPARE(vecNames, QStringList({ DecodeUtf8("차례"), DecodeUtf8("핵심 숫자"), DecodeUtf8("지식"), DecodeUtf8("관계"), DecodeUtf8("데이터"), DecodeUtf8("내 메모") }));
        QCOMPARE(layout.strTitle, QString("YOLO9000: Better, Faster, Stronger"));
    }

    void layoutReportsLineRanges()
    {
        const KnowledgeLayout layout = KnowledgeMarkdownLayout::Scan(KnowledgeTestData::CanonicalDocument());
        QVERIFY(layout.bFrontMatterClosed);
        QCOMPARE(layout.iFrontMatterStart, 0);
        QCOMPARE(layout.iFrontMatterEnd, 12);
        QCOMPARE(layout.vecHeader.size(), 11);
        QCOMPARE(layout.vecHeader[0].strKey, QString("type"));
        QCOMPARE(layout.vecHeader[0].iLine, 1);
        QCOMPARE(layout.iTitleLine, 14);
        QCOMPARE(layout.iSummaryFirst, 16);
        QCOMPARE(layout.iSummaryLast, 16);
        QCOMPARE(layout.vecSections.size(), 5);
        QCOMPARE(layout.vecSections[0].strName, DecodeUtf8("차례"));
        QCOMPARE(layout.vecSections[0].iHeadingLine, 18);
        QCOMPARE(layout.vecSections[0].iLastLine, 23);
        QCOMPARE(layout.vecLines[layout.vecSections[4].iLastLine], DecodeUtf8("| + 배치 정규화 | 65.8 |"));
    }

    void crlfBomAndTabsAreAccepted()
    {
        const QString strLf = KnowledgeTestData::CanonicalDocument();
        QString strOther = strLf;
        strOther.replace(DecodeUtf8("\n  - "), DecodeUtf8("\n\t- "));
        strOther.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
        strOther.prepend(QChar(0xFEFF));
        const DocumentReadResult resultLf = KnowledgeMarkdownReader::ReadDocument(strLf);
        const DocumentReadResult resultOther = KnowledgeMarkdownReader::ReadDocument(strOther);
        QVERIFY2(resultOther.bOk, qPrintable(resultOther.strError));
        QVERIFY(resultLf.document == resultOther.document);
        QCOMPARE(resultOther.hashUnreadLines, resultLf.hashUnreadLines);
        QCOMPARE(resultOther.document.vecOutline[1].vecChildren.size(), 1);
        QVERIFY(resultOther.hashUnreadLines.isEmpty());

        QString strConceptOther = KnowledgeTestData::CanonicalConcept();
        strConceptOther.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
        strConceptOther.prepend(QChar(0xFEFF));
        const ConceptReadResult conceptLf = KnowledgeMarkdownReader::ReadConcept(KnowledgeTestData::CanonicalConcept());
        const ConceptReadResult conceptOther = KnowledgeMarkdownReader::ReadConcept(strConceptOther);
        QVERIFY(conceptOther.bOk);
        QVERIFY(conceptLf.concept == conceptOther.concept);
    }

    void escapesAreDecoded()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText.replace(DecodeUtf8("| + 배치 정규화 | 65.8 |"), DecodeUtf8("| a \\| b | 65.8 |"));
        strText.replace(DecodeUtf8("condition=\"VOC 2007 test\""), DecodeUtf8("condition=\"x &quot;y&quot; &amp; z --&gt;\""));
        strText.replace(DecodeUtf8("- [YOLOv2](../concepts/yolov2.md) **개선**"), DecodeUtf8("- [a\\[1\\]](../concepts/a1.md) **개선**"));
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(result.bOk);
        QCOMPARE(result.document.vecRelations[0].fromConcept.strName, QString("a[1]"));
        QCOMPARE(result.document.vecRelations[0].fromConcept.strConceptId, QString("a1"));
        QCOMPARE(result.document.vecTables[0].vecRows[1], QStringList({ QString("a | b"), "65.8" }));
        QCOMPARE(result.document.vecTables[0].strCondition, QString("x \"y\" & z -->"));
        QVERIFY(result.hashUnreadLines.isEmpty());
    }

    void columnKindsFollowSpec()
    {
        KnowledgeTable table;
        for (const QString& strName : { DecodeUtf8("이름"), QString("mAP"), DecodeUtf8("입력"), DecodeUtf8("빈"), QString("n") })
            table.vecColumns.push_back({ strName, QString() });
        table.vecOrdered = QStringList({ DecodeUtf8("입력") });
        table.vecRows.push_back(QStringList({ "a", "63.4", "400", "", "1" }));
        table.vecRows.push_back(QStringList({ "b", " 1,234 ", "500", "", "" }));
        table.vecRows.push_back(QStringList({ "c", "-0.3", "600", "", "63.4*" }));
        QCOMPARE(KnowledgeText::ClassifyColumn(table, 0), EColumnKind::NOMINAL);
        QCOMPARE(KnowledgeText::ClassifyColumn(table, 1), EColumnKind::QUANTITY);
        QCOMPARE(KnowledgeText::ClassifyColumn(table, 2), EColumnKind::ORDINAL);
        QCOMPARE(KnowledgeText::ClassifyColumn(table, 3), EColumnKind::NOMINAL);
        QCOMPARE(KnowledgeText::ClassifyColumn(table, 4), EColumnKind::NOMINAL);
        table.vecRows[2][4] = "";
        QCOMPARE(KnowledgeText::ClassifyColumn(table, 4), EColumnKind::QUANTITY);
        QCOMPARE(KnowledgeText::ClassifyColumn(table, 9), EColumnKind::NOMINAL);
    }

    void readsConceptFile()
    {
        QString strText = MakeConceptWithRelations(DecodeUtf8("- [YOLOv2](yolov2.md) **종류** → [1단계 검출기](1단계-검출기.md)\n"));
        ConceptReadResult result = KnowledgeMarkdownReader::ReadConcept(strText);
        QVERIFY2(result.bOk, qPrintable(result.strError));
        const KnowledgeConcept& concept = result.concept;
        QCOMPARE(concept.iFormat, 1);
        QCOMPARE(concept.strId, QString("yolov2"));
        QCOMPARE(concept.strKind, QString("method"));
        QCOMPARE(concept.vecAliases, QStringList({ "YOLO9000" }));
        QCOMPARE(concept.strYear, QString("2017"));
        QCOMPARE(concept.strName, QString("YOLOv2"));
        QCOMPARE(concept.strSummary, DecodeUtf8("YOLO 에 anchor · 배치 정규화 등을 더해 정확도를 끌어올린 판"));
        QCOMPARE(concept.vecRelations.size(), 1);
        QCOMPARE(concept.vecRelations[0].fromConcept.strConceptId, QString("yolov2"));
        QCOMPARE(concept.vecRelations[0].strKind, QString("isA"));
        QCOMPARE(concept.vecRelations[0].toConcept.strConceptId, DecodeUtf8("1단계-검출기"));
        QCOMPARE(concept.vecRelations[0].eStatus, ERelationStatus::CONFIRMED);

        strText.replace(QStringLiteral("aliases: YOLO9000"), QStringLiteral("aliases: a,  b ,c"));
        result = KnowledgeMarkdownReader::ReadConcept(strText);
        QCOMPARE(result.concept.vecAliases, QStringList({ "a", "b", "c" }));

        strText.replace(QStringLiteral("year: 2017"), QStringLiteral("year: abc"));
        QCOMPARE(KnowledgeMarkdownReader::ReadConcept(strText).concept.strYear, QString());
    }

    void brokenFilesFail()
    {
        QString strUnclosed = KnowledgeTestData::CanonicalDocument();
        strUnclosed.replace(DecodeUtf8("digest: {{digest}}\n---\n"), DecodeUtf8("digest: {{digest}}\n"));
        QVERIFY(!KnowledgeMarkdownReader::ReadDocument(strUnclosed).bOk);
        QVERIFY(!KnowledgeMarkdownReader::ReadDocument(strUnclosed).strError.isEmpty());

        QVERIFY(!KnowledgeMarkdownReader::ReadConcept(KnowledgeTestData::CanonicalDocument()).bOk);
        QVERIFY(!KnowledgeMarkdownReader::ReadDocument(KnowledgeTestData::CanonicalConcept()).bOk);

        QString strNoId = KnowledgeTestData::CanonicalDocument();
        strNoId.replace(QRegularExpression(QStringLiteral("\nid: [^\n]*")), QString());
        QVERIFY(!KnowledgeMarkdownReader::ReadDocument(strNoId).bOk);

        QString strConceptNoId = KnowledgeTestData::CanonicalConcept();
        strConceptNoId.replace(QStringLiteral("id: yolov2\n"), QString());
        QVERIFY(!KnowledgeMarkdownReader::ReadConcept(strConceptNoId).bOk);

        QVERIFY(!KnowledgeMarkdownReader::ReadDocument(QStringLiteral("그냥 글")).bOk);
        QVERIFY(!KnowledgeMarkdownReader::ReadDocument(QString()).bOk);
    }

    void higherFormatIsReported()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText.replace(QStringLiteral("format: 1"), QStringLiteral("format: 2"));
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(result.bOk);
        QCOMPARE(result.document.iFormat, 2);

        QString strConcept = KnowledgeTestData::CanonicalConcept();
        strConcept.replace(QStringLiteral("format: 1"), QStringLiteral("format: 3"));
        QCOMPARE(KnowledgeMarkdownReader::ReadConcept(strConcept).concept.iFormat, 3);
    }

    void keyNumberWithoutUnitKeepsLabel()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText.replace(DecodeUtf8("- **76.8** mAP — VOC 2007, 67 fps"), DecodeUtf8("- **76.8** — VOC 2007\n- **9000**\n- **5** fps"));
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QCOMPARE(result.document.vecKeyNumbers.size(), 3);
        QCOMPARE(result.document.vecKeyNumbers[0].strUnit, QString());
        QCOMPARE(result.document.vecKeyNumbers[0].strLabel, DecodeUtf8("VOC 2007"));
        QCOMPARE(result.document.vecKeyNumbers[1].strValue, QString("9000"));
        QCOMPARE(result.document.vecKeyNumbers[2].strUnit, QString("fps"));
        QCOMPARE(result.document.vecKeyNumbers[2].strLabel, QString());
    }

    void strayLinesDoNotAttachToNeighbours()
    {
        const KnowledgeDocument canonical = KnowledgeMarkdownReader::ReadDocument(KnowledgeTestData::CanonicalDocument()).document;
        QString strText = KnowledgeTestData::CanonicalDocument();
        // 첫 줄이 문법에 맞지 않는 카드(* 글머리)와 관계(링크 없음, -> 화살표)의 하위 항목은 앞 항목에 붙지 않는다
        strText.replace(DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n"),
                        DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n* 별표로 적은 주장\n  - 근거: 다른 근거\n  - 절: 9\n"));
        strText.replace(DecodeUtf8("  - 상태: AI 제안\n"),
                        DecodeUtf8("  - 상태: AI 제안\n- YOLOv2 **사용** -> YOLO\n  - 설명: 손으로 적은 설명\n  - 위치: 3절\n"));
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(result.bOk);
        QVERIFY(result.document == canonical);
        QCOMPARE(result.hashUnreadLines.value(DecodeUtf8("지식")),
                 QStringList({ DecodeUtf8("* 별표로 적은 주장"), DecodeUtf8("  - 근거: 다른 근거"), DecodeUtf8("  - 절: 9") }));
        QCOMPARE(result.hashUnreadLines.value(DecodeUtf8("관계")),
                 QStringList({ DecodeUtf8("- YOLOv2 **사용** -> YOLO"), DecodeUtf8("  - 설명: 손으로 적은 설명"), DecodeUtf8("  - 위치: 3절") }));

        // 다시 써도 이웃은 그대로이고, 떠돈 줄은 절 끝에서 계속 읽지 못한 줄이다
        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(result.document, strText);
        const DocumentReadResult reread = KnowledgeMarkdownReader::ReadDocument(strWritten);
        QVERIFY(reread.bOk);
        QVERIFY(reread.document.vecCards == canonical.vecCards);
        QVERIFY(reread.document.vecRelations == canonical.vecRelations);
        QCOMPARE(reread.hashUnreadLines, result.hashUnreadLines);

        // 빈 줄 뒤의 하위 항목도 앞 항목에 붙지 않는다(카드 · 관계 · 개념 파일의 관계)
        QString strLoose = KnowledgeTestData::CanonicalDocument();
        strLoose.replace(DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n"),
                         DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n\n  - 근거: 띄워 적은 근거\n"));
        strLoose.replace(DecodeUtf8("  - 상태: AI 제안\n"), DecodeUtf8("  - 상태: AI 제안\n\n  - 설명: 띄워 적은 설명\n"));
        const DocumentReadResult loose = KnowledgeMarkdownReader::ReadDocument(strLoose);
        QVERIFY(loose.document == canonical);
        QCOMPARE(loose.hashUnreadLines.value(DecodeUtf8("지식")), QStringList({ DecodeUtf8("  - 근거: 띄워 적은 근거") }));
        QCOMPARE(loose.hashUnreadLines.value(DecodeUtf8("관계")), QStringList({ DecodeUtf8("  - 설명: 띄워 적은 설명") }));

        // 들여 쓴 줄은 첫 줄이 아니다: 콜론 없는 하위 항목은 제 항목의 원문 하위 항목이고, `- ` 없이 이어 쓴 줄은
        // 읽지 못한 줄이지만 그 뒤 하위 항목은 제 항목에 그대로 붙는다
        QString strNote = KnowledgeTestData::CanonicalDocument();
        strNote.replace(DecodeUtf8("  - 절: 2\n"), DecodeUtf8("  - 절: 2\n  - 다시 볼 것\n"));
        strNote.replace(DecodeUtf8("  - 위치: 2절\n"), DecodeUtf8("  - 위치: 2절\n    이어 쓴 글\n"));
        const DocumentReadResult note = KnowledgeMarkdownReader::ReadDocument(strNote);
        KnowledgeDocument expectedNote = canonical;
        expectedNote.vecCards[0].vecExtraLines = QStringList({ DecodeUtf8("다시 볼 것") });
        QVERIFY(note.document == expectedNote);
        QVERIFY(note.hashUnreadLines.value(DecodeUtf8("지식")).isEmpty());
        QCOMPARE(note.hashUnreadLines.value(DecodeUtf8("관계")), QStringList({ DecodeUtf8("    이어 쓴 글") }));

        const ConceptReadResult concept = KnowledgeMarkdownReader::ReadConcept(MakeConceptWithRelations(DecodeUtf8(
            "- [YOLOv2](yolov2.md) **종류** → [1단계 검출기](1단계-검출기.md)\n"
            "  - 설명: 원래 설명\n"
            "- YOLOv2 **사용** → Darknet\n"
            "  - 설명: 남의 설명\n")));
        QCOMPARE(concept.concept.vecRelations.size(), 1);
        QCOMPARE(concept.concept.vecRelations[0].strNote, DecodeUtf8("원래 설명"));
        QCOMPARE(concept.hashUnreadLines.value(DecodeUtf8("관계")),
                 QStringList({ DecodeUtf8("- YOLOv2 **사용** → Darknet"), DecodeUtf8("  - 설명: 남의 설명") }));
    }

    void colonlessSubItemStaysWithItem()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText.replace(DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n"),
                        DecodeUtf8("  - 다시 볼 것\n  - 개념: [YOLO](../concepts/yolo.md)\n"));
        strText.replace(DecodeUtf8("  - 위치: 2절\n"), DecodeUtf8("  - 위치: 2절\n  - 표 3 참고\n"));
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(result.bOk);
        const KnowledgeCard& card = result.document.vecCards[0];
        QCOMPARE(card.vecConcepts.size(), 1);
        QCOMPARE(card.vecConcepts[0].strName, QString("YOLO"));
        QCOMPARE(card.vecConcepts[0].strConceptId, QString("yolo"));
        QCOMPARE(card.vecExtraLines, QStringList({ DecodeUtf8("다시 볼 것") }));
        QCOMPARE(card.strSection, QString("2"));
        QCOMPARE(result.document.vecRelations[0].vecExtraLines, QStringList({ DecodeUtf8("표 3 참고") }));
        QCOMPARE(result.document.vecRelations[0].strWhere, DecodeUtf8("2절"));
        QVERIFY(result.hashUnreadLines.isEmpty());

        // 다시 써도 두 줄은 그 항목 아래에 남는다(절 끝으로 옮겨지지 않는다)
        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(result.document, strText);
        QVERIFY2(strWritten.contains(DecodeUtf8("  - 절: 2\n  - 개념: [YOLO](../concepts/yolo.md)\n  - 다시 볼 것\n\n## 관계\n")),
                 qPrintable(strWritten));
        QVERIFY2(strWritten.contains(DecodeUtf8("  - 상태: AI 제안\n  - 표 3 참고\n\n## 데이터\n")), qPrintable(strWritten));
        QCOMPARE(strWritten.count(DecodeUtf8("다시 볼 것")), 1);
        QCOMPARE(strWritten.count(DecodeUtf8("표 3 참고")), 1);
        const DocumentReadResult reread = KnowledgeMarkdownReader::ReadDocument(strWritten);
        QVERIFY(reread.document.vecCards == result.document.vecCards);
        QVERIFY(reread.document.vecRelations == result.document.vecRelations);
        QVERIFY(reread.hashUnreadLines.isEmpty());

        // 빈 줄 뒤의 콜론 없는 하위 항목은 여전히 앞 항목에 붙지 않는다
        QString strLoose = KnowledgeTestData::CanonicalDocument();
        strLoose.replace(DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n"), DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n\n  - 띄워 적은 메모\n"));
        const DocumentReadResult loose = KnowledgeMarkdownReader::ReadDocument(strLoose);
        QVERIFY(loose.document.vecCards[0].vecExtraLines.isEmpty());
        QCOMPARE(loose.hashUnreadLines.value(DecodeUtf8("지식")), QStringList({ DecodeUtf8("  - 띄워 적은 메모") }));
    }

    void summarySkipsUserBlocks()
    {
        const QString strSummary = DecodeUtf8("YOLO 에 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올리고, 9000개 넘는 클래스를 검출하게 했다.");
        const QString strHead = DecodeUtf8("# YOLO9000: Better, Faster, Stronger\n\n");
        const QStringList vecBlocks {
            DecodeUtf8("![그림](a.png)\n"),
            DecodeUtf8("![그림](a.png)\n그림 설명\n"),
            DecodeUtf8("> 인용\n> 둘째 줄\n"),
            DecodeUtf8("> [!note] 콜아웃\n> 속 글\n"),
            DecodeUtf8("- 메모\n  이어 쓴 글\n"),
            DecodeUtf8("* 별표\n"),
            DecodeUtf8("+ 더하기\n"),
            DecodeUtf8("1. 번호\n2. 둘째\n"),
            DecodeUtf8("| a | b |\n| --- | --- |\n| 1 | 2 |\n"),
            DecodeUtf8("<div>\n속 글\n</div>\n"),
            DecodeUtf8("<!-- 주석 -->\n"),
            DecodeUtf8("```\n코드\n\n코드 뒤\n```\n"),
            DecodeUtf8("~~~\n# 제목 아님\n\n글\n~~~\n"),
            DecodeUtf8("### 소제목\n"),
            DecodeUtf8("# 둘째 제목\n"),
            DecodeUtf8("![그림](a.png)\n\n> 인용\n\n- 목록\n"),
        };
        for (const QString& strBlock : vecBlocks)
        {
            QString strText = KnowledgeTestData::CanonicalDocument();
            strText.replace(strHead, strHead + strBlock + DecodeUtf8("\n"));
            const KnowledgeLayout layout = KnowledgeMarkdownLayout::Scan(strText);
            QVERIFY2(layout.strSummary == strSummary, qPrintable(strBlock));
            QCOMPARE(layout.iSummaryLast, layout.iSummaryFirst);
            QCOMPARE(layout.vecLines[layout.iSummaryFirst], strSummary);
            QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strText).document.strSummary, strSummary);
        }

        // 한 줄은 빈 줄 없이 이어진 블록 시작 줄에서 끝난다. 여러 줄 문단은 공백 하나로 잇는다.
        for (const QString& strAfter : { DecodeUtf8("- 메모\n"), DecodeUtf8("* 별표\n"), DecodeUtf8("+ 더하기\n"), DecodeUtf8("1. 번호\n"), DecodeUtf8("> 인용\n"),
                                         DecodeUtf8("| a | b |\n"), DecodeUtf8("<br>\n"), DecodeUtf8("![그림](a.png)\n"), DecodeUtf8("```\n코드\n```\n"), DecodeUtf8("### 소제목\n"),
                                         DecodeUtf8("<!-- 주석 -->\n") })
        {
            QString strText = KnowledgeTestData::CanonicalDocument();
            strText.replace(strSummary + DecodeUtf8("\n"), DecodeUtf8("첫 줄\n둘째 줄\n") + strAfter);
            const KnowledgeLayout layout = KnowledgeMarkdownLayout::Scan(strText);
            QVERIFY2(layout.strSummary == DecodeUtf8("첫 줄 둘째 줄"), qPrintable(strAfter));
            QCOMPARE(layout.iSummaryLast, layout.iSummaryFirst + 1);
        }

        // 한 줄 블록(제목 · 가로줄 · 한 줄 HTML 주석 · 닫힌 울타리) 바로 다음 줄의 문단은 빈 줄이 없어도 한 줄이다
        for (const QString& strBefore : { DecodeUtf8("<!-- 다듬을 것 -->\n"), DecodeUtf8("  <!-- 들여 쓴 주석 --> 뒤 글\n"), DecodeUtf8("### 소제목\n"), DecodeUtf8("---\n"),
                                          DecodeUtf8("```\n코드\n```\n") })
        {
            QString strText = KnowledgeTestData::CanonicalDocument();
            strText.replace(strHead, strHead + strBefore);
            const KnowledgeLayout layout = KnowledgeMarkdownLayout::Scan(strText);
            QVERIFY2(layout.strSummary == strSummary, qPrintable(strBefore));
            QCOMPARE(layout.iSummaryLast, layout.iSummaryFirst);
            QCOMPARE(layout.vecLines[layout.iSummaryFirst], strSummary);
            QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strText).document.strSummary, strSummary);
        }

        // 첫 ## 앞에 보통 문단이 없으면 한 줄이 없다
        QString strNone = KnowledgeTestData::CanonicalDocument();
        strNone.replace(strSummary + DecodeUtf8("\n"), DecodeUtf8("> 인용만 있다\n"));
        const KnowledgeLayout none = KnowledgeMarkdownLayout::Scan(strNone);
        QCOMPARE(none.iSummaryFirst, -1);
        QVERIFY(none.strSummary.isEmpty());
    }

    void raggedRowDoesNotMigrateBetweenTables()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText += DecodeUtf8("| 셋 | 칸 | 이다 |\n"
                     "\n"
                     "### 둘째 표\n"
                     "\n"
                     "<!-- ocean:data x=\"a\" y=\"b\" -->\n"
                     "\n"
                     "| a | b | c |\n"
                     "| --- | --: | --- |\n"
                     "| x | 1 | y |\n");
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(result.bOk);
        QCOMPARE(result.document.vecTables.size(), 2);
        QCOMPARE(result.document.vecTables[0].vecRows.size(), 2);
        QCOMPARE(result.document.vecTables[1].vecRows.size(), 1);
        QCOMPARE(result.hashUnreadLines.value(DecodeUtf8("데이터")), QStringList({ DecodeUtf8("| 셋 | 칸 | 이다 |") }));

        // 다시 쓰면 읽지 못한 줄은 절 끝(둘째 표 뒤 빈 줄 다음)으로 간다. 칸 수가 같아도 둘째 표의 행이 되지 않는다.
        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(result.document, strText);
        QVERIFY(strWritten.endsWith(DecodeUtf8("| x | 1 | y |\n\n| 셋 | 칸 | 이다 |\n")));
        const DocumentReadResult reread = KnowledgeMarkdownReader::ReadDocument(strWritten);
        QVERIFY(reread.document.vecTables == result.document.vecTables);
        QCOMPARE(reread.hashUnreadLines, result.hashUnreadLines);

        // 표는 머리 줄을 본 뒤 첫 빈 줄에서 끝난다. 다음 ### 까지의 줄은 읽지 못한 줄이다.
        QString strGap = KnowledgeTestData::CanonicalDocument();
        strGap.replace(DecodeUtf8("| YOLO (기준) | 63.4 |\n"), DecodeUtf8("| YOLO (기준) | 63.4 |\n\n"));
        const DocumentReadResult gap = KnowledgeMarkdownReader::ReadDocument(strGap);
        QCOMPARE(gap.document.vecTables[0].vecRows.size(), 1);
        QCOMPARE(gap.hashUnreadLines.value(DecodeUtf8("데이터")), QStringList({ DecodeUtf8("| + 배치 정규화 | 65.8 |") }));
    }

    void percentEncodedLinksAreDecoded()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText.replace(DecodeUtf8("[YOLO](../concepts/yolo.md)"),
                        DecodeUtf8("[1단계 검출기](../concepts/1%EB%8B%A8%EA%B3%84-%EA%B2%80%EC%B6%9C%EA%B8%B0.md)"));
        strText.replace(DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)"),
                        DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md), [50% 규칙](../concepts/50%-rule.md), [A](../concepts/a%20b.md)"));
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(result.bOk);
        QCOMPARE(result.document.vecRelations[0].toConcept.strConceptId, DecodeUtf8("1단계-검출기"));
        QCOMPARE(result.document.vecRelations[0].toConcept.strName, DecodeUtf8("1단계 검출기"));
        // 올바른 %XX 가 아닌 % 는 그대로 둔다(Ocean 이 만든 id 에 % 가 들어갈 수 있다)
        QCOMPARE(result.document.vecCards[0].vecConcepts.size(), 3);
        QCOMPARE(result.document.vecCards[0].vecConcepts[1].strConceptId, QString("50%-rule"));
        QCOMPARE(result.document.vecCards[0].vecConcepts[2].strConceptId, QString("a b"));
    }

    void obsidianAliasListIsRead()
    {
        // Obsidian 속성 창이 다시 쓴 블록 목록
        QString strBlock = KnowledgeTestData::CanonicalConcept();
        strBlock.replace(QStringLiteral("aliases: YOLO9000\n"),
                         QStringLiteral("aliases:\n  - YOLO9000\n  - \"YOLO, v2\"\n  - 'YOLO 544'\n"));
        ConceptReadResult result = KnowledgeMarkdownReader::ReadConcept(strBlock);
        QVERIFY2(result.bOk, qPrintable(result.strError));
        QCOMPARE(result.concept.vecAliases, QStringList({ "YOLO9000", "YOLO, v2", "YOLO 544" }));
        QCOMPARE(result.concept.strYear, QString("2017"));
        QCOMPARE(result.concept.strKind, QString("method"));

        // 들여 쓰지 않은 블록 목록
        QString strFlush = KnowledgeTestData::CanonicalConcept();
        strFlush.replace(QStringLiteral("aliases: YOLO9000\n"), QStringLiteral("aliases:\n- YOLO9000\n- YOLO v2\n"));
        result = KnowledgeMarkdownReader::ReadConcept(strFlush);
        QCOMPARE(result.concept.vecAliases, QStringList({ "YOLO9000", "YOLO v2" }));
        QCOMPARE(result.concept.strYear, QString("2017"));

        // 흐름 목록
        QString strFlow = KnowledgeTestData::CanonicalConcept();
        strFlow.replace(QStringLiteral("aliases: YOLO9000\n"), QStringLiteral("aliases: [YOLO9000, \"YOLO, v2\", 'it''s']\n"));
        result = KnowledgeMarkdownReader::ReadConcept(strFlow);
        QCOMPARE(result.concept.vecAliases, QStringList({ "YOLO9000", "YOLO, v2", "it's" }));

        // 빈 목록 · 값 없는 키
        QString strEmpty = KnowledgeTestData::CanonicalConcept();
        strEmpty.replace(QStringLiteral("aliases: YOLO9000\n"), QStringLiteral("aliases: []\n"));
        QVERIFY(KnowledgeMarkdownReader::ReadConcept(strEmpty).concept.vecAliases.isEmpty());
        strEmpty = KnowledgeTestData::CanonicalConcept();
        strEmpty.replace(QStringLiteral("aliases: YOLO9000\n"), QStringLiteral("aliases:\n"));
        result = KnowledgeMarkdownReader::ReadConcept(strEmpty);
        QVERIFY(result.concept.vecAliases.isEmpty());
        QCOMPARE(result.concept.strYear, QString("2017"));

        // 블록 목록의 줄 범위
        const KnowledgeLayout layout = KnowledgeMarkdownLayout::Scan(strBlock);
        QCOMPARE(layout.FindHeaderValue(QStringLiteral("aliases")), QStringLiteral("YOLO9000, YOLO, v2, YOLO 544"));
        QCOMPARE(layout.vecHeader.size(), 6);
    }

    void scalarThatLooksLikeListIsKept()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText.replace(DecodeUtf8("meta: Redmon · Farhadi · CVPR 2017"), DecodeUtf8("meta: [초안] 3쪽"));
        QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strText).document.strMeta, DecodeUtf8("[초안] 3쪽"));
        strText.replace(DecodeUtf8("meta: [초안] 3쪽"), DecodeUtf8("meta: [초안]"));
        QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strText).document.strMeta, DecodeUtf8("[초안]"));

        QString strConcept = KnowledgeTestData::CanonicalConcept();
        strConcept.replace(QStringLiteral("aliases: YOLO9000"), QStringLiteral("aliases: [a, b]"));
        QCOMPARE(KnowledgeMarkdownReader::ReadConcept(strConcept).concept.vecAliases, QStringList({ "a", "b" }));
    }

    void quotedScalarsAreRead()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText.replace(QStringLiteral("id: 3f1c2a9e-5b7d-4c8e-9a21-6d0e4b8f7c31"),
                        QStringLiteral("id: \"3f1c2a9e-5b7d-4c8e-9a21-6d0e4b8f7c31\""));
        strText.replace(QStringLiteral("source: yolo9000.pdf"), QStringLiteral("source: \"notes: \\\"v2\\\" \\\\ draft.pdf\""));
        strText.replace(DecodeUtf8("fileInfo: PDF 9쪽"), DecodeUtf8("fileInfo: 'PDF ''9''쪽'"));
        strText.replace(DecodeUtf8("meta: Redmon · Farhadi · CVPR 2017"), DecodeUtf8("meta: 5 \"보통\" 'a'"));
        strText.replace(QStringLiteral("edited: false"), QStringLiteral("edited: \"true\""));
        const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY2(result.bOk, qPrintable(result.strError));
        QCOMPARE(result.document.strId, QString("3f1c2a9e-5b7d-4c8e-9a21-6d0e4b8f7c31"));
        QCOMPARE(result.document.strSource, QString("notes: \"v2\" \\ draft.pdf"));
        QCOMPARE(result.document.strFileInfo, DecodeUtf8("PDF '9'쪽"));
        QCOMPARE(result.document.strMeta, DecodeUtf8("5 \"보통\" 'a'"));   // 앞뒤를 감싼 따옴표만 벗긴다
        QCOMPARE(result.document.bEdited, true);

        // 따옴표가 앞뒤를 감싸지 않으면 그대로
        QString strHalf = KnowledgeTestData::CanonicalDocument();
        strHalf.replace(QStringLiteral("source: yolo9000.pdf"), QStringLiteral("source: \"a\" and \"b\""));
        QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strHalf).document.strSource, QString("\"a\" and \"b\""));

        QString strConcept = KnowledgeTestData::CanonicalConcept();
        strConcept.replace(QStringLiteral("kind: method"), QStringLiteral("kind: 'method'"));
        strConcept.replace(QStringLiteral("aliases: YOLO9000"), QStringLiteral("aliases: \"YOLO9000, YOLO v2\""));
        strConcept.replace(QStringLiteral("year: 2017"), QStringLiteral("year: \"2017\""));
        const ConceptReadResult concept = KnowledgeMarkdownReader::ReadConcept(strConcept);
        QCOMPARE(concept.concept.strKind, QString("method"));
        QCOMPARE(concept.concept.vecAliases, QStringList({ "YOLO9000", "YOLO v2" }));
        QCOMPARE(concept.concept.strYear, QString("2017"));
    }
};

std::unique_ptr<QObject> CreateKnowledgeMarkdownReaderTest() { return std::make_unique<KnowledgeMarkdownReaderTest>(); }

#include "KnowledgeMarkdownReaderTest.moc"
