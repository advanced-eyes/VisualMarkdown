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
constexpr int kSingleLineFormat = 1;   // 여러 줄 근거가 없는 문서의 판
constexpr int kMultilineFormat = 2;    // 여러 줄 근거가 있는 문서의 판

// 정준 글(KnowledgeTestData::CanonicalDocument)의 카드
constexpr const char* kCanonicalCard =
    "- 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올렸다 <!-- card:7f3a2c1e -->\n"
    "  - 근거: VOC 2007 mAP 63.4% → 76.8%, 67 fps\n"
    "  - 절: 2\n"
    "  - 개념: [YOLOv2](../concepts/yolov2.md)\n";

// 문단 둘 · 목록(들여 쓴 목록 포함) · $$ 수식이 든 근거와, 그것을 쓴 하위 항목 줄
constexpr const char* kRichEvidence =
    "VOC 2007 mAP 63.4% → 76.8%, 67 fps\n"
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
    "$$";
constexpr const char* kRichEvidenceLines =
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

QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

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

// 손 글 text(정준 글의 관계 하나 · 표 하나를 그대로 둔 것)를 두 번 다시 써도 관계 · 표가 그대로 읽히고,
// 카드가 처음 읽은 것과 같고, 두 번째 쓰기부터 바이트가 같은지 본다. 맞으면 빈 글, 아니면 까닭과 쓴 글.
QString CheckRewriteKeepsSections(const QString& text)
{
    const auto funcSections = [](const DocumentReadResult& read)
    {
        return read.document.vecRelations.size() == 1 && read.document.vecTables.size() == 1;
    };
    const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(text);
    if (!funcSections(read))
        return QStringLiteral("read lost sections\n") + text;
    const QString strFirst = KnowledgeMarkdownWriter::WriteDocument(read.document, text);
    const DocumentReadResult first = KnowledgeMarkdownReader::ReadDocument(strFirst);
    if (!funcSections(first) || first.document.vecCards != read.document.vecCards)
        return QStringLiteral("first rewrite changed the record\n") + strFirst;
    const QString strSecond = KnowledgeMarkdownWriter::WriteDocument(first.document, strFirst);
    if (strSecond != strFirst)
        return QStringLiteral("second rewrite differs\n") + strSecond;
    return QString();
}

