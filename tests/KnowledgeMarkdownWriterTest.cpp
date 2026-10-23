#include "VisualMarkdown/KnowledgeMarkdownReader.h"
#include "VisualMarkdown/KnowledgeMarkdownWriter.h"
#include "KnowledgeTestData.h"

#include <QRegularExpression>
#include <QTest>

#include <memory>

namespace
{
QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

KnowledgeDocument ReadCanonicalRecord()
{
    return KnowledgeMarkdownReader::ReadDocument(KnowledgeTestData::CanonicalDocument()).document;
}

// 정준 기록을 새로 쓴 글(digest 자리를 채운 정준 글)
QString MakeWrittenCanonical()
{
    return KnowledgeTestData::CanonicalDocument().replace(KnowledgeTestData::kDigestSlot,
                                                         KnowledgeMarkdownWriter::ComputeDigest(ReadCanonicalRecord()));
}

// 글의 digest 줄을 기록의 digest 로 바꾼다(기대 글을 만들 때)
QString ReplaceDigest(QString text, const KnowledgeDocument& document)
{
    static const QRegularExpression reDigest(QStringLiteral("\ndigest: [^\n]*\n"));
    return text.replace(reDigest, QStringLiteral("\ndigest: ") + KnowledgeMarkdownWriter::ComputeDigest(document) + QStringLiteral("\n"));
}

KnowledgeDocument MakeRichRecord()
{
    KnowledgeDocument doc = ReadCanonicalRecord();
    doc.vecOutline[2].vecChildren.push_back({ QStringLiteral("3.1"), DecodeUtf8("Darknet-19"), {} });
    doc.vecKeyNumbers.push_back({ QStringLiteral("9000"), QString(), DecodeUtf8("검출하는 클래스 수") });
    doc.vecKeyNumbers.push_back({ QStringLiteral("67"), QStringLiteral("fps"), QString() });
    doc.vecKeyNumbers.push_back({ QStringLiteral("19"), QString(), QString() });

    KnowledgeCard card;
    card.strId = QStringLiteral("0a1b2c3d");
    card.strClaim = DecodeUtf8("WordTree 로 분류 데이터와 검출 데이터를 함께 학습한다");
    card.strSection = QStringLiteral("4");
    card.vecConcepts = { { QStringLiteral("WordTree"), QStringLiteral("wordtree") },
                         { QStringLiteral("YOLO9000"), QStringLiteral("yolo9000") } };
    card.vecExtraLines = QStringList({ DecodeUtf8("메모: 다시 볼 것"), DecodeUtf8("출처: 그림 6") });
    doc.vecCards.push_back(card);
    KnowledgeCard bare;
    bare.strClaim = DecodeUtf8("주석이 없는 카드");
    bare.strEvidence = DecodeUtf8("손으로 적음");
    doc.vecCards.push_back(bare);

    KnowledgeRelation alternative;
    alternative.fromConcept = { QStringLiteral("YOLO"), QStringLiteral("yolo") };
    alternative.strKind = QStringLiteral("alternative");
    alternative.toConcept = { QStringLiteral("Faster R-CNN"), QStringLiteral("faster-r-cnn") };
    alternative.vecExtraLines = QStringList({ DecodeUtf8("출처: 표 3") });
    doc.vecRelations.push_back(alternative);
    KnowledgeRelation unknownKind;
    unknownKind.fromConcept = { QStringLiteral("YOLO9000"), QStringLiteral("yolo9000") };
    unknownKind.strKind = DecodeUtf8("반박");
    unknownKind.toConcept = { QStringLiteral("YOLO"), QStringLiteral("yolo") };
    unknownKind.strQuote = DecodeUtf8("\"따옴표\" 안의 인용");
    unknownKind.eStatus = ERelationStatus::PROPOSED;
    doc.vecRelations.push_back(unknownKind);

    KnowledgeTable table;
    table.strTitle = DecodeUtf8("해상도별 속도");
    table.strX = DecodeUtf8("해상도");
    table.vecY = QStringList({ QStringLiteral("FPS"), QStringLiteral("mAP") });
    table.strSeries = DecodeUtf8("모델");
    table.vecColumns = { { DecodeUtf8("해상도"), QStringLiteral("px") }, { QStringLiteral("FPS"), QString() },
                         { QStringLiteral("mAP"), QStringLiteral("%") }, { DecodeUtf8("모델"), QString() } };
    table.vecRows = { { QStringLiteral("288"), QStringLiteral("91"), QStringLiteral("69.0"), QStringLiteral("YOLOv2") },
                      { QStringLiteral("544"), QStringLiteral("40"), QString(), QStringLiteral("YOLOv2") } };
    doc.vecTables.push_back(table);
    return doc;
}
}

