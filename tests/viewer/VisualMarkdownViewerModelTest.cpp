#include "VisualMarkdownViewer/VisualMarkdownViewerModel.h"
#include "ViewerTestSupport.h"

#include <QHash>
#include <QTemporaryDir>
#include <QTest>

#include <algorithm>
#include <memory>

namespace
{
// ================================================================================================
// 기대값: design/Views.md §5(보기 6개의 점 · 선) · examples/showcase/README.md §2.1(문서 순서) · §3(표 13개의 시작 차트)
// ================================================================================================
constexpr int kNodeCount = 31;
constexpr int kEdgeCount = 36;
constexpr int kDocumentCount = 6;
constexpr int kDroppedCount = 1;      // YOLOv3 사용 → Darknet-53(개념 파일이 없다)
constexpr int kTableCount = 13;

constexpr const char* kYoloTitle = "You Only Look Once: Unified, Real-Time Object Detection";
constexpr const char* kYolo9000Title = "YOLO9000: Better, Faster, Stronger";
constexpr const char* kYolov3Title = "YOLOv3: An Incremental Improvement";
constexpr const char* kTwoStageTitle = "2단계 검출기의 흐름";
constexpr const char* kFocalLossTitle = "Focal Loss for Dense Object Detection";
constexpr const char* kPaymentTitle = "결제 서비스 개편 기획서";
constexpr const char* kYolo9000Id = "5e2d8b10-7c4a-4f3e-b9d6-2a1f0c9e8d72";
constexpr const char* kYoloFile = "documents/you-only-look-once-unified-real-time-object-detection.md";

// 문서 목록 순(learned 늦은 것부터)의 자리
constexpr int kPaymentDocument = 0;
constexpr int kFocalLossDocument = 1;
constexpr int kMonthlyTable = 0;      // 결제 기획서 > 월별 응답 시간(순서 x → 선)
constexpr int kShareTable = 1;        // 결제 기획서 > 결제 수단별 비중(단위 % · 합 100 → 도넛)
constexpr int kLoadTable = 2;         // 결제 기획서 > 동시 사용자와 응답 시간(수치 x → 산점도)
constexpr int kPriorityTable = 3;     // 결제 기획서 > 기능 우선순위(y 가 범주 → 그리지 않음)

struct ExpectedView
{
    EGraphLayoutType eType;
    const char* pLabel;
    const char* pKey;
    int iNodes;      // vecNodes(타임라인은 놓인 점, 문서 ↔ 개념은 문서 + 개념)
    int iEdges;      // vecEdges. -1 이면 보지 않는다(§5 에 수가 없다)
};
const QVector<ExpectedView> kViews {
    { EGraphLayoutType::NETWORK, "관계망", "network", kNodeCount, kEdgeCount },
    { EGraphLayoutType::LINEAGE, "계보도", "lineage", 8, 5 },
    { EGraphLayoutType::TREE, "분류 트리", "tree", 14, 12 },
    { EGraphLayoutType::TIMELINE, "타임라인", "timeline", 16, -1 },
    { EGraphLayoutType::MATRIX, "관계 행렬", "matrix", kNodeCount, kEdgeCount },
    { EGraphLayoutType::BIPARTITE, "문서 ↔ 개념", "bipartite", -1, -1 },
};
constexpr int kLineageLanes = 3;
constexpr int kTimelineUnplaced = 15;
const QStringList kTimelineLanes { "검출기", "결제 모듈", "그 밖" };
constexpr int kMatrixCells = 40;

// 문서 ↔ 개념: 문서마다 선 수(개념 파일이 없는 Darknet-53 은 뺀다)
struct DocumentLinks
{
    const char* pTitle;
    int iLinks;
};
const QVector<DocumentLinks> kBipartiteLinks {
    { kYoloTitle, 5 }, { kYolo9000Title, 7 }, { kYolov3Title, 5 }, { kTwoStageTitle, 7 }, { kFocalLossTitle, 4 }, { kPaymentTitle, 9 },
};

// README §3 의 표 13개와 시작 차트(README 순)
struct ExpectedChart
{
    const char* pDocument;
    const char* pTable;
    EChartType eType;
};
const QVector<ExpectedChart> kCharts {
    { kYoloTitle, "실시간 검출기와 그 밖의 검출기 — 정확도와 속도", EChartType::SCATTER },
    { kYolo9000Title, "YOLO 에서 YOLOv2 까지 — 하나씩 더한 개선", EChartType::STEP_BAR },
    { kYolo9000Title, "입력 크기에 따른 정확도와 속도", EChartType::LINE },
    { kYolo9000Title, "검출기별 VOC 2007 정확도", EChartType::BAR },
    { kYolov3Title, "입력 크기별 COCO 정확도", EChartType::LINE },
    { kYolov3Title, "처리 시간과 AP50", EChartType::SCATTER },
    { kYolov3Title, "검출기별 COCO 정확도", EChartType::HEATMAP },
    { kTwoStageTitle, "2단계 검출기의 VOC 2007 정확도", EChartType::BAR },
    { kTwoStageTitle, "학습 · 시험 시간", EChartType::BAR },
    { kPaymentTitle, "월별 응답 시간", EChartType::LINE },
    { kPaymentTitle, "결제 수단별 비중", EChartType::DONUT },
    { kPaymentTitle, "동시 사용자와 응답 시간", EChartType::SCATTER },
    { kPaymentTitle, "기능 우선순위", EChartType::NONE },
};

QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

QString DescribeChart(const QString& document, const QString& table, EChartType type)
{
    return document + QStringLiteral(" / ") + table + QStringLiteral(": ") + VisualMarkdownViewerModel::GetChartTypeKey(type);
}
}