// 파일 훑기(KnowledgeMarkdownLayout::Scan)가 text 에서 찾은 절 이름(차례대로)
QStringList ScanSectionNames(const QString& text)
{
    QStringList vecNames;
    for (const KnowledgeSectionRange& section : KnowledgeMarkdownLayout::Scan(text).vecSections)
        vecNames << section.strName;
    return vecNames;
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
        doc.vecCards[0].strEvidence = DecodeUtf8(kRichEvidence);
        doc.vecCards[0].vecExtraLines = QStringList({ DecodeUtf8("메모: 다시 볼 것") });

        const QString strText = VerifyRoundTrip(doc, kMultilineFormat);
        QVERIFY2(!strText.isEmpty(), qUtf8Printable(KnowledgeMarkdownWriter::WriteDocument(doc)));
        const QString strCard = DecodeUtf8("- 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올렸다 <!-- card:7f3a2c1e -->\n")
            + DecodeUtf8(kRichEvidenceLines)
            + DecodeUtf8("  - 절: 2\n  - 개념: [YOLOv2](../concepts/yolov2.md)\n  - 메모: 다시 볼 것\n\n## 관계\n");
        QVERIFY2(strText.contains(strCard), qUtf8Printable(strText));
        QVERIFY2(strText.contains(QStringLiteral("\nformat: 2\n")), qUtf8Printable(strText));
        QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strText).document.vecCards[0].strEvidence, DecodeUtf8(kRichEvidence));

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
        const QString strText = VerifyRoundTrip(expected, kSingleLineFormat);
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
            // 첫 줄이 블록을 열면 첫 줄도 이어지는 줄처럼 다룬다(접지 않고 들여쓰기를 지키며, 울타리가 닫히는지 본다)
            { DecodeUtf8("```\nx  = 1\n```"), DecodeUtf8("```\nx  = 1\n```") },
            { DecodeUtf8("  ```\n  x"), DecodeUtf8("  ```\n  x\n  ```") },
            { DecodeUtf8("  - 하나  둘  \n  - 셋"), DecodeUtf8("  - 하나  둘\n  - 셋") },
            { DecodeUtf8("\n\n    코드  한 줄\n    둘째\n"), DecodeUtf8("    코드  한 줄\n    둘째") },
            { DecodeUtf8("    코드 한 줄\n"), DecodeUtf8("코드 한 줄") },   // 한 줄이면 지금처럼 접는다
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
        doc.vecCards[0].strEvidence = DecodeUtf8(kRichEvidence);
        QString strTabs = KnowledgeMarkdownWriter::WriteDocument(doc);
        strTabs.replace(QStringLiteral("\n  "), QStringLiteral("\n\t"));
        const DocumentReadResult tabs = KnowledgeMarkdownReader::ReadDocument(strTabs);
        QVERIFY(tabs.hashUnreadLines.isEmpty());
        QCOMPARE(tabs.document.vecCards[0].strEvidence, DecodeUtf8(kRichEvidence));
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
        const QString strText = VerifyRoundTrip(doc, kMultilineFormat);
        QVERIFY2(!strText.isEmpty(), qUtf8Printable(KnowledgeMarkdownWriter::WriteDocument(doc)));
        QVERIFY2(strText.contains(DecodeUtf8(
                     "  - 근거: 설명 문단\n    ```python\n    def f(x):\n\n        return x  # 두 칸\n    ```\n\n"
                     "        들여 쓴 코드 블록\n          둘째 줄\n\n    끝 문단\n  - 절: 2\n")),
                 qUtf8Printable(strText));

        // 첫 줄이 울타리면 `- 근거:` 뒤를 비우고 빈 줄 하나 다음에 울타리부터 쓴다(그래야 울타리가 블록을 연다)
        doc.vecCards[0].strEvidence = DecodeUtf8("```\nx  = 1\n```\n설명");
        const QString strFenceFirst = VerifyRoundTrip(doc, kMultilineFormat);
        QVERIFY2(!strFenceFirst.isEmpty(), qUtf8Printable(KnowledgeMarkdownWriter::WriteDocument(doc)));
        QVERIFY2(strFenceFirst.contains(DecodeUtf8("\n  - 근거:\n\n    ```\n    x  = 1\n    ```\n    설명\n  - 절: 2\n")),
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

    // 여러 줄 근거의 첫 줄이 블록을 열면(목록 · 울타리 · $$ · 표 · 제목 · 인용 · 들여 쓴 코드 · 가로줄 · HTML) `  - 근거:` 뒤를 비우고
    // 빈 줄 하나 다음에 첫 줄부터 공백 네 칸 들여 쓴다(Obsidian 이 표 · 목록 · 수식을 하위 항목 아래 블록으로 그린다). 왕복은 글자 그대로다.
    void blockFirstEvidenceStartsAfterBlankLine()
    {
        const QStringList vecEvidences {
            DecodeUtf8("- 하나\n- 둘\n  - 들여 쓴 셋"),
            DecodeUtf8("* 별표 하나\n* 별표 둘"),
            DecodeUtf8("+ 더하기 하나\n+ 더하기 둘"),
            DecodeUtf8("1. 첫째\n2. 둘째"),
            DecodeUtf8("1) 첫째\n2) 둘째"),
            DecodeUtf8("```python\nx  = 1\n```\n설명"),
            DecodeUtf8("~~~\ny\n~~~"),
            DecodeUtf8("$$\nE = mc^2\n$$\n\n질량과 에너지"),
            DecodeUtf8("$$E = mc^2$$\n질량과 에너지"),
            DecodeUtf8("| 단계 | mAP (%) |\n| --- | --: |\n| 기준 | 63.4 |"),
            DecodeUtf8("단계 | mAP\n--- | --:\n기준 | 63.4"),
            DecodeUtf8("## 소제목\n본문"),
            DecodeUtf8("#\n빈 제목 뒤"),
            DecodeUtf8("> 인용\n> 둘째 줄"),
            DecodeUtf8("    들여 쓴 코드\n    둘째  줄"),
            DecodeUtf8("\t탭 코드\n둘째"),
            DecodeUtf8("---\n가로줄 뒤 글"),
            DecodeUtf8("<div>\n속 글\n</div>"),
            DecodeUtf8("  - 두 칸 들인 목록\n  - 둘"),
        };
        for (const QString& strEvidence : vecEvidences)
        {
            QCOMPARE(KnowledgeText::NormalizeEvidence(strEvidence), strEvidence);
            KnowledgeDocument doc = ReadCanonicalRecord();
            doc.vecCards[0].strEvidence = strEvidence;
            const QString strText = VerifyRoundTrip(doc, kMultilineFormat);
            QVERIFY2(!strText.isEmpty(), qUtf8Printable(KnowledgeMarkdownWriter::WriteDocument(doc)));
            QString strBlock;
            for (const QString& strLine : strEvidence.split(QLatin1Char('\n')))
                strBlock += strLine.isEmpty() ? QStringLiteral("\n") : QStringLiteral("    ") + strLine + QLatin1Char('\n');
            QVERIFY2(strText.contains(DecodeUtf8("<!-- card:7f3a2c1e -->\n  - 근거:\n\n") + strBlock + DecodeUtf8("  - 절: 2\n")),
                     qUtf8Printable(strText));
            QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strText).document.vecCards[0].strEvidence, strEvidence);
        }

        // 문단으로 시작하는 여러 줄 근거는 지금처럼 첫 줄을 `근거: ` 뒤에 쓴다(블록을 열지 않는 글)
        const QStringList vecParagraphs {
            DecodeUtf8("-5% 떨어졌다\n둘째"),
            DecodeUtf8("1.5배 빠르다\n둘째"),
            DecodeUtf8("#태그 로 시작\n둘째"),
            DecodeUtf8("![그림](a.png) 설명\n둘째"),
            DecodeUtf8("<모델> 은 입력을 받는다\n둘째"),
            DecodeUtf8("$x$ 는 입력이다\n둘째"),
            DecodeUtf8("a | b 는 표가 아니다\n둘째"),
            DecodeUtf8("   세 칸 들인 문단\n둘째"),
        };
        for (const QString& strEvidence : vecParagraphs)
        {
            KnowledgeDocument doc = ReadCanonicalRecord();
            doc.vecCards[0].strEvidence = strEvidence;
            const QString strText = KnowledgeMarkdownWriter::WriteDocument(doc);
            const QString strFirst = strEvidence.section(QLatin1Char('\n'), 0, 0).simplified();
            QVERIFY2(strText.contains(DecodeUtf8("\n  - 근거: ") + strFirst + DecodeUtf8("\n    둘째\n  - 절: 2\n")), qUtf8Printable(strText));
            QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strText).document.vecCards[0].strEvidence,
                     strFirst + DecodeUtf8("\n둘째"));
        }

        // 한 줄 근거는 블록처럼 보여도 지금처럼 `근거: ` 뒤에 쓰고 1판이다
        KnowledgeDocument single = ReadCanonicalRecord();
        single.vecCards[0].strEvidence = DecodeUtf8("- 한 줄 목록");
        const QString strSingle = VerifyRoundTrip(single, kSingleLineFormat);
        QVERIFY(strSingle.contains(DecodeUtf8("\n  - 근거: - 한 줄 목록\n  - 절: 2\n")));

        // 손으로 쓴 글: `근거:` 값이 비고 빈 줄(여럿이어도) 뒤나 바로 다음 줄에 이어지는 줄이 오면 그 줄부터가 근거다
        for (const QString& strGap : { QStringLiteral("\n"), QStringLiteral("\n\n"), QString() })
        {
            const QString strText = ReplaceCards(DecodeUtf8("- 카드 <!-- card:7f3a2c1e -->\n  - 근거:\n") + strGap
                                                 + DecodeUtf8("    | a | b |\n    | --- | --- |\n    | 1 | 2 |\n  - 절: 2\n"));
            const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
            QVERIFY(read.hashUnreadLines.isEmpty());
            QCOMPARE(read.document.vecCards[0].strEvidence, DecodeUtf8("| a | b |\n| --- | --- |\n| 1 | 2 |"));
            QCOMPARE(read.document.vecCards[0].strSection, QString("2"));
            const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(read.document, strText);
            QVERIFY2(strWritten.contains(DecodeUtf8("  - 근거:\n\n    | a | b |\n    | --- | --- |\n    | 1 | 2 |\n  - 절: 2\n")),
                     qUtf8Printable(strWritten));
        }
    }

    // 손으로 쓴 근거 안 울타리도 들여쓰기 규칙을 따른다. 울타리 안의 빈 줄 · 더 들인 줄은 근거이고, 근거보다 덜 들인 줄은
    // 울타리 안이라도 근거를 끝낸다(CommonMark: 목록 항목이 끝나면 그 안 코드도 닫힌다). 파일 훑기도 그 줄 앞에서 울타리를 닫는다.
    void handWrittenFenceEndsWithItsItem()
    {
        const QString strInside = ReplaceCards(DecodeUtf8(
            "- 카드 <!-- card:7f3a2c1e -->\n"
            "  - 근거: 설명\n"
            "    ```\n"
            "    x = 1\n"
            "\n"
            "        y = 2\n"
            "    ```\n"
            "  - 절: 2\n"));
        const DocumentReadResult inside = KnowledgeMarkdownReader::ReadDocument(strInside);
        QCOMPARE(inside.document.vecCards[0].strEvidence, DecodeUtf8("설명\n```\nx = 1\n\n    y = 2\n```"));
        QCOMPARE(inside.document.vecCards[0].strSection, QString("2"));
        QVERIFY(inside.hashUnreadLines.isEmpty());
        QVERIFY2(CheckRewriteKeepsSections(strInside).isEmpty(), qUtf8Printable(CheckRewriteKeepsSections(strInside)));

        const QString strShallow = ReplaceCards(DecodeUtf8(
            "- 카드 <!-- card:7f3a2c1e -->\n"
            "  - 근거: 설명\n"
            "    ```\n"
            "x = 1\n"
            "    ```\n"
            "  - 절: 2\n"));
        const DocumentReadResult shallow = KnowledgeMarkdownReader::ReadDocument(strShallow);
        QCOMPARE(shallow.document.vecCards.size(), 1);
        QCOMPARE(shallow.document.vecCards[0].strEvidence, DecodeUtf8("설명\n```\n```"));
        QVERIFY(shallow.document.vecCards[0].strSection.isEmpty());
        QCOMPARE(shallow.hashUnreadLines.value(DecodeUtf8("지식")),
                 QStringList({ DecodeUtf8("x = 1"), DecodeUtf8("    ```"), DecodeUtf8("  - 절: 2") }));
        QVERIFY2(CheckRewriteKeepsSections(strShallow).isEmpty(), qUtf8Printable(CheckRewriteKeepsSections(strShallow)));
    }

    // 손으로 쓴 근거의 닫는 울타리가 형제 하위 항목 뒤에 오면, 그 줄은 읽지 못한 줄로 절 끝에 옮겨진다.
    // 옮겨진 들여 쓴 울타리는 뒤 `## ` 줄 앞에서 닫히므로 한 번 · 두 번 다시 써도 관계 · 표가 그대로다(빈 줄을 끼워도 같다).
    void movedStrayFenceDoesNotSwallowSections()
    {
        for (const QString& strGap : { QString(), QStringLiteral("\n") })
        {
            const QString strText = ReplaceCards(DecodeUtf8(
                "- 카드 하나 <!-- card:7f3a2c1e -->\n"
                "  - 근거: 설명\n"
                "    ```\n"
                "    x = 1\n") + strGap + DecodeUtf8(
                "  - 절: 2\n"
                "    ```\n"));
            const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
            QCOMPARE(read.document.vecCards.size(), 1);
            QCOMPARE(read.document.vecCards[0].strEvidence, DecodeUtf8("설명\n```\nx = 1\n```"));
            QCOMPARE(read.document.vecCards[0].strSection, strGap.isEmpty() ? QString("2") : QString());
            const QString strWhy = CheckRewriteKeepsSections(strText);
            QVERIFY2(strWhy.isEmpty(), qUtf8Printable(strWhy));

            const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(read.document, strText);
            QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strWritten).hashUnreadLines, read.hashUnreadLines);
        }
    }

    // 손으로 쓴 근거의 닫히지 않은 울타리는 그 근거가 끝나는 줄(형제 하위 항목 · 다음 카드 · 절 제목) 앞에서 닫힌다.
    // 뒤 카드의 울타리와 짝지어지지 않으므로 뒤 카드 · 절을 삼키지 않는다. 다시 쓸 때 근거에 닫는 줄을 더한다.
    void unclosedFenceClosesWithItsItem()
    {
        const QString strUnclosed = DecodeUtf8(
            "- 카드 하나 <!-- card:7f3a2c1e -->\n"
            "  - 근거: 설명\n"
            "    ```\n"
            "    x = 1\n"
            "  - 절: 2\n");
        const QVector<QPair<QString, QString>> vecSeconds {
            { QString(), QString() },   // 닫지 않은 카드 하나뿐
            { DecodeUtf8("- 카드 둘 <!-- card:0a1b2c3d -->\n  - 근거: 둘째 설명\n    ```python\n    y = 2\n    ```\n  - 절: 3\n"),
              DecodeUtf8("둘째 설명\n```python\ny = 2\n```") },   // 뒤 카드는 닫혔다
            { DecodeUtf8("- 카드 둘 <!-- card:0a1b2c3d -->\n  - 근거: 둘째 설명\n    ```python\n    y = 2\n  - 절: 3\n"),
              DecodeUtf8("둘째 설명\n```python\ny = 2\n```") },   // 뒤 카드도 닫지 않았다
            { DecodeUtf8("- 카드 둘 <!-- card:0a1b2c3d -->\n  - 근거: 둘째 설명\n  - 절: 3\n\n    ```\n"),
              DecodeUtf8("둘째 설명") },                          // 절 끝에 들여 쓴 울타리 줄 하나
        };
        for (const QPair<QString, QString>& pairSecond : vecSeconds)
        {
            const QString strText = ReplaceCards(strUnclosed + pairSecond.first);
            const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
            QVERIFY(read.bOk);
            QCOMPARE(read.document.vecCards.size(), pairSecond.first.isEmpty() ? 1 : 2);
            QCOMPARE(read.document.vecCards[0].strEvidence, DecodeUtf8("설명\n```\nx = 1\n```"));
            QCOMPARE(read.document.vecCards[0].strSection, QString("2"));
            if (!pairSecond.first.isEmpty())
            {
                QCOMPARE(read.document.vecCards[1].strEvidence, pairSecond.second);
                QCOMPARE(read.document.vecCards[1].strSection, QString("3"));
            }
            QCOMPARE(read.document.vecRelations.size(), 1);
            QCOMPARE(read.document.vecTables.size(), 1);
            const QString strWhy = CheckRewriteKeepsSections(strText);
            QVERIFY2(strWhy.isEmpty(), qUtf8Printable(strWhy));
        }
    }

    // 파일 훑기의 울타리: 목록 항목 안 울타리는 같은 글자 닫는 줄이나, 그 항목의 내용 들여쓰기보다 덜 들인 비지 않은 줄
    // (항목이 끝난다) 앞에서 닫힌다. 담는 목록 항목이 없는 맨 바깥 울타리는 지금처럼 같은 글자 울타리 줄에서만 닫힌다.
    void scanClosesListFenceWhenItemEnds()
    {
        const auto funcNames = [](const QString& tail) { return ScanSectionNames(KnowledgeTestData::CanonicalDocument() + tail); };
        // 목록 안 울타리 뒤의 들여 쓰지 않은 ## 줄은 절이다
        QVERIFY(funcNames(DecodeUtf8("\n## 내 메모\n\n- 목록\n  ```\n  코드\n\n## 둘째 메모\n\n글\n")).contains(DecodeUtf8("둘째 메모")));
        // 항목 내용 칸(2) 이상 들인 닫는 줄은 울타리를 닫는다(여는 줄보다 덜 들였어도)
        QVERIFY(funcNames(DecodeUtf8("\n## 내 메모\n\n- 목록\n    ```\n  ```\n## 둘째 메모\n")).contains(DecodeUtf8("둘째 메모")));
        // 더 들인 줄 · 빈 줄 · 다른 글자 울타리는 들여 쓴 울타리를 닫지 않는다. 그 뒤 맨 바깥 울타리 안의 ## 줄은 절이 아니다
        const QStringList vecPaired = funcNames(DecodeUtf8("\n## 내 메모\n\n- 목록\n  ```\n    ~~~\n\n  ```\n```\n## 아님\n```\n## 둘째 메모\n"));
        QVERIFY(vecPaired.contains(DecodeUtf8("둘째 메모")));
        QVERIFY(!vecPaired.contains(DecodeUtf8("아님")));
        // 맨 바깥 울타리는 그대로: 닫지 않으면 뒤 ## 줄을 절로 보지 않고, 들여 쓴 같은 글자 줄로도 닫힌다
        const QStringList vecOpen = funcNames(DecodeUtf8("\n## 내 메모\n\n```\n코드\n## 아님\n"));
        QCOMPARE(vecOpen.last(), DecodeUtf8("내 메모"));
        const QStringList vecIndentedClose = funcNames(DecodeUtf8("\n## 내 메모\n\n```\n## 아님\n  ```\n## 둘째 메모\n"));
        QVERIFY(vecIndentedClose.contains(DecodeUtf8("둘째 메모")));
        QVERIFY(!vecIndentedClose.contains(DecodeUtf8("아님")));

        // 한 줄 찾기도 같은 규칙으로 울타리를 건너뛴다: 목록 안의 닫지 않은 울타리 뒤 문단이 한 줄이다
        const QString strSummary = DecodeUtf8("YOLO 에 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올리고, 9000개 넘는 클래스를 검출하게 했다.");
        QString strText = KnowledgeTestData::CanonicalDocument();
        strText.replace(strSummary, DecodeUtf8("- 목록\n  ```\n  코드\n\n") + strSummary);
        QCOMPARE(KnowledgeMarkdownLayout::Scan(strText).strSummary, strSummary);
    }

    // 울타리를 닫는 기준은 울타리 자기 들여쓰기가 아니라 담는 목록 항목의 내용 들여쓰기다(CommonMark).
    // 담는 항목이 없으면 1–3칸 들였어도 맨 바깥 울타리이고, 담는 곳보다 4칸 이상 들인 ``` 줄은 울타리가 아니다(들여 쓴 코드 · 문단 글).
    void fenceClosesWithItsListItem()
    {
        const QString strBase = KnowledgeTestData::CanonicalDocument();

        // (가) 근거 안 1–3칸 들인 울타리는 근거 항목 안 울타리다: 내용이 덜 들여 써져도 같은 글자 닫는 줄로 닫힌다(줄이 늘지 않는다)
        for (const QString& strEvidence : { DecodeUtf8("설명\n  ```\nx\n  ```"), DecodeUtf8("  ```\nx\n  ```"), DecodeUtf8("설명\n ```\n코드\n ```\n끝"),
                                            DecodeUtf8("목록\n- a\n  ```\n  x\n  ```\n끝"), DecodeUtf8("목록\n- a\n  ```\nx"),
                                            DecodeUtf8("설명\n\n    ```\n    4칸 들인 코드 속 울타리 글") })
        {
            QCOMPARE(KnowledgeText::NormalizeEvidence(strEvidence), strEvidence);
            KnowledgeDocument doc = ReadCanonicalRecord();
            doc.vecCards[0].strEvidence = strEvidence;
            QVERIFY2(!VerifyRoundTrip(doc, kMultilineFormat).isEmpty(), qUtf8Printable(KnowledgeMarkdownWriter::WriteDocument(doc)));
        }
        // 근거 안 목록 항목 속 울타리는 그 항목이 끝나면(덜 들인 x) 닫히고, 그 뒤 ``` 는 새 울타리를 연다 → 닫는 줄을 더한다
        QCOMPARE(KnowledgeText::NormalizeEvidence(DecodeUtf8("목록\n- a\n  ```\nx\n  ```")), DecodeUtf8("목록\n- a\n  ```\nx\n  ```\n  ```"));

        // (나) 맨 바깥에서 1–3칸 들인 울타리 안의 ## 줄은 절이 아니다. 정해진 절 이름이어도 그 절을 두 번 보지 않고 사용자 메모가 갈라지지 않는다
        for (const QString& strName : { DecodeUtf8("아님"), DecodeUtf8("관계") })
        {
            const QString strText = strBase + DecodeUtf8("\n## 내 메모\n\n  ```\nx = 1\n## ") + strName + DecodeUtf8("\n  ```\n\n글\n");
            const QStringList vecNames = ScanSectionNames(strText);
            QCOMPARE(vecNames.last(), DecodeUtf8("내 메모"));
            QCOMPARE(vecNames.count(DecodeUtf8("관계")), 1);
            const QString strWhy = CheckRewriteKeepsSections(strText);
            QVERIFY2(strWhy.isEmpty(), qUtf8Printable(strWhy));
        }
        const QString strMemo = DecodeUtf8("  ```\nx = 1\n## 관계\n  ```\n\n글\n");
        const QString strOwned = strBase + DecodeUtf8("\n## 내 메모\n\n") + strMemo;
        const QString strOwnedWritten = KnowledgeMarkdownWriter::WriteDocument(KnowledgeMarkdownReader::ReadDocument(strOwned).document, strOwned);
        QVERIFY2(strOwnedWritten.endsWith(DecodeUtf8("\n## 내 메모\n\n") + strMemo), qUtf8Printable(strOwnedWritten));

        // (다) 제목 뒤 1–3칸 들인 울타리를 건너뛰고 그 뒤 문단이 한 줄이다
        const QString strSummary = DecodeUtf8("YOLO 에 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올리고, 9000개 넘는 클래스를 검출하게 했다.");
        QString strHead = strBase;
        strHead.replace(strSummary, DecodeUtf8("  ```\n코드 줄\n  ```\n\n") + strSummary);
        QCOMPARE(KnowledgeMarkdownLayout::Scan(strHead).strSummary, strSummary);

        // (라) 지식 절 끝의 1칸 들인 울타리(카드 목록이 빈 줄 뒤 끝났다) 안 ## 줄은 절이 아니다
        const QString strCards = ReplaceCards(DecodeUtf8(kCanonicalCard) + DecodeUtf8("\n ```\n코드\n## 가짜\n ```\n"));
        QVERIFY(!ScanSectionNames(strCards).contains(DecodeUtf8("가짜")));
        const DocumentReadResult cards = KnowledgeMarkdownReader::ReadDocument(strCards);
        QCOMPARE(cards.hashUnreadLines.value(DecodeUtf8("지식")),
                 QStringList({ QStringLiteral(" ```"), DecodeUtf8("코드"), DecodeUtf8("## 가짜"), QStringLiteral(" ```") }));
        const QString strCardsWhy = CheckRewriteKeepsSections(strCards);
        QVERIFY2(strCardsWhy.isEmpty(), qUtf8Printable(strCardsWhy));

        // 맨 바깥 들여쓰기 0 · 1 · 3 울타리: 같은 글자 줄로만 닫히고, 닫지 않으면 뒤 ## 줄을 절로 보지 않는다
        for (const int iIndent : { 0, 1, 3 })
        {
            const QString strIndent(iIndent, QLatin1Char(' '));
            const QStringList vecClosed = ScanSectionNames(strBase + DecodeUtf8("\n## 내 메모\n\n") + strIndent + DecodeUtf8("```\nx\n## 아님\n") + strIndent
                                                           + DecodeUtf8("```\n## 둘째 메모\n"));
            QVERIFY2(!vecClosed.contains(DecodeUtf8("아님")) && vecClosed.contains(DecodeUtf8("둘째 메모")), qPrintable(QString::number(iIndent)));
            const QStringList vecOpen = ScanSectionNames(strBase + DecodeUtf8("\n## 내 메모\n\n") + strIndent + DecodeUtf8("```\nx\n## 아님\n"));
            QCOMPARE(vecOpen.last(), DecodeUtf8("내 메모"));
        }
        // 담는 곳보다 4칸 이상 들인 ``` 줄은 울타리가 아니다: 뒤 ## 줄은 절이다
        QVERIFY(ScanSectionNames(strBase + DecodeUtf8("\n## 내 메모\n\n문단\n    ```\n## 둘째 메모\n")).contains(DecodeUtf8("둘째 메모")));
    }

    // 이미 끝난 목록 항목과 가로줄은 울타리를 담지 않는다(CommonMark). 그 뒤 들인 울타리는 맨 바깥 울타리라 같은 글자 줄로만 닫힌다.
    void fenceIsNotHeldByEndedItemOrBreak()
    {
        const QString strBase = KnowledgeTestData::CanonicalDocument() + DecodeUtf8("\n## 내 메모\n\n");
        // (빈 줄 없이 덜 들인 줄은 게으른 이어짐이라 CommonMark 에선 항목이 이어진다. 옛 Scan 부터의 한계라 여기서 보지 않는다, ReadWrite.md §8)
        const QStringList vecBefores {
            DecodeUtf8("- a\n\n para\n"),   // 빈 줄 뒤 내용 칸(2)보다 덜 들인 문단이 항목을 끝냈다
            DecodeUtf8("- - -\n"),          // 가로줄
            DecodeUtf8("* * *\n"),
            DecodeUtf8("*  *  *\n"),
            DecodeUtf8("- a\n\n- - -\n"),   // 목록 뒤 가로줄
        };
        for (const QString& strBefore : vecBefores)
        {
            const QStringList vecNames = ScanSectionNames(strBase + strBefore + DecodeUtf8("  ```\nx\n## 아님\n  ```\n## 둘째 메모\n"));
            QVERIFY2(!vecNames.contains(DecodeUtf8("아님")) && vecNames.contains(DecodeUtf8("둘째 메모")),
                     qUtf8Printable(strBefore + QStringLiteral(" => ") + vecNames.join(QStringLiteral(" | "))));
        }
        // 끝나지 않은 항목(빈 줄 뒤에도 내용 칸만큼 들인 문단)은 여전히 울타리를 담는다
        const QStringList vecHeld = ScanSectionNames(strBase + DecodeUtf8("- a\n\n  para\n  ```\nx\n## 둘째 메모\n"));
        QVERIFY(vecHeld.contains(DecodeUtf8("둘째 메모")));
    }

    // 쓰기가 만든 정해진 절 끝에 맨 바깥 울타리가 열린 채 남으면(읽지 못한 울타리 줄) 그 절 끝에 닫는 줄을 더한다.
    // 써 낸 파일에서 어떤 정해진 절도 뒤 절을 삼키지 않는다.
    void writerClosesTopLevelFenceAtSectionEnd()
    {
        // 맨 바깥 ``` 를 둘째 카드 근거의 들여 쓴 ``` 가 닫는다(파일 훑기). 읽기는 맨 바깥 줄을 읽지 못한 줄로, 들여 쓴 줄을 근거로 읽는다.
        const QString strText = ReplaceCards(DecodeUtf8(
            "- 카드 하나 <!-- card:7f3a2c1e -->\n"
            "  - 절: 2\n"
            "```\n"
            "- 카드 둘 <!-- card:0a1b2c3d -->\n"
            "  - 근거: 둘째\n"
            "    ```\n"
            "    x\n"));
        const DocumentReadResult read = KnowledgeMarkdownReader::ReadDocument(strText);
        QCOMPARE(read.document.vecCards.size(), 2);
        QCOMPARE(read.document.vecCards[1].strEvidence, DecodeUtf8("둘째\n```\nx\n```"));
        QCOMPARE(read.hashUnreadLines.value(DecodeUtf8("지식")), QStringList({ QStringLiteral("```") }));
        const QString strWhy = CheckRewriteKeepsSections(strText);
        QVERIFY2(strWhy.isEmpty(), qUtf8Printable(strWhy));

        const QString strWritten = KnowledgeMarkdownWriter::WriteDocument(read.document, strText);
        QVERIFY2(strWritten.contains(QStringLiteral("\n\n```\n```\n\n## ")), qUtf8Printable(strWritten));
        QCOMPARE(KnowledgeMarkdownReader::ReadDocument(strWritten).hashUnreadLines.value(DecodeUtf8("지식")),
                 QStringList({ QStringLiteral("```"), QStringLiteral("```") }));
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
        multi.vecCards[0].strEvidence = DecodeUtf8(kRichEvidence);
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
