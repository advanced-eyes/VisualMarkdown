#include "VisualMarkdown/KnowledgeMarkdownReader.h"
#include "VisualMarkdown/KnowledgeText.h"
#include "VisualMarkdown/KnowledgeTypes.h"
#include "VisualMarkdown/KnowledgeVocabulary.h"
#include "WikiTestFiles.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QTest>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <utility>

namespace
{
// ================================================================================================
// 기대값: examples/showcase/README.md 에 적은 것이다. 예시를 고치면 README 와 여기를 같은 커밋에서 고친다(README §5).
// 선은 "from 종류 to" 글로 적는다(개념 id 와 종류 코드를 공백 하나로 잇는다. id 에는 공백이 없다).
// 목록은 순서를 보지 않는다. 순서를 보는 것은 문서 순서와 표의 y 뿐이다.
// ================================================================================================

// 문서 파일 이름(확장자 뺀 것)
constexpr const char* kYolo = "you-only-look-once-unified-real-time-object-detection";
constexpr const char* kYolo9000 = "yolo9000-better-faster-stronger";
constexpr const char* kYolov3 = "yolov3-an-incremental-improvement";
constexpr const char* kTwoStageNote = "2단계-검출기의-흐름";
constexpr const char* kFocalLoss = "focal-loss-for-dense-object-detection";
constexpr const char* kPaymentPlan = "결제-서비스-개편-기획서";

struct NameCount
{
    const char* pName;
    int iCount;
};

// §1 · §4 파일
constexpr int kDocumentFileCount = 6;
constexpr int kConceptFileCount = 32;
constexpr int kConceptKindCount = 7;                 // 어휘의 개념 종류 모두가 쓰인다
constexpr int kConceptsWithRelationSection = 11;     // `## 관계` 가 있는 개념 파일
constexpr int kConceptsWithAliases = 11;
const QStringList kKoreanConceptIds { "1단계-검출기", "roi-풀링", "결제-모듈-2판" };

// §2.1 관계 줄 → 선
constexpr int kDocumentRelationLines = 27;
constexpr int kConceptRelationLines = 13;
constexpr int kRelationLines = 40;
constexpr int kEdgeCount = 36;
const QVector<NameCount> kEdgesByKind { { "improves", 5 }, { "uses", 9 }, { "isA", 12 }, { "solves", 5 }, { "alternative", 5 } };
constexpr int kDirectedEdges = 31;
constexpr int kUndirectedEdges = 5;
constexpr int kConfirmedEdges = 25;
constexpr int kProposedEdges = 11;

// §2.1 합쳐지는 선(관계 줄 둘 이상이 선 하나가 된 것)
struct ExpectedMergedEdge
{
    const char* pEdge;
    ERelationStatus eStatus;     // 하나라도 확인됨이면 확인됨
    QStringList vecDocuments;    // 이 관계를 말한 문서
    bool bConceptFile;           // 개념 파일에도 적혔다
};
const QVector<ExpectedMergedEdge> kMergedEdges {
    { "yolov2 improves yolo", ERelationStatus::CONFIRMED, { kYolo9000, kYolov3 }, false },
    { "retinanet alternative yolov3", ERelationStatus::CONFIRMED, { kYolov3 }, true },   // 문서는 YOLOv3 ↔ RetinaNet 으로 적었다
    { "faster-r-cnn solves 객체-검출", ERelationStatus::CONFIRMED, { kYolo }, true },
};

// §2.1 빠지는 선(개념 파일이 없는 쪽을 가리킨다)
const QStringList kDroppedRelations { "yolov3 uses darknet-53" };

// §2.1 점
const QStringList kHiddenConcepts { "dpm" };                                           // 아무도 가리키지 않는다
constexpr int kVisibleConcepts = 31;
const QStringList kConceptsWithoutDocuments { "detr", "검출기", "거래-로그" };            // 개념 파일 관계로만 보인다
const QStringList kConceptsWithoutEdges { "coco", "pascal-voc", "승인율", "응답-시간" };   // 카드 · 표만 가리킨다
const QList<int> kComponentSizes { 19, 8, 1, 1, 1, 1 };                               // 덩어리(방향을 보지 않고 이어진 점들), 큰 것부터

// §2.1 다루는 문서 수(한 문서는 한 개념을 한 번 센다). 여기와 kConceptsWithoutDocuments 에 없는 보이는 점은 1개다
const QVector<NameCount> kConceptsInSeveralDocuments {
    { "fast-r-cnn", 3 }, { "faster-r-cnn", 3 }, { "yolo", 3 }, { "pascal-voc", 2 }, { "retinanet", 2 }, { "yolov2", 2 },
};

// §2.1 문서 순서(learned 늦은 것부터)
const QStringList kDocumentsByLearned { kPaymentPlan, kFocalLoss, kTwoStageNote, kYolov3, kYolo9000, kYolo };

// §2.2 계보도: 개선 선(새것 → 옛것). 다섯 모두 설명이 있다
const QStringList kImprovesEdges {
    "fast-r-cnn improves r-cnn", "faster-r-cnn improves fast-r-cnn", "yolov2 improves yolo", "yolov3 improves yolov2",
    "결제-모듈-2판 improves 결제-모듈",
};
const QStringList kSameYearImprovesEdges { "faster-r-cnn improves fast-r-cnn" };   // 같은 해(2015)의 두 판

// §2.2 분류 트리: 종류 선(하위 → 상위). AI 제안은 RetinaNet 의 것뿐이다
const QStringList kIsAEdges {
    "1단계-검출기 isA 검출기", "2단계-검출기 isA 검출기", "detr isA 검출기",
    "yolo isA 1단계-검출기", "yolov2 isA 1단계-검출기", "yolov3 isA 1단계-검출기", "ssd isA 1단계-검출기", "retinanet isA 1단계-검출기",
    "r-cnn isA 2단계-검출기", "fast-r-cnn isA 2단계-검출기", "faster-r-cnn isA 2단계-검출기",
    "결제-모듈-2판 isA 결제-모듈",
};
const QStringList kProposedIsAEdges { "retinanet isA 1단계-검출기" };

// §2.2 타임라인: 연도 있는 보이는 점. 숨은 dpm 의 연도는 나오지 않는다
const QVector<NameCount> kVisibleConceptsPerYear {
    { "2014", 2 }, { "2015", 5 }, { "2016", 2 }, { "2017", 4 }, { "2018", 1 }, { "2020", 1 }, { "2026", 1 },
};
constexpr int kVisibleConceptsWithYear = 16;
constexpr int kVisibleConceptsWithoutYear = 15;
constexpr const char* kHiddenDpmYear = "2008";

// §2.2 관계 행렬: 종류 둘 이상이 겹치는 칸(두 id 는 글자 순)
const QStringList kPairsWithSeveralKinds { "결제-모듈 결제-모듈-2판: improves isA" };

// §2.2 문서 ↔ 개념: 문서마다 가리키는 개념 수(개념 파일이 없는 darknet-53 도 센다)
const QVector<NameCount> kPointedConceptsPerDocument {
    { kYolo, 5 }, { kYolo9000, 7 }, { kYolov3, 6 }, { kTwoStageNote, 7 }, { kFocalLoss, 4 }, { kPaymentPlan, 9 },
};
constexpr const char* kMissingConcept = "darknet-53";   // YOLOv3 문서가 가리키지만 개념 파일이 없다

// §3 표 제목. README 는 앞의 두 표 이름에서 " — " 뒤 부제를 뺐다
constexpr const char* kSpeedTable = "실시간 검출기와 그 밖의 검출기 — 정확도와 속도";
constexpr const char* kStepsTable = "YOLO 에서 YOLOv2 까지 — 하나씩 더한 개선";
constexpr const char* kInputSizeTable = "입력 크기에 따른 정확도와 속도";
constexpr const char* kVocDetectorsTable = "검출기별 VOC 2007 정확도";
constexpr const char* kCocoTable = "입력 크기별 COCO 정확도";
constexpr const char* kTimeTable = "처리 시간과 AP50";
constexpr const char* kTwoStageVocTable = "2단계 검출기의 VOC 2007 정확도";
constexpr const char* kTrainTestTimeTable = "학습 · 시험 시간";
constexpr const char* kMonthlyTable = "월별 응답 시간";
constexpr const char* kPaymentMethodTable = "결제 수단별 비중";
constexpr const char* kLoadTable = "동시 사용자와 응답 시간";
constexpr const char* kPriorityTable = "기능 우선순위";

// §3 표마다 x(열 종류) · y · series · cumulative
struct ExpectedTable
{
    const char* pDocument;
    const char* pTitle;
    const char* pX;
    EColumnKind eXKind;
    QStringList vecY;
    const char* pSeries;
    bool bCumulative;
};
constexpr int kTableCount = 12;
const QVector<ExpectedTable> kTables {
    { kYolo, kSpeedTable, "FPS", EColumnKind::QUANTITY, { "mAP" }, "구분", false },
    { kYolo9000, kStepsTable, "단계", EColumnKind::ORDINAL, { "mAP" }, "", true },
    { kYolo9000, kInputSizeTable, "입력 크기", EColumnKind::ORDINAL, { "mAP", "FPS" }, "", false },
    { kYolo9000, kVocDetectorsTable, "검출기", EColumnKind::NOMINAL, { "mAP" }, "", false },
    { kYolov3, kCocoTable, "입력 크기", EColumnKind::ORDINAL, { "AP", "AP50" }, "", false },
    { kYolov3, kTimeTable, "시간", EColumnKind::QUANTITY, { "AP50" }, "", false },
    { kTwoStageNote, kTwoStageVocTable, "방법", EColumnKind::NOMINAL, { "mAP" }, "", false },
    { kTwoStageNote, kTrainTestTimeTable, "방법", EColumnKind::NOMINAL, { "학습 시간", "시험 시간" }, "", false },
    { kPaymentPlan, kMonthlyTable, "월", EColumnKind::ORDINAL, { "p50", "p95" }, "", false },
    { kPaymentPlan, kPaymentMethodTable, "결제 수단", EColumnKind::NOMINAL, { "비중" }, "", false },
    { kPaymentPlan, kLoadTable, "동시 사용자", EColumnKind::QUANTITY, { "응답 시간" }, "", false },
    { kPaymentPlan, kPriorityTable, "기능", EColumnKind::NOMINAL, { "우선순위" }, "", false },
};

// §3 y 가 수치가 아닌 표(그리지 않고 표로 보인다)
const QStringList kTablesWithNonQuantityY { kPriorityTable };

// §3 빈 칸(표 · 행의 첫 칸 · 열)
struct ExpectedCell
{
    const char* pTableTitle;
    const char* pRow;
    const char* pColumn;
};
const QVector<ExpectedCell> kEmptyCells { { kTrainTestTimeTable, "Faster R-CNN", "학습 시간" }, { kMonthlyTable, "7월", "p95" } };

// §3 그 밖: 축이 아닌 열과 숫자 열의 종류
struct ExpectedColumn
{
    const char* pTableTitle;
    const char* pColumn;
    EColumnKind eKind;
};
const QVector<ExpectedColumn> kColumnKinds {
    { kInputSizeTable, "입력 크기", EColumnKind::ORDINAL },   // 숫자 열인데 ordered 라 순서
    { kCocoTable, "입력 크기", EColumnKind::ORDINAL },
    { kTimeTable, "입력 크기", EColumnKind::QUANTITY },       // ordered 가 없는 숫자 열(축이 아니다)
    { kLoadTable, "동시 사용자", EColumnKind::QUANTITY },     // 천 단위 쉼표(1,000)
    { kSpeedTable, "학습", EColumnKind::NOMINAL },            // 축이 아닌 범주 열(2007+2012)
};

// §3 그 밖: y 열의 단위(y 순서대로)
struct ExpectedUnits
{
    const char* pTableTitle;
    QStringList vecUnits;
};
const QVector<ExpectedUnits> kYUnits {
    { kInputSizeTable, { "%", "" } },       // 단위가 다르다(% · 없음)
    { kCocoTable, { "%", "%" } },           // 단위가 같은 y 둘
    { kTrainTestTimeTable, { "h", "s" } },  // 단위가 다르다(h · s)
};

// §3 그 밖 · 시험하는 것
const QVector<NameCount> kTableRows { { kVocDetectorsTable, 7 }, { kTwoStageVocTable, 3 } };
const QStringList kSpeedSeries { "실시간", "실시간 아님" };       // 계열 2개
constexpr double kSpeedXMin = 0.5;                                 // 넓은 x 범위(0.5–155)
constexpr double kSpeedXMax = 155;
const QStringList kStepsDecreasing { "69.5 → 69.2" };             // 줄어드는 단계
constexpr const char* kQuotedWord = "\"취소\"";                    // 결제 수단별 비중의 condition. 파일에는 &quot; 로 적는다
constexpr const char* kEscapedQuotedWord = "&quot;취소&quot;";
constexpr const char* kPipeCell = "간편결제 (카드 | 계좌)";         // 파일에는 \| 로 적는다
constexpr const char* kEscapedPipeCell = "간편결제 (카드 \\| 계좌)";
constexpr const char* kThousandsCell = "1,000";                    // 동시 사용자 열

// §4 형식 경계 사례
constexpr const char* kYoloMemo = "메모: 표 1 의 실시간 검출기와 비교";   // YOLO 둘째 카드의 모르는 하위 항목
constexpr const char* kRpnLinkName = "Region Proposal Network";             // 2단계 노트 카드의 링크 글자. 개념 이름은 RPN
constexpr const char* kUnitlessNumber = "9000";                               // YOLO9000 핵심 숫자, 단위 없음
constexpr const char* kUnitlessNumberLabel = "검출할 수 있는 클래스 수";
constexpr const char* kPercentNumber = "99.5";                                // 결제 기획서 핵심 숫자, 단위 %
constexpr int kAnalyzedDocuments = 5;                                         // analysis 가 있는 문서(손으로 쓴 노트만 없다)

// ================================================================================================

QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

// examples/showcase/wiki 안의 경로(CMake 가 VISUALMARKDOWN_SHOWCASE_DIR 로 절대 경로를 넣는다)
QString MakeShowcasePath(const QString& relativePath)
{
    return QStringLiteral(VISUALMARKDOWN_SHOWCASE_DIR) + QLatin1Char('/') + relativePath;
}

// 순서를 보지 않고 두 목록을 견준다. 같으면 빈 글, 다르면 무엇이 빠지고 무엇이 더 있는지 적는다(겹친 항목도 센다).
// QCOMPARE 는 긴 글을 잘라(한국어는 \uXXXX 로) 보여 주어 목록이 길면 다른 곳이 보이지 않는다.
QString DiffItems(const QString& what, const QStringList& actual, const QStringList& expected)
{
    QStringList vecMissing = expected;
    QStringList vecExtra;
    for (const QString& strItem : actual)
        if (!vecMissing.removeOne(strItem))
            vecExtra << strItem;
    if (vecMissing.isEmpty() && vecExtra.isEmpty())
        return QString();
    vecMissing.sort();
    vecExtra.sort();
    return what + QString::fromUtf8(": 빠진 것 [") + vecMissing.join(QStringLiteral(", ")) + QString::fromUtf8("] · 더 있는 것 [")
        + vecExtra.join(QStringLiteral(", ")) + QLatin1Char(']');
}

QStringList ToList(const QSet<QString>& items) { return QStringList(items.cbegin(), items.cend()); }

// 정렬해 ", " 로 잇는다(짧은 목록을 한 글에 넣을 때)
QString JoinSorted(QStringList items)
{
    items.sort();
    return items.join(QStringLiteral(", "));
}

// 셈을 "이름 수" 글의 목록으로 바꾼다
QStringList MakeCountItems(const QMap<QString, int>& counts)
{
    QStringList vecItems;
    for (auto it = counts.cbegin(); it != counts.cend(); ++it)
        vecItems << it.key() + QLatin1Char(' ') + QString::number(it.value());
    return vecItems;
}

QStringList MakeCountItems(const QVector<NameCount>& counts)
{
    QStringList vecItems;
    for (const NameCount& count : counts)
        vecItems << DecodeUtf8(count.pName) + QLatin1Char(' ') + QString::number(count.iCount);
    return vecItems;
}

QString JoinNumbers(const QList<int>& numbers)
{
    QStringList vecItems;
    for (const int iNumber : numbers)
        vecItems << QString::number(iNumber);
    return vecItems.join(QLatin1Char(' '));
}

QString MakeEdgeText(const QString& from, const QString& kind, const QString& to)
{
    return from + QLatin1Char(' ') + kind + QLatin1Char(' ') + to;
}

QString GetKindName(EColumnKind kind)
{
    if (kind == EColumnKind::QUANTITY)
        return QStringLiteral("quantity");
    if (kind == EColumnKind::ORDINAL)
        return QStringLiteral("ordinal");
    return QStringLiteral("nominal");
}

// 반복 안에서 견줄 때 쓰는 글. 실패 메시지에 어느 표 · 선인지 함께 보인다.
QString DescribeCell(const QString& table, const QString& row, const QString& column)
{
    return table + QStringLiteral(" / ") + row + QStringLiteral(" / ") + column;
}

QString DescribeColumnKind(const QString& table, const QString& column, EColumnKind kind)
{
    return table + QStringLiteral(" / ") + column + QStringLiteral(": ") + GetKindName(kind);
}

QString DescribeMergedEdge(const QString& edge, ERelationStatus status, const QString& documents, bool conceptFile)
{
    return edge + QStringLiteral(": ") + (status == ERelationStatus::CONFIRMED ? QStringLiteral("confirmed") : QStringLiteral("proposed"))
        + QString::fromUtf8(", 출처 문서 [") + documents + QString::fromUtf8("], 개념 파일 ")
        + (conceptFile ? QString::fromUtf8("있음") : QString::fromUtf8("없음"));
}

int FindColumn(const KnowledgeTable& table, const QString& name)
{
    for (int iColumn = 0; iColumn < table.vecColumns.size(); ++iColumn)
        if (table.vecColumns[iColumn].strName == name)
            return iColumn;
    return -1;
}

struct ShowcaseDocument
{
    QString strName;   // 파일 이름(확장자 뺀 것)
    QString strText;   // 파일 글 그대로
    KnowledgeDocument document;
};

struct ShowcaseConcept
{
    KnowledgeConcept concept;
    bool bHasRelationSection = false;
};

// 합친 선 하나
struct IndexEdge
{
    QString strFrom, strKind, strTo;
    ERelationStatus eStatus = ERelationStatus::PROPOSED;
    int iLines = 0;               // 이 선이 된 관계 줄 수
    QSet<QString> setDocuments;   // 이 관계를 말한 문서
    bool bConceptFile = false;    // 개념 파일에도 적혔다
    bool bHasNote = false;        // 어느 줄에든 설명이 있다
};

// 색인 규칙(format/Visualization.md §2)대로 모은 것. 라이브러리에는 아직 색인이 없어(README §7 의 4단계) 시험이 직접 센다.
struct ShowcaseIndex
{
    QMap<QString, IndexEdge> mapEdges;         // "from 종류 to" 글(MakeEdgeText) → 선
    QStringList vecDropped;                    // 개념 파일이 없는 쪽을 가리켜 빠진 관계("from 종류 to" 글)
    QMap<QString, QSet<QString>> mapPointed;   // 문서 → 그 문서가 가리키는 개념 id(카드 개념 · 관계 from/to · 표 concepts)
    QSet<QString> setReferenced;               // 문서가 가리키거나 개념 파일 관계에 나오는 개념 id
};

ShowcaseIndex BuildIndex(const QVector<ShowcaseDocument>& documents, const QMap<QString, ShowcaseConcept>& concepts)
{
    const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
    ShowcaseIndex index;
    // 관계 줄 하나를 선에 더한다. document 가 비면 개념 파일의 줄이다.
    // from 이나 to 의 개념 파일이 없으면 넣지 않고, 방향 없는 종류는 두 id 를 글자 순으로 놓아 합친다.
    const auto funcAddRelation = [&](const KnowledgeRelation& relation, const QString& document)
    {
        QString strFrom = relation.fromConcept.strConceptId;
        QString strTo = relation.toConcept.strConceptId;
        if (!concepts.contains(strFrom) || !concepts.contains(strTo))
        {
            index.vecDropped << MakeEdgeText(strFrom, relation.strKind, strTo);
            return;
        }
        if (vocabulary.IsUndirected(relation.strKind) && strTo < strFrom)
            std::swap(strFrom, strTo);
        IndexEdge& edge = index.mapEdges[MakeEdgeText(strFrom, relation.strKind, strTo)];
        edge.strFrom = strFrom;
        edge.strKind = relation.strKind;
        edge.strTo = strTo;
        ++edge.iLines;
        if (relation.eStatus == ERelationStatus::CONFIRMED)
            edge.eStatus = ERelationStatus::CONFIRMED;
        if (!relation.strNote.isEmpty())
            edge.bHasNote = true;
        if (document.isEmpty())
            edge.bConceptFile = true;
        else
            edge.setDocuments.insert(document);
    };

    for (const ShowcaseDocument& doc : documents)
    {
        QSet<QString>& setPointed = index.mapPointed[doc.strName];
        for (const KnowledgeCard& card : doc.document.vecCards)
            for (const ConceptLink& link : card.vecConcepts)
                setPointed.insert(link.strConceptId);
        for (const KnowledgeRelation& relation : doc.document.vecRelations)
        {
            setPointed.insert(relation.fromConcept.strConceptId);
            setPointed.insert(relation.toConcept.strConceptId);
            funcAddRelation(relation, doc.strName);
        }
        for (const KnowledgeTable& table : doc.document.vecTables)
            for (const QString& strConceptId : table.vecConceptIds)
                setPointed.insert(strConceptId);
        index.setReferenced += setPointed;
    }
    for (const ShowcaseConcept& concept : concepts)
    {
        for (const KnowledgeRelation& relation : concept.concept.vecRelations)
        {
            index.setReferenced.insert(relation.fromConcept.strConceptId);
            index.setReferenced.insert(relation.toConcept.strConceptId);
            funcAddRelation(relation, QString());
        }
    }
    return index;
}

// 선으로 이어진 점들의 덩어리 크기(방향은 보지 않는다), 큰 것부터
QList<int> CountComponentSizes(const QStringList& concepts, const QMap<QString, IndexEdge>& edges)
{
    QHash<QString, QStringList> hashNeighbors;
    for (const IndexEdge& edge : edges)
    {
        hashNeighbors[edge.strFrom] << edge.strTo;
        hashNeighbors[edge.strTo] << edge.strFrom;
    }
    QSet<QString> setSeen;
    QList<int> vecSizes;
    for (const QString& strStart : concepts)
    {
        if (setSeen.contains(strStart))
            continue;
        setSeen.insert(strStart);
        QStringList vecQueue { strStart };
        int iSize = 0;
        while (!vecQueue.isEmpty())
        {
            const QString strId = vecQueue.takeFirst();
            ++iSize;
            for (const QString& strNext : hashNeighbors.value(strId))
            {
                if (setSeen.contains(strNext))
                    continue;
                setSeen.insert(strNext);
                vecQueue << strNext;
            }
        }
        vecSizes << iSize;
    }
    std::sort(vecSizes.begin(), vecSizes.end(), std::greater<int>());
    return vecSizes;
}
}

