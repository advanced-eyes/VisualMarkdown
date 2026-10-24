#include "VisualMarkdown/KnowledgeIndex.h"
#include "VisualMarkdown/KnowledgeMarkdownWriter.h"
#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

#include <memory>
#include <optional>

namespace
{
QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

constexpr const char* kLearnedFirst = "2026-10-01T09:00:00+09:00";
constexpr const char* kLearnedSecond = "2026-10-02T09:00:00+09:00";
constexpr const char* kLearnedThird = "2026-10-03T09:00:00+09:00";
constexpr const char* kLearnedFourth = "2026-10-04T09:00:00+09:00";

// 링크 글자는 id 그대로 둔다(색인은 링크의 id 만 본다)
ConceptLink MakeLink(const QString& conceptId)
{
    return { conceptId, conceptId };
}

KnowledgeRelation MakeRelation(const QString& from, const QString& kind, const QString& to,
                               ERelationStatus status = ERelationStatus::CONFIRMED, const QString& note = QString())
{
    KnowledgeRelation relation;
    relation.fromConcept = MakeLink(from);
    relation.strKind = kind;
    relation.toConcept = MakeLink(to);
    relation.eStatus = status;
    relation.strNote = note;
    return relation;
}

KnowledgeConcept MakeConcept(const QString& id, const QString& name, const QStringList& aliases = QStringList())
{
    KnowledgeConcept concept;
    concept.strId = id;
    concept.strKind = QStringLiteral("method");
    concept.strName = name;
    concept.vecAliases = aliases;
    concept.strSummary = name + DecodeUtf8(" 의 정의");
    return concept;
}

KnowledgeDocument MakeDocument(const QString& id, const char* learned)
{
    KnowledgeDocument document;
    document.strId = id;
    document.strTitle = id + DecodeUtf8(" 문서");
    document.strSummary = DecodeUtf8("요약");
    document.strLearned = QString::fromLatin1(learned);
    document.iRules = 1;
    return document;
}

KnowledgeCard MakeCard(const QStringList& conceptIds)
{
    KnowledgeCard card;
    card.strClaim = DecodeUtf8("주장");
    for (const QString& strId : conceptIds)
        card.vecConcepts.push_back(MakeLink(strId));
    return card;
}

// 행 하나짜리 표. 색인은 concepts 만 본다
KnowledgeTable MakeTable(const QStringList& conceptIds)
{
    KnowledgeTable table;
    table.strTitle = DecodeUtf8("표");
    table.strX = QStringLiteral("x");
    table.vecY = QStringList { QStringLiteral("y") };
    table.vecConceptIds = conceptIds;
    table.vecColumns = { { QStringLiteral("x"), QString() }, { QStringLiteral("y"), QString() } };
    table.vecRows = { QStringList { QStringLiteral("a"), QStringLiteral("1") } };
    return table;
}

// 개념 파일의 `## 관계` 절(format/Relation.md, 링크는 <id>.md). WriteConcept 는 이 절을 만들지 않아 시험이 적는다.
QString MakeRelationSection(const QVector<KnowledgeRelation>& relations)
{
    const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
    QStringList vecLines { DecodeUtf8("## 관계"), QString() };
    for (const KnowledgeRelation& relation : relations)
    {
        const QString strArrow = vocabulary.IsUndirected(relation.strKind) ? DecodeUtf8("↔") : DecodeUtf8("→");
        vecLines << QStringLiteral("- [%1](%2.md) **%3** %4 [%5](%6.md)")
                        .arg(relation.fromConcept.strName, relation.fromConcept.strConceptId,
                             vocabulary.ToRelationLabel(relation.strKind), strArrow,
                             relation.toConcept.strName, relation.toConcept.strConceptId);
        if (!relation.strNote.isEmpty())
            vecLines << DecodeUtf8("  - 설명: ") + relation.strNote;
        if (relation.eStatus == ERelationStatus::PROPOSED)
            vecLines << DecodeUtf8("  - 상태: ") + vocabulary.ToStatusLabel(QStringLiteral("proposed"));
    }
    return vecLines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

// 선은 "from 종류 to" 글로 견준다
QString DescribeEdge(const QString& from, const QString& kind, const QString& to)
{
    return from + QLatin1Char(' ') + kind + QLatin1Char(' ') + to;
}

QStringList DescribeEdges(const QVector<KnowledgeEdge>& edges)
{
    QStringList vecItems;
    for (const KnowledgeEdge& edge : edges)
        vecItems << DescribeEdge(edge.strFromId, edge.strKind, edge.strToId);
    return vecItems;
}

// 빠진 관계는 "from 종류 to @ 출처" 글로 견준다
QStringList DescribeDropped(const QVector<KnowledgeDroppedRelation>& relations)
{
    QStringList vecItems;
    for (const KnowledgeDroppedRelation& relation : relations)
        vecItems << DescribeEdge(relation.strFromId, relation.strKind, relation.strToId) + QStringLiteral(" @ ") + relation.strSource;
    return vecItems;
}

QStringList ListIds(const QVector<KnowledgeConcept>& concepts)
{
    QStringList vecIds;
    for (const KnowledgeConcept& concept : concepts)
        vecIds << concept.strId;
    return vecIds;
}

QStringList ListIds(const QVector<KnowledgeDocument>& documents)
{
    QStringList vecIds;
    for (const KnowledgeDocument& document : documents)
        vecIds << document.strId;
    return vecIds;
}

// 경고 하나에 들어 있어야 하는 파일 이름과 까닭
struct ExpectedWarning
{
    const char* pFileName;
    const char* pReason;
};
}

// KnowledgeIndex(design/Views.md §4)를 작은 wiki 로 시험한다. wiki 는 시험마다 임시 폴더에 KnowledgeMarkdownWriter 로 쓴다.
// 규칙은 format/Visualization.md §2, 읽기는 Ocean KnowledgeBase::Reload · RebuildIndex 와 같다.
class KnowledgeIndexTest : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_upDir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_upDir->isValid());
    }

    void cleanup()
    {
        m_upDir.reset();
    }

    // 개념 id 는 파일 이름이다. 머리글 id 가 다르면 경고를 남기고 파일 이름을 쓴다(Folder.md §2)
    void conceptIdFollowsFileName()
    {
        WriteConceptFileAs(QStringLiteral("yolo-v1.md"), MakeConcept("yolo", "YOLO"));
        WriteConceptFile(MakeConcept("faster-r-cnn", "Faster R-CNN"), { MakeRelation("faster-r-cnn", "improves", "yolo-v1") });

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        const std::optional<KnowledgeConcept> optConcept = index.FindConcept("yolo-v1");
        QVERIFY(optConcept.has_value());
        QCOMPARE(optConcept->strId, QString("yolo-v1"));
        QCOMPARE(optConcept->strName, QString("YOLO"));
        QVERIFY(!index.FindConcept("yolo").has_value());
        QCOMPARE(index.FindConceptIdByName("YOLO"), QString("yolo-v1"));
        QCOMPARE(DescribeEdges(index.ListEdges()), QStringList { "faster-r-cnn improves yolo-v1" });

        const QStringList vecWarnings = index.GetWarnings();
        QCOMPARE(vecWarnings.size(), 1);
        QVERIFY2(vecWarnings[0].contains("yolo-v1.md") && vecWarnings[0].contains("header id yolo "), qUtf8Printable(vecWarnings[0]));
    }

    // 읽지 못한 파일(머리글 없음 · 닫히지 않음 · type 이 다름 · id 없음)은 건너뛰고 파일마다 경고 하나를 남긴다.
    // 두 폴더 바로 아래의 *.md 만 읽는다. 하위 폴더 · 다른 확장자는 보지 않아 경고도 없다
    void unreadableFilesAreSkipped()
    {
        WriteDocumentFile("good.md", MakeDocument("good", kLearnedFirst));
        WriteFile("documents/no-id.md", DecodeUtf8("---\ntype: document\nformat: 1\n---\n\n# 제목\n"));
        WriteFile("documents/unclosed.md", DecodeUtf8("---\ntype: document\nformat: 1\nid: unclosed\n\n# 제목\n"));
        WriteFile("documents/notes.txt", KnowledgeMarkdownWriter::WriteDocument(MakeDocument("text-file", kLearnedFirst)));
        WriteFile("documents/archive/old.md", KnowledgeMarkdownWriter::WriteDocument(MakeDocument("archived", kLearnedFirst)));
        WriteConceptFile(MakeConcept("yolo", "YOLO"));
        WriteFile("concepts/no-front-matter.md", DecodeUtf8("# 이름\n\n정의\n"));
        WriteFile("concepts/wrong-type.md", KnowledgeMarkdownWriter::WriteDocument(MakeDocument("wrong-type", kLearnedFirst)));
        WriteFile("concepts/archive/old-concept.md", KnowledgeMarkdownWriter::WriteConcept(MakeConcept("old-concept", "Old")));

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        QCOMPARE(ListIds(index.ListDocuments()), QStringList { "good" });
        QCOMPARE(ListIds(index.ListConcepts(true)), QStringList { "yolo" });

        // 경고는 documents → concepts, 폴더 안에서는 파일 이름 순이다
        const QVector<ExpectedWarning> vecExpected {
            { "no-id.md", "id is missing" },
            { "unclosed.md", "front matter is not closed" },
            { "no-front-matter.md", "front matter is missing" },
            { "wrong-type.md", "type is not concept" },
        };
        const QStringList vecWarnings = index.GetWarnings();
        QCOMPARE(vecWarnings.size(), vecExpected.size());
        for (int iWarning = 0; iWarning < vecExpected.size(); ++iWarning)
        {
            const ExpectedWarning& expected = vecExpected[iWarning];
            QVERIFY2(vecWarnings[iWarning].contains(expected.pFileName) && vecWarnings[iWarning].contains(expected.pReason),
                     qUtf8Printable(vecWarnings[iWarning]));
        }
    }

    // 머리글 id 가 같은 문서가 둘이면 파일 이름 순으로 앞선 것을 쓰고, 뒤의 것은 경고를 남기고 건너뛴다
    void duplicateDocumentIdIsSkipped()
    {
        KnowledgeDocument first = MakeDocument("same", kLearnedFirst);
        first.strTitle = DecodeUtf8("첫 파일");
        KnowledgeDocument second = MakeDocument("same", kLearnedSecond);
        second.strTitle = DecodeUtf8("둘째 파일");
        WriteDocumentFile("a.md", first);
        WriteDocumentFile("b.md", second);

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        QCOMPARE(index.ListDocuments().size(), 1);
        const std::optional<KnowledgeDocument> optDocument = index.FindDocument("same");
        QVERIFY(optDocument.has_value());
        QCOMPARE(optDocument->strTitle, DecodeUtf8("첫 파일"));
        const QStringList vecWarnings = index.GetWarnings();
        QCOMPARE(vecWarnings.size(), 1);
        QVERIFY2(vecWarnings[0].contains("b.md") && vecWarnings[0].contains("duplicate id same"), qUtf8Printable(vecWarnings[0]));
    }

    // 방향 없는 종류는 A ↔ B 와 B ↔ A 가 선 하나다(작은 id 가 from). 방향 있는 종류는 두 방향이 다른 선이다.
    // 한 문서가 같은 선을 두 번 말해도 출처 문서는 한 번이다
    void undirectedRelationsMergeAcrossDirections()
    {
        WriteConceptFile(MakeConcept("yolo", "YOLO"));
        WriteConceptFile(MakeConcept("faster-r-cnn", "Faster R-CNN"));
        KnowledgeDocument documentA = MakeDocument("a", kLearnedFirst);
        documentA.vecRelations = {
            MakeRelation("yolo", "alternative", "faster-r-cnn", ERelationStatus::PROPOSED),
            MakeRelation("yolo", "improves", "faster-r-cnn"),
            MakeRelation("faster-r-cnn", "alternative", "yolo", ERelationStatus::PROPOSED),
        };
        KnowledgeDocument documentB = MakeDocument("b", kLearnedSecond);
        documentB.vecRelations = {
            MakeRelation("faster-r-cnn", "alternative", "yolo", ERelationStatus::PROPOSED),
            MakeRelation("faster-r-cnn", "improves", "yolo"),
        };
        WriteDocumentFile("a.md", documentA);
        WriteDocumentFile("b.md", documentB);

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        const QVector<KnowledgeEdge> vecEdges = index.ListEdges();
        QCOMPARE(DescribeEdges(vecEdges),
                 (QStringList { "faster-r-cnn alternative yolo", "yolo improves faster-r-cnn", "faster-r-cnn improves yolo" }));
        const KnowledgeEdge& alternative = vecEdges[0];
        QCOMPARE(alternative.vecDocumentIds, (QStringList { "a", "b" }));
        QCOMPARE(alternative.eStatus, ERelationStatus::PROPOSED);
        QVERIFY(!alternative.bFromConceptFile);
    }

    // 출처 하나라도 확인됨이면 선은 확인됨이다. 출처는 그 관계를 말한 문서 id 들과 개념 파일에서 왔는지다
    void confirmedSourceMakesEdgeConfirmed()
    {
        WriteConceptFile(MakeConcept("retinanet", "RetinaNet"), { MakeRelation("retinanet", "alternative", "yolov3") });
        WriteConceptFile(MakeConcept("yolov3", "YOLOv3"), { MakeRelation("yolov3", "isA", "detector", ERelationStatus::PROPOSED) });
        WriteConceptFile(MakeConcept("darknet-53", "Darknet-53"));
        WriteConceptFile(MakeConcept("detector", "Detector"));
        KnowledgeDocument document = MakeDocument("a", kLearnedFirst);
        document.vecRelations = {
            MakeRelation("yolov3", "alternative", "retinanet", ERelationStatus::PROPOSED),   // 개념 파일과 거꾸로 적었다
            MakeRelation("yolov3", "uses", "darknet-53", ERelationStatus::PROPOSED),
            MakeRelation("yolov3", "isA", "detector"),
        };
        WriteDocumentFile("a.md", document);

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        const QVector<KnowledgeEdge> vecEdges = index.ListEdges();
        QCOMPARE(DescribeEdges(vecEdges), (QStringList { "retinanet alternative yolov3", "yolov3 uses darknet-53", "yolov3 isA detector" }));

        // 문서의 AI 제안 + 개념 파일의 확인됨 → 확인됨, 출처 둘
        QCOMPARE(vecEdges[0].eStatus, ERelationStatus::CONFIRMED);
        QCOMPARE(vecEdges[0].vecDocumentIds, QStringList { "a" });
        QVERIFY(vecEdges[0].bFromConceptFile);
        // AI 제안뿐이면 AI 제안
        QCOMPARE(vecEdges[1].eStatus, ERelationStatus::PROPOSED);
        QCOMPARE(vecEdges[1].vecDocumentIds, QStringList { "a" });
        QVERIFY(!vecEdges[1].bFromConceptFile);
        // 먼저 온 확인됨은 뒤의 AI 제안으로 바뀌지 않는다
        QCOMPARE(vecEdges[2].eStatus, ERelationStatus::CONFIRMED);
        QCOMPARE(vecEdges[2].vecDocumentIds, QStringList { "a" });
        QVERIFY(vecEdges[2].bFromConceptFile);
    }

    // from 이나 to 의 개념 파일이 없는 관계는 선에 넣지 않고, 빠진 관계로 출처와 함께 남긴다(경고는 아니다).
    // 문서가 그 id 를 가리킨 것은 그대로 센다
    void relationsToMissingConceptsAreDropped()
    {
        WriteConceptFile(MakeConcept("yolov3", "YOLOv3"), { MakeRelation("yolov3", "isA", "ghost") });
        WriteConceptFile(MakeConcept("darknet-19", "Darknet-19"));
        KnowledgeDocument document = MakeDocument("a", kLearnedFirst);
        document.vecRelations = {
            MakeRelation("yolov3", "uses", "darknet-53", ERelationStatus::PROPOSED),
            MakeRelation("yolov3", "uses", "darknet-19"),
        };
        WriteDocumentFile("a.md", document);

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        QCOMPARE(DescribeEdges(index.ListEdges()), QStringList { "yolov3 uses darknet-19" });
        QCOMPARE(DescribeDropped(index.ListDroppedRelations()), (QStringList { "yolov3 uses darknet-53 @ a", "yolov3 isA ghost @ yolov3" }));
        QCOMPARE(index.ListDocumentIdsPointingTo("darknet-53"), QStringList { "a" });
        QVERIFY(!index.FindConcept("darknet-53").has_value());
        QVERIFY(!index.IsHidden("darknet-53"));
        QVERIFY2(index.GetWarnings().isEmpty(), qUtf8Printable(index.GetWarnings().join(QLatin1Char('\n'))));
    }

    // 어떤 문서도 가리키지 않고 개념 파일 관계에도 나오지 않는 개념은 숨는다. 숨은 개념도 id · 이름으로는 찾는다
    void unreferencedConceptsAreHidden()
    {
        KnowledgeDocument document = MakeDocument("a", kLearnedFirst);
        document.vecCards = { MakeCard({ "yolo" }) };
        WriteDocumentFile("a.md", document);
        WriteConceptFile(MakeConcept("yolo", "YOLO"));
        WriteConceptFile(MakeConcept("orphan", "Orphan"));

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        QCOMPARE(ListIds(index.ListConcepts()), QStringList { "yolo" });
        QCOMPARE(ListIds(index.ListConcepts(true)), (QStringList { "orphan", "yolo" }));
        QVERIFY(index.IsHidden("orphan"));
        QVERIFY(!index.IsHidden("yolo"));
        QVERIFY(!index.IsHidden("ghost"));   // 개념 파일이 없는 id 는 숨은 개념이 아니다
        QVERIFY(index.FindConcept("orphan").has_value());
        QCOMPARE(index.FindConceptIdByName("Orphan"), QString("orphan"));
    }

    // 개념 파일 관계의 양 끝은 다루는 문서가 없어도 보인다. 관계를 적은 개념 파일이라도 자기가 그 끝이 아니면 숨는다
    void conceptFileRelationsMakeConceptsVisible()
    {
        WriteConceptFile(MakeConcept("hand-a", "Hand A"), { MakeRelation("hand-a", "uses", "hand-b") });
        WriteConceptFile(MakeConcept("hand-b", "Hand B"));
        WriteConceptFile(MakeConcept("hand-c", "Hand C"), { MakeRelation("hand-d", "isA", "hand-e") });
        WriteConceptFile(MakeConcept("hand-d", "Hand D"));
        WriteConceptFile(MakeConcept("hand-e", "Hand E"));

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));   // documents/ 가 없어도 읽는다
        QCOMPARE(ListIds(index.ListConcepts()), (QStringList { "hand-a", "hand-b", "hand-d", "hand-e" }));
        QVERIFY(index.IsHidden("hand-c"));
        QVERIFY(index.ListDocumentIdsPointingTo("hand-b").isEmpty());
        const QVector<KnowledgeEdge> vecEdges = index.ListEdges();
        QCOMPARE(DescribeEdges(vecEdges), (QStringList { "hand-a uses hand-b", "hand-d isA hand-e" }));
        QVERIFY(vecEdges[0].bFromConceptFile);
        QVERIFY(vecEdges[0].vecDocumentIds.isEmpty());
        QCOMPARE(vecEdges[0].eStatus, ERelationStatus::CONFIRMED);   // 상태 줄이 없으면 확인됨
    }

    // 이름과 별칭이 이름 열쇠다(대소문자 · 공백 · - 를 보지 않는다). 열쇠가 겹치면 id 순으로 앞선 개념이 갖는다
    void nameKeysFindNamesAndAliases()
    {
        WriteConceptFile(MakeConcept("faster-r-cnn", "Faster R-CNN", { "FRCNN" }));
        WriteConceptFile(MakeConcept("b-net", "Net"));
        WriteConceptFile(MakeConcept("a-net", "Alpha Net", { "NET" }));

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        QCOMPARE(index.FindConceptIdByName("faster rcnn"), QString("faster-r-cnn"));
        QCOMPARE(index.FindConceptIdByName("Faster-RCNN"), QString("faster-r-cnn"));
        QCOMPARE(index.FindConceptIdByName("frcnn"), QString("faster-r-cnn"));
        QCOMPARE(index.FindConceptIdByName("net"), QString("a-net"));   // b-net 의 이름보다 a-net 의 별칭이 앞선다
        QCOMPARE(index.FindConceptIdByName("alpha-net"), QString("a-net"));
        QCOMPARE(index.FindConceptIdByName("unknown"), QString());
        QCOMPARE(index.FindConceptIdByName(QString()), QString());
    }

    // ListConcepts 는 이름 열쇠 순이고, 열쇠가 같으면 id 순이다
    void conceptsAreSortedByNameKey()
    {
        WriteConceptFile(MakeConcept("a", "Beta"));
        WriteConceptFile(MakeConcept("c", "al-pha"));
        WriteConceptFile(MakeConcept("b", "Alpha"));

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        QCOMPARE(ListIds(index.ListConcepts(true)), (QStringList { "b", "c", "a" }));
    }

    // 문서는 머리글 id 로 찾고, learned 가 늦은 것부터 준다(같으면 id 순). 파일 이름과는 상관없다
    void documentsAreOrderedByLearned()
    {
        WriteDocumentFile("first.md", MakeDocument("c", kLearnedFirst));
        WriteDocumentFile("second.md", MakeDocument("b", kLearnedThird));
        WriteDocumentFile("third.md", MakeDocument("a", kLearnedFirst));

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        QCOMPARE(ListIds(index.ListDocuments()), (QStringList { "b", "a", "c" }));
        const std::optional<KnowledgeDocument> optDocument = index.FindDocument("b");
        QVERIFY(optDocument.has_value());
        QCOMPARE(optDocument->strLearned, QString(kLearnedThird));
        QVERIFY(!index.FindDocument("second").has_value());   // 파일 이름은 id 가 아니다
    }

    // 개념을 가리키는 문서: 카드 `개념:` 링크 · 관계 끝 · 표 concepts. 한 문서는 한 번이고 ListDocuments 순이다
    void documentIdsPointingToConcept()
    {
        for (const char* pId : { "yolo", "yolov2", "other" })
            WriteConceptFile(MakeConcept(pId, pId));
        KnowledgeDocument byCard = MakeDocument("by-card", kLearnedSecond);
        byCard.vecCards = { MakeCard({ "yolo" }) };
        KnowledgeDocument byRelation = MakeDocument("by-relation", kLearnedThird);
        byRelation.vecCards = { MakeCard({ "yolo" }) };
        byRelation.vecRelations = { MakeRelation("yolov2", "improves", "yolo") };
        KnowledgeDocument byTable = MakeDocument("by-table", kLearnedFirst);
        byTable.vecTables = { MakeTable({ "yolo" }) };
        KnowledgeDocument elsewhere = MakeDocument("elsewhere", kLearnedFourth);
        elsewhere.vecCards = { MakeCard({ "other" }) };
        for (const KnowledgeDocument& document : { byCard, byRelation, byTable, elsewhere })
            WriteDocumentFile(document.strId + QStringLiteral(".md"), document);

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        QCOMPARE(index.ListDocumentIdsPointingTo("yolo"), (QStringList { "by-relation", "by-card", "by-table" }));
        QCOMPARE(index.ListDocumentIdsPointingTo("yolov2"), QStringList { "by-relation" });
        QCOMPARE(index.ListDocumentIdsPointingTo("other"), QStringList { "elsewhere" });
        QVERIFY(index.ListDocumentIdsPointingTo("ghost").isEmpty());
    }

    // 설명은 출처 순(문서 id 순, 그다음 개념 파일 id 순)으로 모은다. 빈 설명과 글자까지 같은 설명은 뺀다
    void notesAreMergedInSourceOrder()
    {
        const QString strDocumentNote = DecodeUtf8("문서 설명");
        WriteConceptFile(MakeConcept("yolov2", "YOLOv2"),
                         { MakeRelation("yolov2", "improves", "yolo", ERelationStatus::CONFIRMED, DecodeUtf8("yolov2 파일 설명")) });
        WriteConceptFile(MakeConcept("yolo", "YOLO"),
                         { MakeRelation("yolov2", "improves", "yolo", ERelationStatus::CONFIRMED, DecodeUtf8("yolo 파일 설명")) });
        // 파일 이름 순과 id 순을 거꾸로 둔다
        KnowledgeDocument documentC = MakeDocument("c", kLearnedFirst);
        documentC.vecRelations = { MakeRelation("yolov2", "improves", "yolo", ERelationStatus::PROPOSED, strDocumentNote) };
        KnowledgeDocument documentB = MakeDocument("b", kLearnedFirst);
        documentB.vecRelations = { MakeRelation("yolov2", "improves", "yolo", ERelationStatus::PROPOSED, strDocumentNote) };
        KnowledgeDocument documentA = MakeDocument("a", kLearnedFirst);
        documentA.vecRelations = { MakeRelation("yolov2", "improves", "yolo", ERelationStatus::PROPOSED) };
        WriteDocumentFile("1.md", documentC);
        WriteDocumentFile("2.md", documentB);
        WriteDocumentFile("3.md", documentA);

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        const QVector<KnowledgeEdge> vecEdges = index.ListEdges();
        QCOMPARE(vecEdges.size(), 1);
        QCOMPARE(vecEdges[0].vecDocumentIds, (QStringList { "a", "b", "c" }));
        QVERIFY(vecEdges[0].bFromConceptFile);
        QCOMPARE(vecEdges[0].eStatus, ERelationStatus::CONFIRMED);
        QCOMPARE(vecEdges[0].vecNotes, (QStringList { strDocumentNote, DecodeUtf8("yolo 파일 설명"), DecodeUtf8("yolov2 파일 설명") }));
    }

    // 선은 처음 나온 순서다: 문서를 id 순으로, 그다음 개념 파일을 id 순으로 훑는다. 빠진 관계도 같은 순서다
    void edgesKeepFirstAppearanceOrder()
    {
        WriteConceptFile(MakeConcept("p", "P"), { MakeRelation("p", "isA", "r") });
        WriteConceptFile(MakeConcept("q", "Q"));
        WriteConceptFile(MakeConcept("r", "R"), { MakeRelation("r", "uses", "p"), MakeRelation("r", "uses", "ghost") });
        KnowledgeDocument documentB = MakeDocument("b", kLearnedFirst);
        documentB.vecRelations = { MakeRelation("q", "uses", "r"), MakeRelation("q", "uses", "ghost-b") };
        KnowledgeDocument documentA = MakeDocument("a", kLearnedFirst);
        documentA.vecRelations = { MakeRelation("p", "uses", "q"), MakeRelation("q", "uses", "r") };
        WriteDocumentFile("1.md", documentB);
        WriteDocumentFile("2.md", documentA);

        KnowledgeIndex index;
        QVERIFY(index.Load(GetWikiPath()));
        const QVector<KnowledgeEdge> vecEdges = index.ListEdges();
        QCOMPARE(DescribeEdges(vecEdges), (QStringList { "p uses q", "q uses r", "p isA r", "r uses p" }));
        QCOMPARE(vecEdges[1].vecDocumentIds, (QStringList { "a", "b" }));
        QCOMPARE(DescribeDropped(index.ListDroppedRelations()), (QStringList { "q uses ghost-b @ b", "r uses ghost @ r" }));
    }

    // wiki 폴더가 없거나 documents/ · concepts/ 가 다 없으면 false 와 경고 하나다. 다시 읽으면 앞서 읽은 것은 지운다
    void loadNeedsWikiFolderAndReplacesEarlierResults()
    {
        KnowledgeIndex index;
        QVERIFY(!index.Load(GetWikiPath()));   // 아직 wiki 폴더가 없다
        QCOMPARE(index.GetWarnings().size(), 1);
        QVERIFY(QDir().mkpath(GetWikiPath() + QStringLiteral("/attachments")));
        QVERIFY(!index.Load(GetWikiPath()));   // 두 폴더가 다 없다
        QCOMPARE(index.GetWarnings().size(), 1);

        KnowledgeDocument document = MakeDocument("a", kLearnedFirst);
        document.vecRelations = { MakeRelation("yolov2", "improves", "yolo"), MakeRelation("yolo", "uses", "ghost") };
        WriteDocumentFile("a.md", document);
        WriteConceptFile(MakeConcept("yolo", "YOLO"));
        WriteConceptFile(MakeConcept("yolov2", "YOLOv2"));
        WriteFile("concepts/broken.md", DecodeUtf8("# 머리글 없음\n"));
        QVERIFY(index.Load(GetWikiPath()));
        QCOMPARE(index.GetWarnings().size(), 1);   // broken.md
        QCOMPARE(index.ListDocuments().size(), 1);
        QCOMPARE(index.ListConcepts().size(), 2);
        QCOMPARE(index.ListEdges().size(), 1);
        QCOMPARE(index.ListDroppedRelations().size(), 1);

        // 빈 경로는 QDir 에서 지금 폴더가 되지만 읽지 않는다. 지금 폴더를 이 wiki 로 옮겨 본다(임시 폴더를 지우기 전에 되돌린다)
        const QString strCurrent = QDir::currentPath();
        QVERIFY(QDir::setCurrent(GetWikiPath()));
        const bool bLoadedEmptyPath = index.Load(QString());
        QVERIFY(QDir::setCurrent(strCurrent));
        QVERIFY(!bLoadedEmptyPath);
        QCOMPARE(index.GetWarnings().size(), 1);   // 앞의 broken.md 경고는 남지 않는다
        QVERIFY(index.ListDocuments().isEmpty());
        QVERIFY(!index.FindDocument("a").has_value());
        QVERIFY(index.ListConcepts(true).isEmpty());
        QVERIFY(!index.FindConcept("yolo").has_value());
        QVERIFY(index.FindConceptIdByName("YOLO").isEmpty());
        QVERIFY(index.ListEdges().isEmpty());
        QVERIFY(index.ListDroppedRelations().isEmpty());
        QVERIFY(index.ListDocumentIdsPointingTo("yolo").isEmpty());
    }

