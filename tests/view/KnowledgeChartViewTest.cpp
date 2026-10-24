#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"
#include "VisualMarkdown/Algorithm/ChartBuilder/KnowledgeChartBuilder.h"
#include "VisualMarkdown/KnowledgeIndex.h"
#include "VisualMarkdown/UI/KnowledgeChartView.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"
#include "ViewTestSupport.h"

#include <QMap>
#include <QTest>

#include <cmath>
#include <limits>
#include <memory>

namespace
{
// ================================================================================================
// 기대값: examples/showcase/README.md §3 의 표 12개(차트 종류는 KnowledgeChartBuilderTest 가 본다).
// ================================================================================================

// 표 제목(PNG 이름의 번호 순)
const QStringList kTableTitles {
    QStringLiteral("실시간 검출기와 그 밖의 검출기 — 정확도와 속도"),   // 1 산점도, 계열 2개
    QStringLiteral("YOLO 에서 YOLOv2 까지 — 하나씩 더한 개선"),         // 2 단계 막대
    QStringLiteral("입력 크기에 따른 정확도와 속도"),                   // 3 선 둘, 축 둘
    QStringLiteral("검출기별 VOC 2007 정확도"),                        // 4 막대
    QStringLiteral("입력 크기별 COCO 정확도"),                         // 5 선 둘
    QStringLiteral("처리 시간과 AP50"),                                // 6 산점도
    QStringLiteral("2단계 검출기의 VOC 2007 정확도"),                   // 7 막대
    QStringLiteral("학습 · 시험 시간"),                                // 8 나란한 막대, 축 둘, 빈 값
    QStringLiteral("월별 응답 시간"),                                  // 9 선 둘, 빈 값에서 끊긴다
    QStringLiteral("결제 수단별 비중"),                                // 10 막대
    QStringLiteral("동시 사용자와 응답 시간"),                          // 11 산점도
    QStringLiteral("기능 우선순위"),                                   // 12 그리지 않는다
};
constexpr int kSpeedChart = 0;
constexpr int kStepsChart = 1;
constexpr int kInputSizeChart = 2;
constexpr int kVocDetectorsChart = 3;
constexpr int kTrainTestChart = 7;
constexpr int kMonthlyChart = 8;
constexpr int kPriorityChart = 11;

// 알려진 표시: 검출기별 VOC 2007 정확도의 YOLO 막대(넷째 범주, 63.4 %)
constexpr int kYoloBar = 3;
constexpr const char* kYoloLabel = "YOLO";
constexpr const char* kYoloValue = "63.4%";
// 월별 응답 시간: p50 의 6월(170 ms), p95 의 7월은 빈 값
constexpr int kJuneP50 = 1;
constexpr const char* kJuneValue = "170 ms";
constexpr int kP95Series = 1;
constexpr int kJulyPoint = 2;
// 학습 · 시험 시간: 학습 시간의 Faster R-CNN 은 빈 값
constexpr int kFasterRcnnPoint = 2;
// 실시간 검출기 산점도: 실시간 계열의 셋째 점(FPS 155, mAP 52.7)
constexpr int kRealTimeSeries = 0;
constexpr int kFastestPoint = 2;
constexpr const char* kFastestValue = "52.7%";
// 단계 막대: 줄어드는 단계는 넷째 막대(69.5 → 69.2) 하나
constexpr int kDecreasingStep = 3;
// 입력 크기 표: FPS 계열(오른쪽 축)
constexpr int kFpsSeries = 1;

constexpr const char* kPriorityWord = "우선순위";   // NONE 의 까닭(경고)에 나오는 열 이름
const QSize kSmallSize(420, 300);
const QSize kNarrowSize(900, 560);

// 표시 자리를 견줄 때의 여유(px)
constexpr double kPixelTolerance = 0.5;      // 막대 끝 · 점 높이가 기대 자리에서 이만큼 안이다
constexpr double kStepJoinTolerance = 1.0;   // 뜬 막대의 아래 끝이 앞 막대의 위 끝에 이만큼 안으로 붙는다
constexpr double kOrderTolerance = 0.01;     // x 가 가장 큰 점이 가장 오른쪽이다(같은 자리는 이만큼 봐준다)
constexpr double kMarkSlack = 8;             // 값이 있는 표시의 가운데는 그림 칸 둘레 이만큼 안에 있다
constexpr double kMinBarSlack = 2.01;        // 가장 낮은 막대(2px)는 끝 자리를 보지 않는다

constexpr double kCategoryHalfWidth = 0.5;   // 범주 축은 -0.5 부터 범주 수 - 0.5 까지
constexpr int kSyntheticPoints = 2;          // 합성 설정의 점 수(범주 차트면 범주 가 · 나)

// 설명 글의 자리 표시(%1 · %2)를 담은 이름: 그대로 나와야 한다
constexpr const char* kPercentSeries = "계열 %1 이름 %2";
constexpr const char* kPercentAxis = "축 %2";
constexpr const char* kPercentCategory = "범주 %1";

// 막대 · 선 · 점을 하나씩 넣은 합성 설정(축 범위만 바꿔 시험한다)
ChartSpec MakeSyntheticSpec(EChartType type, double yMin, double yMax, double value)
{
    ChartSpec spec;
    spec.eType = type;
    spec.strTitle = QStringLiteral("합성 차트");
    spec.strCaption = QStringLiteral("축 시험");
    spec.xAxis.strTitle = QStringLiteral("x");
    const bool bCategorical = type != EChartType::SCATTER;
    if (bCategorical)
    {
        spec.xAxis.vecCategories = QStringList { QStringLiteral("가"), QStringLiteral("나") };
        spec.xAxis.dMin = -kCategoryHalfWidth;
        spec.xAxis.dMax = spec.xAxis.vecCategories.size() - kCategoryHalfWidth;
    }
    else
    {
        spec.xAxis.dMin = value;
        spec.xAxis.dMax = value;
        spec.xAxis.vecTicks = { value };
    }
    ChartAxis yAxis;
    yAxis.strTitle = QStringLiteral("y");
    yAxis.strUnit = QStringLiteral("%");
    yAxis.dMin = yMin;
    yAxis.dMax = yMax;
    yAxis.vecTicks = { yMin, yMax };
    spec.vecYAxes << yAxis;
    ChartSeries series;
    series.strName = QStringLiteral("값");
    series.strUnit = QStringLiteral("%");
    for (int iPoint = 0; iPoint < kSyntheticPoints; ++iPoint)
    {
        ChartPoint point;
        point.dX = bCategorical ? iPoint : value;
        point.optY = value;
        point.dBase = type == EChartType::STEP_BAR && iPoint > 0 ? value : 0;
        point.strLabel = bCategorical ? spec.xAxis.vecCategories[iPoint] : QString::number(value);
        series.vecPoints << point;
    }
    spec.vecSeries << series;
    return spec;
}
}