// examples/showcase/wiki(보기를 시험하는 예시)가 라이브러리가 쓰는 정준 모양인지,
// 그리고 examples/showcase/README.md 의 기대값이 라이브러리가 읽은 기록에서 그대로 나오는지 본다.
// 실패하면 기대값을 시험에 맞춰 고치지 않는다. 예시 · README · 라이브러리 가운데 틀린 쪽을 찾는다.
class ShowcaseWikiTest : public QObject
{
    Q_OBJECT

private slots:
    // 모든 파일을 한 번 읽어 기록과 색인을 만든다
    void initTestCase()
    {
        for (const QString& strPath : WikiTestFiles::ListMarkdownFiles(MakeShowcasePath(WikiTestFiles::kDocumentsFolder)))
        {
            QByteArray baRaw;
            QVERIFY2(WikiTestFiles::ReadBytes(strPath, &baRaw), qUtf8Printable(strPath));
            ShowcaseDocument doc;
            doc.strName = QFileInfo(strPath).completeBaseName();
            doc.strText = QString::fromUtf8(baRaw);
            const DocumentReadResult result = KnowledgeMarkdownReader::ReadDocument(doc.strText);
            QVERIFY2(result.bOk, qUtf8Printable(strPath + DecodeUtf8(": ") + result.strError));
            doc.document = result.document;
            m_vecDocuments.push_back(doc);
        }
        for (const QString& strPath : WikiTestFiles::ListMarkdownFiles(MakeShowcasePath(WikiTestFiles::kConceptsFolder)))
        {
            QByteArray baRaw;
            QVERIFY2(WikiTestFiles::ReadBytes(strPath, &baRaw), qUtf8Printable(strPath));
            const QString strText = QString::fromUtf8(baRaw);
            const ConceptReadResult result = KnowledgeMarkdownReader::ReadConcept(strText);
            QVERIFY2(result.bOk, qUtf8Printable(strPath + DecodeUtf8(": ") + result.strError));
            // 선을 "from 종류 to" 글로 모으므로 id 에 공백이 없어야 한다(Folder.md §2 로 만든 id 에는 없다)
            const QString strId = QFileInfo(strPath).completeBaseName();
            QVERIFY2(!strId.contains(QLatin1Char(' ')), qUtf8Printable(strPath));
            m_mapConcepts.insert(strId, { result.concept, WikiTestFiles::HasSection(strText, DecodeUtf8("관계")) });
        }
        m_index = BuildIndex(m_vecDocuments, m_mapConcepts);
    }

