#include "VisualMarkdown/KnowledgeMarkdownLayout.h"
#include "VisualMarkdown/KnowledgeMarkdownReader.h"
#include "VisualMarkdown/KnowledgeMarkdownWriter.h"
#include "VisualMarkdown/KnowledgeText.h"
#include "VisualMarkdown/KnowledgeVocabulary.h"
#include "KnowledgeTestData.h"

#include <QTest>

#include <memory>

namespace
{
QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

const char* const kCanonicalCard =
    "- 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올렸다 <!-- card:7f3a2c1e -->\n"
    "  - 근거: VOC 2007 mAP 63.4% → 76.8%, 67 fps\n"
    "  - 절: 2\n"
    "  - 개념: [YOLOv2](../concepts/yolov2.md)\n";

KnowledgeDocument ReadCanonicalRecord()
{
    return KnowledgeMarkdownReader::ReadDocument(KnowledgeTestData::CanonicalDocument()).document;
}

// 정준 글의 카드 줄(## 지식 절 본문)을 cards 로 바꾼 글
QString ReplaceCards(const QString& cards)
{
    QString strText = KnowledgeTestData::CanonicalDocument();
    const QString strCard = DecodeUtf8(kCanonicalCard);
    if (!strText.contains(strCard))
        return QString();
    strText.replace(strCard, cards);
    return strText;
}

// 문단 둘 · 목록(들여 쓴 목록 포함) · $$ 수식이 든 근거
QString MakeRichEvidence()
{
    return DecodeUtf8("VOC 2007 mAP 63.4% → 76.8%, 67 fps\n"
                      "배치 정규화만으로 2%p 넘게 오른다.\n"
                      "\n"
                      "둘째 문단은 빈 줄 뒤에 온다.\n"
                      "\n"
                      "- 목록 하나\n"
                      "- 목록 둘\n"
                      "  - 들여 쓴 목록\n"
                      "\n"
                      "$$\n"
                      "\\mathrm{mAP} = \\frac{1}{N} \\sum_{i=1}^{N} \\mathrm{AP}_i\n"
                      "$$");
}

const char* const kRichEvidenceLines =
    "  - 근거: VOC 2007 mAP 63.4% → 76.8%, 67 fps\n"
    "    배치 정규화만으로 2%p 넘게 오른다.\n"
    "\n"
    "    둘째 문단은 빈 줄 뒤에 온다.\n"
    "\n"
    "    - 목록 하나\n"
    "    - 목록 둘\n"
    "      - 들여 쓴 목록\n"
    "\n"
    "    $$\n"
    "    \\mathrm{mAP} = \\frac{1}{N} \\sum_{i=1}^{N} \\mathrm{AP}_i\n"
    "    $$\n";

// 쓰고 다시 읽으면 기록이 그대로(digest · 판은 쓴 값)이고, 다시 써도 바이트가 같은지 본다. 쓴 글을 돌려준다.
QString VerifyRoundTrip(const KnowledgeDocument& document, int expectedFormat)
{
    const QString strText = KnowledgeMarkdownWriter::WriteDocument(document);
    const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
    if (!read.bOk || !read.hashUnreadLines.isEmpty())
        return QString();
    KnowledgeDocument expected = document;
    expected.iFormat = expectedFormat;
    expected.strDigest = KnowledgeMarkdownWriter::ComputeDigest(document);
    if (read.document != expected || KnowledgeMarkdownWriter::ComputeDigest(read.document) != read.document.strDigest)
        return QString();
    if (KnowledgeMarkdownWriter::WriteDocument(read.document, strText) != strText
        || KnowledgeMarkdownWriter::WriteDocument(read.document) != strText)
        return QString();
    return strText;
}
}

// 카드 근거의 여러 줄 마크다운(format/DocumentFile.md §5.1): 쓰기 · 읽기 · 정리 · 판
class KnowledgeEvidenceTest : public QObject
{
    Q_OBJECT

private slots:
    void canonicalCardIsInTestData()
    {
        QVERIFY(!ReplaceCards(DecodeUtf8(kCanonicalCard)).isEmpty());
    }