// 보기 앱의 화면 로직(VisualMarkdownViewerModel, QtCore)을 showcase 로 시험한다
class VisualMarkdownViewerModelTest : public QObject
{
    Q_OBJECT

private slots:
    // showcase 를 읽는다: 점 31 · 선 36 · 문서 6(learned 늦은 것부터) · 빠진 관계 1 · 경고 0
    void loadsShowcase()
    {
        VisualMarkdownViewerModel model;
        QVERIFY(!model.IsLoaded());
        QVERIFY2(model.Load(ViewerTestSupport::GetShowcaseDir()), qUtf8Printable(model.GetLastError()));
        QVERIFY(model.IsLoaded());
        QCOMPARE(model.GetWikiRoot(), ViewerTestSupport::GetShowcaseDir());
        QCOMPARE(model.GetNodeCount(), kNodeCount);
        QCOMPARE(model.GetEdgeCount(), kEdgeCount);
        QCOMPARE(model.GetGraph().GetNodes().size(), kNodeCount);
        QCOMPARE(model.GetDocumentCount(), kDocumentCount);
        QCOMPARE(model.GetDroppedRelationCount(), kDroppedCount);
        QCOMPARE(model.GetWarnings(), QStringList());

        QStringList vecTitles;
        for (int iDocument = 0; iDocument < model.GetDocumentCount(); ++iDocument)
        {
            QVERIFY(model.FindDocument(iDocument) != nullptr);
            vecTitles << model.FindDocument(iDocument)->strTitle;
        }
        const QStringList vecExpected { DecodeUtf8(kPaymentTitle), DecodeUtf8(kFocalLossTitle), DecodeUtf8(kTwoStageTitle),
                                        DecodeUtf8(kYolov3Title),  DecodeUtf8(kYolo9000Title),  DecodeUtf8(kYoloTitle) };
        QCOMPARE(vecTitles, vecExpected);
        QVERIFY(model.FindDocument(-1) == nullptr);
        QVERIFY(model.FindDocument(kDocumentCount) == nullptr);
        QCOMPARE(model.FindDocumentIndex(DecodeUtf8(kYolo9000Id)), vecExpected.indexOf(DecodeUtf8(kYolo9000Title)));
        QCOMPARE(model.FindDocumentIndex(QStringLiteral("no-such-document")), -1);

        // 빠진 관계는 이름 · 종류 · 출처 문서로 적는다
        const QStringList vecDropped = model.DescribeDroppedRelations();
        QCOMPARE(vecDropped.size(), kDroppedCount);
        for (const QString& strPart : { QStringLiteral("YOLOv3"), QStringLiteral("사용"), QStringLiteral("darknet-53"), DecodeUtf8(kYolov3Title) })
            QVERIFY2(vecDropped[0].contains(strPart), qUtf8Printable(vecDropped[0]));
    }