    // 파일 목록은 돌 때마다 폴더에서 훑는다
    void showcaseFilesAreFound()
    {
        const QString strDocuments = MakeShowcasePath(WikiTestFiles::kDocumentsFolder);
        const QString strConcepts = MakeShowcasePath(WikiTestFiles::kConceptsFolder);
        QVERIFY2(QDir(strDocuments).exists(), qUtf8Printable(strDocuments));
        QVERIFY2(QDir(strConcepts).exists(), qUtf8Printable(strConcepts));
        QCOMPARE(WikiTestFiles::ListMarkdownFiles(strDocuments).size(), kDocumentFileCount);
        QCOMPARE(WikiTestFiles::ListMarkdownFiles(strConcepts).size(), kConceptFileCount);
    }

    void documentRewriteIsByteExact_data()
    {
        WikiTestFiles::AddFileRows(MakeShowcasePath(WikiTestFiles::kDocumentsFolder));
    }

    // 문서 파일: 다 읽히고, digest 가 맞고, 새로 써도 · 있는 글에 갈아 끼워도 파일과 바이트가 같다
    void documentRewriteIsByteExact()
    {
        QFETCH(QString, path);
        QVERIFY2(!path.isEmpty(), "examples/showcase/wiki/documents has no *.md file");
        WikiTestFiles::VerifyDocumentRewrite(path);
    }