private:
    QString GetWikiPath() const
    {
        return m_upDir->filePath(QStringLiteral("wiki"));
    }

    // wiki 폴더 안의 relativePath 에 글을 UTF-8 로 쓴다(폴더가 없으면 만든다)
    void WriteFile(const QString& relativePath, const QString& text) const
    {
        const QString strPath = GetWikiPath() + QLatin1Char('/') + relativePath;
        QVERIFY(QDir().mkpath(QFileInfo(strPath).absolutePath()));
        QFile file(strPath);
        QVERIFY2(file.open(QIODevice::WriteOnly | QIODevice::Truncate), qUtf8Printable(strPath));
        const QByteArray baText = text.toUtf8();
        QCOMPARE(file.write(baText), static_cast<qint64>(baText.size()));
    }

    void WriteDocumentFile(const QString& fileName, const KnowledgeDocument& document) const
    {
        WriteFile(QStringLiteral("documents/") + fileName, KnowledgeMarkdownWriter::WriteDocument(document));
    }

    // 파일 이름이 머리글 id 와 같은 개념 파일
    void WriteConceptFile(const KnowledgeConcept& concept, const QVector<KnowledgeRelation>& relations = QVector<KnowledgeRelation>()) const
    {
        WriteConceptFileAs(concept.strId + QStringLiteral(".md"), concept, relations);
    }

    void WriteConceptFileAs(const QString& fileName, const KnowledgeConcept& concept,
                            const QVector<KnowledgeRelation>& relations = QVector<KnowledgeRelation>()) const
    {
        QString strText = KnowledgeMarkdownWriter::WriteConcept(concept);
        if (!relations.isEmpty())
            strText += QLatin1Char('\n') + MakeRelationSection(relations);
        WriteFile(QStringLiteral("concepts/") + fileName, strText);
    }

private:
    std::unique_ptr<QTemporaryDir> m_upDir;
};

std::unique_ptr<QObject> CreateKnowledgeIndexTest() { return std::make_unique<KnowledgeIndexTest>(); }

#include "KnowledgeIndexTest.moc"
