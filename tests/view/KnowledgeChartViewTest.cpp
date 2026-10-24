#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"
#include "VisualMarkdown/Algorithm/ChartBuilder/KnowledgeChartBuilder.h"
#include "VisualMarkdown/KnowledgeIndex.h"
#include "VisualMarkdown/UI/KnowledgeChartView.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"
#include "ViewTestSupport.h"

#include <QMap>
#include <QPair>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace
{
// ================================================================================================
// 기대값: examples/showcase/README.md §3 의 표 13개(차트 종류는 KnowledgeChartBuilderTest 가 본다).
// ================================================================================================

// 표 제목(PNG 이름의 번호 순)
const QStringList kTableTitles {
    QStringLiteral("실시간 검출기와 그 밖의 검출기 — 정확도와 속도"),   // 1 산점도, 계열 2개
    QStringLiteral("YOLO 에서 YOLOv2 까지 — 하나씩 더한 개선"),         // 2 단계 막대
    QStringLiteral("입력 크기에 따른 정확도와 속도"),                   // 3 선 둘, 축 둘
    QStringLiteral("검출기별 VOC 2007 정확도"),                        // 4 막대
    QStringLiteral("입력 크기별 COCO 정확도"),                         // 5 선 둘
    QStringLiteral("처리 시간과 AP50"),                                // 6 산점도
    QStringLiteral("검출기별 COCO 정확도"),                            // 7 히트맵(검출기 10 × AP … APL 6)
    QStringLiteral("2단계 검출기의 VOC 2007 정확도"),                   // 8 막대
    QStringLiteral("학습 · 시험 시간"),                                // 9 나란한 막대, 축 둘, 빈 값
    QStringLiteral("월별 응답 시간"),                                  // 10 선 둘, 빈 값에서 끊긴다
    QStringLiteral("결제 수단별 비중"),                                // 11 막대
    QStringLiteral("동시 사용자와 응답 시간"),                          // 12 산점도
    QStringLiteral("기능 우선순위"),                                   // 13 그리지 않는다
};
constexpr int kSpeedChart = 0;
constexpr int kStepsChart = 1;
constexpr int kInputSizeChart = 2;
constexpr int kVocDetectorsChart = 3;
constexpr int kHeatmapChart = 6;
constexpr int kTrainTestChart = 8;
constexpr int kMonthlyChart = 9;
constexpr int kPriorityChart = 12;

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
// 히트맵: 행 10(검출기) × 열 6(AP · AP50 · AP75 · APS · APM · APL). AP 열에서 가장 큰 값은 RetinaNet X-101(40.8), 가장 작은 값은 YOLOv2(21.6)
constexpr int kHeatmapRows = 10;
constexpr int kHeatmapColumns = 6;
constexpr int kApColumn = 0;
constexpr int kHighestApRow = 8;
constexpr int kLowestApRow = 4;
constexpr const char* kLowestApTip = "YOLOv2 · AP: 21.6%";   // 마우스 올림: 행 이름 · 열 이름: 값 단위
constexpr double kCellGap = 2;               // 칸 사이 바탕 색 틈(px)
constexpr double kMinFillTextContrast = 4.5; // 채움 위 글자 대비(WCAG 본문)
constexpr double kRampMiddle = 0.5;          // 범위가 0 인 색 눈금의 칸 색: 크기 눈금의 가운데
constexpr const char* kEmptyCellLabel = "값 없음";
constexpr const char* kNoValueTitle = "그릴 값이 없다";
const QSize kHeatmapLabelSize(640, 420);     // 긴 이름이 줄어드는 작은 위젯
// 색 눈금 하나인 히트맵의 긴 y 열 이름(다 쓰면 색 범례가 위젯 밖으로 나간다). 값은 10 · 30 ms
constexpr const char* kLongScaleName = "이미지 한 장을 처리하는 데 걸린 시간의 평균값(배치 크기 1, GPU 한 장, 전처리와 후처리를 모두 넣은 값)";
constexpr const char* kLongScaleUnit = "ms";
constexpr double kLongScaleLow = 10;
constexpr double kLongScaleHigh = 30;

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

// 히트맵 합성 설정: 행 가 · 나 · 다, 열 AP(%, 10 · 빈 값 · 30)와 FPS(가 에만 점 5, 범위 0). 나 · FPS 칸은 점이 없다
ChartSpec MakeSyntheticHeatmap()
{
    ChartSpec spec;
    spec.eType = EChartType::HEATMAP;
    spec.strTitle = QStringLiteral("합성 히트맵");
    spec.strCaption = QStringLiteral("칸 시험");
    spec.xAxis.strTitle = QStringLiteral("방법");
    spec.xAxis.vecCategories = QStringList { QStringLiteral("가"), QStringLiteral("나"), QStringLiteral("다") };
    spec.xAxis.dMin = -kCategoryHalfWidth;
    spec.xAxis.dMax = spec.xAxis.vecCategories.size() - kCategoryHalfWidth;
    ChartAxis apAxis;
    apAxis.strTitle = QStringLiteral("AP");
    apAxis.strUnit = QStringLiteral("%");
    apAxis.dMin = 10;
    apAxis.dMax = 30;
    ChartAxis fpsAxis;
    fpsAxis.strTitle = QStringLiteral("FPS");
    fpsAxis.dMin = 5;
    fpsAxis.dMax = 5;
    spec.vecYAxes << apAxis << fpsAxis;
    ChartSeries ap;
    ap.strName = QStringLiteral("AP");
    ap.strUnit = QStringLiteral("%");
    ap.iAxis = 0;
    const QVector<std::optional<double>> vecApValues { 10.0, std::nullopt, 30.0 };
    for (int iRow = 0; iRow < vecApValues.size(); ++iRow)
    {
        ChartPoint point;
        point.dX = iRow;
        point.optY = vecApValues[iRow];
        point.strLabel = spec.xAxis.vecCategories[iRow];
        ap.vecPoints << point;
    }
    ChartSeries fps;
    fps.strName = QStringLiteral("FPS");
    fps.iAxis = 1;
    ChartPoint fpsPoint;
    fpsPoint.dX = 0;
    fpsPoint.optY = 5;
    fpsPoint.strLabel = spec.xAxis.vecCategories[0];
    fps.vecPoints << fpsPoint;
    spec.vecSeries << ap << fps;
    return spec;
}

// 색 눈금 하나인 히트맵 합성 설정: 행 가 · 나, y 열 하나(name · unit)에 값 low · high
ChartSpec MakeSingleScaleHeatmap(const QString& name, const QString& unit, double low, double high)
{
    ChartSpec spec;
    spec.eType = EChartType::HEATMAP;
    spec.strTitle = QStringLiteral("긴 y 이름");
    spec.xAxis.strTitle = QStringLiteral("방법");
    spec.xAxis.vecCategories = QStringList { QStringLiteral("가"), QStringLiteral("나") };
    spec.xAxis.dMin = -kCategoryHalfWidth;
    spec.xAxis.dMax = spec.xAxis.vecCategories.size() - kCategoryHalfWidth;
    ChartAxis axis;
    axis.strTitle = name;
    axis.strUnit = unit;
    axis.dMin = low;
    axis.dMax = high;
    spec.vecYAxes << axis;
    ChartSeries series;
    series.strName = name;
    series.strUnit = unit;
    const QVector<double> vecValues { low, high };
    for (int iRow = 0; iRow < vecValues.size(); ++iRow)
    {
        ChartPoint point;
        point.dX = iRow;
        point.optY = vecValues[iRow];
        point.strLabel = spec.xAxis.vecCategories[iRow];
        series.vecPoints << point;
    }
    spec.vecSeries << series;
    return spec;
}

// rect 안에서 그 색과 똑같은 점이 있는 가장 왼쪽 · 가장 오른쪽 x. 없으면 (-1, -1)
QPair<int, int> FindColorColumns(const QImage& image, const QRect& rect, const QColor& color)
{
    const QRect rectInside = rect.intersected(image.rect());
    QPair<int, int> pairColumns(-1, -1);
    for (int iY = rectInside.top(); iY <= rectInside.bottom(); ++iY)
    {
        for (int iX = rectInside.left(); iX <= rectInside.right(); ++iX)
        {
            if (image.pixelColor(iX, iY) != color)
                continue;
            pairColumns.first = pairColumns.first < 0 ? iX : std::min(pairColumns.first, iX);
            pairColumns.second = std::max(pairColumns.second, iX);
        }
    }
    return pairColumns;
}
}