    void conceptRewriteIsByteExact_data()
    {
        WikiTestFiles::AddFileRows(MakeShowcasePath(WikiTestFiles::kConceptsFolder));
    }

    // 개념 파일: 다 읽히고, id 가 파일 이름이고, 있는 글에 다시 쓰면 바이트가 같다(`## 관계` 가 없으면 새로 써도 같다)
    void conceptRewriteIsByteExact()
    {
        QFETCH(QString, path);
        QVERIFY2(!path.isEmpty(), "examples/showcase/wiki/concepts has no *.md file");
        WikiTestFiles::VerifyConceptRewrite(path);
    }

    // §1 · §2.1 · §4: 개념 파일 32개, 종류 7개 모두, `## 관계` 11개, 별칭 11개, 한국어 id
    void conceptFiles()
    {
        QCOMPARE(m_mapConcepts.size(), kConceptFileCount);
        QSet<QString> setKinds;
        int iWithRelationSection = 0;
        int iWithAliases = 0;
        for (const ShowcaseConcept& concept : m_mapConcepts)
        {
            setKinds.insert(concept.concept.strKind);
            if (concept.bHasRelationSection)
                ++iWithRelationSection;
            if (!concept.concept.vecAliases.isEmpty())
                ++iWithAliases;
        }
        const QStringList vecVocabularyKinds = KnowledgeVocabulary::Get().GetConceptCodes();
        QCOMPARE(vecVocabularyKinds.size(), kConceptKindCount);
        const QString strKinds = DiffItems(DecodeUtf8("개념 종류"), ToList(setKinds), vecVocabularyKinds);
        QVERIFY2(strKinds.isEmpty(), qUtf8Printable(strKinds));
        QCOMPARE(iWithRelationSection, kConceptsWithRelationSection);
        QCOMPARE(iWithAliases, kConceptsWithAliases);
        for (const QString& strId : kKoreanConceptIds)
            QVERIFY2(m_mapConcepts.contains(strId), qUtf8Printable(strId));
    }