    // 보기 6개: 순서 · 이름 · 파일 이름 열쇠, 점 · 선 수(design/Views.md §5).
    // 배치는 그 보기를 처음 달라고 할 때 계산하고(다른 보기는 그대로 계산 전), 같은 것을 다시 준다
    void viewsMatchDesign()
    {
        VisualMarkdownViewerModel model;
        QVERIFY(model.Load(ViewerTestSupport::GetShowcaseDir()));
        QVector<EGraphLayoutType> vecExpectedTypes;
        for (const ExpectedView& expected : kViews)
            vecExpectedTypes << expected.eType;
        QCOMPARE(VisualMarkdownViewerModel::ListViews(), vecExpectedTypes);
        for (const ExpectedView& expected : kViews)
        {
            QVERIFY(!model.HasLayout(expected.eType));
            QVERIFY(model.FindLayout(expected.eType) == nullptr);   // 찾기는 계산하지 않는다
        }

        for (int iView = 0; iView < kViews.size(); ++iView)
        {
            const ExpectedView& expected = kViews[iView];
            const QString strName = DecodeUtf8(expected.pKey);
            QCOMPARE(VisualMarkdownViewerModel::GetViewLabel(expected.eType), DecodeUtf8(expected.pLabel));
            QCOMPARE(VisualMarkdownViewerModel::GetViewKey(expected.eType), strName);
            const GraphLayoutOutput& layout = model.GetLayout(expected.eType);
            QVERIFY2(model.HasLayout(expected.eType), qUtf8Printable(strName));
            QCOMPARE(model.FindLayout(expected.eType), &layout);
            for (int iLater = iView + 1; iLater < kViews.size(); ++iLater)
                QVERIFY2(!model.HasLayout(kViews[iLater].eType), qUtf8Printable(strName));
            QCOMPARE(&model.GetLayout(expected.eType), &layout);   // 다시 계산하지 않는다
            if (expected.iNodes >= 0)
                QVERIFY2(layout.vecNodes.size() == expected.iNodes, qUtf8Printable(strName + QStringLiteral(" nodes ") + QString::number(layout.vecNodes.size())));
            if (expected.iEdges >= 0)
                QVERIFY2(layout.vecEdges.size() == expected.iEdges, qUtf8Printable(strName + QStringLiteral(" edges ") + QString::number(layout.vecEdges.size())));
        }

        QCOMPARE(model.GetLayout(EGraphLayoutType::LINEAGE).vecLanes.size(), kLineageLanes);
        const GraphLayoutOutput& timeline = model.GetLayout(EGraphLayoutType::TIMELINE);
        QCOMPARE(timeline.vecUnplaced.size(), kTimelineUnplaced);
        QStringList vecLanes;
        for (const LayoutLane& lane : timeline.vecLanes)
            vecLanes << lane.strLabel;
        QCOMPARE(vecLanes, kTimelineLanes);
        const GraphLayoutOutput& matrix = model.GetLayout(EGraphLayoutType::MATRIX);
        QCOMPARE(matrix.vecMatrixOrder.size(), kNodeCount);
        QCOMPARE(matrix.vecCells.size(), kMatrixCells);

        // 문서 ↔ 개념: 문서 6, 문서마다 선 수
        const GraphLayoutOutput& bipartite = model.GetLayout(EGraphLayoutType::BIPARTITE);
        QHash<QString, int> hashLinks;
        int iDocuments = 0;
        for (const LayoutNode& node : bipartite.vecNodes)
            if (node.bDocument)
                ++iDocuments;
        for (const LayoutEdge& edge : bipartite.vecEdges)
            if (edge.iFrom >= 0 && bipartite.vecNodes[edge.iFrom].bDocument)
                ++hashLinks[bipartite.vecNodes[edge.iFrom].strLabel];
        QCOMPARE(iDocuments, kDocumentCount);
        for (const DocumentLinks& expected : kBipartiteLinks)
            QVERIFY2(hashLinks.value(DecodeUtf8(expected.pTitle)) == expected.iLinks,
                     qUtf8Printable(DecodeUtf8(expected.pTitle) + QStringLiteral(": ") + QString::number(hashLinks.value(DecodeUtf8(expected.pTitle)))));
    }