// 차트 위젯(KnowledgeChartView, design/Views.md §6 · §7 · design/MoreViews.md §4.3)을 showcase 표 13개의 차트 설정으로 시험한다(offscreen).
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

    // 표 13개의 차트가 1600 × 1000 에 비지 않게 그려지고, 두 번 그리면 같다. 밝은 · 어두운 테마(PNG 를 남길 수 있다)
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
                // 값이 없는 점은 표시가 없다(히트맵은 빈 칸을 테두리와 "—" 로 그려 칸이 있다)
                if (!spec.vecSeries[iSeries].vecPoints[iPoint].optY && spec.eType != EChartType::HEATMAP)
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

    // 범례: 계열이 둘 이상이면 있고, 하나뿐이면 없다(히트맵의 색 범례는 heatmapDrawsCells 가 본다)
    void legendOnlyForSeveralSeries()
    {
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        for (const ChartSpec& spec : m_vecSpecs)
        {
            view.SetSpec(spec);
            if (spec.eType == EChartType::NONE || spec.eType == EChartType::STEP_BAR || spec.eType == EChartType::HEATMAP)
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
        for (const EChartType eType : { EChartType::BAR, EChartType::STEP_BAR, EChartType::LINE, EChartType::SCATTER, EChartType::HEATMAP })
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
        // 히트맵은 숫자가 아닌 값을 빈 칸으로 본다: 모든 칸이 비면 그림 칸 대신 "그릴 값이 없다" 다
        const bool bNoHeatmapValue = static_cast<EChartType>(type) == EChartType::HEATMAP && !std::isfinite(value);
        QCOMPARE(view.GetPlotRect().isEmpty(), bNoHeatmapValue);
        QCOMPARE(view.GetMessageText().isEmpty(), !bNoHeatmapValue);
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

    // 히트맵(검출기별 COCO 정확도, design/MoreViews.md §4.3): 행 = 검출기(위 → 아래), 열 = AP … APL(왼쪽 → 오른쪽), 칸 사이 2px 바탕 틈.
    // 그림 칸은 칸 격자이고 색 범례는 그 위다. 칸 색은 그 y 열 색 눈금 자리의 색, 칸 글은 채움과 대비가 큰 글자 색(4.5:1 이상)이다.
    // 칸 가운데에서 그 점을 찾고, 설명은 "행 이름 · 열 이름: 값 단위" 다
    void heatmapDrawsCells_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }
    void heatmapDrawsCells()
    {
        QFETCH(bool, dark);
        const VisualMarkdownTheme theme = dark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
        const ChartSpec& spec = m_vecSpecs[kHeatmapChart];
        QCOMPARE(spec.eType, EChartType::HEATMAP);
        QCOMPARE(spec.xAxis.vecCategories.size(), kHeatmapRows);
        QCOMPARE(spec.vecSeries.size(), kHeatmapColumns);
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetTheme(theme);
        view.SetSpec(spec);
        const QImage image = ViewTestSupport::Render(view);
        QVERIFY(ViewTestSupport::HasEnoughInk(image, theme.background));
        QCOMPARE(ViewTestSupport::Render(view), image);
        QCOMPARE(view.GetMessageText(), QString());

        // 색 범례는 칸 격자 위에 있고, 눈금의 낮은 끝 · 높은 끝 색이 있다
        const QRectF rectPlot = view.GetPlotRect();
        const QRectF rectLegend = view.GetLegendRect();
        QVERIFY(!rectPlot.isEmpty());
        QVERIFY(!rectLegend.isEmpty());
        QVERIFY2(rectLegend.bottom() <= rectPlot.top(), qUtf8Printable(ViewTestSupport::DescribeRect(rectLegend) + ViewTestSupport::DescribeRect(rectPlot)));
        QVERIFY(ViewTestSupport::HasColorIn(image, rectLegend.toAlignedRect(), theme.GetRampColor(0)));
        QVERIFY(ViewTestSupport::HasColorIn(image, rectLegend.toAlignedRect(), theme.GetRampColor(1)));

        QRectF rectCells;
        for (int iColumn = 0; iColumn < kHeatmapColumns; ++iColumn)
        {
            const ChartSeries& series = spec.vecSeries[iColumn];
            const ChartAxis& axis = spec.vecYAxes[series.iAxis];
            for (int iRow = 0; iRow < kHeatmapRows; ++iRow)
            {
                const ChartPoint& point = series.vecPoints[iRow];
                const QRectF rectCell = view.GetMarkRect(iColumn, iRow);
                const QString strWhat = series.strName + QLatin1Char('/') + point.strLabel + QLatin1Char(' ') + ViewTestSupport::DescribeRect(rectCell);
                QVERIFY2(!rectCell.isEmpty(), qUtf8Printable(strWhat));
                QVERIFY2(rectPlot.contains(rectCell), qUtf8Printable(strWhat + ViewTestSupport::DescribeRect(rectPlot)));
                rectCells = rectCells.united(rectCell);
                QCOMPARE(view.FindPointAt(rectCell.center().toPoint()), qMakePair(iColumn, iRow));

                // 칸은 행 · 열 격자에 맞고 이웃 칸과 2px 틈으로 떨어진다
                if (iColumn > 0)
                {
                    const QRectF rectLeft = view.GetMarkRect(iColumn - 1, iRow);
                    QCOMPARE(rectCell.top(), rectLeft.top());
                    QCOMPARE(rectCell.bottom(), rectLeft.bottom());
                    QVERIFY2(std::abs(rectCell.left() - rectLeft.right() - kCellGap) < kPixelTolerance, qUtf8Printable(strWhat));
                }
                if (iRow > 0)
                {
                    const QRectF rectAbove = view.GetMarkRect(iColumn, iRow - 1);
                    QCOMPARE(rectCell.left(), rectAbove.left());
                    QCOMPARE(rectCell.right(), rectAbove.right());
                    QVERIFY2(std::abs(rectCell.top() - rectAbove.bottom() - kCellGap) < kPixelTolerance, qUtf8Printable(strWhat));
                }

                // 칸 색: 그 y 열 색 눈금에서 (값 − 가장 작은 값) / (가장 큰 값 − 가장 작은 값) 자리의 색
                const double dT = (*point.optY - axis.dMin) / (axis.dMax - axis.dMin);
                const QColor fill = theme.GetRampColor(dT);
                const QColor pixel = image.pixelColor(rectCell.topLeft().toPoint() + QPoint(1, 1));
                QVERIFY2(pixel == fill, qUtf8Printable(strWhat + QLatin1Char(' ') + pixel.name() + QStringLiteral(" 기대 ") + fill.name()));

                // 칸 글: 채움과 대비가 큰 쪽(fillTextDark · fillTextLight)으로 쓰고 4.5:1 이상이다
                const bool bLightText = ViewTestSupport::GetContrast(theme.fillTextLight, fill) > ViewTestSupport::GetContrast(theme.fillTextDark, fill);
                const QColor textColor = bLightText ? theme.fillTextLight : theme.fillTextDark;
                QVERIFY2(ViewTestSupport::GetContrast(textColor, fill) >= kMinFillTextContrast, qUtf8Printable(strWhat + fill.name()));
                QVERIFY2(ViewTestSupport::HasColorIn(image, rectCell.toAlignedRect(), textColor), qUtf8Printable(strWhat + QLatin1Char(' ') + textColor.name()));

                // 설명: 행 이름 · 열 이름: 값 단위
                const QString strTip = view.GetToolTipAt(rectCell.center().toPoint());
                QVERIFY2(strTip.startsWith(point.strLabel + QStringLiteral(" · ") + series.strName + QStringLiteral(": ")) && strTip.endsWith(QLatin1Char('%')),
                         qUtf8Printable(strTip));
            }
        }
        // 칸 격자 전체가 그림 칸이다
        for (const double dDifference : { rectCells.left() - rectPlot.left(), rectCells.top() - rectPlot.top(), rectCells.right() - rectPlot.right(),
                                          rectCells.bottom() - rectPlot.bottom() })
            QVERIFY2(std::abs(dDifference) < kPixelTolerance, qUtf8Printable(ViewTestSupport::DescribeRect(rectCells) + ViewTestSupport::DescribeRect(rectPlot)));

        // AP 열: 가장 큰 값(RetinaNet X-101)은 높은 끝 색, 가장 작은 값(YOLOv2)은 낮은 끝 색. 설명은 값과 단위
        const QRectF rectHighest = view.GetMarkRect(kApColumn, kHighestApRow);
        const QRectF rectLowest = view.GetMarkRect(kApColumn, kLowestApRow);
        QCOMPARE(image.pixelColor(rectHighest.topLeft().toPoint() + QPoint(1, 1)), theme.GetRampColor(1));
        QCOMPARE(image.pixelColor(rectLowest.topLeft().toPoint() + QPoint(1, 1)), theme.GetRampColor(0));
        QCOMPARE(view.GetToolTipAt(rectLowest.center().toPoint()), QString::fromUtf8(kLowestApTip));
        // 칸 밖(격자 아래)에서는 찾지 않는다
        QCOMPARE(view.FindPointAt(QPoint(rectPlot.center().x(), rectPlot.bottom() + 4)), qMakePair(-1, -1));
    }

    // 히트맵의 빈 칸: 값이 없는 점은 채우지 않고 격자 색 1px 테두리와 흐린 "—" 로 그린다(칸은 있어 찾을 수 있다).
    // 점이 없는 칸(나 · FPS)도 같이 그리고, 찾을 점은 없지만 설명은 "값 없음" 이다. 범위가 0 인 눈금(FPS 5–5)의 칸은 가운데 색이다
    void heatmapEmptyCells()
    {
        const VisualMarkdownTheme theme = VisualMarkdownTheme::MakeLight();
        const ChartSpec spec = MakeSyntheticHeatmap();
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth / 2, ViewTestSupport::kSnapshotHeight / 2);
        view.SetTheme(theme);
        view.SetSpec(spec);
        const QImage image = ViewTestSupport::Render(view);
        QVERIFY(ViewTestSupport::SaveSnapshot(image, QStringLiteral("extra-chart-heatmap-empty-light")));

        const QString strEmptyLabel = QString::fromUtf8(kEmptyCellLabel);
        const QRectF rectEmpty = view.GetMarkRect(0, 1);
        QVERIFY(!rectEmpty.isEmpty());
        QCOMPARE(view.FindPointAt(rectEmpty.center().toPoint()), qMakePair(0, 1));
        QCOMPARE(view.GetToolTipAt(rectEmpty.center().toPoint()), QStringLiteral("나 · AP: ") + strEmptyLabel);
        QCOMPARE(image.pixelColor(rectEmpty.topLeft().toPoint() + QPoint(3, 3)), theme.background);   // 채우지 않는다
        QVERIFY(ViewTestSupport::HasColorIn(image, rectEmpty.toAlignedRect(), theme.grid));          // 1px 테두리
        QVERIFY(ViewTestSupport::CountInk(image.copy(rectEmpty.toAlignedRect().adjusted(2, 2, -2, -2)), theme.background) > 0);   // 가운데 "—"

        // 점이 없는 칸: 나 행 × FPS 열
        const QRectF rectFps = view.GetMarkRect(1, 0);
        const QPoint ptMissing(static_cast<int>(rectFps.center().x()), static_cast<int>(rectEmpty.center().y()));
        QCOMPARE(view.FindPointAt(ptMissing), qMakePair(-1, -1));
        QCOMPARE(view.GetToolTipAt(ptMissing), QStringLiteral("나 · FPS: ") + strEmptyLabel);
        QCOMPARE(image.pixelColor(QPoint(static_cast<int>(rectFps.left()) + 3, static_cast<int>(rectEmpty.top()) + 3)), theme.background);

        // 색: AP 10 은 낮은 끝, 30 은 높은 끝, FPS(범위 0)는 가운데
        QCOMPARE(image.pixelColor(view.GetMarkRect(0, 0).topLeft().toPoint() + QPoint(1, 1)), theme.GetRampColor(0));
        QCOMPARE(image.pixelColor(view.GetMarkRect(0, 2).topLeft().toPoint() + QPoint(1, 1)), theme.GetRampColor(1));
        QCOMPARE(image.pixelColor(rectFps.topLeft().toPoint() + QPoint(1, 1)), theme.GetRampColor(kRampMiddle));
        // 색 범례: 눈금이 둘이라 양 끝은 낮음 · 높음이다(색은 낮은 끝 · 높은 끝)
        const QRect rectLegend = view.GetLegendRect().toAlignedRect();
        QVERIFY(ViewTestSupport::HasColorIn(image, rectLegend, theme.GetRampColor(0)));
        QVERIFY(ViewTestSupport::HasColorIn(image, rectLegend, theme.GetRampColor(1)));
    }

    // 히트맵의 색(크기 눈금 · 채움 위 글자)은 다른 차트에 쓰이지 않는다: 그 값을 바꿔도 막대 · 단계 막대 · 선 · 산점도 · 그리지 않는 표는 같은 그림이다
    void otherChartsIgnoreHeatmapColors()
    {
        VisualMarkdownTheme changed = VisualMarkdownTheme::MakeLight();
        changed.vecSequentialRamp = { QColor(Qt::red), QColor(Qt::green) };
        changed.fillTextDark = QColor(Qt::magenta);
        changed.fillTextLight = QColor(Qt::cyan);
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        for (const ChartSpec& spec : m_vecSpecs)
        {
            if (spec.eType == EChartType::HEATMAP)
                continue;
            view.SetSpec(spec);
            view.SetTheme(VisualMarkdownTheme::MakeLight());
            const QImage image = ViewTestSupport::Render(view);
            view.SetTheme(changed);
            QVERIFY2(ViewTestSupport::Render(view) == image, qUtf8Printable(spec.strTitle));
        }
    }

    // 색 범례: 색 눈금이 하나면 양 끝에 가장 작은 값 · 가장 큰 값(단위 붙임), 여럿이면 낮음 · 높음. 범례에 올리면 눈금마다 이름과 범위를 쓴다
    void heatmapLegendDescribesScales()
    {
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth, ViewTestSupport::kSnapshotHeight);
        view.SetSpec(m_vecSpecs[kHeatmapChart]);
        const QString strTip = view.GetToolTipAt(view.GetLegendRect().center().toPoint());
        QCOMPARE(strTip.split(QLatin1Char('\n')).size(), kHeatmapColumns);
        for (const QString& strScale : { QStringLiteral("AP: 21.6% – 40.8%"), QStringLiteral("APS: 5% – 24.1%"), QStringLiteral("APL: 35.5% – 52.1%") })
            QVERIFY2(strTip.contains(strScale), qUtf8Printable(strTip));

        // 색 눈금 하나(합성 히트맵에서 FPS 열을 뺀다)
        ChartSpec single = MakeSyntheticHeatmap();
        single.vecSeries.removeLast();
        single.vecYAxes.removeLast();
        const VisualMarkdownTheme theme = VisualMarkdownTheme::MakeLight();
        view.resize(ViewTestSupport::kSnapshotWidth / 2, ViewTestSupport::kSnapshotHeight / 2);
        view.SetTheme(theme);
        view.SetSpec(single);
        const QImage image = ViewTestSupport::Render(view);
        QVERIFY(ViewTestSupport::SaveSnapshot(image, QStringLiteral("extra-chart-heatmap-single-light")));
        const QRectF rectLegend = view.GetLegendRect();
        QVERIFY(!rectLegend.isEmpty() && rectLegend.bottom() <= view.GetPlotRect().top());
        QCOMPARE(view.GetToolTipAt(rectLegend.center().toPoint()), QStringLiteral("AP: 10% – 30%"));
        QVERIFY(ViewTestSupport::HasColorIn(image, rectLegend.toAlignedRect(), theme.GetRampColor(0)));
        QVERIFY(ViewTestSupport::HasColorIn(image, rectLegend.toAlignedRect(), theme.GetRampColor(1)));
    }

    // 색 범례는 위젯보다 넓어지지 않는다(design/MoreViews.md §4.3): 색 눈금이 하나일 때 앞에 쓰는 긴 y 열 이름은 '…' 로 줄이고,
    // 띠와 양 끝 값(10 ms · 30 ms)은 줄이지 않고 위젯 안에 그린다. 범례 위 설명에는 이름을 다 쓴다
    void heatmapLegendElidesLongScaleName()
    {
        const QString strName = QString::fromUtf8(kLongScaleName);
        const VisualMarkdownTheme theme = VisualMarkdownTheme::MakeLight();
        KnowledgeChartView view;
        view.resize(kHeatmapLabelSize);
        view.SetTheme(theme);
        view.SetSpec(MakeSingleScaleHeatmap(strName, QString::fromUtf8(kLongScaleUnit), kLongScaleLow, kLongScaleHigh));
        const QImage image = ViewTestSupport::Render(view);
        QVERIFY(ViewTestSupport::SaveSnapshot(image, QStringLiteral("extra-chart-heatmap-long-scale-name-light")));

        const QRectF rectLegend = view.GetLegendRect();
        QVERIFY2(!rectLegend.isEmpty() && QRectF(view.rect()).contains(rectLegend), qUtf8Printable(ViewTestSupport::DescribeRect(rectLegend)));
        QVERIFY(rectLegend.bottom() <= view.GetPlotRect().top());

        // 띠: 낮은 끝 · 높은 끝 색이 범례 안에 있다(높은 끝이 낮은 끝보다 오른쪽)
        const QRect rectLegendPixels = rectLegend.toAlignedRect();
        const QPair<int, int> pairLow = FindColorColumns(image, rectLegendPixels, theme.GetRampColor(0));
        const QPair<int, int> pairHigh = FindColorColumns(image, rectLegendPixels, theme.GetRampColor(1));
        QVERIFY2(pairLow.first >= 0 && pairHigh.second >= 0 && pairLow.first < pairHigh.second,
                 qUtf8Printable(QStringLiteral("띠 %1 … %2").arg(pairLow.first).arg(pairHigh.second)));
        // 양 끝 값(글자 색): 띠 왼쪽에 가장 작은 값, 띠 오른쪽에 가장 큰 값이 범례 안에 있다
        const QRect rectBeforeBar(rectLegendPixels.left(), rectLegendPixels.top(), pairLow.first - rectLegendPixels.left(), rectLegendPixels.height());
        const QRect rectAfterBar(pairHigh.second + 1, rectLegendPixels.top(), rectLegendPixels.right() - pairHigh.second, rectLegendPixels.height());
        QVERIFY(ViewTestSupport::HasColorIn(image, rectBeforeBar, theme.text));
        QVERIFY(ViewTestSupport::HasColorIn(image, rectAfterBar, theme.text));

        // 범례 위 설명: 줄이지 않은 이름과 범위
        const QString strTip = view.GetToolTipAt(rectLegend.center().toPoint());
        QVERIFY2(strTip.contains(strName) && strTip.contains(QStringLiteral("10 ms – 30 ms")), qUtf8Printable(strTip));
    }

    // 모든 칸이 비면 다른 차트처럼 "그릴 값이 없다" 와 경고를 쓰고, 그림 칸 · 찾을 칸이 없다. 값이 하나라도 있으면 다시 칸을 그린다
    void heatmapWithoutValuesShowsMessage()
    {
        ChartSpec spec = MakeSyntheticHeatmap();
        for (ChartSeries& series : spec.vecSeries)
            for (ChartPoint& point : series.vecPoints)
                point.optY.reset();
        spec.vecWarnings << QStringLiteral("합성 경고");
        KnowledgeChartView view;
        view.resize(ViewTestSupport::kSnapshotWidth / 2, ViewTestSupport::kSnapshotHeight / 2);
        view.SetSpec(spec);
        QVERIFY(ViewTestSupport::HasEnoughInk(ViewTestSupport::Render(view), VisualMarkdownTheme::MakeLight().background));
        const QString strMessage = view.GetMessageText();
        QVERIFY2(strMessage.startsWith(QString::fromUtf8(kNoValueTitle)) && strMessage.contains(QStringLiteral("합성 경고")), qUtf8Printable(strMessage));
        QVERIFY(view.GetPlotRect().isEmpty());
        QVERIFY(view.GetLegendRect().isEmpty());
        QVERIFY(view.GetMarkRect(0, 0).isEmpty());
        QCOMPARE(view.FindPointAt(view.rect().center()), qMakePair(-1, -1));
        QCOMPARE(view.GetToolTipAt(view.rect().center()), QString());

        view.SetSpec(MakeSyntheticHeatmap());
        QCOMPARE(view.GetMessageText(), QString());
        QVERIFY(!view.GetPlotRect().isEmpty());
    }

    // 넘치는 행 · 열 이름은 '…' 로 줄이고, 그 이름 위에 올리면 설명에 다 쓴다(칸 설명에도 다 쓴다). 이름은 위젯 밖으로 나가지 않는다
    void heatmapLongNamesShowInToolTips()
    {
        ChartSpec spec = MakeSyntheticHeatmap();
        const QString strLongRow = QStringLiteral("아주 긴 행 이름이라 행 이름 칸의 너비를 한참 넘쳐서 줄여 써야 하는 검출기 이름");
        const QString strLongColumn = QStringLiteral("아주 긴 열 이름 · 칸 너비를 넘쳐 두 줄로 접어도 다 들어가지 않는 계열 이름 · 끝");
        spec.xAxis.vecCategories[2] = strLongRow;
        spec.vecSeries[0].vecPoints[2].strLabel = strLongRow;
        spec.vecSeries[0].strName = strLongColumn;
        const VisualMarkdownTheme theme = VisualMarkdownTheme::MakeLight();
        KnowledgeChartView view;
        view.resize(kHeatmapLabelSize);
        view.SetTheme(theme);
        view.SetSpec(spec);
        const QImage image = ViewTestSupport::Render(view);
        QVERIFY(ViewTestSupport::SaveSnapshot(image, QStringLiteral("extra-chart-heatmap-long-names-light")));
        const QRectF rectPlot = view.GetPlotRect();
        QVERIFY(QRectF(view.rect()).contains(rectPlot));
        const QRectF rectCell = view.GetMarkRect(0, 2);
        QVERIFY(!rectCell.isEmpty());
        QCOMPARE(view.GetToolTipAt(QPoint(static_cast<int>(rectPlot.left()) - 2, static_cast<int>(rectCell.center().y()))), strLongRow);
        QCOMPARE(view.GetToolTipAt(QPoint(static_cast<int>(rectCell.center().x()), static_cast<int>(rectPlot.top()) - 2)), strLongColumn);
        QVERIFY2(view.GetToolTipAt(rectCell.center().toPoint()).startsWith(strLongRow + QStringLiteral(" · ") + strLongColumn + QStringLiteral(": ")),
                 qUtf8Printable(view.GetToolTipAt(rectCell.center().toPoint())));
        for (int iY = 0; iY < image.height(); ++iY)
        {
            QCOMPARE(image.pixelColor(1, iY), theme.background);
            QCOMPARE(image.pixelColor(image.width() - 2, iY), theme.background);
        }
    }

    // 작은 위젯에서도 죽지 않고 칸 격자는 위젯 안이며 칸 가운데에서 그 점을 찾는다(칸이 작아 값 글은 줄어든다. 눈으로 보려고 PNG 를 남긴다)
    void heatmapSmallWidget()
    {
        const ChartSpec& spec = m_vecSpecs[kHeatmapChart];
        KnowledgeChartView view;
        view.resize(kSmallSize);
        view.SetSpec(spec);
        const QImage image = ViewTestSupport::Render(view);
        QVERIFY(ViewTestSupport::HasEnoughInk(image, VisualMarkdownTheme::MakeLight().background));
        QVERIFY(ViewTestSupport::SaveSnapshot(image, QStringLiteral("extra-chart-heatmap-small-light")));
        QVERIFY(QRectF(view.rect()).contains(view.GetPlotRect()));
        for (int iSeries = 0; iSeries < spec.vecSeries.size(); ++iSeries)
        {
            for (int iPoint = 0; iPoint < spec.vecSeries[iSeries].vecPoints.size(); ++iPoint)
            {
                const QRectF rectCell = view.GetMarkRect(iSeries, iPoint);
                QVERIFY2(!rectCell.isEmpty() && QRectF(view.rect()).contains(rectCell), qUtf8Printable(ViewTestSupport::DescribeRect(rectCell)));
                QCOMPARE(view.FindPointAt(rectCell.center().toPoint()), qMakePair(iSeries, iPoint));
            }
        }
        view.resize(40, 30);
        ViewTestSupport::Render(view);
        view.FindPointAt(QPoint(20, 15));
        view.GetToolTipAt(QPoint(20, 15));
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
        case EChartType::HEATMAP: return QStringLiteral("heatmap");
        }
        return QStringLiteral("unknown");
    }

private:
    KnowledgeIndex m_index;            // showcase wiki 를 읽은 색인
    QVector<ChartSpec> m_vecSpecs;     // kTableTitles 순의 시작 차트 설정
};

std::unique_ptr<QObject> CreateKnowledgeChartViewTest() { return std::make_unique<KnowledgeChartViewTest>(); }

#include "KnowledgeChartViewTest.moc"