    // §2.1: 관계 줄 40(문서 27 · 개념 파일 13)
    void relationLines()
    {
        int iDocumentLines = 0;
        for (const ShowcaseDocument& doc : m_vecDocuments)
            iDocumentLines += doc.document.vecRelations.size();
        int iConceptLines = 0;
        for (const ShowcaseConcept& concept : m_mapConcepts)
            iConceptLines += concept.concept.vecRelations.size();
        QCOMPARE(iDocumentLines, kDocumentRelationLines);
        QCOMPARE(iConceptLines, kConceptRelationLines);
        QCOMPARE(iDocumentLines + iConceptLines, kRelationLines);
    }

    // §2.1: 선 36. 종류 · 방향 · 상태별 수
    void edges()
    {
        QCOMPARE(m_index.mapEdges.size(), kEdgeCount);
        const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
        QMap<QString, int> mapByKind;
        int iDirected = 0;
        int iUndirected = 0;
        int iConfirmed = 0;
        int iProposed = 0;
        for (const IndexEdge& edge : m_index.mapEdges)
        {
            ++mapByKind[edge.strKind];
            if (vocabulary.IsUndirected(edge.strKind))
                ++iUndirected;
            else
                ++iDirected;
            if (edge.eStatus == ERelationStatus::CONFIRMED)
                ++iConfirmed;
            else
                ++iProposed;
        }
        const QString strByKind = DiffItems(DecodeUtf8("종류별 선 수"), MakeCountItems(mapByKind), MakeCountItems(kEdgesByKind));
        QVERIFY2(strByKind.isEmpty(), qUtf8Printable(strByKind));
        QCOMPARE(iDirected, kDirectedEdges);
        QCOMPARE(iUndirected, kUndirectedEdges);
        QCOMPARE(iConfirmed, kConfirmedEdges);
        QCOMPARE(iProposed, kProposedEdges);
    }

    // §2.1: 합쳐지는 선 3개(상태 · 출처)와 빠지는 선 1개
    void mergedAndDroppedEdges()
    {
        QStringList vecMerged;
        for (const IndexEdge& edge : m_index.mapEdges)
            if (edge.iLines > 1)
                vecMerged << MakeEdgeText(edge.strFrom, edge.strKind, edge.strTo);
        QStringList vecExpected;
        for (const ExpectedMergedEdge& expected : kMergedEdges)
            vecExpected << DecodeUtf8(expected.pEdge);
        const QString strMerged = DiffItems(DecodeUtf8("합쳐지는 선"), vecMerged, vecExpected);
        QVERIFY2(strMerged.isEmpty(), qUtf8Printable(strMerged));

        for (const ExpectedMergedEdge& expected : kMergedEdges)
        {
            const QString strEdge = DecodeUtf8(expected.pEdge);
            QVERIFY2(m_index.mapEdges.contains(strEdge), qUtf8Printable(strEdge));
            const IndexEdge edge = m_index.mapEdges.value(strEdge);
            QCOMPARE(DescribeMergedEdge(strEdge, edge.eStatus, JoinSorted(ToList(edge.setDocuments)), edge.bConceptFile),
                     DescribeMergedEdge(strEdge, expected.eStatus, JoinSorted(expected.vecDocuments), expected.bConceptFile));
        }

        const QString strDropped = DiffItems(DecodeUtf8("빠지는 선"), m_index.vecDropped, kDroppedRelations);
        QVERIFY2(strDropped.isEmpty(), qUtf8Printable(strDropped));
    }

    // §2.2 계보도 · 분류 트리: 개선 선 다섯(모두 설명 있음, 같은 해의 두 판 하나), 종류 선 열둘(AI 제안은 RetinaNet 하나)
    void lineageAndTaxonomy()
    {
        QStringList vecImproves;
        QStringList vecImprovesWithoutNote;
        QStringList vecSameYear;
        QStringList vecIsA;
        QStringList vecProposedIsA;
        for (const IndexEdge& edge : m_index.mapEdges)
        {
            const QString strEdge = MakeEdgeText(edge.strFrom, edge.strKind, edge.strTo);
            if (edge.strKind == QLatin1String("improves"))
            {
                vecImproves << strEdge;
                if (!edge.bHasNote)
                    vecImprovesWithoutNote << strEdge;
                const QString strFromYear = m_mapConcepts.value(edge.strFrom).concept.strYear;
                if (!strFromYear.isEmpty() && strFromYear == m_mapConcepts.value(edge.strTo).concept.strYear)
                    vecSameYear << strEdge;
            }
            else if (edge.strKind == QLatin1String("isA"))
            {
                vecIsA << strEdge;
                if (edge.eStatus == ERelationStatus::PROPOSED)
                    vecProposedIsA << strEdge;
            }
        }
        const QString strImproves = DiffItems(DecodeUtf8("개선 선"), vecImproves, kImprovesEdges);
        QVERIFY2(strImproves.isEmpty(), qUtf8Printable(strImproves));
        const QString strWithoutNote = DiffItems(DecodeUtf8("설명 없는 개선 선"), vecImprovesWithoutNote, QStringList());
        QVERIFY2(strWithoutNote.isEmpty(), qUtf8Printable(strWithoutNote));
        const QString strSameYear = DiffItems(DecodeUtf8("같은 해의 개선 선"), vecSameYear, kSameYearImprovesEdges);
        QVERIFY2(strSameYear.isEmpty(), qUtf8Printable(strSameYear));
        const QString strIsA = DiffItems(DecodeUtf8("종류 선"), vecIsA, kIsAEdges);
        QVERIFY2(strIsA.isEmpty(), qUtf8Printable(strIsA));
        const QString strProposedIsA = DiffItems(DecodeUtf8("AI 제안인 종류 선"), vecProposedIsA, kProposedIsAEdges);
        QVERIFY2(strProposedIsA.isEmpty(), qUtf8Printable(strProposedIsA));
    }

    // §2.1: 숨는 점, 개념 파일 관계로만 보이는 점, 선 없는 점, 덩어리
    void visibleConcepts()
    {
        QStringList vecHidden;
        for (auto it = m_mapConcepts.cbegin(); it != m_mapConcepts.cend(); ++it)
            if (!m_index.setReferenced.contains(it.key()))
                vecHidden << it.key();
        const QString strHidden = DiffItems(DecodeUtf8("숨는 점"), vecHidden, kHiddenConcepts);
        QVERIFY2(strHidden.isEmpty(), qUtf8Printable(strHidden));

        const QStringList vecVisible = ListVisibleConcepts();
        QCOMPARE(vecVisible.size(), kVisibleConcepts);

        QStringList vecWithoutDocuments;
        for (const QString& strId : vecVisible)
            if (CountDocuments(strId) == 0)
                vecWithoutDocuments << strId;
        const QString strWithoutDocuments = DiffItems(DecodeUtf8("다루는 문서가 없는 점"), vecWithoutDocuments, kConceptsWithoutDocuments);
        QVERIFY2(strWithoutDocuments.isEmpty(), qUtf8Printable(strWithoutDocuments));

        QSet<QString> setEndpoints;
        for (const IndexEdge& edge : m_index.mapEdges)
        {
            setEndpoints.insert(edge.strFrom);
            setEndpoints.insert(edge.strTo);
        }
        QStringList vecWithoutEdges;
        for (const QString& strId : vecVisible)
            if (!setEndpoints.contains(strId))
                vecWithoutEdges << strId;
        const QString strWithoutEdges = DiffItems(DecodeUtf8("선 없는 점"), vecWithoutEdges, kConceptsWithoutEdges);
        QVERIFY2(strWithoutEdges.isEmpty(), qUtf8Printable(strWithoutEdges));

        QCOMPARE(JoinNumbers(CountComponentSizes(vecVisible, m_index.mapEdges)), JoinNumbers(kComponentSizes));
    }