    // 표 13개의 시작 차트가 showcase README §3 과 같은 종류다. 차트 설정도 그 종류 · 그 표 제목으로 만든다
    void tablesStartWithReadmeCharts()
    {
        VisualMarkdownViewerModel model;
        QVERIFY(model.Load(ViewerTestSupport::GetShowcaseDir()));
        QStringList vecActual;
        for (int iDocument = 0; iDocument < model.GetDocumentCount(); ++iDocument)
        {
            for (int iTable = 0; iTable < model.GetTableCount(iDocument); ++iTable)
            {
                const KnowledgeTable* pTable = model.FindTable(iDocument, iTable);
                QVERIFY(pTable != nullptr);
                const EChartType eRecommended = model.GetRecommendedChart(iDocument, iTable);
                vecActual << DescribeChart(model.FindDocument(iDocument)->strTitle, pTable->strTitle, eRecommended);
                QVERIFY(!model.GetChosenChart(iDocument, iTable).has_value());
                QCOMPARE(model.GetChartType(iDocument, iTable), eRecommended);
                const ChartSpec spec = model.BuildChart(iDocument, iTable);
                QCOMPARE(spec.eType, eRecommended);
                QCOMPARE(spec.strTitle, pTable->strTitle);
            }
        }
        QStringList vecExpected;
        for (const ExpectedChart& expected : kCharts)
            vecExpected << DescribeChart(DecodeUtf8(expected.pDocument), DecodeUtf8(expected.pTable), expected.eType);
        std::sort(vecActual.begin(), vecActual.end());
        std::sort(vecExpected.begin(), vecExpected.end());
        QCOMPARE(vecActual.size(), kTableCount);
        QCOMPARE(vecActual, vecExpected);

        // 표가 없는 문서(Focal Loss)
        QCOMPARE(model.GetTableCount(kFocalLossDocument), 0);
        QVERIFY(model.FindTable(kFocalLossDocument, 0) == nullptr);
        QVERIFY(model.FindTable(-1, 0) == nullptr);

        // 종류 이름(snapshot.tsv)과 화면 이름
        const QVector<QPair<EChartType, QPair<QString, QString>>> vecNames {
            { EChartType::BAR, { QStringLiteral("BAR"), QStringLiteral("막대") } },
            { EChartType::STEP_BAR, { QStringLiteral("STEP_BAR"), QStringLiteral("단계 막대") } },
            { EChartType::LINE, { QStringLiteral("LINE"), QStringLiteral("선") } },
            { EChartType::SCATTER, { QStringLiteral("SCATTER"), QStringLiteral("산점도") } },
            { EChartType::HEATMAP, { QStringLiteral("HEATMAP"), QStringLiteral("히트맵") } },
            { EChartType::DONUT, { QStringLiteral("DONUT"), QStringLiteral("도넛") } },
            { EChartType::NONE, { QStringLiteral("NONE"), QStringLiteral("그리지 않음") } },
        };
        for (const auto& pairName : vecNames)
        {
            QCOMPARE(VisualMarkdownViewerModel::GetChartTypeKey(pairName.first), pairName.second.first);
            QCOMPARE(VisualMarkdownViewerModel::GetChartTypeLabel(pairName.first), pairName.second.second);
        }
    }