    // 둘째 줄부터는 하위 항목의 내용 들여쓰기(공백 네 칸)로, 빈 줄은 공백 없이 쓴다. 다시 읽으면 글자 그대로다.
    void multilineEvidenceIsWrittenIndented()
    {
        KnowledgeDocument doc = ReadCanonicalRecord();
        doc.vecCards[0].strEvidence = MakeRichEvidence();
        doc.vecCards[0].vecExtraLines = QStringList({ DecodeUtf8("메모: 다시 볼 것") });

        const QString strText = VerifyRoundTrip(doc, 2);
        QVERIFY2(!strText.isEmpty(), qUtf8Printable(KnowledgeMarkdownWriter::WriteDocument(doc)));
        const QString strCard = DecodeUtf8("- 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올렸다 <!-- card:7f3a2c1e -->\n")
            + DecodeUtf8(kRichEvidenceLines)
            + DecodeUtf8("  - 절: 2\n  - 개념: [YOLOv2](../concepts/yolov2.md)\n  - 메모: 다시 볼 것\n\n## 관계\n");
        QVERIFY2(strText.contains(strCard), qUtf8Printable(strText));
        QVERIFY2(strText.contains(QStringLiteral("\nformat: 2\n")), qUtf8Printable(strText));
        QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strText).document.vecCards[0].strEvidence, MakeRichEvidence());

        // 줄 끝 공백 · 공백만 있는 줄이 없다
        for (const QString& strLine : strText.split(QLatin1Char('\n')))
            QVERIFY2(strLine == strLine.trimmed() || !strLine.endsWith(QLatin1Char(' ')), qUtf8Printable(strLine));
        QVERIFY(!strText.contains(QStringLiteral(" \n")));

        // 있는 글(한 줄 근거)에 갈아 끼워도 같은 글이 된다
        const QString strCanonical = KnowledgeTestData::CanonicalDocument();
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc, strCanonical), strText);
    }

    // 한 줄 근거는 지금처럼 쓰고(이어진 공백은 하나로), 판은 1 이다
    void singleLineEvidenceIsUnchanged()
    {
        KnowledgeDocument doc = ReadCanonicalRecord();
        doc.vecCards[0].strEvidence = DecodeUtf8("  앞뒤   공백과\t탭  \n\n");
        KnowledgeDocument expected = doc;
        expected.vecCards[0].strEvidence = DecodeUtf8("앞뒤 공백과 탭");
        const QString strText = VerifyRoundTrip(expected, 1);
        QVERIFY(!strText.isEmpty());
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(doc), strText);
        QVERIFY(strText.contains(DecodeUtf8("\n  - 근거: 앞뒤 공백과 탭\n  - 절: 2\n")));
        QVERIFY(strText.contains(QStringLiteral("\nformat: 1\n")));
    }

    // 정리: 줄바꿈은 \n 로, 줄 끝 공백 · 앞뒤 빈 줄은 빼고, 첫 줄은 이어진 공백을 하나로, 둘째 줄부터는 들여쓰기 · 안쪽 공백을 지킨다.
    // 닫히지 않은 울타리는 닫는 줄을 더한다. 정리는 두 번 해도 같다.
    void evidenceIsNormalized()
    {
        const QVector<QPair<QString, QString>> vecCases {
            { DecodeUtf8("  한   줄  "), DecodeUtf8("한 줄") },
            { DecodeUtf8("한 줄\n   \n\t\n"), DecodeUtf8("한 줄") },
            { DecodeUtf8("\n\n한 줄"), DecodeUtf8("한 줄") },
            { DecodeUtf8("  \n \t \n"), QString() },
            { QString(), QString() },
            { DecodeUtf8("첫 줄\r\n둘째 줄\r셋째 줄"), DecodeUtf8("첫 줄\n둘째 줄\n셋째 줄") },
            { DecodeUtf8("\n  첫   줄  \n둘째  줄\t \n\t\n    들여 쓴 줄\n\n\n"), DecodeUtf8("첫 줄\n둘째  줄\n\n    들여 쓴 줄") },
            { DecodeUtf8("첫 줄\n\n\n\n둘째 줄"), DecodeUtf8("첫 줄\n\n\n\n둘째 줄") },
            { DecodeUtf8("설명\n```python\nx = 1"), DecodeUtf8("설명\n```python\nx = 1\n```") },
            { DecodeUtf8("설명\n  ~~~\n  y\n"), DecodeUtf8("설명\n  ~~~\n  y\n  ~~~") },
            { DecodeUtf8("설명\n```\nx\n```"), DecodeUtf8("설명\n```\nx\n```") },
            { DecodeUtf8("설명\n```\n~~~\n```"), DecodeUtf8("설명\n```\n~~~\n```") },
            // 첫 줄이 울타리면 첫 줄도 이어지는 줄처럼 다룬다(들여쓰기를 지키고 닫는지 본다)
            { DecodeUtf8("```\nx  = 1\n```"), DecodeUtf8("```\nx  = 1\n```") },
            { DecodeUtf8("  ```\n  x"), DecodeUtf8("  ```\n  x\n  ```") },
            // 첫 줄의 ``` 는 "근거: " 뒤라 울타리가 아니다(둘째 줄의 ``` 가 여는 줄이다)
            { DecodeUtf8("코드는 ``` 로 감싼다\n```\nx"), DecodeUtf8("코드는 ``` 로 감싼다\n```\nx\n```") },
        };
        for (const QPair<QString, QString>& pairCase : vecCases)
        {
            const QString strNormalized = KnowledgeText::NormalizeEvidence(pairCase.first);
            QVERIFY2(strNormalized == pairCase.second, qUtf8Printable(pairCase.first + QStringLiteral(" => ") + strNormalized));
            QCOMPARE(KnowledgeText::NormalizeEvidence(strNormalized), strNormalized);

            // 쓰고 읽으면 정리한 글이 된다
            KnowledgeDocument doc = ReadCanonicalRecord();
            doc.vecCards[0].strEvidence = pairCase.first;
            const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(KnowledgeMarkdownWriter::WriteDocument(doc));
            QVERIFY(read.bOk);
            QVERIFY2(read.hashUnreadLines.isEmpty(), qUtf8Printable(pairCase.first));
            QCOMPARE(read.document.vecCards[0].strEvidence, pairCase.second);
            QCOMPARE(read.document.vecRelations.size(), 1);
            QCOMPARE(read.document.vecTables.size(), 1);
        }
    }

    // 손으로 쓴 글: `- 근거:` 줄의 들여쓰기 + 2 칸 이상 들인 줄이 이어지는 줄이다(그만큼만 떼고 남은 들여쓰기는 지킨다).
    // 빈 줄은 다음 비지 않은 줄이 이어지는 줄일 때만 근거 안의 빈 줄이고, 아니면 지금처럼 항목을 끝낸다.
    void continuationLinesFollowIndent()
    {
        const QString strCards = DecodeUtf8(
            "- 카드 하나 <!-- card:7f3a2c1e -->\n"
            "  - 근거: 첫 줄\n"
            "    둘째 줄\n"
            "\n"
            "    셋째 문단\n"
            "        네 칸 더 들인 줄\n"
            "    - 근거 안의 목록\n"
            "  - 절: 2\n"
            "  - 개념: [YOLOv2](../concepts/yolov2.md)\n"
            "- 카드 둘 <!-- card:0a1b2c3d -->\n"
            "  - 근거: 첫 줄\n"
            "   셋 칸만 들인 줄\n"
            "  - 절: 3\n"
            "- 카드 셋 <!-- card:1b2c3d4e -->\n"
            "  - 근거: 첫 줄\n"
            "\n"
            "  - 절: 9\n"
            "- 카드 넷 <!-- card:2c3d4e5f -->\n"
            "  - 근거: 첫 줄\n"
            "    둘째 줄\n"
            "\n"
            "- 카드 다섯 <!-- card:3d4e5f60 -->\n"
            "  - 근거:   첫  줄   공백  \n"
            "    둘째  줄  공백\n"
            "  - 다시 볼 것\n");
        const QString strText = ReplaceCards(strCards);
        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY2(read.bOk, qPrintable(read.strError));
        const QVector<KnowledgeCard>& vecCards = read.document.vecCards;
        QCOMPARE(vecCards.size(), 5);
        QCOMPARE(vecCards[0].strEvidence, DecodeUtf8("첫 줄\n둘째 줄\n\n셋째 문단\n    네 칸 더 들인 줄\n- 근거 안의 목록"));
        QCOMPARE(vecCards[0].strSection, QString("2"));
        QCOMPARE(vecCards[0].vecConcepts.size(), 1);
        QVERIFY(vecCards[0].vecExtraLines.isEmpty());
        QCOMPARE(vecCards[1].strEvidence, DecodeUtf8("첫 줄"));
        QCOMPARE(vecCards[1].strSection, QString("3"));
        QCOMPARE(vecCards[2].strEvidence, DecodeUtf8("첫 줄"));
        QVERIFY(vecCards[2].strSection.isEmpty());
        QCOMPARE(vecCards[3].strEvidence, DecodeUtf8("첫 줄\n둘째 줄"));
        QCOMPARE(vecCards[4].strEvidence, DecodeUtf8("첫 줄 공백\n둘째  줄  공백"));
        QCOMPARE(vecCards[4].vecExtraLines, QStringList({ DecodeUtf8("다시 볼 것") }));
        QCOMPARE(read.hashUnreadLines.value(DecodeUtf8("지식")),
                 QStringList({ DecodeUtf8("   셋 칸만 들인 줄"), DecodeUtf8("  - 절: 9") }));
        QCOMPARE(read.hashUnreadLines.size(), 1);

        // 다시 쓰면 정준 모양(네 칸)이 되고, 다시 읽어도 기록과 읽지 못한 줄이 그대로다
        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(read.document, strText);
        QVERIFY2(strWritten.contains(DecodeUtf8("  - 근거: 첫 줄\n    둘째 줄\n\n    셋째 문단\n        네 칸 더 들인 줄\n    - 근거 안의 목록\n  - 절: 2\n")),
                 qUtf8Printable(strWritten));
        QVERIFY2(strWritten.contains(DecodeUtf8("  - 근거: 첫 줄 공백\n    둘째  줄  공백\n  - 다시 볼 것\n")), qUtf8Printable(strWritten));
        const DocumentReadResult reread = KnowledgeMarkdownReader::ReadDocument(strWritten);
        QVERIFY(reread.document.vecCards == vecCards);
        QCOMPARE(reread.hashUnreadLines, read.hashUnreadLines);
        QCOMPARE(KnowledgeMarkdownWriter::WriteDocument(reread.document, strWritten), strWritten);
    }

    // 빈 줄 뒤에 떠돈 근거(읽지 못한 줄)의 이어지는 줄도 읽지 못한 줄이다. 다시 써도 앞 카드에 붙지 않는다.
    void strayEvidenceKeepsItsLines()
    {
        const QString strCards = DecodeUtf8(
            "- 카드 <!-- card:7f3a2c1e -->\n"
            "  - 절: 2\n"
            "\n"
            "  - 근거: 떠돈 근거\n"
            "    떠돈 둘째 줄\n");
        const QString strText = ReplaceCards(strCards);
        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(read.bOk);
        QCOMPARE(read.document.vecCards.size(), 1);
        QVERIFY(read.document.vecCards[0].strEvidence.isEmpty());
        QCOMPARE(read.hashUnreadLines.value(DecodeUtf8("지식")),
                 QStringList({ DecodeUtf8("  - 근거: 떠돈 근거"), DecodeUtf8("    떠돈 둘째 줄") }));
        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(read.document, strText);
        const DocumentReadResult reread = KnowledgeMarkdownReader::ReadDocument(strWritten);
        QVERIFY(reread.document.vecCards == read.document.vecCards);
        QCOMPARE(reread.hashUnreadLines, read.hashUnreadLines);
    }

    // 탭은 CommonMark 처럼 다음 4의 배수 칸까지 센다. 뗄 칸이 탭 가운데에 걸리면 남는 칸은 공백으로 둔다.
    void tabIndentCountsToNextTabStop()
    {
        // `\t- 근거:` 는 4칸 들인 하위 항목이라 이어지는 줄은 6칸 이상이다
        const DocumentReadResult tabbed = KnowledgeMarkdownReader::ReadDocument(ReplaceCards(DecodeUtf8(
            "- 카드 <!-- card:7f3a2c1e -->\n"
            "\t- 근거: 첫 줄\n"
            "\t  둘째 줄\n"
            "\t\t- 탭 둘\n"
            "    네 칸\n"
            "\t- 절: 2\n")));
        QVERIFY(tabbed.bOk);
        QCOMPARE(tabbed.document.vecCards[0].strEvidence, DecodeUtf8("첫 줄\n둘째 줄\n  - 탭 둘"));
        QCOMPARE(tabbed.document.vecCards[0].strSection, QString("2"));
        QCOMPARE(tabbed.hashUnreadLines.value(DecodeUtf8("지식")), QStringList({ DecodeUtf8("    네 칸") }));

        // 공백 두 칸 하위 항목 뒤의 탭 하나(4칸)는 이어지는 줄이다. `\t- 절:` 도 근거 안의 목록이다(Obsidian 에서도 근거 아래로 보인다)
        const DocumentReadResult mixed = KnowledgeMarkdownReader::ReadDocument(ReplaceCards(DecodeUtf8(
            "- 카드 <!-- card:7f3a2c1e -->\n"
            "  - 근거: 첫 줄\n"
            "\t둘째 줄\n"
            "\t- 절: 2\n"
            "  - 절: 3\n")));
        QVERIFY(mixed.bOk);
        QCOMPARE(mixed.document.vecCards[0].strEvidence, DecodeUtf8("첫 줄\n둘째 줄\n- 절: 2"));
        QCOMPARE(mixed.document.vecCards[0].strSection, QString("3"));
        QVERIFY(mixed.hashUnreadLines.isEmpty());

        // 탭으로 들인 정준 글(모든 하위 항목 · 이어지는 줄 앞 두 칸을 탭으로)은 공백 글과 같은 기록이다
        KnowledgeDocument doc = ReadCanonicalRecord();
        doc.vecCards[0].strEvidence = MakeRichEvidence();
        QString strTabs = KnowledgeMarkdownWriter::WriteDocument(doc);
        strTabs.replace(QStringLiteral("\n  "), QStringLiteral("\n\t"));
        const DocumentReadResult tabs = KnowledgeMarkdownReader::ReadDocument(strTabs);
        QVERIFY(tabs.hashUnreadLines.isEmpty());
        QCOMPARE(tabs.document.vecCards[0].strEvidence, MakeRichEvidence());
    }

    // 코드 블록: 들여 쓴 코드 블록 · 울타리 코드 블록(빈 줄 · 더 들인 줄 · 안쪽 공백)이 쓰고 읽으면 글자 그대로다
    void codeBlocksRoundTrip()
    {
        const QString strEvidence = DecodeUtf8(
            "설명 문단\n"
            "```python\n"
            "def f(x):\n"
            "\n"
            "    return x  # 두 칸\n"
            "```\n"
            "\n"
            "    들여 쓴 코드 블록\n"
            "      둘째 줄\n"
            "\n"
            "끝 문단");
        KnowledgeDocument doc = ReadCanonicalRecord();
        doc.vecCards[0].strEvidence = strEvidence;
        const QString strText = VerifyRoundTrip(doc, 2);
        QVERIFY2(!strText.isEmpty(), qUtf8Printable(KnowledgeMarkdownWriter::WriteDocument(doc)));
        QVERIFY2(strText.contains(DecodeUtf8(
                     "  - 근거: 설명 문단\n    ```python\n    def f(x):\n\n        return x  # 두 칸\n    ```\n\n"
                     "        들여 쓴 코드 블록\n          둘째 줄\n\n    끝 문단\n  - 절: 2\n")),
                 qUtf8Printable(strText));

        // 첫 줄이 울타리면 `- 근거:` 뒤를 비우고 울타리부터 이어지는 줄로 쓴다(그래야 울타리가 블록을 연다)
        doc.vecCards[0].strEvidence = DecodeUtf8("```\nx  = 1\n```\n설명");
        const QString strFenceFirst = VerifyRoundTrip(doc, 2);
        QVERIFY2(!strFenceFirst.isEmpty(), qUtf8Printable(KnowledgeMarkdownWriter::WriteDocument(doc)));
        QVERIFY2(strFenceFirst.contains(DecodeUtf8("\n  - 근거:\n    ```\n    x  = 1\n    ```\n    설명\n  - 절: 2\n")),
                 qUtf8Printable(strFenceFirst));

        // 닫히지 않은 울타리는 닫는 줄을 더해 쓴다. 뒤의 절이 코드 안으로 들어가지 않는다.
        doc.vecCards[0].strEvidence = DecodeUtf8("설명\n```\nx = 1");
        const QString strUnclosed = KnowledgeMarkdownWriter::WriteDocument(doc);
        QVERIFY2(strUnclosed.contains(DecodeUtf8("  - 근거: 설명\n    ```\n    x = 1\n    ```\n  - 절: 2\n")), qUtf8Printable(strUnclosed));
        const DocumentReadResult unclosed = KnowledgeMarkdownReader::ReadDocument(strUnclosed);
        QCOMPARE(unclosed.document.vecCards[0].strEvidence, DecodeUtf8("설명\n```\nx = 1\n```"));
        QCOMPARE(unclosed.document.vecRelations.size(), 1);
        QCOMPARE(unclosed.document.vecTables.size(), 1);
    }

    // 손으로 쓴 근거 안 울타리: 절 안에서 닫히면 닫는 줄까지는 빈 줄 · 들여쓰기와 상관없이 근거다(파일 훑기가 코드로 보는 범위와 같다).
    // 뗄 들여쓰기는 있는 만큼만 뗀다.
    void handWrittenFenceRunsToItsClose()
    {
        const QString strText = ReplaceCards(DecodeUtf8(
            "- 카드 <!-- card:7f3a2c1e -->\n"
            "  - 근거: 설명\n"
            "    ```\n"
            "x = 1\n"
            "\n"
            "  y = 2\n"
            "- 목록처럼 보이는 코드\n"
            "    ```\n"
            "  - 절: 2\n"));
        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(read.bOk);
        QCOMPARE(read.document.vecCards.size(), 1);
        QCOMPARE(read.document.vecCards[0].strEvidence, DecodeUtf8("설명\n```\nx = 1\n\ny = 2\n- 목록처럼 보이는 코드\n```"));
        QCOMPARE(read.document.vecCards[0].strSection, QString("2"));
        QVERIFY(read.hashUnreadLines.isEmpty());
        QCOMPARE(read.document.vecRelations.size(), 1);

        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(read.document, strText);
        const DocumentReadResult reread = KnowledgeMarkdownReader::ReadDocument(strWritten);
        QVERIFY(reread.document.vecCards == read.document.vecCards);
        QVERIFY(reread.document.vecRelations == read.document.vecRelations);
        QVERIFY(reread.hashUnreadLines.isEmpty());
    }

    // 손으로 쓴 근거의 닫히지 않은 울타리는 다음 카드(id 주석이 있는 첫 줄)나 덜 들인 아는 하위 항목(`- 절:` 등)을 넘어
    // 닫는 줄을 찾지 않는다. 들여쓰기 규칙대로 읽고, 다시 쓸 때 닫는 줄을 더한다(Obsidian 에서도 목록 항목이 끝나면 코드가 닫힌다).
    void unclosedFenceDoesNotSwallowNextCard()
    {
        // 두 카드 모두 울타리를 닫지 않았다. 파일 훑기는 첫 카드의 ``` 를 둘째 카드의 ```python 로 닫는다고 본다(뒤 절은 그대로 읽힌다).
        const QString strText = ReplaceCards(DecodeUtf8(
            "- 카드 하나 <!-- card:7f3a2c1e -->\n"
            "  - 근거: 설명\n"
            "    ```\n"
            "    x = 1\n"
            "  - 절: 2\n"
            "- 카드 둘 <!-- card:0a1b2c3d -->\n"
            "  - 근거: 둘째 설명\n"
            "    ```python\n"
            "    y = 2\n"
            "  - 절: 3\n"));
        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(read.bOk);
        QCOMPARE(read.document.vecCards.size(), 2);
        QCOMPARE(read.document.vecCards[0].strEvidence, DecodeUtf8("설명\n```\nx = 1\n```"));
        QCOMPARE(read.document.vecCards[0].strSection, QString("2"));
        QCOMPARE(read.document.vecCards[1].strEvidence, DecodeUtf8("둘째 설명\n```python\ny = 2\n```"));
        QCOMPARE(read.document.vecCards[1].strSection, QString("3"));
        QCOMPARE(read.document.vecRelations.size(), 1);
        QVERIFY(read.hashUnreadLines.isEmpty());

        // 카드 구조 줄이 없으면 덜 들인 줄을 넘어 닫는 줄까지가 근거다(위 handWrittenFenceRunsToItsClose). id 없는 `- ` 줄은 카드 구조가 아니다.
        const DocumentReadResult bare = KnowledgeMarkdownReader::ReadDocument(ReplaceCards(DecodeUtf8(
            "- 카드 하나 <!-- card:7f3a2c1e -->\n"
            "  - 근거: 설명\n"
            "    ```\n"
            "- 코드 속 목록\n"
            "  - 메모: 코드 속 하위 항목처럼 보이는 줄\n"
            "    ```\n"
            "  - 절: 2\n")));
        QCOMPARE(bare.document.vecCards.size(), 1);
        QCOMPARE(bare.document.vecCards[0].strEvidence,
                 DecodeUtf8("설명\n```\n- 코드 속 목록\n- 메모: 코드 속 하위 항목처럼 보이는 줄\n```"));
        QCOMPARE(bare.document.vecCards[0].strSection, QString("2"));

        // 다시 쓴 글은 울타리가 짝이 맞아 뒤 절이 그대로 읽힌다
        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(read.document, strText);
        const DocumentReadResult reread = KnowledgeMarkdownReader::ReadDocument(strWritten);
        QVERIFY(reread.document.vecCards == read.document.vecCards);
        QCOMPARE(reread.document.vecRelations.size(), 1);
        QCOMPARE(reread.document.vecTables.size(), 1);
        QVERIFY(reread.hashUnreadLines.isEmpty());
    }

    // 관계의 하위 항목은 한 줄 그대로다: 들여 쓴 줄은 이어지는 줄이 아니라 읽지 못한 줄이다
    void relationSubItemsStayOneLine()
    {
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText.replace(DecodeUtf8("  - 설명: 배치 정규화 · anchor · 고해상도 학습으로 mAP 63.4 → 76.8, 속도 유지\n"),
                        DecodeUtf8("  - 설명: 배치 정규화 · anchor · 고해상도 학습으로 mAP 63.4 → 76.8, 속도 유지\n    이어 쓴 설명\n"));
        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QVERIFY(read.bOk);
        QCOMPARE(read.document.vecRelations[0].strNote, DecodeUtf8("배치 정규화 · anchor · 고해상도 학습으로 mAP 63.4 → 76.8, 속도 유지"));
        QCOMPARE(read.hashUnreadLines.value(DecodeUtf8("관계")), QStringList({ DecodeUtf8("    이어 쓴 설명") }));

        KnowledgeDocument doc = ReadCanonicalRecord();
        doc.vecRelations[0].strNote = DecodeUtf8("첫 줄\n둘째 줄");
        QVERIFY(KnowledgeMarkdownWriter::WriteDocument(doc).contains(DecodeUtf8("\n  - 설명: 첫 줄 둘째 줄\n")));
    }

    // 판: 쓰는 판은 그 기록을 나타낼 수 있는 가장 낮은 판이다(여러 줄 근거가 하나라도 있으면 2, 아니면 1).
    // 기록의 판이 아는 판보다 높으면(더 새 파일에서 읽은 기록) 그 판을 지킨다.
    void formatFollowsContent()
    {
        QCOMPARE(KnowledgeVocabulary::Get().GetFormat(), 2);
        const auto funcWrittenFormat = [](const KnowledgeDocument& document)
        {
            return KnowledgeMarkdownReader::ReadDocument(KnowledgeMarkdownWriter::WriteDocument(document)).document.iFormat;
        };

        KnowledgeDocument doc = ReadCanonicalRecord();
        QCOMPARE(funcWrittenFormat(doc), 1);
        doc.iFormat = 2;
        QCOMPARE(funcWrittenFormat(doc), 1);
        doc.vecCards[0].strEvidence = DecodeUtf8("첫 줄\n둘째 줄");
        QCOMPARE(funcWrittenFormat(doc), 2);
        doc.iFormat = 1;
        QCOMPARE(funcWrittenFormat(doc), 2);
        doc.iFormat = 0;
        QCOMPARE(funcWrittenFormat(doc), 2);
        doc.iFormat = 3;
        QCOMPARE(funcWrittenFormat(doc), 3);
        doc.vecCards[0].strEvidence = DecodeUtf8("한 줄\n\n");
        QCOMPARE(funcWrittenFormat(doc), 3);
        doc.iFormat = 2;
        QCOMPARE(funcWrittenFormat(doc), 1);

        // 여러 줄 근거를 지우면 2판 파일도 1판으로 다시 쓴다(옛 Ocean 이 다시 고칠 수 있다)
        KnowledgeDocument multi = ReadCanonicalRecord();
        multi.vecCards[0].strEvidence = MakeRichEvidence();
        const QString strMulti = KnowledgeMarkdownWriter::WriteDocument(multi);
        KnowledgeDocument reread = KnowledgeMarkdownReader::ReadDocument(strMulti).document;
        QCOMPARE(reread.iFormat, 2);
        reread.vecCards[0].strEvidence = DecodeUtf8("한 줄");
        QVERIFY(KnowledgeMarkdownWriter::WriteDocument(reread, strMulti).contains(QStringLiteral("\nformat: 1\n")));

        // 개념 파일 문법은 바뀌지 않았다: 1판으로 쓰고, 아는 판보다 높은 기록만 그 판을 지킨다
        KnowledgeConcept concept = KnowledgeMarkdownReader::ReadConcept(KnowledgeTestData::CanonicalConcept()).concept;
        concept.iFormat = 2;
        QVERIFY(KnowledgeMarkdownWriter::WriteConcept(concept).contains(QStringLiteral("\nformat: 1\n")));
        concept.iFormat = 3;
        QVERIFY(KnowledgeMarkdownWriter::WriteConcept(concept).contains(QStringLiteral("\nformat: 3\n")));
    }
};

std::unique_ptr<QObject> CreateKnowledgeEvidenceTest() { return std::make_unique<KnowledgeEvidenceTest>(); }

#include "KnowledgeEvidenceTest.moc"