    // §2.1 다루는 문서가 많은 점 · §2.2 문서 ↔ 개념: 개념마다 문서 수, 문서마다 개념 수
    void documentConceptCounts()
    {
        QMap<QString, int> mapExpectedOtherThanOne;
        for (const NameCount& expected : kConceptsInSeveralDocuments)
            mapExpectedOtherThanOne.insert(DecodeUtf8(expected.pName), expected.iCount);
        for (const QString& strId : kConceptsWithoutDocuments)
            mapExpectedOtherThanOne.insert(strId, 0);
        QMap<QString, int> mapOtherThanOne;
        for (const QString& strId : ListVisibleConcepts())
        {
            const int iDocuments = CountDocuments(strId);
            if (iDocuments != 1)
                mapOtherThanOne.insert(strId, iDocuments);
        }
        const QString strDocumentCounts = DiffItems(DecodeUtf8("다루는 문서 수가 1 이 아닌 점"), MakeCountItems(mapOtherThanOne), MakeCountItems(mapExpectedOtherThanOne));
        QVERIFY2(strDocumentCounts.isEmpty(), qUtf8Printable(strDocumentCounts));

        QMap<QString, int> mapPointed;
        for (auto it = m_index.mapPointed.cbegin(); it != m_index.mapPointed.cend(); ++it)
            mapPointed.insert(it.key(), it.value().size());
        const QString strPointed = DiffItems(DecodeUtf8("문서마다 가리키는 개념 수"), MakeCountItems(mapPointed), MakeCountItems(kPointedConceptsPerDocument));
        QVERIFY2(strPointed.isEmpty(), qUtf8Printable(strPointed));
        QVERIFY(m_index.mapPointed.value(DecodeUtf8(kYolov3)).contains(DecodeUtf8(kMissingConcept)));
        QVERIFY(!m_mapConcepts.contains(DecodeUtf8(kMissingConcept)));
    }

    // §2.2 관계 행렬: 종류 둘이 겹치는 칸은 하나
    void pairsWithSeveralKinds()
    {
        QMap<QString, QSet<QString>> mapPairKinds;   // 두 id(글자 순) → 종류들
        for (const IndexEdge& edge : m_index.mapEdges)
        {
            const bool bFromFirst = edge.strFrom < edge.strTo;
            const QString strPair = bFromFirst ? edge.strFrom + QLatin1Char(' ') + edge.strTo : edge.strTo + QLatin1Char(' ') + edge.strFrom;
            mapPairKinds[strPair].insert(edge.strKind);
        }
        QStringList vecSeveral;
        for (auto it = mapPairKinds.cbegin(); it != mapPairKinds.cend(); ++it)
        {
            if (it.value().size() < 2)
                continue;
            QStringList vecKinds(it.value().cbegin(), it.value().cend());
            vecKinds.sort();
            vecSeveral << it.key() + QStringLiteral(": ") + vecKinds.join(QLatin1Char(' '));
        }
        const QString strSeveral = DiffItems(DecodeUtf8("종류가 겹치는 칸"), vecSeveral, kPairsWithSeveralKinds);
        QVERIFY2(strSeveral.isEmpty(), qUtf8Printable(strSeveral));
    }

    // §2.2 타임라인: 연도 있는 보이는 점 16(해마다 수) · 없는 점 15, 숨은 dpm 은 2008
    void years()
    {
        QMap<QString, int> mapPerYear;
        int iWithYear = 0;
        int iWithoutYear = 0;
        for (const QString& strId : ListVisibleConcepts())
        {
            const QString strYear = m_mapConcepts.value(strId).concept.strYear;
            if (strYear.isEmpty())
            {
                ++iWithoutYear;
                continue;
            }
            ++mapPerYear[strYear];
            ++iWithYear;
        }
        const QString strPerYear = DiffItems(DecodeUtf8("해마다 점 수"), MakeCountItems(mapPerYear), MakeCountItems(kVisibleConceptsPerYear));
        QVERIFY2(strPerYear.isEmpty(), qUtf8Printable(strPerYear));
        QCOMPARE(iWithYear, kVisibleConceptsWithYear);
        QCOMPARE(iWithoutYear, kVisibleConceptsWithoutYear);
        for (const QString& strHidden : kHiddenConcepts)
            QVERIFY2(m_mapConcepts.contains(strHidden), qUtf8Printable(strHidden));
        QCOMPARE(m_mapConcepts.value(QStringLiteral("dpm")).concept.strYear, DecodeUtf8(kHiddenDpmYear));
    }

    // §2.1 문서 순서: learned 가 늦은 것부터(같으면 id 순)
    void documentOrder()
    {
        for (const ShowcaseDocument& doc : m_vecDocuments)
            QVERIFY2(QDateTime::fromString(doc.document.strLearned, Qt::ISODate).isValid(),
                     qUtf8Printable(doc.strName + DecodeUtf8(": ") + doc.document.strLearned));
        QVector<ShowcaseDocument> vecSorted = m_vecDocuments;
        std::sort(vecSorted.begin(), vecSorted.end(), [](const ShowcaseDocument& a, const ShowcaseDocument& b)
        {
            const QDateTime dtA = QDateTime::fromString(a.document.strLearned, Qt::ISODate);
            const QDateTime dtB = QDateTime::fromString(b.document.strLearned, Qt::ISODate);
            if (dtA != dtB)
                return dtA > dtB;
            return a.document.strId < b.document.strId;
        });
        QStringList vecNames;
        for (const ShowcaseDocument& doc : vecSorted)
            vecNames << doc.strName;
        QCOMPARE(vecNames, kDocumentsByLearned);   // 다르면 처음 다른 자리를 보여 준다
    }

    // §3: 표는 12개이고 모두 README 의 표다
    void tablesAreAllListed()
    {
        QStringList vecTitles;
        for (const ShowcaseDocument& doc : m_vecDocuments)
            for (const KnowledgeTable& table : doc.document.vecTables)
                vecTitles << table.strTitle;
        QStringList vecExpected;
        for (const ExpectedTable& expected : kTables)
            vecExpected << DecodeUtf8(expected.pTitle);
        QCOMPARE(kTables.size(), kTableCount);
        QCOMPARE(vecTitles.size(), kTableCount);
        const QString strTitles = DiffItems(DecodeUtf8("표 제목"), vecTitles, vecExpected);
        QVERIFY2(strTitles.isEmpty(), qUtf8Printable(strTitles));
    }

    void tableMatchesReadme_data()
    {
        QTest::addColumn<int>("row");
        for (int iRow = 0; iRow < kTables.size(); ++iRow)
            QTest::newRow(kTables[iRow].pTitle) << iRow;
    }