    // 차트 종류를 바꾸고(SetStrategy) 추천으로 되돌린다(ResetStrategy). 표마다 따로 기억하고, 다시 읽으면 지운다
    void chartTypeCanChangeAndReset()
    {
        VisualMarkdownViewerModel model;
        QVERIFY(model.Load(ViewerTestSupport::GetShowcaseDir()));
        QCOMPARE(model.GetRecommendedChart(kPaymentDocument, kMonthlyTable), EChartType::LINE);
        QCOMPARE(model.GetRecommendedChart(kPaymentDocument, kLoadTable), EChartType::SCATTER);

        model.ChooseChart(kPaymentDocument, kMonthlyTable, EChartType::BAR);
        QCOMPARE(model.GetChosenChart(kPaymentDocument, kMonthlyTable), std::optional<EChartType>(EChartType::BAR));
        QCOMPARE(model.GetChartType(kPaymentDocument, kMonthlyTable), EChartType::BAR);
        QCOMPARE(model.BuildChart(kPaymentDocument, kMonthlyTable).eType, EChartType::BAR);
        QCOMPARE(model.GetRecommendedChart(kPaymentDocument, kMonthlyTable), EChartType::LINE);
        // 다른 표는 추천 그대로(고른 종류가 다른 표에 새지 않는다)
        QVERIFY(!model.GetChosenChart(kPaymentDocument, kLoadTable).has_value());
        QCOMPARE(model.BuildChart(kPaymentDocument, kLoadTable).eType, EChartType::SCATTER);
        QCOMPARE(model.BuildChart(kPaymentDocument, kMonthlyTable).eType, EChartType::BAR);

        model.ChooseChart(kPaymentDocument, kMonthlyTable, EChartType::STEP_BAR);
        QCOMPARE(model.BuildChart(kPaymentDocument, kMonthlyTable).eType, EChartType::STEP_BAR);
        model.ChooseChart(kPaymentDocument, kMonthlyTable, EChartType::HEATMAP);
        QCOMPARE(model.BuildChart(kPaymentDocument, kMonthlyTable).eType, EChartType::HEATMAP);
        model.ChooseChart(kPaymentDocument, kMonthlyTable, EChartType::DONUT);
        QCOMPARE(model.BuildChart(kPaymentDocument, kMonthlyTable).eType, EChartType::DONUT);
        model.ChooseChart(kPaymentDocument, kMonthlyTable, std::nullopt);
        QVERIFY(!model.GetChosenChart(kPaymentDocument, kMonthlyTable).has_value());
        QCOMPARE(model.GetChartType(kPaymentDocument, kMonthlyTable), EChartType::LINE);
        QCOMPARE(model.BuildChart(kPaymentDocument, kMonthlyTable).eType, EChartType::LINE);

        // 몫 표(결제 수단별 비중)는 도넛을 추천하고, 막대를 고르면 막대로 그리고 되돌리면 다시 도넛이다
        QCOMPARE(model.GetRecommendedChart(kPaymentDocument, kShareTable), EChartType::DONUT);
        QCOMPARE(model.BuildChart(kPaymentDocument, kShareTable).eType, EChartType::DONUT);
        model.ChooseChart(kPaymentDocument, kShareTable, EChartType::BAR);
        QCOMPARE(model.BuildChart(kPaymentDocument, kShareTable).eType, EChartType::BAR);
        model.ChooseChart(kPaymentDocument, kShareTable, std::nullopt);
        QCOMPARE(model.BuildChart(kPaymentDocument, kShareTable).eType, EChartType::DONUT);

        // 그리지 않는 표는 다른 종류를 골라도 NONE 이고 까닭을 남긴다
        QCOMPARE(model.GetRecommendedChart(kPaymentDocument, kPriorityTable), EChartType::NONE);
        model.ChooseChart(kPaymentDocument, kPriorityTable, EChartType::BAR);
        const ChartSpec specNone = model.BuildChart(kPaymentDocument, kPriorityTable);
        QCOMPARE(specNone.eType, EChartType::NONE);
        QVERIFY(!specNone.vecWarnings.isEmpty());

        // 다시 읽으면 고른 종류를 지운다
        model.ChooseChart(kPaymentDocument, kMonthlyTable, EChartType::SCATTER);
        QVERIFY(model.Load(ViewerTestSupport::GetShowcaseDir()));
        QVERIFY(!model.GetChosenChart(kPaymentDocument, kMonthlyTable).has_value());
        QVERIFY(!model.GetChosenChart(kPaymentDocument, kPriorityTable).has_value());

        // 없는 표는 고르지 않고, 빈 설정을 준다
        model.ChooseChart(kDocumentCount, 0, EChartType::BAR);
        QVERIFY(!model.GetChosenChart(kDocumentCount, 0).has_value());
        QCOMPARE(model.BuildChart(kDocumentCount, 0).eType, EChartType::NONE);
        QCOMPARE(model.BuildChart(kDocumentCount, 0).strTitle, QString());
    }

    // 없는 폴더를 읽으면 실패하고(까닭에 그 경로) 앞 상태(색인 · 배치 · 고른 차트)를 지킨다
    void failedLoadKeepsPreviousState()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString strMissing = dir.filePath(QStringLiteral("no-such-wiki"));

        VisualMarkdownViewerModel model;
        QVERIFY(model.Load(ViewerTestSupport::GetShowcaseDir()));
        const int iNetworkNodes = model.GetLayout(EGraphLayoutType::NETWORK).vecNodes.size();
        model.ChooseChart(kPaymentDocument, kMonthlyTable, EChartType::BAR);

        QVERIFY(!model.Load(strMissing));
        QVERIFY2(model.GetLastError().contains(strMissing), qUtf8Printable(model.GetLastError()));
        QVERIFY(model.IsLoaded());
        QCOMPARE(model.GetWikiRoot(), ViewerTestSupport::GetShowcaseDir());
        QCOMPARE(model.GetNodeCount(), kNodeCount);
        QCOMPARE(model.GetEdgeCount(), kEdgeCount);
        QCOMPARE(model.GetDocumentCount(), kDocumentCount);
        QCOMPARE(model.GetIndex().ListDocuments().size(), kDocumentCount);
        QVERIFY(model.HasLayout(EGraphLayoutType::NETWORK));
        QCOMPARE(model.GetLayout(EGraphLayoutType::NETWORK).vecNodes.size(), iNetworkNodes);
        QCOMPARE(model.GetChosenChart(kPaymentDocument, kMonthlyTable), std::optional<EChartType>(EChartType::BAR));