// 차트 위젯(KnowledgeChartView, design/Views.md §6 · §7)을 showcase 표 12개의 차트 설정으로 시험한다(offscreen).
class KnowledgeChartViewTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(m_index.Load(QStringLiteral(VISUALMARKDOWN_SHOWCASE_DIR)), VISUALMARKDOWN_SHOWCASE_DIR);
        QMap<QString, KnowledgeTable> mapTables;
        for (const KnowledgeDocument& document : m_index.ListDocuments())
            for (const KnowledgeTable& table : document.vecTables)
                mapTables.insert(table.strTitle, table);
        QCOMPARE(mapTables.size(), kTableTitles.size());
        KnowledgeChartBuilder builder;
        for (const QString& strTitle : kTableTitles)
        {
            QVERIFY2(mapTables.contains(strTitle), qUtf8Printable(strTitle));
            const KnowledgeTable table = mapTables.value(strTitle);
            ChartBuildInput in;
            in.pTable = &table;
            ChartSpec spec;
            QVERIFY(builder.Execute(in, spec));
            m_vecSpecs << spec;
        }
        QVERIFY(m_vecSpecs[kPriorityChart].eType == EChartType::NONE);
    }

    // 표 12개의 차트가 1600 × 1000 에 비지 않게 그려지고, 두 번 그리면 같다. 밝은 · 어두운 테마(PNG 를 남길 수 있다)
    void rendersEveryChart_data()
    {
        QTest::addColumn<int>("chart");
        for (int iChart = 0; iChart < kTableTitles.size(); ++iChart)
            QTest::newRow(qUtf8Printable(kTableTitles[iChart])) << iChart;
    }
    void rendersEveryChart()
    {
        QFETCH(int, chart);
        const ChartSpec& spec = m_vecSpecs[chart];
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetSpec(spec);
        QCOMPARE(view.GetSpec().strTitle, spec.strTitle);
        for (const bool bDark : { false, true })
        {
            const VisualMarkdownTheme theme = bDark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
            view.SetTheme(theme);
            const QImage image = ViewTestSupport::Render(view);
            QVERIFY(ViewTestSupport::HasEnoughInk(image, theme.background));
            QCOMPARE(ViewTestSupport::Render(view), image);
            QCOMPARE(image.pixelColor(1, view.height() - 2), theme.background);
            const QString strName = QStringLiteral("chart-%1-%2-%3").arg(chart + 1, 2, 10, QLatin1Char('0')).arg(GetTypeName(spec.eType), bDark ? "dark" : "light");
            QVERIFY2(ViewTestSupport::SaveSnapshot(image, strName), qPrintable(strName));
        }
        if (spec.eType == EChartType::NONE)
            return;
        // 값이 있는 점은 모두 그림 칸 안에 표시가 있고, 그 표시 가운데에서 그 점을 찾는다
        const QRectF rectPlot = view.GetPlotRect();
        QVERIFY(!rectPlot.isEmpty());
        for (int iSeries = 0; iSeries < spec.vecSeries.size(); ++iSeries)
        {
            for (int iPoint = 0; iPoint < spec.vecSeries[iSeries].vecPoints.size(); ++iPoint)
            {
                const QRectF rectMark = view.GetMarkRect(iSeries, iPoint);
                const QString strWhat = spec.vecSeries[iSeries].strName + QLatin1Char('/') + spec.vecSeries[iSeries].vecPoints[iPoint].strLabel
                                        + QLatin1Char(' ') + ViewTestSupport::DescribeRect(rectMark);
                if (!spec.vecSeries[iSeries].vecPoints[iPoint].optY)
                {
                    QVERIFY2(rectMark.isEmpty(), qUtf8Printable(strWhat));
                    continue;
                }
                QVERIFY2(!rectMark.isEmpty(), qUtf8Printable(strWhat));
                QVERIFY2(rectPlot.adjusted(-kMarkSlack, -kMarkSlack, kMarkSlack, kMarkSlack).contains(rectMark.center()),
                         qUtf8Printable(strWhat + ViewTestSupport::DescribeRect(rectPlot)));
                QCOMPARE(view.FindPointAt(rectMark.center().toPoint()), qMakePair(iSeries, iPoint));
                QVERIFY2(view.GetToolTipAt(rectMark.center().toPoint()).contains(spec.vecSeries[iSeries].strName), qUtf8Printable(strWhat));
            }
        }
    }

    // 그릴 수 없는 표(기능 우선순위): 가운데에 까닭을 쓰고, 찾을 점이 없다
    void noneChartShowsMessage()
    {
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetSpec(m_vecSpecs[kPriorityChart]);
        const QString strMessage = view.GetMessageText();
        QVERIFY(!strMessage.isEmpty());
        QVERIFY2(strMessage.contains(QString::fromUtf8(kPriorityWord)), qUtf8Printable(strMessage));
        for (const QString& strWarning : m_vecSpecs[kPriorityChart].vecWarnings)
            QVERIFY2(strMessage.contains(strWarning), qUtf8Printable(strWarning));
        QVERIFY(view.GetPlotRect().isEmpty());
        QCOMPARE(view.FindPointAt(QPoint(800, 500)), qMakePair(-1, -1));
        QCOMPARE(view.GetToolTipAt(QPoint(800, 500)), QString());

        // 그리는 차트는 가운데 글이 없다
        view.SetSpec(m_vecSpecs[kVocDetectorsChart]);
        QCOMPARE(view.GetMessageText(), QString());
    }

    // 알려진 막대: YOLO(63.4 %) 막대를 찾고 설명에 계열 · x 이름표 · 값과 단위가 나온다
    void findsKnownBar()
    {
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetSpec(m_vecSpecs[kVocDetectorsChart]);
        const QRectF rectBar = view.GetMarkRect(0, kYoloBar);
        QVERIFY(!rectBar.isEmpty());
        QCOMPARE(view.FindPointAt(rectBar.center().toPoint()), qMakePair(0, kYoloBar));
        const QString strTip = view.GetToolTipAt(rectBar.center().toPoint());
        for (const QString& strPart : { QStringLiteral("mAP"), QString::fromUtf8(kYoloLabel), QString::fromUtf8(kYoloValue) })
            QVERIFY2(strTip.contains(strPart), qUtf8Printable(strPart + QStringLiteral(" in ") + strTip));
        // 막대 키는 값에 비례한다: YOLO(63.4) 막대는 Fast R-CNN(70.0) 막대보다 낮다
        QVERIFY(rectBar.height() < view.GetMarkRect(0, 0).height());
        QVERIFY(std::abs(rectBar.bottom() - view.GetMarkRect(0, 0).bottom()) < kPixelTolerance);
    }

    // 선의 점: p50 의 6월(170 ms). 빈 값(p95 의 7월)은 표시가 없고 그 자리에서 찾지 않는다
    void lineBreaksAtMissingValue()
    {
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetSpec(m_vecSpecs[kMonthlyChart]);
        const QRectF rectJune = view.GetMarkRect(0, kJuneP50);
        QCOMPARE(view.FindPointAt(rectJune.center().toPoint()), qMakePair(0, kJuneP50));
        const QString strTip = view.GetToolTipAt(rectJune.center().toPoint());
        for (const QString& strPart : { QStringLiteral("p50"), QStringLiteral("6월"), QString::fromUtf8(kJuneValue) })
            QVERIFY2(strTip.contains(strPart), qUtf8Printable(strPart + QStringLiteral(" in ") + strTip));
        QVERIFY(view.GetMarkRect(kP95Series, kJulyPoint).isEmpty());
        // p95 의 6월과 8월 사이 가운데(7월 자리)에는 p95 점이 없다
        const QPointF ptGap = (view.GetMarkRect(kP95Series, kJulyPoint - 1).center() + view.GetMarkRect(kP95Series, kJulyPoint + 1).center()) / 2;
        QVERIFY(view.FindPointAt(ptGap.toPoint()) != qMakePair(kP95Series, kJulyPoint));

        // 나란한 막대의 빈 값(학습 시간의 Faster R-CNN)도 표시가 없다
        view.SetSpec(m_vecSpecs[kTrainTestChart]);
        QVERIFY(view.GetMarkRect(0, kFasterRcnnPoint).isEmpty());
        QVERIFY(!view.GetMarkRect(1, kFasterRcnnPoint).isEmpty());
    }

    // 산점도의 점: 실시간 계열의 FPS 155(52.7 %)
    void findsKnownScatterPoint()
    {
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetSpec(m_vecSpecs[kSpeedChart]);
        const QRectF rectPoint = view.GetMarkRect(kRealTimeSeries, kFastestPoint);
        QCOMPARE(view.FindPointAt(rectPoint.center().toPoint()), qMakePair(kRealTimeSeries, kFastestPoint));
        const QString strTip = view.GetToolTipAt(rectPoint.center().toPoint());
        for (const QString& strPart : { QStringLiteral("실시간"), QStringLiteral("155"), QString::fromUtf8(kFastestValue) })
            QVERIFY2(strTip.contains(strPart), qUtf8Printable(strPart + QStringLiteral(" in ") + strTip));
        // x 가 가장 큰 점(155)이 가장 오른쪽이다
        for (int iSeries = 0; iSeries < m_vecSpecs[kSpeedChart].vecSeries.size(); ++iSeries)
            for (int iPoint = 0; iPoint < m_vecSpecs[kSpeedChart].vecSeries[iSeries].vecPoints.size(); ++iPoint)
                QVERIFY(view.GetMarkRect(iSeries, iPoint).center().x() <= rectPoint.center().x() + kOrderTolerance);
    }

    // 단계 막대: 앞 값에서 이 값까지 뜬 막대. 값이 있는 첫 막대(시작 값)는 시작 색, 줄어든 단계는 줄어듦 색, 나머지는 늘어남 색이다.
    // 범례는 시작 · 늘어남 · 줄어듦 셋이다. 시작 막대의 설명에는 앞 단계가 없다
    void stepBarsColourByDirection_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }
    void stepBarsColourByDirection()
    {
        QFETCH(bool, dark);
        const VisualMarkdownTheme theme = dark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetTheme(theme);
        const ChartSpec& spec = m_vecSpecs[kStepsChart];
        view.SetSpec(spec);
        const QImage image = ViewTestSupport::Render(view);
        const ChartSeries& series = spec.vecSeries[0];
        const QVector<QColor> vecColors { theme.stepStart, theme.stepIncrease, theme.stepDecrease };
        for (int iPoint = 0; iPoint < series.vecPoints.size(); ++iPoint)
        {
            const QRectF rectBar = view.GetMarkRect(0, iPoint);
            const QColor pixel = image.pixelColor(rectBar.center().toPoint());
            const bool bDecrease = *series.vecPoints[iPoint].optY < series.vecPoints[iPoint].dBase;
            QCOMPARE(bDecrease, iPoint == kDecreasingStep);
            const QColor expected = iPoint == 0 ? theme.stepStart : (bDecrease ? theme.stepDecrease : theme.stepIncrease);
            // 막대 가운데 점은 세 색 가운데 기대 색에 가장 가깝다
            for (const QColor& other : vecColors)
                QVERIFY2(other == expected || ViewTestSupport::GetColorDistance(pixel, expected) < ViewTestSupport::GetColorDistance(pixel, other),
                         qUtf8Printable(series.vecPoints[iPoint].strLabel + QLatin1Char(' ') + pixel.name() + QStringLiteral(" 기대 ") + expected.name()));
        }
        // 뜬 막대: 둘째 막대(63.4 → 65.8)의 아래 끝은 첫 막대의 위 끝과 같은 높이다
        QVERIFY(std::abs(view.GetMarkRect(0, 1).bottom() - view.GetMarkRect(0, 0).top()) < kStepJoinTolerance);
        // 범례: 시작 · 늘어남 · 줄어듦 견본이 모두 있다
        const QRect rectLegend = view.GetLegendRect().toAlignedRect();
        QVERIFY(!rectLegend.isEmpty());
        for (const QColor& color : vecColors)
            QVERIFY2(ViewTestSupport::HasColorIn(image, rectLegend, color), qUtf8Printable(color.name()));
        // 설명: 시작 막대는 앞 단계를 말하지 않고, 그다음 막대는 앞 단계에서 얼마나 바뀌었는지 말한다
        QVERIFY(!view.GetToolTipAt(view.GetMarkRect(0, 0).center().toPoint()).contains(QStringLiteral("앞 단계")));
        QVERIFY(view.GetToolTipAt(view.GetMarkRect(0, 1).center().toPoint()).contains(QStringLiteral("앞 단계")));
    }

    // 앞쪽 점이 비면 값이 있는 첫 막대가 시작 막대다(합성 단계 막대: 첫 칸 빈 값)
    void stepStartIsFirstValuedBar()
    {
        ChartSpec spec = MakeSyntheticSpec(EChartType::STEP_BAR, 0, 20, 10);
        spec.xAxis.vecCategories << QStringLiteral("다");
        spec.xAxis.dMax = spec.xAxis.vecCategories.size() - kCategoryHalfWidth;
        ChartPoint third;
        third.dX = 2;
        third.optY = 15;
        third.dBase = 10;
        third.strLabel = QStringLiteral("다");
        spec.vecSeries[0].vecPoints[0].optY.reset();   // 첫 칸은 빈 값
        spec.vecSeries[0].vecPoints[1].dBase = 0;      // 값이 있는 첫 점은 0 부터
        spec.vecSeries[0].vecPoints << third;
        const VisualMarkdownTheme theme = VisualMarkdownTheme::MakeLight();
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth / 2, ViewTestSupport::kSnapshotHeight / 2);
        view.SetTheme(theme);
        view.SetSpec(spec);
        const QImage image = ViewTestSupport::Render(view);
        QVERIFY(view.GetMarkRect(0, 0).isEmpty());
        const QColor start = image.pixelColor(view.GetMarkRect(0, 1).center().toPoint());
        const QColor increase = image.pixelColor(view.GetMarkRect(0, 2).center().toPoint());
        QVERIFY2(ViewTestSupport::GetColorDistance(start, theme.stepStart) < ViewTestSupport::GetColorDistance(start, theme.stepIncrease), qPrintable(start.name()));
        QVERIFY2(ViewTestSupport::GetColorDistance(increase, theme.stepIncrease) < ViewTestSupport::GetColorDistance(increase, theme.stepStart),
                 qPrintable(increase.name()));
        QVERIFY(!view.GetToolTipAt(view.GetMarkRect(0, 1).center().toPoint()).contains(QStringLiteral("앞 단계")));
    }

    // 설명에 넣는 계열 이름 · x 축 이름 · x 이름표에 %1 · %2 가 있어도 그대로 나온다(막대 · 단계 막대 합성 설정)
    void seriesNameWithPercentSignsIsKept_data()
    {
        QTest::addColumn<int>("type");
        QTest::newRow("bar") << static_cast<int>(EChartType::BAR);
        QTest::newRow("stepbar") << static_cast<int>(EChartType::STEP_BAR);
    }
    void seriesNameWithPercentSignsIsKept()
    {
        QFETCH(int, type);
        ChartSpec spec = MakeSyntheticSpec(static_cast<EChartType>(type), 0, 20, 10);
        spec.vecSeries[0].strName = QString::fromUtf8(kPercentSeries);
        spec.xAxis.strTitle = QString::fromUtf8(kPercentAxis);
        spec.xAxis.vecCategories[1] = QString::fromUtf8(kPercentCategory);
        spec.vecSeries[0].vecPoints[1].strLabel = QString::fromUtf8(kPercentCategory);
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth / 2, ViewTestSupport::kSnapshotHeight / 2);
        view.SetSpec(spec);
        const QString strTip = view.GetToolTipAt(view.GetMarkRect(0, 1).center().toPoint());
        for (const QString& strPart : { QString::fromUtf8(kPercentSeries), QString::fromUtf8(kPercentAxis) + QStringLiteral(": ") + QString::fromUtf8(kPercentCategory),
                                        QStringLiteral("10%") })
            QVERIFY2(strTip.contains(strPart), qUtf8Printable(strPart + QStringLiteral(" in ") + strTip));
    }

    // 나란한 막대: 한 범주 안에서 계열 순으로 겹치지 않게 서고, 모두 그 범주 칸 안에 있다. 각 막대는 제 축(h · s)의 값 높이다
    void groupedBarsSitSideBySide()
    {
        const ChartSpec& spec = m_vecSpecs[kTrainTestChart];
        QCOMPARE(spec.vecSeries.size(), 2);
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetSpec(spec);
        const QRectF rectPlot = view.GetPlotRect();
        const int iCategories = spec.xAxis.vecCategories.size();
        const double dSlot = rectPlot.width() / iCategories;
        for (int iPoint = 0; iPoint < iCategories; ++iPoint)
        {
            const QRectF rectFirst = view.GetMarkRect(0, iPoint);
            const QRectF rectSecond = view.GetMarkRect(1, iPoint);
            const double dSlotLeft = rectPlot.left() + iPoint * dSlot;
            for (const QRectF& rectBar : { rectFirst, rectSecond })
                if (!rectBar.isEmpty())
                    QVERIFY(rectBar.left() >= dSlotLeft && rectBar.right() <= dSlotLeft + dSlot);
            if (!rectFirst.isEmpty() && !rectSecond.isEmpty())
                QVERIFY(rectFirst.right() <= rectSecond.left());
            for (int iSeries = 0; iSeries < 2; ++iSeries)
            {
                const ChartPoint& point = spec.vecSeries[iSeries].vecPoints[iPoint];
                if (!point.optY)
                    continue;
                const ChartAxis& axis = spec.vecYAxes[spec.vecSeries[iSeries].iAxis];
                const double dExpectedTop = rectPlot.bottom() - (*point.optY - axis.dMin) / (axis.dMax - axis.dMin) * rectPlot.height();
                const QRectF rectBar = view.GetMarkRect(iSeries, iPoint);
                QVERIFY2(std::abs(rectBar.top() - dExpectedTop) < kPixelTolerance || rectBar.height() <= kMinBarSlack, qUtf8Printable(point.strLabel));
                QVERIFY(std::abs(rectBar.bottom() - rectPlot.bottom()) < kPixelTolerance || rectBar.height() <= kMinBarSlack);
            }
        }
    }

    // 범례: 계열이 둘 이상이면 있고, 하나뿐이면 없다
    void legendOnlyForSeveralSeries()
    {
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        for (const ChartSpec& spec : m_vecSpecs)
        {
            view.SetSpec(spec);
            if (spec.eType == EChartType::NONE || spec.eType == EChartType::STEP_BAR)
                continue;
            const bool bLegend = !view.GetLegendRect().isEmpty();
            QVERIFY2(bLegend == (spec.vecSeries.size() > 1), qUtf8Printable(spec.strTitle));
            if (bLegend)
                QVERIFY(!view.GetLegendRect().intersects(view.GetPlotRect()));
        }
    }

    // 오른쪽 축 계열은 오른쪽 축 범위로 놓인다: FPS 91(288 px)은 FPS 축(30–100)의 위쪽, mAP 69.0 은 mAP 축(68–80)의 아래쪽
    void rightAxisSeriesUseRightScale()
    {
        const ChartSpec& spec = m_vecSpecs[kInputSizeChart];
        QCOMPARE(spec.vecYAxes.size(), 2);
        QCOMPARE(spec.vecSeries[kFpsSeries].iAxis, 1);
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetSpec(spec);
        const QRectF rectPlot = view.GetPlotRect();
        for (int iSeries = 0; iSeries < spec.vecSeries.size(); ++iSeries)
        {
            const ChartAxis& axis = spec.vecYAxes[spec.vecSeries[iSeries].iAxis];
            for (int iPoint = 0; iPoint < spec.vecSeries[iSeries].vecPoints.size(); ++iPoint)
            {
                const double dValue = *spec.vecSeries[iSeries].vecPoints[iPoint].optY;
                const double dExpectedY = rectPlot.bottom() - (dValue - axis.dMin) / (axis.dMax - axis.dMin) * rectPlot.height();
                QVERIFY2(std::abs(view.GetMarkRect(iSeries, iPoint).center().y() - dExpectedY) < kPixelTolerance,
                         qUtf8Printable(spec.vecSeries[iSeries].strName + QString::number(dValue)));
            }
        }
    }

    // 범위가 0 · 거꾸로 · 유한하지 않은 축, 아주 큰 값도 죽지 않고 그린다(축을 ±1 로 넓힌다)
    void degenerateAxesDoNotCrash_data()
    {
        QTest::addColumn<int>("type");
        QTest::addColumn<double>("yMin");
        QTest::addColumn<double>("yMax");
        QTest::addColumn<double>("value");
        const double dInfinity = std::numeric_limits<double>::infinity();
        const double dNaN = std::numeric_limits<double>::quiet_NaN();
        const double dHuge = std::numeric_limits<double>::max();
        for (const EChartType eType : { EChartType::BAR, EChartType::STEP_BAR, EChartType::LINE, EChartType::SCATTER })
        {
            const QString strType = GetTypeName(eType);
            QTest::newRow(qPrintable(strType + " zero width")) << static_cast<int>(eType) << 5.0 << 5.0 << 5.0;
            QTest::newRow(qPrintable(strType + " inverted")) << static_cast<int>(eType) << 10.0 << -10.0 << 3.0;
            QTest::newRow(qPrintable(strType + " infinite")) << static_cast<int>(eType) << -dInfinity << dInfinity << 1.0;
            QTest::newRow(qPrintable(strType + " nan")) << static_cast<int>(eType) << dNaN << dNaN << 2.0;
            QTest::newRow(qPrintable(strType + " huge")) << static_cast<int>(eType) << 1e308 << 1e308 << 1e308;
            QTest::newRow(qPrintable(strType + " max")) << static_cast<int>(eType) << dHuge << dHuge << dHuge;
            QTest::newRow(qPrintable(strType + " huge span")) << static_cast<int>(eType) << -dHuge << dHuge << 0.0;
            QTest::newRow(qPrintable(strType + " nan value")) << static_cast<int>(eType) << 0.0 << 10.0 << dNaN;
        }
    }
    void degenerateAxesDoNotCrash()
    {
        QFETCH(int, type);
        QFETCH(double, yMin);
        QFETCH(double, yMax);
        QFETCH(double, value);
        const ChartSpec spec = MakeSyntheticSpec(static_cast<EChartType>(type), yMin, yMax, value);
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth / 2, ViewTestSupport::kSnapshotHeight / 2);
        view.SetSpec(spec);
        const QImage image = ViewTestSupport::Render(view);
        QVERIFY(ViewTestSupport::HasEnoughInk(image, VisualMarkdownTheme::MakeLight().background));
        QVERIFY(!view.GetPlotRect().isEmpty());
        for (int iPoint = 0; iPoint < spec.vecSeries[0].vecPoints.size(); ++iPoint)
        {
            const QRectF rectMark = view.GetMarkRect(0, iPoint);
            if (rectMark.isEmpty())
                continue;
            QVERIFY(std::isfinite(rectMark.x()) && std::isfinite(rectMark.y()) && std::isfinite(rectMark.width()) && std::isfinite(rectMark.height()));
            view.FindPointAt(rectMark.center().toPoint());
        }
        for (int iY = 0; iY < view.height(); iY += 37)
            view.GetToolTipAt(QPoint(view.width() / 2, iY));
    }

    // 붐비는 범주(긴 이름 40개)와 작은 위젯도 죽지 않고 그린다. 위젯이 작아도 표시는 그림 칸 안이다
    void crowdedCategoriesAndSmallWidget()
    {
        ChartSpec spec = MakeSyntheticSpec(EChartType::BAR, 0, 100, 50);
        spec.xAxis.vecCategories.clear();
        spec.vecSeries[0].vecPoints.clear();
        constexpr int kCategories = 40;
        for (int iCategory = 0; iCategory < kCategories; ++iCategory)
        {
            spec.xAxis.vecCategories << QStringLiteral("아주 긴 범주 이름 %1번째 항목").arg(iCategory);
            ChartPoint point;
            point.dX = iCategory;
            point.optY = iCategory * 2.5;
            point.strLabel = spec.xAxis.vecCategories.last();
            spec.vecSeries[0].vecPoints << point;
        }
        spec.xAxis.dMin = -kCategoryHalfWidth;
        spec.xAxis.dMax = kCategories - kCategoryHalfWidth;
        KnowledgeChartView view;
        view.resize(kSmallSize);
        view.SetSpec(spec);
        QVERIFY(ViewTestSupport::HasEnoughInk(ViewTestSupport::Render(view), VisualMarkdownTheme::MakeLight().background));
        QVERIFY(QRectF(view.rect()).contains(view.GetPlotRect()));
        // 큰 위젯에서도 40 범주는 기울여 쓴다. 좁은 위젯의 단계 막대 이름표는 두 줄로 접는다(눈으로 보려고 PNG 를 남긴다)
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight / 2);
        const QImage imageCrowded = ViewTestSupport::Render(view);
        QVERIFY(ViewTestSupport::SaveSnapshot(imageCrowded, QStringLiteral("extra-chart-crowded-light")));
        // 기울인 이름표도 위젯 가장자리 밖으로 나가지 않는다: 맨 왼쪽 · 오른쪽 열은 바탕 색뿐이다
        const QColor background = VisualMarkdownTheme::MakeLight().background;
        for (int iY = 0; iY < imageCrowded.height(); ++iY)
        {
            QCOMPARE(imageCrowded.pixelColor(1, iY), background);
            QCOMPARE(imageCrowded.pixelColor(imageCrowded.width() - 2, iY), background);
        }
        view.resize(kNarrowSize);
        view.SetSpec(m_vecSpecs[kStepsChart]);
        QVERIFY(QRectF(view.rect()).contains(view.GetPlotRect()));
        QVERIFY(ViewTestSupport::SaveSnapshot(ViewTestSupport::Render(view), QStringLiteral("extra-chart-02-narrow-light")));
        view.resize(kSmallSize);
        view.SetSpec(m_vecSpecs[kTrainTestChart]);
        QVERIFY(ViewTestSupport::HasEnoughInk(ViewTestSupport::Render(view), VisualMarkdownTheme::MakeLight().background));
        view.resize(40, 30);
        ViewTestSupport::Render(view);
        view.SetSpec(ChartSpec());
        ViewTestSupport::Render(view);
        QCOMPARE(view.FindPointAt(QPoint(5, 5)), qMakePair(-1, -1));
    }

private:
    static QString GetTypeName(EChartType type)
    {
        switch (type)
        {
        case EChartType::NONE: return QStringLiteral("none");
        case EChartType::BAR: return QStringLiteral("bar");
        case EChartType::STEP_BAR: return QStringLiteral("stepbar");
        case EChartType::LINE: return QStringLiteral("line");
        case EChartType::SCATTER: return QStringLiteral("scatter");
        }
        return QStringLiteral("unknown");
    }

private:
    KnowledgeIndex m_index;            // showcase wiki 를 읽은 색인
    QVector<ChartSpec> m_vecSpecs;     // kTableTitles 순의 시작 차트 설정
};

std::unique_ptr<QObject> CreateKnowledgeChartViewTest() { return std::make_unique<KnowledgeChartViewTest>(); }

#include "KnowledgeChartViewTest.moc"