    // §3: 표마다 x · x 열 종류 · y · series · cumulative
    void tableMatchesReadme()
    {
        QFETCH(int, row);
        const ExpectedTable& expected = kTables[row];
        const KnowledgeTable* pTable = FindTable(DecodeUtf8(expected.pDocument), DecodeUtf8(expected.pTitle));
        QVERIFY2(pTable != nullptr, qUtf8Printable(DecodeUtf8(expected.pDocument) + DecodeUtf8(" 에 표가 없다: ") + DecodeUtf8(expected.pTitle)));
        QCOMPARE(pTable->strX, DecodeUtf8(expected.pX));
        const int iX = FindColumn(*pTable, pTable->strX);
        QVERIFY2(iX >= 0, qUtf8Printable(DecodeUtf8("x 가 열 이름이 아니다: ") + pTable->strX));
        QCOMPARE(GetKindName(KnowledgeText::ClassifyColumn(*pTable, iX)), GetKindName(expected.eXKind));
        QCOMPARE(pTable->vecY.join(QStringLiteral(", ")), expected.vecY.join(QStringLiteral(", ")));
        QCOMPARE(pTable->strSeries, DecodeUtf8(expected.pSeries));
        QCOMPARE(pTable->bCumulative, expected.bCumulative);
    }

    // §3 의 그 밖 · 시험하는 것: y 종류, 빈 칸, 숫자 열의 종류, 단위, 행 수, 계열, x 범위, 줄어드는 단계, 따옴표 · \| · 천 단위 쉼표
    void tableColumnsAndCells()
    {
        QStringList vecNonQuantityY;
        QStringList vecEmptyCells;
        for (const ShowcaseDocument& doc : m_vecDocuments)
        {
            for (const KnowledgeTable& table : doc.document.vecTables)
            {
                for (const QString& strY : table.vecY)
                {
                    const int iColumn = FindColumn(table, strY);
                    QVERIFY2(iColumn >= 0, qUtf8Printable(table.strTitle + DecodeUtf8(": y 가 열 이름이 아니다: ") + strY));
                    if (KnowledgeText::ClassifyColumn(table, iColumn) != EColumnKind::QUANTITY && !vecNonQuantityY.contains(table.strTitle))
                        vecNonQuantityY << table.strTitle;
                }
                for (const QStringList& vecRow : table.vecRows)
                    for (int iColumn = 0; iColumn < vecRow.size(); ++iColumn)
                        if (vecRow[iColumn].isEmpty())
                            vecEmptyCells << DescribeCell(table.strTitle, vecRow.value(0), table.vecColumns.value(iColumn).strName);
            }
        }
        const QString strNonQuantityY = DiffItems(DecodeUtf8("y 가 수치가 아닌 표"), vecNonQuantityY, kTablesWithNonQuantityY);
        QVERIFY2(strNonQuantityY.isEmpty(), qUtf8Printable(strNonQuantityY));
        QStringList vecExpectedEmptyCells;
        for (const ExpectedCell& cell : kEmptyCells)
            vecExpectedEmptyCells << DescribeCell(DecodeUtf8(cell.pTableTitle), DecodeUtf8(cell.pRow), DecodeUtf8(cell.pColumn));
        const QString strEmptyCells = DiffItems(DecodeUtf8("빈 칸"), vecEmptyCells, vecExpectedEmptyCells);
        QVERIFY2(strEmptyCells.isEmpty(), qUtf8Printable(strEmptyCells));

        for (const ExpectedColumn& expected : kColumnKinds)
        {
            const KnowledgeTable* pTable = FindTable(DecodeUtf8(expected.pTableTitle));
            QVERIFY2(pTable != nullptr, expected.pTableTitle);
            const int iColumn = FindColumn(*pTable, DecodeUtf8(expected.pColumn));
            QVERIFY2(iColumn >= 0, expected.pColumn);
            QCOMPARE(DescribeColumnKind(pTable->strTitle, DecodeUtf8(expected.pColumn), KnowledgeText::ClassifyColumn(*pTable, iColumn)),
                     DescribeColumnKind(pTable->strTitle, DecodeUtf8(expected.pColumn), expected.eKind));
        }

        for (const ExpectedUnits& expected : kYUnits)
        {
            const KnowledgeTable* pTable = FindTable(DecodeUtf8(expected.pTableTitle));
            QVERIFY2(pTable != nullptr, expected.pTableTitle);
            QStringList vecUnits;
            for (const QString& strY : pTable->vecY)
                vecUnits << pTable->vecColumns.value(FindColumn(*pTable, strY)).strUnit;
            QCOMPARE(pTable->strTitle + QStringLiteral(": ") + vecUnits.join(QStringLiteral(" · ")),
                     pTable->strTitle + QStringLiteral(": ") + expected.vecUnits.join(QStringLiteral(" · ")));
        }

        for (const NameCount& expected : kTableRows)
        {
            const KnowledgeTable* pTable = FindTable(DecodeUtf8(expected.pName));
            QVERIFY2(pTable != nullptr, expected.pName);
            QCOMPARE(pTable->strTitle + QLatin1Char(' ') + QString::number(pTable->vecRows.size()),
                     pTable->strTitle + QLatin1Char(' ') + QString::number(expected.iCount));
        }

        // YOLO 표: series 열의 값 둘(계열 2개), x 의 범위
        const KnowledgeTable* pSpeed = FindTable(DecodeUtf8(kSpeedTable));
        QVERIFY2(pSpeed != nullptr, kSpeedTable);
        const int iSeries = FindColumn(*pSpeed, pSpeed->strSeries);
        const int iSpeedX = FindColumn(*pSpeed, pSpeed->strX);
        QVERIFY(iSeries >= 0 && iSpeedX >= 0);
        QSet<QString> setSeries;
        double dMin = std::numeric_limits<double>::max();
        double dMax = std::numeric_limits<double>::lowest();
        for (const QStringList& vecRow : pSpeed->vecRows)
        {
            setSeries.insert(vecRow[iSeries]);
            bool bNumber = false;
            const double dValue = vecRow[iSpeedX].toDouble(&bNumber);
            QVERIFY2(bNumber, qUtf8Printable(vecRow[iSpeedX]));
            dMin = std::min(dMin, dValue);
            dMax = std::max(dMax, dValue);
        }
        const QString strSeries = DiffItems(DecodeUtf8("계열"), ToList(setSeries), kSpeedSeries);
        QVERIFY2(strSeries.isEmpty(), qUtf8Printable(strSeries));
        QCOMPARE(dMin, kSpeedXMin);
        QCOMPARE(dMax, kSpeedXMax);

        // YOLO9000 단계 표: 앞 행보다 줄어드는 단계
        const KnowledgeTable* pSteps = FindTable(DecodeUtf8(kStepsTable));
        QVERIFY2(pSteps != nullptr, kStepsTable);
        const int iStepsY = FindColumn(*pSteps, pSteps->vecY.value(0));
        QVERIFY(iStepsY >= 0);
        QStringList vecDecreasing;
        for (int iRow = 1; iRow < pSteps->vecRows.size(); ++iRow)
        {
            const QString& strBefore = pSteps->vecRows[iRow - 1][iStepsY];
            const QString& strAfter = pSteps->vecRows[iRow][iStepsY];
            if (strAfter.toDouble() < strBefore.toDouble())
                vecDecreasing << strBefore + QStringLiteral(" → ") + strAfter;
        }
        const QString strDecreasing = DiffItems(DecodeUtf8("줄어드는 단계"), vecDecreasing, kStepsDecreasing);
        QVERIFY2(strDecreasing.isEmpty(), qUtf8Printable(strDecreasing));

        // 결제 수단별 비중: condition 의 " 는 파일에서 &quot; 로 적혔다가 읽을 때 돌아온다. 칸 속 | 는 \| 로 적혔다
        const ShowcaseDocument* pPlan = FindDocument(DecodeUtf8(kPaymentPlan));
        const KnowledgeTable* pMethods = FindTable(DecodeUtf8(kPaymentMethodTable));
        QVERIFY(pPlan != nullptr && pMethods != nullptr);
        QVERIFY2(pMethods->strCondition.contains(DecodeUtf8(kQuotedWord)), qUtf8Printable(pMethods->strCondition));
        QVERIFY2(pPlan->strText.contains(DecodeUtf8(kEscapedQuotedWord)), kEscapedQuotedWord);
        QVERIFY2(HasCell(*pMethods, DecodeUtf8(kPipeCell)), kPipeCell);
        QVERIFY2(pPlan->strText.contains(DecodeUtf8(kEscapedPipeCell)), kEscapedPipeCell);

        // 동시 사용자 열: 천 단위 쉼표가 있는 칸도 수치다(열 종류는 위 kColumnKinds)
        const KnowledgeTable* pLoad = FindTable(DecodeUtf8(kLoadTable));
        QVERIFY2(pLoad != nullptr, kLoadTable);
        QVERIFY2(HasCell(*pLoad, DecodeUtf8(kThousandsCell)), kThousandsCell);
    }