class KnowledgeMarkdownWriterTest : public QObject
{
    Q_OBJECT

private slots:
    void freshDocumentMatchesCanonicalText()
    {
        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(KnowledgeTestData::CanonicalDocument());
        const QString strDigest = KnowledgeMarkdownWriter::ComputeDigest(read.document);
        QVERIFY(QRegularExpression("^[0-9a-f]{64}$").match(strDigest).hasMatch());
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(read.document),
                 KnowledgeTestData::CanonicalDocument().replace(KnowledgeTestData::kDigestSlot, strDigest));
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(KnowledgeMarkdownReader::ReadConcept(KnowledgeTestData::CanonicalConcept()).concept),
                 KnowledgeTestData::CanonicalConcept());
    }

    void roundTripIsByteExact()
    {
        const KnowledgeDocument doc = MakeRichRecord();
        const QString strText = KnowledgeMarkdownWriter::WriteDocument(doc);
        QVERIFY(!strText.contains(QLatin1Char('\r')));
        QVERIFY(strText.endsWith(QLatin1Char('\n')) && !strText.endsWith(QStringLiteral("\n\n")));

        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY2(read.bOk, qPrintable(read.strError));
        QVERIFY(read.hashUnreadLines.isEmpty());
        KnowledgeDocument expected = doc;
        expected.strDigest = KnowledgeMarkdownWriter::ComputeDigest(doc);
        QVERIFY(read.document == expected);
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(read.document, strText), strText);

        const QString strConcept = KnowledgeTestData::CanonicalConcept()
            + DecodeUtf8("\n## 관계\n\n- [YOLOv2](yolov2.md) **종류** → [1단계 검출기](1단계-검출기.md)\n");
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(KnowledgeMarkdownReader::ReadConcept(strConcept).concept, strConcept), strConcept);
    }

    void unknownPartsSurviveRewrite()
    {
        QString strText = MakeWrittenCanonical();
        strText.replace(DecodeUtf8("## 관계\n"), DecodeUtf8("## 내 메모\n\n자유롭게 쓴 글\n\n## 관계\n"));
        strText.replace(DecodeUtf8("meta: Redmon · Farhadi · CVPR 2017\n"), DecodeUtf8("meta: Redmon · Farhadi · CVPR 2017\ntags: a, b\n"));
        strText.replace(DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n"),
                        DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n  - 메모: 다시 볼 것\n"));
        strText.replace(DecodeUtf8("  - 상태: AI 제안\n"), DecodeUtf8("  - 상태: AI 제안\n\n그냥 문장\n"));

        DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(read.bOk);
        QCOMPARE(read.hashUnreadLines.value(DecodeUtf8("관계")), QStringList({ DecodeUtf8("그냥 문장") }));
        QCOMPARE(read.document.vecCards[0].vecExtraLines, QStringList({ DecodeUtf8("메모: 다시 볼 것") }));
        const QString strOldClaim = DecodeUtf8("- 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올렸다 <!--");
        const QString strNewClaim = DecodeUtf8("- 작은 개선 여럿을 쌓아 정확도를 올렸다 <!--");
        read.document.vecCards[0].strClaim = DecodeUtf8("작은 개선 여럿을 쌓아 정확도를 올렸다");

        QString strExpected = strText;
        strExpected.replace(strOldClaim, strNewClaim);
        strExpected = ReplaceDigest(strExpected, read.document);
        QVERIFY(strExpected != strText);
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(read.document, strText), strExpected);

        // 다른 편집기에서 저장한 글(CRLF · BOM · 탭으로 들여 쓴 하위 항목): 결과는 LF, 정해진 부분 밖은 그대로
        QString strOther = strText;
        strOther.replace(DecodeUtf8("\n  - "), DecodeUtf8("\n\t- "));
        strOther.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
        strOther.prepend(QChar(0xFEFF));
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(read.document, strOther), strExpected);
    }

    void userBlocksAroundSummaryStay()
    {
        const QString strSummary = DecodeUtf8("YOLO 에 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올리고, 9000개 넘는 클래스를 검출하게 했다.");
        const QString strTitle = DecodeUtf8("# YOLO9000: Better, Faster, Stronger\n\n");
        QString strText = MakeWrittenCanonical();
        strText.replace(strTitle, strTitle + DecodeUtf8("![그림](a.png)\n\n"));
        strText.replace(strSummary + DecodeUtf8("\n"), strSummary + DecodeUtf8("\n- 메모\n"));

        DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(read.bOk);
        QCOMPARE(read.document.strSummary, strSummary);
        KnowledgeDocument canonical = ReadCanonicalRecord();
        canonical.strDigest = read.document.strDigest;
        QVERIFY(read.document == canonical);
        QVERIFY(read.hashUnreadLines.isEmpty());
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(read.document, strText), strText);

        // 한 줄을 바꿔 다시 쓰면 한 줄만 바뀌고 그림 · 메모는 같은 자리에 글자 그대로 있다
        read.document.strSummary = DecodeUtf8("새 한 줄");
        QString strExpected = strText;
        strExpected.replace(strSummary + DecodeUtf8("\n"), DecodeUtf8("새 한 줄\n"));
        strExpected = ReplaceDigest(strExpected, read.document);
        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(read.document, strText);
        QCOMPARE(strWritten, strExpected);
        QVERIFY(strWritten.contains(strTitle + DecodeUtf8("![그림](a.png)\n\n새 한 줄\n- 메모\n\n## 차례\n")));

        // 인용 · 콜아웃 · HTML 도 그 자리에 남는다
        QString strQuote = MakeWrittenCanonical();
        strQuote.replace(strTitle, strTitle + DecodeUtf8("> [!note] 메모\n> 둘째 줄\n\n<div>\n속 글\n</div>\n\n"));
        strQuote.replace(strSummary + DecodeUtf8("\n"), strSummary + DecodeUtf8("\n> 뒤 인용\n"));
        KnowledgeDocument quoted = KnowledgeMarkdownReader::ReadDocument(strQuote).document;
        QCOMPARE(quoted.strSummary, strSummary);
        quoted.strSummary = DecodeUtf8("새 한 줄");
        strExpected = strQuote;
        strExpected.replace(strSummary + DecodeUtf8("\n"), DecodeUtf8("새 한 줄\n"));
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(quoted, strQuote), ReplaceDigest(strExpected, quoted));

        // 정해진 절이 하나도 없어 새로 넣을 때, 한 줄 뒤의 사용자 줄은 새 절 안으로 들어가지 않고 그 앞 자리에 남는다
        const QString strHeadOnly = strText.left(strText.indexOf(DecodeUtf8("## 차례\n")));
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(ReadCanonicalRecord(), strHeadOnly), strText);
        // 한 줄도 없으면 한 줄은 제목 바로 뒤에, 절은 사용자 줄 뒤에
        QString strNoSummary = strHeadOnly;
        strNoSummary.remove(strSummary + DecodeUtf8("\n"));
        QString strExpectedNoSummary = strText;
        strExpectedNoSummary.replace(strTitle + DecodeUtf8("![그림](a.png)\n\n") + strSummary + DecodeUtf8("\n"),
                                     strTitle + strSummary + DecodeUtf8("\n\n![그림](a.png)\n\n"));
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(ReadCanonicalRecord(), strNoSummary), strExpectedNoSummary);
    }

    void unreadBlocksKeepBlankLines()
    {
        QString strText = MakeWrittenCanonical();
        strText.replace(DecodeUtf8("  - 상태: AI 제안\n"), DecodeUtf8("  - 상태: AI 제안\n\n메모 첫 문단\n\n메모 둘째 문단\n"));
        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(read.bOk);
        QCOMPARE(read.hashUnreadLines.value(DecodeUtf8("관계")), QStringList({ DecodeUtf8("메모 첫 문단"), DecodeUtf8("메모 둘째 문단") }));
        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(read.document, strText);
        QVERIFY2(strWritten.contains(DecodeUtf8("\n메모 첫 문단\n\n메모 둘째 문단\n")), qPrintable(strWritten));
        QCOMPARE(strWritten, strText);

        // 절 가운데 있던 덩어리는 절 끝으로 가도 가운데 빈 줄(여럿이면 그 수대로)을 지킨다. 앞뒤 빈 줄은 덩어리에 들지 않는다.
        QString strMiddle = MakeWrittenCanonical();
        strMiddle.replace(DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n"),
                          DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n\n메모 A\n\n\n메모 B\n\n- 둘째 카드 <!-- card:0a1b2c3d -->\n"));
        const DocumentReadResult middle = KnowledgeMarkdownReader::ReadDocument(strMiddle);
        QCOMPARE(middle.document.vecCards.size(), 2);
        QCOMPARE(middle.hashUnreadLines.value(DecodeUtf8("지식")), QStringList({ DecodeUtf8("메모 A"), DecodeUtf8("메모 B") }));
        const QString strMoved = KnowledgeMarkdownWriter::WriteDocument(middle.document, strMiddle);
        QVERIFY2(strMoved.contains(DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n- 둘째 카드 <!-- card:0a1b2c3d -->\n\n메모 A\n\n\n메모 B\n\n## 관계\n")),
                 qPrintable(strMoved));
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(middle.document, strMoved), strMoved);

        // 읽은 줄이 끼면 덩어리가 나뉜다(나뉜 덩어리는 빈 줄 없이 잇는다)
        QString strSplit = MakeWrittenCanonical();
        strSplit.replace(DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n"),
                         DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n\n메모 A\n\n- 둘째 카드 <!-- card:0a1b2c3d -->\n\n메모 B\n"));
        const DocumentReadResult split = KnowledgeMarkdownReader::ReadDocument(strSplit);
        const QString strSplitWritten = KnowledgeMarkdownWriter::WriteDocument(split.document, strSplit);
        QVERIFY2(strSplitWritten.contains(DecodeUtf8("- 둘째 카드 <!-- card:0a1b2c3d -->\n\n메모 A\n메모 B\n\n## 관계\n")),
                 qPrintable(strSplitWritten));
    }

    void headingWithoutTableSurvivesRewrite()
    {
        const QString strText = MakeWrittenCanonical() + DecodeUtf8("\n### 메모\n\n그냥 글\n");
        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(read.bOk);
        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(read.document, strText);
        QVERIFY(!strWritten.contains(DecodeUtf8("ocean:data x=\"\"")));
        QCOMPARE(strWritten.count(DecodeUtf8("<!-- ocean:data")), 1);
        QVERIFY(strWritten.endsWith(DecodeUtf8("\n### 메모\n\n그냥 글\n")));
        const DocumentReadResult again = KnowledgeMarkdownReader::ReadDocument(strWritten);
        QCOMPARE(again.hashUnreadLines.value(DecodeUtf8("데이터")), QStringList({ DecodeUtf8("### 메모"), DecodeUtf8("그냥 글") }));
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(again.document, strWritten), strWritten);
    }

    void missingSectionIsInsertedInOrder()
    {
        const KnowledgeDocument doc = ReadCanonicalRecord();
        const QString strFull = MakeWrittenCanonical();

        QString strNoRelations = strFull;
        const int iStart = strNoRelations.indexOf(DecodeUtf8("## 관계\n"));
        strNoRelations.remove(iStart, strNoRelations.indexOf(DecodeUtf8("## 데이터\n")) - iStart);
        QVERIFY(!strNoRelations.contains(DecodeUtf8("## 관계")));
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc, strNoRelations), strFull);

        // 지식 뒤에 모르는 절이 있어도 관계는 지식 범위 바로 뒤에 생긴다
        const QString strMemo = DecodeUtf8("## 내 메모\n\n글\n\n");
        QString strWithMemo = strNoRelations;
        strWithMemo.replace(DecodeUtf8("## 데이터\n"), strMemo + DecodeUtf8("## 데이터\n"));
        QString strExpected = strFull;
        strExpected.replace(DecodeUtf8("## 데이터\n"), strMemo + DecodeUtf8("## 데이터\n"));
        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(doc, strWithMemo);
        QCOMPARE(strWritten, strExpected);
        QVERIFY(strWritten.indexOf(DecodeUtf8("## 지식")) < strWritten.indexOf(DecodeUtf8("## 관계")));
        QVERIFY(strWritten.indexOf(DecodeUtf8("## 관계")) < strWritten.indexOf(DecodeUtf8("## 내 메모")));
        QVERIFY(strWritten.indexOf(DecodeUtf8("## 내 메모")) < strWritten.indexOf(DecodeUtf8("## 데이터")));

        // 정해진 절이 하나도 없으면 한 줄 뒤에, 한 줄도 없으면 제목 뒤에
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc, strFull.left(strFull.indexOf(DecodeUtf8("## 차례\n")))), strFull);
        const QString strTitleOnly = strFull.left(strFull.indexOf(DecodeUtf8("# YOLO9000"))) + DecodeUtf8("# YOLO9000: Better, Faster, Stronger\n");
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc, strTitleOnly), strFull);
    }

    void headerAndTitleAreCreatedWhenMissing()
    {
        const KnowledgeDocument doc = ReadCanonicalRecord();
        const QString strFull = MakeWrittenCanonical();
        const int iTitle = strFull.indexOf(DecodeUtf8("# YOLO9000"));
        const QString strHeader = strFull.left(iTitle);
        const QString strBody = strFull.mid(iTitle);

        // 머리글이 없는 글 → 맨 앞에 만든다
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc, strBody), strFull);
        // 제목도 없는 그냥 글 → 그 글은 그대로 두고 뒤에 제목부터 쓴다
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc, DecodeUtf8("그냥 적은 글\n")), strHeader + DecodeUtf8("그냥 적은 글\n\n") + strBody);
        // 모르는 절만 있는 글 → 제목부터는 그 절 앞에
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc, DecodeUtf8("## 내 메모\n\n글")), strFull + DecodeUtf8("\n## 내 메모\n\n글\n"));
        // 닫히지 않은 머리글은 본문으로 두고 새 머리글을 만든다
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc, DecodeUtf8("---\nid: x\n")), strHeader + DecodeUtf8("---\nid: x\n\n") + strBody);

        // 아는 키가 빠진 머리글: 있는 키는 제자리에서 값만, 모르는 키는 그대로, 빠진 키는 닫는 --- 앞에 정준 순서로
        QString strRest = strHeader.mid(strHeader.indexOf(QStringLiteral("format: 1\n")));
        strRest.remove(QRegularExpression(QStringLiteral("\nid: [^\n]*")));
        const QString strPartial = DecodeUtf8("---\nid: old\ntags: a\ntype: document\n---\n\n") + strBody;
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc, strPartial),
                 QStringLiteral("---\nid: ") + doc.strId + DecodeUtf8("\ntags: a\ntype: document\n") + strRest + strBody);
    }

    void emptiedSectionIsRemovedUnlessUnread()
    {
        const QString strFull = MakeWrittenCanonical();
        KnowledgeDocument doc = ReadCanonicalRecord();
        doc.vecTables.clear();
        const QString strExpected = ReplaceDigest(strFull.left(strFull.indexOf(DecodeUtf8("\n## 데이터\n"))), doc);
        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(doc, strFull);
        QCOMPARE(strWritten, strExpected);
        QVERIFY(!strWritten.contains(DecodeUtf8("## 데이터")));
        QCOMPARE(strWritten, KnowledgeMarkdownWriter::WriteDocument(doc));

        // 그 절에 읽지 못한 줄이 있었으면 머리 줄과 그 줄만 남는다
        QString strWithUnread = strFull;
        strWithUnread.replace(DecodeUtf8("## 데이터\n\n"), DecodeUtf8("## 데이터\n\n표 설명 메모\n\n"));
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc, strWithUnread), strExpected + DecodeUtf8("\n## 데이터\n\n표 설명 메모\n"));

        // 가운데 절이 비면 그 절과 뒤 빈 줄이 함께 빠진다
        KnowledgeDocument noNumbers = ReadCanonicalRecord();
        noNumbers.vecKeyNumbers.clear();
        QString strNoNumbers = strFull;
        strNoNumbers.remove(DecodeUtf8("## 핵심 숫자\n\n- **76.8** mAP — VOC 2007, 67 fps\n\n"));
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(noNumbers, strFull), ReplaceDigest(strNoNumbers, noNumbers));
    }

    void specialCharactersRoundTrip()
    {
        KnowledgeDocument doc = ReadCanonicalRecord();
        const ConceptLink odd { DecodeUtf8("a[1]\\b"), QStringLiteral("a1b") };
        doc.vecCards[0].vecConcepts.push_back(odd);
        doc.vecRelations[0].fromConcept = odd;
        doc.vecTables[0].vecRows[1][0] = DecodeUtf8("x | y");
        doc.vecTables[0].strCondition = DecodeUtf8("5 \"보통\" & 빠름 -->");
        doc.vecCards[0].strClaim = DecodeUtf8("첫 줄\n둘째 줄");
        doc.strMeta = DecodeUtf8("Redmon\nFarhadi");

        const QString strText = KnowledgeMarkdownWriter::WriteDocument(doc);
        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY2(read.bOk, qPrintable(read.strError));
        QVERIFY(read.hashUnreadLines.isEmpty());
        KnowledgeDocument expected = doc;
        expected.vecCards[0].strClaim = DecodeUtf8("첫 줄 둘째 줄");
        expected.strMeta = DecodeUtf8("Redmon Farhadi");
        expected.strDigest = KnowledgeMarkdownWriter::ComputeDigest(doc);
        QVERIFY(read.document == expected);
        QCOMPARE(KnowledgeMarkdownWriter::ComputeDigest(expected), expected.strDigest);
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(read.document, strText), strText);

        QCOMPARE(strText.count(QStringLiteral("-->")), 2);
        for (const QString& strLine : strText.split(QLatin1Char('\n')))
        {
            if (strLine.contains(QStringLiteral("-->")))
                QVERIFY2(strLine.endsWith(QStringLiteral(" -->")) && strLine.count(QStringLiteral("-->")) == 1, qPrintable(strLine));
        }
    }

    void digestSeesOwnedPartsOnly()
    {
        const KnowledgeDocument doc = ReadCanonicalRecord();
        const QString strDigest = KnowledgeMarkdownWriter::ComputeDigest(doc);

        KnowledgeDocument meta = doc;
        meta.strMeta = DecodeUtf8("다른 메타");
        QCOMPARE(KnowledgeMarkdownWriter::ComputeDigest(meta), strDigest);

        KnowledgeDocument evidence = doc;
        evidence.vecCards[0].strEvidence = DecodeUtf8("다른 근거");
        QVERIFY(KnowledgeMarkdownWriter::ComputeDigest(evidence) != strDigest);

        // 정해진 부분 밖을 고친 글을 읽어도 digest 는 그대로
        const QString strEdited = MakeWrittenCanonical() + DecodeUtf8("\n## 내 메모\n\n글\n");
        QCOMPARE(KnowledgeMarkdownWriter::ComputeDigest(KnowledgeMarkdownReader::ReadDocument(strEdited).document), strDigest);
    }

    void conceptWriteKeepsHandRelations()
    {
        const QString strRelations = DecodeUtf8("\n## 관계\n\n- [YOLOv2](yolov2.md) **isA** → [1단계 검출기](1단계-검출기.md)\n"
                                       "\t- 설명:   손으로 적음\n");
        const QString strText = KnowledgeTestData::CanonicalConcept() + strRelations;
        ConceptReadResult read = KnowledgeMarkdownReader::ReadConcept(strText);
        QVERIFY(read.bOk);
        QCOMPARE(read.concept.vecRelations.size(), 1);
        read.concept.vecAliases << QStringLiteral("YOLO v2");

        QString strExpected = strText;
        strExpected.replace(QStringLiteral("aliases: YOLO9000\n"), QStringLiteral("aliases: YOLO9000, YOLO v2\n"));
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(read.concept, strText), strExpected);

        // 새로 쓸 때는 기록에 관계가 있어도 ## 관계 를 만들지 않는다
        QVERIFY(!KnowledgeMarkdownWriter::WriteConcept(read.concept).contains(DecodeUtf8("## 관계")));
    }

    void emptyOptionalKeysAreOmitted()
    {
        KnowledgeDocument doc = ReadCanonicalRecord();
        doc.strSource.clear();
        const QString strText = KnowledgeMarkdownWriter::WriteDocument(doc);
        QVERIFY(!strText.contains(QStringLiteral("\nsource:")));
        QVERIFY(strText.contains(QStringLiteral("\nfileInfo: ")));
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc, MakeWrittenCanonical()), strText);

        KnowledgeConcept concept = KnowledgeMarkdownReader::ReadConcept(KnowledgeTestData::CanonicalConcept()).concept;
        concept.strYear.clear();
        const QString strConcept = KnowledgeMarkdownWriter::WriteConcept(concept);
        QVERIFY(!strConcept.contains(QStringLiteral("year:")));
        QVERIFY(strConcept.contains(QStringLiteral("\naliases: YOLO9000\n")));
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(concept, KnowledgeTestData::CanonicalConcept()), strConcept);

        KnowledgeDocument bare;
        bare.strId = QStringLiteral("x");
        const QString strBare = KnowledgeMarkdownWriter::WriteDocument(bare);
        QCOMPARE(strBare.left(strBare.indexOf(QStringLiteral("\n---\n")) + 5),
                 QStringLiteral("---\ntype: document\nformat: 1\nid: x\nrules: 0\nedited: false\ndigest: ")
                     + KnowledgeMarkdownWriter::ComputeDigest(bare) + QStringLiteral("\n---\n"));

        KnowledgeConcept bareConcept;
        bareConcept.strId = QStringLiteral("c");
        const QString strBareConcept = KnowledgeMarkdownWriter::WriteConcept(bareConcept);
        QVERIFY(strBareConcept.startsWith(QStringLiteral("---\ntype: concept\nformat: 1\nid: c\n---\n")));
    }

    void urlSubItemSurvivesRewrite()
    {
        QString strText = MakeWrittenCanonical();
        strText.replace(DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n"),
                        DecodeUtf8("  - 개념: [YOLOv2](../concepts/yolov2.md)\n"
                          "  - https://arxiv.org/abs/1612.08242\n"
                          "  - [paper](https://x)\n"));
        strText.replace(DecodeUtf8("  - 상태: AI 제안\n"), DecodeUtf8("  - 상태: AI 제안\n  - 메모:붙여 씀\n  - 출처:  두 칸  띄움\n"));
        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(read.bOk);
        QVERIFY(read.hashUnreadLines.isEmpty());
        QCOMPARE(read.document.vecCards[0].vecExtraLines,
                 QStringList({ QStringLiteral("https://arxiv.org/abs/1612.08242"), QStringLiteral("[paper](https://x)") }));
        QCOMPARE(read.document.vecRelations[0].vecExtraLines, QStringList({ DecodeUtf8("메모:붙여 씀"), DecodeUtf8("출처:  두 칸  띄움") }));
        QCOMPARE(read.document.vecCards[0].strEvidence, DecodeUtf8("VOC 2007 mAP 63.4% → 76.8%, 67 fps"));

        // 다시 써도 모르는 하위 항목은 글자 하나 바뀌지 않는다(digest 만 새 기록의 것)
        const QString strExpected = ReplaceDigest(strText, read.document);
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(read.document, strText), strExpected);
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(read.document), strExpected);
    }

    void obsidianAliasListStaysBlockOnRewrite()
    {
        QString strText = KnowledgeTestData::CanonicalConcept();
        strText.replace(QStringLiteral("aliases: YOLO9000\n"),
                        QStringLiteral("aliases:\n  - YOLO9000\n  - YOLO v2\ntags:\n  - detector\n"));
        const ConceptReadResult read = KnowledgeMarkdownReader::ReadConcept(strText);
        QVERIFY(read.bOk);
        QCOMPARE(read.concept.vecAliases, QStringList({ "YOLO9000", "YOLO v2" }));
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(read.concept, strText), strText);

        // 별칭이 늘면 블록 목록에 항목 줄이 는다. 남는 줄(고아 항목)이 없다.
        KnowledgeConcept concept = read.concept;
        concept.vecAliases << QStringLiteral("YOLO 544") << QStringLiteral("*star");
        QString strExpected = strText;
        strExpected.replace(QStringLiteral("  - YOLO v2\n"), QStringLiteral("  - YOLO v2\n  - YOLO 544\n  - \"*star\"\n"));
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(concept, strText), strExpected);
        QCOMPARE(KnowledgeMarkdownReader::ReadConcept(strExpected).concept.vecAliases,
                 QStringList({ "YOLO9000", "YOLO v2", "YOLO 544", "*star" }));

        // 줄어도 블록 전체가 바뀐다
        concept.vecAliases = QStringList({ "YOLO9000" });
        strExpected = strText;
        strExpected.replace(QStringLiteral("  - YOLO v2\n"), QString());
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(concept, strText), strExpected);

        // 비면 키와 항목 줄이 함께 빠진다. 모르는 키의 블록 목록은 그대로다.
        concept.vecAliases.clear();
        strExpected = strText;
        strExpected.replace(QStringLiteral("aliases:\n  - YOLO9000\n  - YOLO v2\n"), QString());
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(concept, strText), strExpected);

        // 들여 쓰지 않은 블록 목록은 그 들여쓰기로
        QString strFlush = KnowledgeTestData::CanonicalConcept();
        strFlush.replace(QStringLiteral("aliases: YOLO9000\n"), QStringLiteral("aliases:\n- YOLO9000\n"));
        concept = KnowledgeMarkdownReader::ReadConcept(strFlush).concept;
        concept.vecAliases << QStringLiteral("YOLO v2");
        strExpected = strFlush;
        strExpected.replace(QStringLiteral("- YOLO9000\n"), QStringLiteral("- YOLO9000\n- YOLO v2\n"));
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(concept, strFlush), strExpected);

        // 흐름 목록은 흐름 목록으로
        QString strFlow = KnowledgeTestData::CanonicalConcept();
        strFlow.replace(QStringLiteral("aliases: YOLO9000\n"), QStringLiteral("aliases: [YOLO9000]\n"));
        concept = KnowledgeMarkdownReader::ReadConcept(strFlow).concept;
        QCOMPARE(concept.vecAliases, QStringList({ "YOLO9000" }));
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(concept, strFlow), strFlow);
        concept.vecAliases << QStringLiteral("YOLO, v2") << QStringLiteral("v3");
        strExpected = strFlow;
        strExpected.replace(QStringLiteral("aliases: [YOLO9000]\n"), QStringLiteral("aliases: [YOLO9000, \"YOLO, v2\", v3]\n"));
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(concept, strFlow), strExpected);
        QCOMPARE(KnowledgeMarkdownReader::ReadConcept(strExpected).concept.vecAliases, concept.vecAliases);
    }

    void scalarThatLooksLikeListIsKept()
    {
        QString strText = MakeWrittenCanonical();
        strText.replace(DecodeUtf8("meta: Redmon · Farhadi · CVPR 2017"), DecodeUtf8("meta: [초안]"));
        const KnowledgeDocument doc = KnowledgeMarkdownReader::ReadDocument(strText).document;
        QCOMPARE(doc.strMeta, DecodeUtf8("[초안]"));
        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(doc, strText);
        QVERIFY2(strWritten.contains(DecodeUtf8("\nmeta: \"[초안]\"\n")), qPrintable(strWritten));
        QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strWritten).document.strMeta, DecodeUtf8("[초안]"));

        // 목록으로 읽는 키는 aliases 뿐
        QString strConcept = KnowledgeTestData::CanonicalConcept();
        strConcept.replace(QStringLiteral("aliases: YOLO9000\n"), QStringLiteral("aliases: [a, b]\n"));
        const KnowledgeConcept concept = KnowledgeMarkdownReader::ReadConcept(strConcept).concept;
        QCOMPARE(concept.vecAliases, QStringList({ "a", "b" }));
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(concept, strConcept), strConcept);
    }

    void unsafeValuesAreQuoted()
    {
        KnowledgeDocument doc = ReadCanonicalRecord();
        doc.strSource = QStringLiteral("notes: draft.md");
        doc.strFileInfo = DecodeUtf8("[초안] 3쪽");
        doc.strMeta = DecodeUtf8("#1 논문 · \"보통\" \\ 끝");
        doc.strAnalysisId = QStringLiteral("a #b");
        doc.strLearned = QStringLiteral("@today");
        const QString strText = KnowledgeMarkdownWriter::WriteDocument(doc);
        QVERIFY2(strText.contains(QStringLiteral("\nsource: \"notes: draft.md\"\n")), qPrintable(strText));
        QVERIFY(strText.contains(DecodeUtf8("\nfileInfo: \"[초안] 3쪽\"\n")));
        QVERIFY(strText.contains(DecodeUtf8("\nmeta: \"#1 논문 · \\\"보통\\\" \\\\ 끝\"\n")));
        QVERIFY(strText.contains(QStringLiteral("\nanalysis: \"a #b\"\n")));
        QVERIFY(strText.contains(QStringLiteral("\nlearned: \"@today\"\n")));
        QVERIFY(strText.contains(QStringLiteral("\nid: 3f1c2a9e-5b7d-4c8e-9a21-6d0e4b8f7c31\n")));

        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY2(read.bOk, qPrintable(read.strError));
        KnowledgeDocument expected = doc;
        expected.strDigest = KnowledgeMarkdownWriter::ComputeDigest(doc);
        QVERIFY(read.document == expected);
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(read.document, strText), strText);

        // 안전한 값은 따옴표 없이(가운데의 ':' · '#' · 따옴표는 괜찮다)
        doc = ReadCanonicalRecord();
        doc.strSource = QStringLiteral("a:b c#d \"e\".pdf");
        QVERIFY(KnowledgeMarkdownWriter::WriteDocument(doc).contains(QStringLiteral("\nsource: a:b c#d \"e\".pdf\n")));

        // 첫 글자마다
        for (const QString& strStart : { "-", "?", ":", ",", "[", "]", "{", "}", "#", "&", "*", "!", "|", ">", "'", "\"", "%", "@", "`" })
        {
            doc.strSource = strStart + QStringLiteral("x");
            const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(doc);
            QVERIFY2(strWritten.contains(QStringLiteral("\nsource: \"")), qPrintable(strStart));
            QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strWritten).document.strSource, doc.strSource);
        }

        // 별칭: 머리글 한 줄로 쓸 때도 따옴표
        KnowledgeConcept concept = KnowledgeMarkdownReader::ReadConcept(KnowledgeTestData::CanonicalConcept()).concept;
        concept.vecAliases = QStringList({ "*star", "YOLO: v2" });
        const QString strConcept = KnowledgeMarkdownWriter::WriteConcept(concept);
        QVERIFY2(strConcept.contains(QStringLiteral("\naliases: \"*star, YOLO: v2\"\n")), qPrintable(strConcept));
        QCOMPARE(KnowledgeMarkdownReader::ReadConcept(strConcept).concept, concept);

        // 있는 파일에서 따옴표로 감싼 값은 그 따옴표 모양을 지킨다
        QString strQuoted = KnowledgeTestData::CanonicalConcept();
        strQuoted.replace(QStringLiteral("kind: method"), QStringLiteral("kind: 'method'"));
        strQuoted.replace(QStringLiteral("year: 2017"), QStringLiteral("year: \"2017\""));
        const KnowledgeConcept quoted = KnowledgeMarkdownReader::ReadConcept(strQuoted).concept;
        QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(quoted, strQuoted), strQuoted);
    }

    void summaryStartingWithBlockMarkerRoundTrips()
    {
        // 한 줄 · 정의가 블록을 여는 글자로 시작하면 GFM 이스케이프 \ 하나를 붙여 쓰고, 읽을 때 그 \ 하나만 뺀다
        const QVector<QPair<QString, QString>> vecCases {
            { DecodeUtf8("<모델> 은 입력을 받는다"), DecodeUtf8("\\<모델> 은 입력을 받는다") },
            { DecodeUtf8("> 비교: 더 빠르다"), DecodeUtf8("\\> 비교: 더 빠르다") },
            { DecodeUtf8("- 단계를 나눈다"), DecodeUtf8("\\- 단계를 나눈다") },
            { DecodeUtf8("* 별표"), DecodeUtf8("\\* 별표") },
            { DecodeUtf8("+ 더하기"), DecodeUtf8("\\+ 더하기") },
            { DecodeUtf8("1. 단계 하나"), DecodeUtf8("1\\. 단계 하나") },
            { DecodeUtf8("12) 둘째"), DecodeUtf8("12\\) 둘째") },
            { DecodeUtf8("---x 로 시작"), DecodeUtf8("---x 로 시작") },   // 가로줄도 목록도 아니다
            { DecodeUtf8("---"), DecodeUtf8("\\---") },
            { DecodeUtf8("* * *"), DecodeUtf8("\\* * *") },
            { DecodeUtf8("___"), DecodeUtf8("\\___") },
            { DecodeUtf8("# 제목처럼"), DecodeUtf8("\\# 제목처럼") },
            { DecodeUtf8("## 절처럼"), DecodeUtf8("\\## 절처럼") },
            { DecodeUtf8("``` 울타리처럼"), DecodeUtf8("\\``` 울타리처럼") },
            { DecodeUtf8("~~~ 물결"), DecodeUtf8("\\~~~ 물결") },
            { DecodeUtf8("| 표처럼 |"), DecodeUtf8("\\| 표처럼 |") },
            { DecodeUtf8("![그림](a.png) 처럼"), DecodeUtf8("\\![그림](a.png) 처럼") },
            { DecodeUtf8("<!-- 주석처럼 --> 글"), DecodeUtf8("\\<!-- 주석처럼 --> 글") },
            // 이미 \ 로 시작해 읽을 때 빠질 글은 \ 를 하나 더 붙인다
            { DecodeUtf8("\\- 손으로 쓴 이스케이프"), DecodeUtf8("\\\\- 손으로 쓴 이스케이프") },
            { DecodeUtf8("1\\. 손으로 쓴 이스케이프"), DecodeUtf8("1\\\\. 손으로 쓴 이스케이프") },
            { DecodeUtf8("\\\\# 두 번"), DecodeUtf8("\\\\\\# 두 번") },
            // 블록을 열지 않는 글은 그대로
            { DecodeUtf8("\\*별표\\* 강조 아님"), DecodeUtf8("\\*별표\\* 강조 아님") },
            { DecodeUtf8("1.5배 빠르다"), DecodeUtf8("1.5배 빠르다") },
            { DecodeUtf8("!중요"), DecodeUtf8("!중요") },
            { DecodeUtf8("`코드` 로 시작"), DecodeUtf8("`코드` 로 시작") },
        };
        for (const QPair<QString, QString>& pairCase : vecCases)
        {
            const QString& strSummary = pairCase.first;
            const QString& strLine = pairCase.second;

            KnowledgeDocument doc = ReadCanonicalRecord();
            doc.strSummary = strSummary;
            const QString strText = KnowledgeMarkdownWriter::WriteDocument(doc);
            QVERIFY2(strText.contains(DecodeUtf8("\n\n") + strLine + DecodeUtf8("\n\n## 차례\n")), qPrintable(strSummary));
            QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc, MakeWrittenCanonical()), strText);

            const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
            QVERIFY2(read.bOk, qPrintable(read.strError));
            QCOMPARE(read.document.strSummary, strSummary);
            QCOMPARE(KnowledgeMarkdownWriter::ComputeDigest(read.document), read.document.strDigest);
            KnowledgeDocument expected = doc;
            expected.strDigest = KnowledgeMarkdownWriter::ComputeDigest(doc);
            QVERIFY2(read.document == expected, qPrintable(strSummary));

            // 다시 저장해도 글자 하나 바뀌지 않고, 한 줄이 하나 더 붙지 않는다
            const QString strSecond = KnowledgeMarkdownWriter::WriteDocument(read.document, strText);
            QCOMPARE(strSecond, strText);
            QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(KnowledgeMarkdownReader::ReadDocument(strSecond).document, strSecond), strText);

            // 개념 정의도 같다
            KnowledgeConcept concept = KnowledgeMarkdownReader::ReadConcept(KnowledgeTestData::CanonicalConcept()).concept;
            concept.strSummary = strSummary;
            const QString strConcept = KnowledgeMarkdownWriter::WriteConcept(concept);
            QVERIFY2(strConcept.endsWith(DecodeUtf8("\n\n") + strLine + DecodeUtf8("\n")), qPrintable(strConcept));
            QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(concept, KnowledgeTestData::CanonicalConcept()), strConcept);
            const KnowledgeConcept readConcept = KnowledgeMarkdownReader::ReadConcept(strConcept).concept;
            QCOMPARE(readConcept.strSummary, strSummary);
            QVERIFY2(readConcept == concept, qPrintable(strSummary));
            QCOMPARE(KnowledgeMarkdownWriter::WriteConcept(readConcept, strConcept), strConcept);
        }
    }
};

std::unique_ptr<QObject> CreateKnowledgeMarkdownWriterTest() { return std::make_unique<KnowledgeMarkdownWriterTest>(); }

#include "KnowledgeMarkdownWriterTest.moc"