        // 처음부터 읽지 못하면 빈 상태다(보기는 빈 배치)
        VisualMarkdownViewerModel empty;
        QVERIFY(!empty.Load(strMissing));
        QVERIFY(!empty.IsLoaded());
        QVERIFY(!empty.GetLastError().isEmpty());
        QCOMPARE(empty.GetNodeCount(), 0);
        QCOMPARE(empty.GetDocumentCount(), 0);
        QVERIFY(empty.GetLayout(EGraphLayoutType::NETWORK).vecNodes.isEmpty());
    }

    // 다른 wiki 를 읽으면 배치를 지우고 새 wiki 로 다시 계산한다(YOLO 문서를 뺀 사본)
    void reloadReplacesLayouts()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString strCopy = dir.filePath(QStringLiteral("wiki"));
        QVERIFY(ViewerTestSupport::CopyWiki(ViewerTestSupport::GetShowcaseDir(), strCopy));
        QVERIFY(QFile::remove(strCopy + QLatin1Char('/') + DecodeUtf8(kYoloFile)));

        VisualMarkdownViewerModel model;
        QVERIFY(model.Load(ViewerTestSupport::GetShowcaseDir()));
        QCOMPARE(model.GetLayout(EGraphLayoutType::BIPARTITE).vecNodes.first().strLabel, DecodeUtf8(kPaymentTitle));
        int iDocuments = 0;
        for (const LayoutNode& node : model.GetLayout(EGraphLayoutType::BIPARTITE).vecNodes)
            iDocuments += node.bDocument ? 1 : 0;
        QCOMPARE(iDocuments, kDocumentCount);

        QVERIFY2(model.Load(strCopy), qUtf8Printable(model.GetLastError()));
        QCOMPARE(model.GetWikiRoot(), strCopy);
        QCOMPARE(model.GetDocumentCount(), kDocumentCount - 1);
        QVERIFY(!model.HasLayout(EGraphLayoutType::BIPARTITE));
        iDocuments = 0;
        for (const LayoutNode& node : model.GetLayout(EGraphLayoutType::BIPARTITE).vecNodes)
            iDocuments += node.bDocument ? 1 : 0;
        QCOMPARE(iDocuments, kDocumentCount - 1);
    }

    // 상태 줄의 개념 글: 이름 · 종류 · 연도(없으면 "연도 없음"). 모르는 id 는 그대로
    void describesConcepts()
    {
        VisualMarkdownViewerModel model;
        QVERIFY(model.Load(ViewerTestSupport::GetShowcaseDir()));
        QCOMPARE(model.DescribeConcept(QStringLiteral("yolo")), QStringLiteral("YOLO · 방법 · 2016"));
        QCOMPARE(model.DescribeConcept(QStringLiteral("객체-검출")), QStringLiteral("객체 검출 · 문제 · 연도 없음"));
        QCOMPARE(model.DescribeConcept(QStringLiteral("no-such-concept")), QStringLiteral("no-such-concept"));
    }

    // 경고가 있는 wiki(읽지 못한 개념 파일 하나): 경고 수와 목록을 준다
    void reportsWarnings()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString strCopy = dir.filePath(QStringLiteral("wiki"));
        QVERIFY(ViewerTestSupport::CopyWiki(ViewerTestSupport::GetShowcaseDir(), strCopy));
        QVERIFY(ViewerTestSupport::WriteText(strCopy + QStringLiteral("/concepts/broken.md"), QStringLiteral("# 머리글 없음\n")));

        VisualMarkdownViewerModel model;
        QVERIFY(model.Load(strCopy));
        const QStringList vecWarnings = model.GetWarnings();
        QCOMPARE(vecWarnings.size(), 1);
        QVERIFY2(vecWarnings[0].contains(QStringLiteral("broken.md")), qUtf8Printable(vecWarnings[0]));
        QCOMPARE(model.GetNodeCount(), kNodeCount);
    }
};

std::unique_ptr<QObject> CreateVisualMarkdownViewerModelTest() { return std::make_unique<VisualMarkdownViewerModelTest>(); }

#include "VisualMarkdownViewerModelTest.moc"