    // §4 형식 경계 사례
    void formatEdgeCases()
    {
        // 손으로 쓴 노트: id 없는 둘째 카드, 개념 이름과 다른 링크 글자, rules 0 · edited true · analysis 없음, 핵심 숫자 없음
        const ShowcaseDocument* pNote = FindDocument(DecodeUtf8(kTwoStageNote));
        QVERIFY(pNote != nullptr);
        const KnowledgeDocument& note = pNote->document;
        QVERIFY(note.vecCards.size() >= 2);
        QVERIFY2(note.vecCards[1].strId.isEmpty(), qUtf8Printable(note.vecCards[1].strId));
        QString strLinkedId;
        bool bLinkFound = false;
        for (const KnowledgeCard& card : note.vecCards)
            for (const ConceptLink& link : card.vecConcepts)
                if (link.strName == DecodeUtf8(kRpnLinkName))
                {
                    strLinkedId = link.strConceptId;
                    bLinkFound = true;
                }
        QVERIFY2(bLinkFound, kRpnLinkName);
        QCOMPARE(strLinkedId, QString("rpn"));
        QCOMPARE(m_mapConcepts.value(strLinkedId).concept.strName, QString("RPN"));
        QCOMPARE(note.iRules, 0);
        QCOMPARE(note.bEdited, true);
        QVERIFY2(note.strAnalysisId.isEmpty(), qUtf8Printable(note.strAnalysisId));
        QVERIFY(note.vecKeyNumbers.isEmpty());

        // YOLO 둘째 카드의 모르는 하위 항목
        const ShowcaseDocument* pYolo = FindDocument(DecodeUtf8(kYolo));
        QVERIFY(pYolo != nullptr);
        QVERIFY(pYolo->document.vecCards.size() >= 2);
        QCOMPARE(pYolo->document.vecCards[1].vecExtraLines, QStringList { DecodeUtf8(kYoloMemo) });

        // 핵심 숫자: 단위 없는 것(YOLO9000), 단위가 % 인 것(결제 기획서)
        const KnowledgeKeyNumber* pUnitless = FindKeyNumber(DecodeUtf8(kYolo9000), DecodeUtf8(kUnitlessNumber));
        QVERIFY2(pUnitless != nullptr, kUnitlessNumber);
        QVERIFY2(pUnitless->strUnit.isEmpty(), qUtf8Printable(pUnitless->strUnit));
        QCOMPARE(pUnitless->strLabel, DecodeUtf8(kUnitlessNumberLabel));
        const KnowledgeKeyNumber* pPercent = FindKeyNumber(DecodeUtf8(kPaymentPlan), DecodeUtf8(kPercentNumber));
        QVERIFY2(pPercent != nullptr, kPercentNumber);
        QCOMPARE(pPercent->strUnit, QString("%"));

        // 데이터 절 없는 문서
        const ShowcaseDocument* pFocalLoss = FindDocument(DecodeUtf8(kFocalLoss));
        QVERIFY(pFocalLoss != nullptr);
        QVERIFY(pFocalLoss->document.vecTables.isEmpty());

        // AI 가 분석한 문서 5편(analysis 가 있다)
        int iAnalyzed = 0;
        for (const ShowcaseDocument& doc : m_vecDocuments)
            if (!doc.document.strAnalysisId.isEmpty())
                ++iAnalyzed;
        QCOMPARE(iAnalyzed, kAnalyzedDocuments);
    }

private:
    const ShowcaseDocument* FindDocument(const QString& name) const
    {
        for (const ShowcaseDocument& doc : m_vecDocuments)
            if (doc.strName == name)
                return &doc;
        return nullptr;
    }

    const KnowledgeTable* FindTable(const QString& document, const QString& title) const
    {
        const ShowcaseDocument* pDocument = FindDocument(document);
        if (pDocument == nullptr)
            return nullptr;
        for (const KnowledgeTable& table : pDocument->document.vecTables)
            if (table.strTitle == title)
                return &table;
        return nullptr;
    }

    // 제목은 wiki 안에서 겹치지 않는다(tablesAreAllListed)
    const KnowledgeTable* FindTable(const QString& title) const
    {
        for (const ShowcaseDocument& doc : m_vecDocuments)
            for (const KnowledgeTable& table : doc.document.vecTables)
                if (table.strTitle == title)
                    return &table;
        return nullptr;
    }

    const KnowledgeKeyNumber* FindKeyNumber(const QString& document, const QString& value) const
    {
        const ShowcaseDocument* pDocument = FindDocument(document);
        if (pDocument == nullptr)
            return nullptr;
        for (const KnowledgeKeyNumber& number : pDocument->document.vecKeyNumbers)
            if (number.strValue == value)
                return &number;
        return nullptr;
    }

    static bool HasCell(const KnowledgeTable& table, const QString& cell)
    {
        for (const QStringList& vecRow : table.vecRows)
            if (vecRow.contains(cell))
                return true;
        return false;
    }

    // 숨지 않는 점: 개념 파일이 있고, 문서가 가리키거나 개념 파일 관계에 나온다
    QStringList ListVisibleConcepts() const
    {
        QStringList vecVisible;
        for (auto it = m_mapConcepts.cbegin(); it != m_mapConcepts.cend(); ++it)
            if (m_index.setReferenced.contains(it.key()))
                vecVisible << it.key();
        return vecVisible;
    }

    // 이 개념을 가리키는 문서 수(한 문서는 한 번)
    int CountDocuments(const QString& conceptId) const
    {
        int iCount = 0;
        for (const QSet<QString>& setPointed : m_index.mapPointed)
            if (setPointed.contains(conceptId))
                ++iCount;
        return iCount;
    }

private:
    QVector<ShowcaseDocument> m_vecDocuments;       // 파일 이름 순
    QMap<QString, ShowcaseConcept> m_mapConcepts;   // id(파일 이름) → 개념
    ShowcaseIndex m_index;
};

std::unique_ptr<QObject> CreateShowcaseWikiTest() { return std::make_unique<ShowcaseWikiTest>(); }

#include "ShowcaseWikiTest.moc"
