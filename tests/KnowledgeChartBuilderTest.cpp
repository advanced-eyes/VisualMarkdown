#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartBuilderCommon.h"
#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"
#include "VisualMarkdown/Algorithm/ChartBuilder/KnowledgeChartBuilder.h"
#include "VisualMarkdown/KnowledgeIndex.h"
#include "VisualMarkdown/KnowledgeTypes.h"

#include <QMap>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

namespace
{
// ================================================================================================
// 기대값: examples/showcase/README.md §3 의 표 12개와 design/Views.md §6 의 차트 규칙이다.
// 예시를 고치면 README · ShowcaseWikiTest 와 함께 여기를 고친다.
// ================================================================================================

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
constexpr int kTableCount = 12;

// 표마다의 시작 차트: 종류, 계열(이름 · 단위 · 축), y 축(단위 · 이름), x 축(이름 · 단위 · 범주), 밑글
struct ExpectedChart
{
    const char* pTitle;
    EChartType eType;
    QStringList vecSeries;
    QStringList vecSeriesUnits;
    QStringList vecSeriesAxes;
    QStringList vecAxisUnits;
    QStringList vecAxisTitles;
    const char* pXTitle;
    const char* pXUnit;
    QStringList vecCategories;   // 범주 x 의 이름표(행 순서). 산점도 · NONE 은 비었다
    const char* pCaption;
};
const QVector<ExpectedChart> kCharts {
    // 계열로 색을 나누는 산점도(계열 2개), 넓은 x 범위
    { kSpeedTable, EChartType::SCATTER, { "실시간", "실시간 아님" }, { "%", "%" }, { "0", "0" }, { "%" }, { "mAP" }, "FPS", "", {},
      "VOC 2007 test · 표 1" },
    // 단계 막대
    { kStepsTable, EChartType::STEP_BAR, { "mAP" }, { "%" }, { "0" }, { "%" }, { "mAP" }, "단계", "",
      { "YOLO (기준)", "+ 배치 정규화", "+ 고해상도 분류기", "+ 합성곱 · anchor 상자", "+ 새 네트워크", "+ 차원 사전 · 위치 예측", "+ passthrough",
        "+ 다중 크기 학습", "+ 고해상도 검출기" },
      "VOC 2007 test · 표 2" },
    // 숫자 열인데 ordered 라 순서: 선 둘, 단위가 달라(% · 없음) 축 둘
    { kInputSizeTable, EChartType::LINE, { "mAP", "FPS" }, { "%", "" }, { "0", "1" }, { "%", "" }, { "mAP", "FPS" }, "입력 크기", "px",
      { "288", "352", "416", "480", "544" }, "VOC 2007 test · GTX Titan X · 표 3" },
    { kVocDetectorsTable, EChartType::BAR, { "mAP" }, { "%" }, { "0" }, { "%" }, { "mAP" }, "검출기", "",
      { "Fast R-CNN", "Faster R-CNN VGG-16", "Faster R-CNN ResNet", "YOLO", "SSD300", "SSD500", "YOLOv2 544" }, "VOC 2007 test · 표 3" },
    // 단위가 같은 y 둘: 축 하나
    { kCocoTable, EChartType::LINE, { "AP", "AP50" }, { "%", "%" }, { "0", "0" }, { "%" }, { "AP · AP50" }, "입력 크기", "px",
      { "320", "416", "608" }, "COCO test-dev · 그림 1 · 그림 3" },
    // ordered 가 없는 숫자 x 는 수치: 산점도
    { kTimeTable, EChartType::SCATTER, { "AP50" }, { "%" }, { "0" }, { "%" }, { "AP50" }, "시간", "ms", {}, "COCO test-dev · Titan X · 그림 3" },
    { kTwoStageVocTable, EChartType::BAR, { "mAP" }, { "%" }, { "0" }, { "%" }, { "mAP" }, "방법", "", { "R-CNN", "Fast R-CNN", "Faster R-CNN" },
      "VOC 2007 test · VGG16 · VOC 2007 trainval 학습 · Fast R-CNN 표 4 · Faster R-CNN 표 3" },
    // 나란한 막대, 단위가 달라(h · s) 축 둘, 빈 값
    { kTrainTestTimeTable, EChartType::BAR, { "학습 시간", "시험 시간" }, { "h", "s" }, { "0", "1" }, { "h", "s" }, { "학습 시간", "시험 시간" }, "방법", "",
      { "R-CNN", "Fast R-CNN", "Faster R-CNN" }, "VGG16 · 시험 시간은 이미지 한 장 · Fast R-CNN 표 4 · Faster R-CNN 초록" },
    // 글자 순서 x 의 선 둘, 빈 값에서 끊긴다
    { kMonthlyTable, EChartType::LINE, { "p50", "p95" }, { "ms", "ms" }, { "0", "0" }, { "ms" }, { "p50 · p95" }, "월", "",
      { "5월", "6월", "7월", "8월", "9월" }, "운영 환경 · 하루 평균 · 3.1절" },
    // 칸 속 | 와 condition 속 "
    { kPaymentMethodTable, EChartType::BAR, { "비중" }, { "%" }, { "0" }, { "%" }, { "비중" }, "결제 수단", "",
      { "신용카드", "체크카드", "간편결제 (카드 | 계좌)", "계좌이체" }, "9월 거래 건수 · \"취소\" 건 빼고 · 2절" },
    // 천 단위 쉼표
    { kLoadTable, EChartType::SCATTER, { "응답 시간" }, { "ms" }, { "0" }, { "ms" }, { "응답 시간" }, "동시 사용자", "명", {},
      "부하 시험 · 결제 모듈 2판 · 부록 A" },
    // y 가 수치가 아니다: 그리지 않는다
    { kPriorityTable, EChartType::NONE, {}, {}, {}, {}, {}, "", "", {}, "4절" },
};

// 산점도의 점(x 값 · y 값 · x 칸 글)
struct ExpectedPoint
{
    double dX;
    double dY;
    const char* pLabel;
};
const QVector<ExpectedPoint> kRealTimePoints { { 100, 16.0, "100" }, { 30, 26.1, "30" }, { 155, 52.7, "155" }, { 45, 63.4, "45" } };
const QVector<ExpectedPoint> kNotRealTimePoints {
    { 15, 30.4, "15" }, { 6, 53.5, "6" }, { 0.5, 70.0, "0.5" }, { 7, 73.2, "7" }, { 18, 62.1, "18" }, { 21, 66.4, "21" },
};
constexpr double kSpeedXMin = 0.5;   // 넓은 x 범위(0.5–155)
constexpr double kSpeedXMax = 155;
const QStringList kSpeedXTexts { "100", "30", "155", "45", "15", "6", "0.5", "7", "18", "21" };   // 막대로 바꾸면 범주(행 순서)

// 단계 막대: 행마다의 값. 첫 막대는 0 부터, 그다음은 앞 값부터
const QVector<double> kStepValues { 63.4, 65.8, 69.5, 69.2, 69.6, 74.4, 75.4, 76.8, 78.6 };
constexpr const char* kDecreasingStep = "+ 합성곱 · anchor 상자";   // 줄어드는 단계 하나(69.5 → 69.2)
constexpr double kDecreaseFrom = 69.5;
constexpr double kDecreaseTo = 69.2;

// 빈 값(표 · 계열 · x 칸 글). showcase 의 빈 칸은 이 둘뿐이다
struct ExpectedGap
{
    const char* pTable;
    const char* pSeries;
    const char* pLabel;
};
const QVector<ExpectedGap> kGaps { { kTrainTestTimeTable, "학습 시간", "Faster R-CNN" }, { kMonthlyTable, "p95", "7월" } };
const QVector<double> kTrainTimes { 84, 9.5 };                 // 학습 시간(빈 값 앞)
const QVector<double> kTestTimes { 47.0, 0.32, 0.2 };          // 시험 시간(Faster R-CNN 까지 있다)
const QVector<double> kMonthlyP50 { 180, 170, 150, 120, 110 };

// 천 단위 쉼표(1,000)
const QVector<ExpectedPoint> kLoadPoints { { 100, 95, "100" }, { 500, 120, "500" }, { 1000, 160, "1,000" }, { 2000, 240, "2,000" }, { 5000, 610, "5,000" } };

constexpr const char* kNonQuantityY = "우선순위";

// 눈금 하나를 못박는다: 검출기별 VOC 2007 정확도(0 을 넣어 0–78.6) → 20 간격
const QVector<double> kVocDetectorTicks { 0, 20, 40, 60, 80 };

// 선 · 산점도는 0 을 넣지 않고 값 범위에 5% 여유를 둔다. 값이 모두 0 이상이면 여유는 0 아래로 가지 않는다
constexpr int kXAxis = -1;
struct ExpectedTicks
{
    const char* pTable;
    int iAxis;                 // y 축 자리. kXAxis 면 x 축
    QVector<double> vecTicks;
};
const QVector<ExpectedTicks> kMarginTicks {
    { kInputSizeTable, 0, { 68, 70, 72, 74, 76, 78, 80 } },        // mAP 69.0–78.6
    { kInputSizeTable, 1, { 30, 40, 50, 60, 70, 80, 90, 100 } },   // FPS 40–91
    { kTimeTable, kXAxis, { 20, 25, 30, 35, 40, 45, 50, 55 } },    // 시간 22–51
    { kTimeTable, 0, { 50, 52, 54, 56, 58, 60 } },                 // AP50 51.5–57.9
    { kSpeedTable, kXAxis, { 0, 50, 100, 150, 200 } },             // FPS 0.5–155: 여유가 0 에서 멈춘다(-50 이 아니다)
    { kLoadTable, kXAxis, { 0, 1000, 2000, 3000, 4000, 5000, 6000 } },   // 동시 사용자 100–5,000: 0 에서 멈춘다(-1000 이 아니다)
};

// 음수뿐인 산점도(합성 표): 여유는 0 위로 가지 않는다
const QVector<QStringList> kNegativeRows { { "-100", "-10" }, { "-50", "-5" }, { "-2", "-0.1" } };
const QVector<double> kNegativeXTicks { -120, -100, -80, -60, -40, -20, 0 };   // x -100–-2
const QVector<double> kNegativeYTicks { -12, -10, -8, -6, -4, -2, 0 };         // y -10–-0.1

const QVector<EChartType> kDrawnTypes { EChartType::BAR, EChartType::STEP_BAR, EChartType::LINE, EChartType::SCATTER };

// 규칙 4
constexpr int kMaxYAxes = 2;               // 왼쪽 · 오른쪽

// 규칙 7
constexpr int kMinTicks = 5;
constexpr int kMaxTicks = 8;
const QVector<double> kNiceMantissas { 1, 2, 5, 10 };   // 잰 간격을 반올림한 가수(10 은 다음 자리의 1)
constexpr double kDecimalBase = 10;        // 눈금 간격은 가수 × 10ⁿ
constexpr double kMarginRatio = 0.05;      // 선 · 산점도: 값 범위 양쪽 여유
constexpr double kZeroRangePadding = 1;    // 범위가 0 이면 ±1
constexpr double kCategoryHalfWidth = 0.5; // 범주 축은 -0.5 … 범주 수 - 0.5
constexpr double kRelativeTolerance = 1e-9;
constexpr double kStepTolerance = 1e-6;    // 눈금 간격이 1 · 2 · 5 × 10ⁿ 인지 볼 때의 여유

constexpr int kRoundTripDigits = 17;       // FormatNumber 의 자리 수

// ================================================================================================

QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

QString GetTypeName(EChartType type)
{
    switch (type)
    {
    case EChartType::NONE: return QStringLiteral("NONE");
    case EChartType::BAR: return QStringLiteral("BAR");
    case EChartType::STEP_BAR: return QStringLiteral("STEP_BAR");
    case EChartType::LINE: return QStringLiteral("LINE");
    case EChartType::SCATTER: return QStringLiteral("SCATTER");
    }
    return QStringLiteral("?");
}

QString FormatNumber(double value) { return QString::number(value, 'g', kRoundTripDigits); }

QString FormatValue(const std::optional<double>& value) { return value ? FormatNumber(*value) : QStringLiteral("(없음)"); }

QStringList FormatNumbers(const QVector<double>& values)
{
    QStringList vecTexts;
    for (const double dValue : values)
        vecTexts << FormatNumber(dValue);
    return vecTexts;
}

double GetTolerance(double scale) { return kRelativeTolerance * std::max(1.0, std::fabs(scale)); }

KnowledgeColumn MakeColumn(const QString& name, const QString& unit = QString())
{
    KnowledgeColumn column;
    column.strName = name;
    column.strUnit = unit;
    return column;
}

// 합성 표
KnowledgeTable MakeTable(const QVector<KnowledgeColumn>& columns, const QVector<QStringList>& rows, const QString& x, const QStringList& y,
                         const QString& series = QString(), const QStringList& ordered = QStringList(), bool cumulative = false)
{
    KnowledgeTable table;
    table.strTitle = QStringLiteral("합성 표");
    table.strX = x;
    table.vecY = y;
    table.strSeries = series;
    table.vecOrdered = ordered;
    table.bCumulative = cumulative;
    table.vecColumns = columns;
    table.vecRows = rows;
    return table;
}

ChartSpec BuildChart(const KnowledgeTable& table, std::optional<EChartType> forced = std::nullopt)
{
    KnowledgeChartBuilder builder;
    if (forced)
        builder.SetStrategy(*forced);
    ChartBuildInput in;
    in.pTable = &table;
    ChartSpec spec;
    builder.Execute(in, spec);
    return spec;
}

QStringList ListSeriesNames(const ChartSpec& spec)
{
    QStringList vecNames;
    for (const ChartSeries& series : spec.vecSeries)
        vecNames << series.strName;
    return vecNames;
}

QStringList ListSeriesUnits(const ChartSpec& spec)
{
    QStringList vecUnits;
    for (const ChartSeries& series : spec.vecSeries)
        vecUnits << series.strUnit;
    return vecUnits;
}

QStringList ListSeriesAxes(const ChartSpec& spec)
{
    QStringList vecAxes;
    for (const ChartSeries& series : spec.vecSeries)
        vecAxes << QString::number(series.iAxis);
    return vecAxes;
}

QStringList ListAxisUnits(const ChartSpec& spec)
{
    QStringList vecUnits;
    for (const ChartAxis& axis : spec.vecYAxes)
        vecUnits << axis.strUnit;
    return vecUnits;
}

QStringList ListAxisTitles(const ChartSpec& spec)
{
    QStringList vecTitles;
    for (const ChartAxis& axis : spec.vecYAxes)
        vecTitles << axis.strTitle;
    return vecTitles;
}

const ChartSeries* FindSeries(const ChartSpec& spec, const QString& name)
{
    for (const ChartSeries& series : spec.vecSeries)
        if (series.strName == name)
            return &series;
    return nullptr;
}

// 점들의 x 칸 글 · dX · y 값 · 시작 값을 글로
QStringList ListLabels(const ChartSeries& series)
{
    QStringList vecLabels;
    for (const ChartPoint& point : series.vecPoints)
        vecLabels << point.strLabel;
    return vecLabels;
}

QStringList ListXs(const ChartSeries& series)
{
    QStringList vecXs;
    for (const ChartPoint& point : series.vecPoints)
        vecXs << FormatNumber(point.dX);
    return vecXs;
}

QStringList ListYs(const ChartSeries& series)
{
    QStringList vecYs;
    for (const ChartPoint& point : series.vecPoints)
        vecYs << FormatValue(point.optY);
    return vecYs;
}

QStringList ListBases(const ChartSeries& series)
{
    QStringList vecBases;
    for (const ChartPoint& point : series.vecPoints)
        vecBases << FormatNumber(point.dBase);
    return vecBases;
}

QStringList ListExpectedXs(const QVector<ExpectedPoint>& points)
{
    QStringList vecXs;
    for (const ExpectedPoint& point : points)
        vecXs << FormatNumber(point.dX);
    return vecXs;
}

QStringList ListExpectedYs(const QVector<ExpectedPoint>& points)
{
    QStringList vecYs;
    for (const ExpectedPoint& point : points)
        vecYs << FormatNumber(point.dY);
    return vecYs;
}

QStringList ListExpectedLabels(const QVector<ExpectedPoint>& points)
{
    QStringList vecLabels;
    for (const ExpectedPoint& point : points)
        vecLabels << DecodeUtf8(point.pLabel);
    return vecLabels;
}

// 수치 x 축에 놓인 값(산점도)
QVector<double> ListXValues(const ChartSpec& spec)
{
    QVector<double> vecValues;
    for (const ChartSeries& series : spec.vecSeries)
        for (const ChartPoint& point : series.vecPoints)
            vecValues << point.dX;
    return vecValues;
}

// y 축 하나에 그려지는 값(단계 막대는 시작 값도)
QVector<double> ListYValues(const ChartSpec& spec, int axis)
{
    QVector<double> vecValues;
    for (const ChartSeries& series : spec.vecSeries)
    {
        if (series.iAxis != axis)
            continue;
        for (const ChartPoint& point : series.vecPoints)
        {
            if (point.optY)
                vecValues << *point.optY;
            if (point.optY && spec.eType == EChartType::STEP_BAR)
                vecValues << point.dBase;
        }
    }
    return vecValues;
}

// 경고 가운데 word 를 담은 것이 있는지
bool HasWarningAbout(const ChartSpec& spec, const QString& word)
{
    for (const QString& strWarning : spec.vecWarnings)
        if (strWarning.contains(word))
            return true;
    return false;
}

QString DescribeWarnings(const ChartSpec& spec)
{
    return QStringLiteral("warnings: [") + spec.vecWarnings.join(QStringLiteral(" | ")) + QLatin1Char(']');
}

// 수치 축: 눈금 5–8개, 간격이 1 · 2 · 5 × 10ⁿ 로 고르다, 범위가 바깥 눈금이다, 값을 모두 덮는다, 모두 유한하다
QStringList CheckScale(const QString& what, double min, double max, const QVector<double>& ticks, const QVector<double>& values)
{
    QStringList vecProblems;
    const QString strTicks = QStringLiteral(" [") + FormatNumbers(ticks).join(QStringLiteral(", ")) + QLatin1Char(']');
    if (ticks.size() < kMinTicks || ticks.size() > kMaxTicks)
        return { what + QStringLiteral(": 눈금 수 %1").arg(ticks.size()) + strTicks };
    for (const double dTick : ticks)
        if (!std::isfinite(dTick))
            return { what + QStringLiteral(": 유한하지 않은 눈금") + strTicks };
    if (min != ticks.first() || max != ticks.last())
        vecProblems << what + QStringLiteral(": 범위 %1–%2 가 바깥 눈금이 아니다").arg(FormatNumber(min), FormatNumber(max)) + strTicks;
    // 잰 간격(눈금 차이)을 가장 가까운 1 · 2 · 5 × 10ⁿ 과 견주고, 고른 간격과 배수는 그 간격으로 본다
    const double dMeasuredStep = ticks[1] - ticks[0];
    if (!(dMeasuredStep > 0))
        return vecProblems << what + QStringLiteral(": 눈금이 오르지 않는다") + strTicks;
    const double dExponent = std::floor(std::log10(dMeasuredStep) + kStepTolerance);
    const double dMantissa = dMeasuredStep / std::pow(kDecimalBase, dExponent);
    const double dNiceMantissa = std::round(dMantissa);
    const bool bNiceStep = kNiceMantissas.contains(dNiceMantissa) && std::fabs(dMantissa - dNiceMantissa) < kStepTolerance;
    if (!bNiceStep)
        return vecProblems << what + QStringLiteral(": 간격 %1 이 1 · 2 · 5 × 10ⁿ 이 아니다").arg(FormatNumber(dMeasuredStep)) + strTicks;
    const double dStep = dNiceMantissa * std::pow(kDecimalBase, dExponent);
    for (int iTick = 1; iTick < ticks.size(); ++iTick)
        if (std::fabs(ticks[iTick] - ticks[iTick - 1] - dStep) > kStepTolerance * dStep)
            vecProblems << what + QStringLiteral(": 눈금 간격이 고르지 않다") + strTicks;
    const double dUnits = ticks.first() / dStep;
    if (std::fabs(dUnits - std::round(dUnits)) > kStepTolerance)
        vecProblems << what + QStringLiteral(": 눈금이 간격의 배수가 아니다") + strTicks;
    for (const double dValue : values)
        if (dValue < min - GetTolerance(dValue) || dValue > max + GetTolerance(dValue))
            vecProblems << what + QStringLiteral(": 값 %1 이 범위 밖이다").arg(FormatNumber(dValue)) + strTicks;
    return vecProblems;
}

QStringList CheckScale(const QString& what, const ChartAxis& axis, const QVector<double>& values)
{
    return CheckScale(what, axis.dMin, axis.dMax, axis.vecTicks, values);
}

// 선 · 산점도: 범위가 값 범위의 양쪽에 5% 여유(범위가 0 이면 ±1)를 둔다.
// 값이 모두 0 이상이면 아래 끝이, 모두 0 이하면 위 끝이 0 을 넘지 않는다(여유는 0 까지만 있으면 된다). 모두 0 이면 0 이상으로 본다
QStringList CheckMargin(const QString& what, double min, double max, const QVector<double>& values)
{
    if (values.isEmpty())
        return {};
    const double dLow = *std::min_element(values.cbegin(), values.cend());
    const double dHigh = *std::max_element(values.cbegin(), values.cend());
    const double dMargin = dHigh > dLow ? (dHigh - dLow) * kMarginRatio : kZeroRangePadding;
    double dNeedLow = dLow - dMargin;
    double dNeedHigh = dHigh + dMargin;
    const QString strRange = QStringLiteral(" (범위 %1–%2, 값 %3–%4)").arg(FormatNumber(min), FormatNumber(max), FormatNumber(dLow), FormatNumber(dHigh));
    QStringList vecProblems;
    if (dLow >= 0)
    {
        dNeedLow = std::max(dNeedLow, 0.0);
        if (min < 0)
            vecProblems << what + QStringLiteral(": 값이 모두 0 이상인데 범위가 0 아래로 간다") + strRange;
    }
    else if (dHigh <= 0)
    {
        dNeedHigh = std::min(dNeedHigh, 0.0);
        if (max > 0)
            vecProblems << what + QStringLiteral(": 값이 모두 0 이하인데 범위가 0 위로 간다") + strRange;
    }
    const double dTolerance = GetTolerance(std::max(std::fabs(dLow), std::fabs(dHigh)));
    if (min > dNeedLow + dTolerance || max < dNeedHigh - dTolerance)
        vecProblems << what + QStringLiteral(": 여유가 없다") + strRange;
    return vecProblems;
}

// 차트 설정이 앞뒤가 맞는지(종류를 바꿔도 지켜야 하는 것)
QStringList CheckSpec(const ChartSpec& spec)
{
    QStringList vecProblems;
    if (spec.eType == EChartType::NONE)
    {
        if (!spec.vecSeries.isEmpty())
            vecProblems << QStringLiteral("NONE 인데 계열이 있다");
        if (!spec.vecYAxes.isEmpty())
            vecProblems << QStringLiteral("NONE 인데 y 축이 있다");
        if (spec.vecWarnings.isEmpty())
            vecProblems << QStringLiteral("NONE 인데 까닭이 없다");
        return vecProblems;
    }
    const bool bCategorical = spec.eType != EChartType::SCATTER;
    const bool bBars = spec.eType == EChartType::BAR || spec.eType == EChartType::STEP_BAR;
    if (spec.vecYAxes.isEmpty() || spec.vecYAxes.size() > kMaxYAxes)
        vecProblems << QStringLiteral("y 축이 %1 개다").arg(spec.vecYAxes.size());

    // x 축: 범주 축은 범주만, 수치 축은 눈금만
    const ChartAxis& xAxis = spec.xAxis;
    if (bCategorical)
    {
        const int iCategories = xAxis.vecCategories.size();
        if (!xAxis.vecTicks.isEmpty())
            vecProblems << QStringLiteral("범주 x 축에 눈금이 있다");
        if (xAxis.dMin != -kCategoryHalfWidth || xAxis.dMax != std::max(iCategories, 1) - kCategoryHalfWidth)
            vecProblems << QStringLiteral("범주 x 축 범위 %1–%2").arg(FormatNumber(xAxis.dMin), FormatNumber(xAxis.dMax));
    }
    else
    {
        if (!xAxis.vecCategories.isEmpty())
            vecProblems << QStringLiteral("수치 x 축에 범주가 있다");
        vecProblems << CheckScale(QStringLiteral("x"), xAxis, ListXValues(spec));
        vecProblems << CheckMargin(QStringLiteral("x"), xAxis.dMin, xAxis.dMax, ListXValues(spec));
    }

    // 계열과 점
    for (const ChartSeries& series : spec.vecSeries)
    {
        if (series.iAxis < 0 || series.iAxis >= spec.vecYAxes.size())
        {
            vecProblems << series.strName + QStringLiteral(": 없는 축 %1").arg(series.iAxis);
            continue;
        }
        std::optional<double> optPrevious;
        for (const ChartPoint& point : series.vecPoints)
        {
            const QString strPoint = series.strName + QStringLiteral(" / ") + point.strLabel;
            if (!std::isfinite(point.dX) || (point.optY && !std::isfinite(*point.optY)) || !std::isfinite(point.dBase))
                vecProblems << strPoint + QStringLiteral(": 유한하지 않은 값");
            if (bCategorical)
            {
                const int iCategory = static_cast<int>(point.dX);
                if (point.dX != iCategory || iCategory < 0 || iCategory >= xAxis.vecCategories.size())
                    vecProblems << strPoint + QStringLiteral(": 범주 자리가 아니다 ") + FormatNumber(point.dX);
                else if (xAxis.vecCategories[iCategory] != point.strLabel)
                    vecProblems << strPoint + QStringLiteral(": 범주 이름표 ") + xAxis.vecCategories[iCategory] + QStringLiteral(" 와 다르다");
            }
            if (spec.eType == EChartType::BAR && point.dBase != 0)
                vecProblems << strPoint + QStringLiteral(": 막대가 0 에서 시작하지 않는다");
            if (spec.eType == EChartType::STEP_BAR && point.optY)
            {
                if (point.dBase != optPrevious.value_or(0))
                    vecProblems << strPoint + QStringLiteral(": 단계 막대가 앞 값 %1 이 아닌 %2 에서 시작한다")
                                                  .arg(FormatNumber(optPrevious.value_or(0)), FormatNumber(point.dBase));
                optPrevious = point.optY;
            }
        }
    }

    // y 축: 막대 · 단계 막대는 0 을 넣고, 선 · 산점도는 여유를 둔다
    for (int iAxis = 0; iAxis < spec.vecYAxes.size(); ++iAxis)
    {
        const ChartAxis& axis = spec.vecYAxes[iAxis];
        const QString strWhat = QStringLiteral("y%1").arg(iAxis);
        const QVector<double> vecValues = ListYValues(spec, iAxis);
        vecProblems << CheckScale(strWhat, axis, vecValues);
        if (bBars && (axis.dMin > 0 || axis.dMax < 0))
            vecProblems << strWhat + QStringLiteral(": 막대 축에 0 이 없다");
        if (!bBars)
            vecProblems << CheckMargin(strWhat, axis.dMin, axis.dMax, vecValues);
    }
    return vecProblems;
}

QString DescribeProblems(const QString& what, const QStringList& problems)
{
    return what + QStringLiteral(": ") + problems.join(QStringLiteral(" | "));
}
}

// 지식 안 차트 설정(KnowledgeChartBuilder, design/Views.md §6)을 showcase 표 12개(examples/showcase/README.md §3)와 작은 합성 표로 시험한다.
// 표마다의 시작 차트 · 계열 · 축 · 빈 값 · 밑글, 숫자 읽기, 눈금, 종류를 바꿔도 앞뒤가 맞는 설정이 나오는지를 본다.
class KnowledgeChartBuilderTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QVERIFY2(m_index.Load(QStringLiteral(VISUALMARKDOWN_SHOWCASE_DIR)), VISUALMARKDOWN_SHOWCASE_DIR);
        QVERIFY2(m_index.GetWarnings().isEmpty(), qUtf8Printable(m_index.GetWarnings().join(QLatin1Char('\n'))));
        for (const KnowledgeDocument& document : m_index.ListDocuments())
        {
            for (const KnowledgeTable& table : document.vecTables)
            {
                QVERIFY2(!m_mapTables.contains(table.strTitle), qUtf8Printable(table.strTitle));
                m_mapTables.insert(table.strTitle, table);
            }
        }
        QCOMPARE(m_mapTables.size(), kTableCount);
        QCOMPARE(kCharts.size(), kTableCount);
        for (const ExpectedChart& expected : kCharts)
            QVERIFY2(m_mapTables.contains(DecodeUtf8(expected.pTitle)), expected.pTitle);
    }

    // README §3: 표마다의 시작 차트 · 계열 · 축 · 범주 · 밑글
    void showcaseChartsMatchReadme_data()
    {
        QTest::addColumn<int>("chart");
        for (int iChart = 0; iChart < kCharts.size(); ++iChart)
            QTest::newRow(kCharts[iChart].pTitle) << iChart;
    }

    void showcaseChartsMatchReadme()
    {
        QFETCH(int, chart);
        const ExpectedChart& expected = kCharts[chart];
        const KnowledgeTable& table = GetTable(expected.pTitle);
        QCOMPARE(GetTypeName(KnowledgeChartBuilder::Recommend(table)), GetTypeName(expected.eType));

        const ChartSpec spec = BuildChart(table);
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(expected.eType));
        QCOMPARE(spec.strTitle, DecodeUtf8(expected.pTitle));
        QCOMPARE(spec.strCaption, DecodeUtf8(expected.pCaption));
        QCOMPARE(ListSeriesNames(spec), expected.vecSeries);
        QCOMPARE(ListSeriesUnits(spec), expected.vecSeriesUnits);
        QCOMPARE(ListSeriesAxes(spec), expected.vecSeriesAxes);
        QCOMPARE(ListAxisUnits(spec), expected.vecAxisUnits);
        QCOMPARE(ListAxisTitles(spec), expected.vecAxisTitles);
        QCOMPARE(spec.xAxis.strTitle, DecodeUtf8(expected.pXTitle));
        QCOMPARE(spec.xAxis.strUnit, DecodeUtf8(expected.pXUnit));
        QCOMPARE(spec.xAxis.vecCategories, expected.vecCategories);
        const QStringList vecProblems = CheckSpec(spec);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(DescribeProblems(spec.strTitle, vecProblems)));
        if (expected.eType == EChartType::NONE)
            return;

        // 그리는 표는 경고가 없고, 계열마다 점은 그 계열의 행 수다(series 가 없으면 모든 행)
        QVERIFY2(spec.vecWarnings.isEmpty(), qUtf8Printable(DescribeWarnings(spec)));
        const int iSeriesColumn = FindColumn(table, table.strSeries);
        for (const ChartSeries& series : spec.vecSeries)
        {
            int iRows = 0;
            for (const QStringList& vecRow : table.vecRows)
                if (iSeriesColumn < 0 || vecRow.value(iSeriesColumn) == series.strName)
                    ++iRows;
            QCOMPARE(series.vecPoints.size(), iRows);
        }
    }

    // 계열로 색을 나누는 산점도: 구분 값마다 계열 하나, 처음 나온 순서. x 범위가 넓다(0.5–155)
    void scatterSeriesFollowSeriesColumn()
    {
        const ChartSpec spec = BuildChart(GetTable(kSpeedTable));
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::SCATTER));
        QCOMPARE(spec.vecSeries.size(), 2);
        const ChartSeries& realTime = spec.vecSeries[0];
        const ChartSeries& notRealTime = spec.vecSeries[1];
        QCOMPARE(ListXs(realTime), ListExpectedXs(kRealTimePoints));
        QCOMPARE(ListYs(realTime), ListExpectedYs(kRealTimePoints));
        QCOMPARE(ListLabels(realTime), ListExpectedLabels(kRealTimePoints));
        QCOMPARE(ListXs(notRealTime), ListExpectedXs(kNotRealTimePoints));
        QCOMPARE(ListYs(notRealTime), ListExpectedYs(kNotRealTimePoints));
        QCOMPARE(ListLabels(notRealTime), ListExpectedLabels(kNotRealTimePoints));
        const QStringList vecMargin = CheckMargin(QStringLiteral("x"), spec.xAxis.dMin, spec.xAxis.dMax, { kSpeedXMin, kSpeedXMax });
        QVERIFY2(vecMargin.isEmpty(), qUtf8Printable(vecMargin.join(QStringLiteral(" | "))));
        QCOMPARE(spec.xAxis.dMin, 0.0);   // FPS 는 모두 0 이상이라 여유가 0 에서 멈춘다
        QVERIFY(spec.xAxis.vecCategories.isEmpty());
    }

    // 단계 막대: 첫 막대는 0 부터, 그다음은 앞 값에서 이 값까지. 줄어드는 단계는 69.5 → 69.2 하나다
    void stepBarsStartAtPreviousValue()
    {
        const ChartSpec spec = BuildChart(GetTable(kStepsTable));
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::STEP_BAR));
        QCOMPARE(spec.vecSeries.size(), 1);
        const ChartSeries& series = spec.vecSeries[0];
        QCOMPARE(ListYs(series), FormatNumbers(kStepValues));
        QVector<double> vecBases { 0 };
        vecBases << kStepValues.mid(0, kStepValues.size() - 1);
        QCOMPARE(ListBases(series), FormatNumbers(vecBases));
        QStringList vecDecreasing;
        for (const ChartPoint& point : series.vecPoints)
            if (point.optY && *point.optY < point.dBase)
                vecDecreasing << point.strLabel;
        QCOMPARE(vecDecreasing, QStringList { DecodeUtf8(kDecreasingStep) });
        const int iDecrease = ListLabels(series).indexOf(DecodeUtf8(kDecreasingStep));
        QVERIFY(iDecrease >= 0);
        const ChartPoint& decrease = series.vecPoints[iDecrease];
        QCOMPARE(decrease.dBase, kDecreaseFrom);
        QCOMPARE(FormatValue(decrease.optY), FormatNumber(kDecreaseTo));
        QCOMPARE(spec.vecYAxes.size(), 1);
        QVERIFY(spec.vecYAxes[0].dMin <= 0);

        // 빈 단계는 그리지 않고, 다음 막대는 값이 있는 앞 점의 값부터다
        const KnowledgeTable gap = MakeTable({ MakeColumn("단계"), MakeColumn("값") }, { { "a", "10" }, { "b", "" }, { "c", "15" }, { "d", "12" } },
                                             "단계", { "값" }, QString(), { "단계" }, true);
        const ChartSpec gapSpec = BuildChart(gap);
        QCOMPARE(GetTypeName(gapSpec.eType), GetTypeName(EChartType::STEP_BAR));
        QCOMPARE(gapSpec.vecSeries.size(), 1);
        QCOMPARE(ListYs(gapSpec.vecSeries[0]), (QStringList { "10", FormatValue(std::nullopt), "15", "12" }));
        QCOMPARE(ListBases(gapSpec.vecSeries[0]), (QStringList { "0", "10", "10", "15" }));
        const QStringList vecProblems = CheckSpec(gapSpec);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));
    }

    // 빈 칸은 값 없는 점이다(Faster R-CNN 의 학습 시간 · 7월 p95). 다른 점은 모두 값이 있다
    void emptyCellsHaveNoValue()
    {
        for (const ExpectedGap& gap : kGaps)
        {
            const ChartSpec spec = BuildChart(GetTable(gap.pTable));
            const ChartSeries* pSeries = FindSeries(spec, DecodeUtf8(gap.pSeries));
            QVERIFY2(pSeries != nullptr, gap.pSeries);
            const int iPoint = ListLabels(*pSeries).indexOf(DecodeUtf8(gap.pLabel));
            QVERIFY2(iPoint >= 0, gap.pLabel);
            QVERIFY2(!pSeries->vecPoints[iPoint].optY.has_value(), gap.pLabel);
        }
        int iEmptyPoints = 0;
        for (const ExpectedChart& expected : kCharts)
        {
            const ChartSpec spec = BuildChart(GetTable(expected.pTitle));
            for (const ChartSeries& series : spec.vecSeries)
                for (const ChartPoint& point : series.vecPoints)
                    if (!point.optY)
                        ++iEmptyPoints;
        }
        QCOMPARE(iEmptyPoints, kGaps.size());

        // 빈 값 둘레의 값
        const ChartSpec trainTest = BuildChart(GetTable(kTrainTestTimeTable));
        const ChartSeries* pTrain = FindSeries(trainTest, QStringLiteral("학습 시간"));
        const ChartSeries* pTest = FindSeries(trainTest, QStringLiteral("시험 시간"));
        QVERIFY(pTrain != nullptr && pTest != nullptr);
        QCOMPARE(ListYs(*pTrain), FormatNumbers(kTrainTimes) << FormatValue(std::nullopt));
        QCOMPARE(ListYs(*pTest), FormatNumbers(kTestTimes));
        const ChartSpec monthly = BuildChart(GetTable(kMonthlyTable));
        const ChartSeries* pP50 = FindSeries(monthly, QStringLiteral("p50"));
        QVERIFY(pP50 != nullptr);
        QCOMPARE(ListYs(*pP50), FormatNumbers(kMonthlyP50));
    }

    // 천 단위 쉼표: "1,000" 은 1000 이다. 이름표는 칸 글 그대로
    void thousandsSeparatorIsRead()
    {
        const ChartSpec spec = BuildChart(GetTable(kLoadTable));
        QCOMPARE(spec.vecSeries.size(), 1);
        QCOMPARE(ListXs(spec.vecSeries[0]), ListExpectedXs(kLoadPoints));
        QCOMPARE(ListYs(spec.vecSeries[0]), ListExpectedYs(kLoadPoints));
        QCOMPARE(ListLabels(spec.vecSeries[0]), ListExpectedLabels(kLoadPoints));
    }

    // y 가 수치가 아닌 표: 그리지 않고 까닭(그 열 이름)을 남긴다. 종류를 정해도 그리지 않는다
    void nonQuantityYIsNotDrawn()
    {
        const KnowledgeTable& table = GetTable(kPriorityTable);
        QCOMPARE(GetTypeName(KnowledgeChartBuilder::Recommend(table)), GetTypeName(EChartType::NONE));
        const ChartSpec spec = BuildChart(table);
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::NONE));
        QVERIFY(spec.vecSeries.isEmpty());
        QVERIFY(spec.vecYAxes.isEmpty());
        QVERIFY2(HasWarningAbout(spec, DecodeUtf8(kNonQuantityY)), qUtf8Printable(DescribeWarnings(spec)));
        QCOMPARE(spec.strTitle, DecodeUtf8(kPriorityTable));
        for (const EChartType eType : kDrawnTypes)
        {
            const ChartSpec forced = BuildChart(table, eType);
            QCOMPARE(GetTypeName(forced.eType), GetTypeName(EChartType::NONE));
            QVERIFY2(HasWarningAbout(forced, DecodeUtf8(kNonQuantityY)), qUtf8Printable(DescribeWarnings(forced)));
        }
    }

    // 규칙 7: 눈금은 1 · 2 · 5 × 10ⁿ 간격 5–8개로 값을 덮고, 막대는 0 을 넣고, 선 · 산점도는 여유를 둔다
    void ticksAreNiceAndCoverData()
    {
        for (const ExpectedChart& expected : kCharts)
        {
            const ChartSpec spec = BuildChart(GetTable(expected.pTitle));
            if (spec.eType == EChartType::NONE)
                continue;
            QVERIFY2(!spec.vecYAxes.isEmpty(), expected.pTitle);
            const QStringList vecProblems = CheckSpec(spec);
            QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(DescribeProblems(spec.strTitle, vecProblems)));
            for (int iAxis = 0; iAxis < spec.vecYAxes.size(); ++iAxis)
            {
                const ChartAxis& axis = spec.vecYAxes[iAxis];
                if (spec.eType == EChartType::BAR || spec.eType == EChartType::STEP_BAR)
                    QVERIFY2(axis.dMin <= 0 && axis.dMax >= 0, expected.pTitle);
                for (const double dValue : ListYValues(spec, iAxis))
                    QVERIFY2(dValue >= axis.dMin && dValue <= axis.dMax, qUtf8Printable(spec.strTitle + QLatin1Char(' ') + FormatNumber(dValue)));
            }
        }
        const ChartSpec vocDetectors = BuildChart(GetTable(kVocDetectorsTable));
        QCOMPARE(vocDetectors.vecYAxes.size(), 1);
        QCOMPARE(FormatNumbers(vocDetectors.vecYAxes[0].vecTicks), FormatNumbers(kVocDetectorTicks));
        QCOMPARE(vocDetectors.vecYAxes[0].dMin, kVocDetectorTicks.first());
        QCOMPARE(vocDetectors.vecYAxes[0].dMax, kVocDetectorTicks.last());
        for (const ExpectedTicks& expected : kMarginTicks)
        {
            const ChartSpec spec = BuildChart(GetTable(expected.pTable));
            QVERIFY2(expected.iAxis < spec.vecYAxes.size(), expected.pTable);
            const ChartAxis& axis = expected.iAxis == kXAxis ? spec.xAxis : spec.vecYAxes[expected.iAxis];
            QCOMPARE(FormatNumbers(axis.vecTicks), FormatNumbers(expected.vecTicks));
        }
    }

    // 종류를 정하면(보기 앱에서 사용자가 고르면) 그 종류로 그린다. 계열 · 축 · 밑글은 그대로이고 설정은 앞뒤가 맞는다
    void forcedTypesStayConsistent()
    {
        for (const ExpectedChart& expected : kCharts)
        {
            const KnowledgeTable& table = GetTable(expected.pTitle);
            for (const EChartType eType : kDrawnTypes)
            {
                const QString strCase = DecodeUtf8(expected.pTitle) + QStringLiteral(" as ") + GetTypeName(eType);
                const ChartSpec spec = BuildChart(table, eType);
                const EChartType eExpectedType = expected.eType == EChartType::NONE ? EChartType::NONE : eType;
                QVERIFY2(spec.eType == eExpectedType, qUtf8Printable(strCase + QStringLiteral(" → ") + GetTypeName(spec.eType)));
                QCOMPARE(spec.strTitle, DecodeUtf8(expected.pTitle));
                QCOMPARE(spec.strCaption, DecodeUtf8(expected.pCaption));
                QCOMPARE(ListSeriesNames(spec), expected.vecSeries);
                QCOMPARE(ListSeriesAxes(spec), expected.vecSeriesAxes);
                QCOMPARE(ListAxisUnits(spec), expected.vecAxisUnits);
                const QStringList vecProblems = CheckSpec(spec);
                QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(DescribeProblems(strCase, vecProblems)));
            }
        }
    }

    // 산점도 표를 막대로: x 칸 글이 범주가 되고(series 가 있어 같은 글은 한 번), 계열은 그대로, y 축은 0 을 넣는다
    void forcedBarOnScatterTable()
    {
        KnowledgeChartBuilder builder;
        builder.SetStrategy(EChartType::BAR);
        QVERIFY(builder.GetStrategy().has_value());
        QCOMPARE(GetTypeName(*builder.GetStrategy()), GetTypeName(EChartType::BAR));
        const KnowledgeTable& table = GetTable(kSpeedTable);
        ChartSpec spec;
        QVERIFY(builder.Execute({ &table }, spec));
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::BAR));
        QCOMPARE(spec.xAxis.vecCategories, kSpeedXTexts);
        QVERIFY(spec.xAxis.vecTicks.isEmpty());
        QCOMPARE(ListSeriesNames(spec), (QStringList { "실시간", "실시간 아님" }));
        QCOMPARE(ListXs(spec.vecSeries[0]), (QStringList { "0", "1", "2", "3" }));
        QCOMPARE(ListXs(spec.vecSeries[1]), (QStringList { "4", "5", "6", "7", "8", "9" }));
        QCOMPARE(ListYs(spec.vecSeries[0]), ListExpectedYs(kRealTimePoints));
        QCOMPARE(spec.vecYAxes.size(), 1);
        QVERIFY(spec.vecYAxes[0].dMin <= 0);
        QVERIFY(spec.vecWarnings.isEmpty());
    }

    // NONE 으로 정하면 그리지 않고 까닭을 남긴다. 정한 것을 지우면 다시 추천 종류로 그린다
    void forcedNoneAndReset()
    {
        const KnowledgeTable& table = GetTable(kVocDetectorsTable);
        KnowledgeChartBuilder builder;
        QVERIFY(!builder.GetStrategy().has_value());
        builder.SetStrategy(EChartType::NONE);
        QCOMPARE(GetTypeName(builder.GetStrategy().value_or(EChartType::BAR)), GetTypeName(EChartType::NONE));
        ChartSpec spec;
        QVERIFY(builder.Execute({ &table }, spec));
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::NONE));
        QCOMPARE(spec.strTitle, DecodeUtf8(kVocDetectorsTable));
        QCOMPARE(spec.strCaption, QStringLiteral("VOC 2007 test · 표 3"));
        QVERIFY(!spec.vecWarnings.isEmpty());
        const QStringList vecProblems = CheckSpec(spec);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));

        builder.ResetStrategy();
        QVERIFY(!builder.GetStrategy().has_value());
        QVERIFY(builder.Execute({ &table }, spec));
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::BAR));
        QVERIFY(spec.vecWarnings.isEmpty());

        // 한 빌더로 여러 표를 그리면 표마다 추천 종류를 쓴다
        const KnowledgeTable& steps = GetTable(kStepsTable);
        QVERIFY(builder.Execute({ &steps }, spec));
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::STEP_BAR));
    }

    // 표가 없으면 false 이고 out 은 빈 설정이다(정한 종류와 상관없이)
    void executeWithoutTable()
    {
        QVector<std::optional<EChartType>> vecCases { std::nullopt, EChartType::NONE };
        for (const EChartType eType : kDrawnTypes)
            vecCases << eType;
        for (const std::optional<EChartType>& optType : vecCases)
        {
            KnowledgeChartBuilder builder;
            if (optType)
                builder.SetStrategy(*optType);
            ChartSpec spec;
            spec.strTitle = QStringLiteral("앞 차트");
            spec.vecWarnings << QStringLiteral("앞 경고");
            QVERIFY(!builder.Execute({ nullptr }, spec));
            QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::NONE));
            QVERIFY(spec.strTitle.isEmpty());
            QVERIFY(spec.vecWarnings.isEmpty());
            QVERIFY(spec.vecSeries.isEmpty());
        }
    }

    // 빈 표: 아무것도 없는 표, 행이 없는 표는 그리지 않는다(행이 없는 y 열은 수치가 아니다)
    void emptyTablesAreNotDrawn()
    {
        const KnowledgeTable blank;
        QCOMPARE(GetTypeName(KnowledgeChartBuilder::Recommend(blank)), GetTypeName(EChartType::NONE));
        const ChartSpec blankSpec = BuildChart(blank);
        QCOMPARE(GetTypeName(blankSpec.eType), GetTypeName(EChartType::NONE));
        QVERIFY(!blankSpec.vecWarnings.isEmpty());
        QVERIFY(blankSpec.vecSeries.isEmpty());

        const KnowledgeTable noRows = MakeTable({ MakeColumn("단계"), MakeColumn("mAP", "%") }, {}, "단계", { "mAP" }, QString(), { "단계" }, true);
        QCOMPARE(GetTypeName(KnowledgeChartBuilder::Recommend(noRows)), GetTypeName(EChartType::NONE));
        QVector<std::optional<EChartType>> vecCases { std::nullopt };
        for (const EChartType eType : kDrawnTypes)
            vecCases << eType;
        for (const std::optional<EChartType>& optType : vecCases)
        {
            const ChartSpec spec = BuildChart(noRows, optType);
            QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::NONE));
            QVERIFY2(HasWarningAbout(spec, QStringLiteral("mAP")), qUtf8Printable(DescribeWarnings(spec)));
            QVERIFY(spec.vecSeries.isEmpty());
            QCOMPARE(spec.strTitle, QStringLiteral("합성 표"));
        }
    }

    // 칸이 모두 빈 y 열은 수치가 아니라 빼고 경고를 단다. 그 열뿐이면 그리지 않는다
    void allEmptyYColumnIsLeftOut()
    {
        const QVector<KnowledgeColumn> vecColumns { MakeColumn("방법"), MakeColumn("학습 시간", "h"), MakeColumn("시험 시간", "s") };
        const QVector<QStringList> vecRows { { "A", "", "1.5" }, { "B", "", "2" } };
        const KnowledgeTable both = MakeTable(vecColumns, vecRows, "방법", { "학습 시간", "시험 시간" });
        const ChartSpec spec = BuildChart(both);
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::BAR));
        QCOMPARE(ListSeriesNames(spec), QStringList { "시험 시간" });
        QCOMPARE(ListAxisUnits(spec), QStringList { "s" });
        QCOMPARE(ListSeriesAxes(spec), QStringList { "0" });
        QCOMPARE(spec.vecWarnings.size(), 1);
        QVERIFY2(HasWarningAbout(spec, QStringLiteral("학습 시간")), qUtf8Printable(DescribeWarnings(spec)));
        const QStringList vecProblems = CheckSpec(spec);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));

        const KnowledgeTable onlyEmpty = MakeTable(vecColumns, vecRows, "방법", { "학습 시간" });
        QCOMPARE(GetTypeName(KnowledgeChartBuilder::Recommend(onlyEmpty)), GetTypeName(EChartType::NONE));
        const ChartSpec none = BuildChart(onlyEmpty);
        QCOMPARE(GetTypeName(none.eType), GetTypeName(EChartType::NONE));
        QVERIFY2(HasWarningAbout(none, QStringLiteral("학습 시간")), qUtf8Printable(DescribeWarnings(none)));
    }

    // 규칙 6: 숫자는 IsQuantityText 와 같은 꼴로 읽는다(부호 · 천 단위 쉼표 · 소수 · 지수)
    void signedAndExponentNumbers()
    {
        struct ParseCase
        {
            const char* pCell;
            std::optional<double> optValue;
        };
        const QVector<ParseCase> vecCases {
            { "63.4", 63.4 }, { "-2", -2.0 }, { "+1.5E+2", 150.0 }, { "1e-3", 0.001 }, { "-1,234.5", -1234.5 }, { "1,000", 1000.0 },
            { " 63.4 ", 63.4 }, { "12,345,678", 12345678.0 }, { "0", 0.0 },
            { "", std::nullopt }, { "63.4±0.2", std::nullopt }, { "12%", std::nullopt }, { ".5", std::nullopt }, { "1 000", std::nullopt },
            { "-", std::nullopt }, { "—", std::nullopt }, { "N/A", std::nullopt }, { "1,00", std::nullopt }, { "0x10", std::nullopt },
            { "inf", std::nullopt }, { "1e999", std::nullopt },
        };
        for (const ParseCase& parseCase : vecCases)
            QVERIFY2(FormatValue(ChartBuilderCommon::ParseQuantity(DecodeUtf8(parseCase.pCell))) == FormatValue(parseCase.optValue), parseCase.pCell);

        // 막대: 음수 · 지수, 0 을 넣은 범위가 값을 덮는다
        const KnowledgeTable bars = MakeTable({ MakeColumn("항목"), MakeColumn("값") },
                                              { { "a", "-2" }, { "b", "1e-3" }, { "c", "+1.5E+2" }, { "d", "-1,234.5" }, { "e", "2.5e1" } }, "항목", { "값" });
        const ChartSpec barSpec = BuildChart(bars);
        QCOMPARE(GetTypeName(barSpec.eType), GetTypeName(EChartType::BAR));
        QCOMPARE(barSpec.vecSeries.size(), 1);
        QCOMPARE(barSpec.vecYAxes.size(), 1);
        QCOMPARE(ListYs(barSpec.vecSeries[0]), FormatNumbers({ -2, 0.001, 150, -1234.5, 25 }));
        QVERIFY(barSpec.vecYAxes[0].dMin <= -1234.5);
        QVERIFY(barSpec.vecYAxes[0].dMax >= 150);
        QStringList vecProblems = CheckSpec(barSpec);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));

        // 산점도: x 도 같은 꼴로 읽는다
        const KnowledgeTable scatter = MakeTable({ MakeColumn("x"), MakeColumn("y") }, { { "-1e2", "1" }, { "0", "2" }, { "+1.5e2", "3" } }, "x", { "y" });
        const ChartSpec scatterSpec = BuildChart(scatter);
        QCOMPARE(GetTypeName(scatterSpec.eType), GetTypeName(EChartType::SCATTER));
        QCOMPARE(scatterSpec.vecSeries.size(), 1);
        QCOMPARE(ListXs(scatterSpec.vecSeries[0]), FormatNumbers({ -100, 0, 150 }));
        vecProblems = CheckSpec(scatterSpec);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));

        // 숫자 꼴이지만 double 로 담을 수 없는 칸은 값 없음으로 두고 경고를 단다
        const KnowledgeTable overflow = MakeTable({ MakeColumn("항목"), MakeColumn("값") }, { { "a", "1" }, { "b", "1e999" } }, "항목", { "값" });
        const ChartSpec overflowSpec = BuildChart(overflow);
        QCOMPARE(GetTypeName(overflowSpec.eType), GetTypeName(EChartType::BAR));
        QCOMPARE(overflowSpec.vecSeries.size(), 1);
        QCOMPARE(ListYs(overflowSpec.vecSeries[0]), (QStringList { "1", FormatValue(std::nullopt) }));
        QVERIFY2(HasWarningAbout(overflowSpec, QStringLiteral("1e999")), qUtf8Printable(DescribeWarnings(overflowSpec)));
        vecProblems = CheckSpec(overflowSpec);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));
    }

    // 규칙 4: y 단위가 셋 이상이면 셋째부터 오른쪽 축에 두고 경고를 단다. 빈 단위도 단위 하나다
    void thirdUnitGoesToRightAxis()
    {
        const KnowledgeTable table = MakeTable(
            { MakeColumn("방법"), MakeColumn("학습", "h"), MakeColumn("시험", "s"), MakeColumn("지연", "ms"), MakeColumn("준비", "h") },
            { { "A", "1", "2", "300", "4" }, { "B", "5", "6", "700", "8" } }, "방법", { "학습", "시험", "지연", "준비" });
        const ChartSpec spec = BuildChart(table);
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::BAR));
        QCOMPARE(ListSeriesNames(spec), (QStringList { "학습", "시험", "지연", "준비" }));
        QCOMPARE(ListSeriesUnits(spec), (QStringList { "h", "s", "ms", "h" }));
        QCOMPARE(ListSeriesAxes(spec), (QStringList { "0", "1", "1", "0" }));
        QCOMPARE(ListAxisUnits(spec), (QStringList { "h", "s" }));
        QCOMPARE(ListAxisTitles(spec), (QStringList { "학습 · 준비", "시험" }));
        QCOMPARE(spec.vecWarnings.size(), 1);
        QVERIFY2(HasWarningAbout(spec, QStringLiteral("지연")) && HasWarningAbout(spec, QStringLiteral("ms")), qUtf8Printable(DescribeWarnings(spec)));
        QVERIFY(spec.vecYAxes[1].dMax >= 700);
        const QStringList vecProblems = CheckSpec(spec);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));

        const KnowledgeTable emptyUnits = MakeTable({ MakeColumn("방법"), MakeColumn("a"), MakeColumn("b"), MakeColumn("c", "%") },
                                                    { { "A", "1", "2", "3" } }, "방법", { "a", "b", "c" });
        const ChartSpec emptyUnitSpec = BuildChart(emptyUnits);
        QCOMPARE(ListAxisUnits(emptyUnitSpec), (QStringList { "", "%" }));
        QCOMPARE(ListAxisTitles(emptyUnitSpec), (QStringList { "a · b", "c" }));
        QCOMPARE(ListSeriesAxes(emptyUnitSpec), (QStringList { "0", "0", "1" }));
        QVERIFY2(emptyUnitSpec.vecWarnings.isEmpty(), qUtf8Printable(DescribeWarnings(emptyUnitSpec)));
    }

    // 표에 없는 x · y · series 이름이면 그리지 않고 그 이름을 경고에 남긴다
    void unknownColumnsAreNotDrawn()
    {
        const QVector<KnowledgeColumn> vecColumns { MakeColumn("방법"), MakeColumn("mAP", "%"), MakeColumn("구분") };
        const QVector<QStringList> vecRows { { "A", "1", "가" }, { "B", "2", "나" } };
        struct UnknownCase
        {
            KnowledgeTable table;
            const char* pName;
        };
        const QVector<UnknownCase> vecCases {
            { MakeTable(vecColumns, vecRows, "없는 x", { "mAP" }), "없는 x" },
            { MakeTable(vecColumns, vecRows, "방법", { "mAP", "없는 y" }), "없는 y" },
            { MakeTable(vecColumns, vecRows, "방법", { "mAP" }, "없는 계열"), "없는 계열" },
            { MakeTable(vecColumns, vecRows, "", { "mAP" }), "x" },
            { MakeTable(vecColumns, vecRows, "방법", {}), "y" },
        };
        for (const UnknownCase& unknownCase : vecCases)
        {
            QCOMPARE(GetTypeName(KnowledgeChartBuilder::Recommend(unknownCase.table)), GetTypeName(EChartType::NONE));
            QVector<std::optional<EChartType>> vecTypes { std::nullopt };
            for (const EChartType eType : kDrawnTypes)
                vecTypes << eType;
            for (const std::optional<EChartType>& optType : vecTypes)
            {
                const ChartSpec spec = BuildChart(unknownCase.table, optType);
                QVERIFY2(spec.eType == EChartType::NONE, unknownCase.pName);
                QVERIFY2(HasWarningAbout(spec, DecodeUtf8(unknownCase.pName)), qUtf8Printable(DescribeWarnings(spec)));
                QVERIFY(spec.vecSeries.isEmpty());
                QVERIFY(spec.vecYAxes.isEmpty());
            }
        }
    }

    // 수치 x 축: x 가 비었거나 숫자가 아닌 행은 빼고 경고를 단다(행마다 한 번)
    void numericAxisSkipsBadRows()
    {
        const KnowledgeTable table = MakeTable({ MakeColumn("크기", "px"), MakeColumn("값"), MakeColumn("값2") },
                                               { { "10", "1", "4" }, { "", "2", "5" }, { "30", "3", "6" } }, "크기", { "값", "값2" });
        const ChartSpec spec = BuildChart(table);
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::SCATTER));
        QCOMPARE(ListSeriesNames(spec), (QStringList { "값", "값2" }));
        QCOMPARE(ListXs(spec.vecSeries[0]), (QStringList { "10", "30" }));
        QCOMPARE(ListYs(spec.vecSeries[1]), (QStringList { "4", "6" }));
        QCOMPARE(spec.vecWarnings.size(), 1);
        QVERIFY2(HasWarningAbout(spec, QStringLiteral("2행")), qUtf8Printable(DescribeWarnings(spec)));
        QStringList vecProblems = CheckSpec(spec);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));

        // 범주 x 를 산점도로: 행마다 경고, 점은 없고 설정은 앞뒤가 맞는다
        const KnowledgeTable& vocDetectors = GetTable(kVocDetectorsTable);
        const ChartSpec forced = BuildChart(vocDetectors, EChartType::SCATTER);
        QCOMPARE(GetTypeName(forced.eType), GetTypeName(EChartType::SCATTER));
        QCOMPARE(forced.vecWarnings.size(), vocDetectors.vecRows.size());
        QVERIFY2(HasWarningAbout(forced, QStringLiteral("Fast R-CNN")), qUtf8Printable(DescribeWarnings(forced)));
        QCOMPARE(ListSeriesNames(forced), QStringList { "mAP" });
        QVERIFY(forced.vecSeries[0].vecPoints.isEmpty());
        vecProblems = CheckSpec(forced);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));
    }

    // 규칙 3: y 가 여럿이고 series 열이 있으면 y × 값마다 계열("<y> · <값>"). 범주는 같은 글을 한 번(처음 나온 순서).
    // series 칸이 빈 행은 어느 계열에도 넣지 않고 경고를 단다
    void seriesColumnWithSeveralY()
    {
        const QVector<KnowledgeColumn> vecColumns { MakeColumn("검출기"), MakeColumn("입력 크기", "px"), MakeColumn("mAP", "%"), MakeColumn("FPS") };
        const QVector<QStringList> vecRows {
            { "SSD", "300", "74.3", "46" }, { "SSD", "500", "76.8", "19" }, { "YOLOv2", "300", "69.0", "91" }, { "YOLOv2", "544", "78.6", "40" },
            { "", "416", "76.8", "67" },
        };
        const KnowledgeTable table = MakeTable(vecColumns, vecRows, "입력 크기", { "mAP", "FPS" }, "검출기", { "입력 크기" });
        const ChartSpec spec = BuildChart(table);
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::LINE));
        QCOMPARE(ListSeriesNames(spec), (QStringList { "mAP · SSD", "mAP · YOLOv2", "FPS · SSD", "FPS · YOLOv2" }));
        QCOMPARE(ListSeriesUnits(spec), (QStringList { "%", "%", "", "" }));
        QCOMPARE(ListSeriesAxes(spec), (QStringList { "0", "0", "1", "1" }));
        QCOMPARE(spec.xAxis.vecCategories, (QStringList { "300", "500", "544" }));
        QCOMPARE(ListXs(spec.vecSeries[0]), (QStringList { "0", "1" }));
        QCOMPARE(ListYs(spec.vecSeries[0]), FormatNumbers({ 74.3, 76.8 }));
        QCOMPARE(ListXs(spec.vecSeries[1]), (QStringList { "0", "2" }));
        QCOMPARE(ListYs(spec.vecSeries[1]), FormatNumbers({ 69.0, 78.6 }));
        QCOMPARE(ListYs(spec.vecSeries[3]), FormatNumbers({ 91, 40 }));
        QCOMPARE(spec.vecWarnings.size(), 1);
        QVERIFY2(HasWarningAbout(spec, QStringLiteral("5행")), qUtf8Printable(DescribeWarnings(spec)));
        const QStringList vecProblems = CheckSpec(spec);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));

        // y 가 하나면 계열 이름은 값 그대로
        const KnowledgeTable oneY = MakeTable(vecColumns, vecRows, "입력 크기", { "mAP" }, "검출기", { "입력 크기" });
        QCOMPARE(ListSeriesNames(BuildChart(oneY)), (QStringList { "SSD", "YOLOv2" }));
        // 수치 아닌 y 를 빼고 하나가 남아도 값 그대로
        const KnowledgeTable droppedY = MakeTable(vecColumns, vecRows, "입력 크기", { "mAP", "검출기" }, "검출기", { "입력 크기" });
        const ChartSpec droppedSpec = BuildChart(droppedY);
        QCOMPARE(ListSeriesNames(droppedSpec), (QStringList { "SSD", "YOLOv2" }));
        QVERIFY2(HasWarningAbout(droppedSpec, QStringLiteral("검출기")), qUtf8Printable(DescribeWarnings(droppedSpec)));
    }

    // series 열이 없으면 범주는 행마다 하나다(같은 글 · 빈 글도 그대로, 행 순서)
    void categoriesFollowRowsWithoutSeries()
    {
        const KnowledgeTable table = MakeTable({ MakeColumn("이름"), MakeColumn("값") }, { { "A", "1" }, { "A", "2" }, { "", "3" } }, "이름", { "값" });
        const ChartSpec spec = BuildChart(table);
        QCOMPARE(GetTypeName(spec.eType), GetTypeName(EChartType::BAR));
        QCOMPARE(spec.xAxis.vecCategories, (QStringList { "A", "A", "" }));
        QCOMPARE(spec.vecSeries.size(), 1);
        QCOMPARE(ListXs(spec.vecSeries[0]), (QStringList { "0", "1", "2" }));
        QCOMPARE(spec.xAxis.dMin, -kCategoryHalfWidth);
        QCOMPARE(spec.xAxis.dMax, 3 - kCategoryHalfWidth);
        QVERIFY(spec.vecWarnings.isEmpty());

        // 순서 x 는 선, 순서 x + cumulative 는 단계 막대
        KnowledgeTable ordered = table;
        ordered.vecOrdered = QStringList { "이름" };
        QCOMPARE(GetTypeName(KnowledgeChartBuilder::Recommend(ordered)), GetTypeName(EChartType::LINE));
        ordered.bCumulative = true;
        QCOMPARE(GetTypeName(KnowledgeChartBuilder::Recommend(ordered)), GetTypeName(EChartType::STEP_BAR));
        // 범주 x 는 cumulative 여도 막대
        KnowledgeTable nominal = table;
        nominal.bCumulative = true;
        QCOMPARE(GetTypeName(KnowledgeChartBuilder::Recommend(nominal)), GetTypeName(EChartType::BAR));
    }

    // 선 · 산점도의 여유는 0 을 넘지 않는다: 값이 모두 0 이하면 위 끝이 0 이다. 0 을 건너는 값은 양쪽에 여유를 둔다
    void marginStopsAtZero()
    {
        const KnowledgeTable negative = MakeTable({ MakeColumn("x"), MakeColumn("y") }, kNegativeRows, "x", { "y" });
        const ChartSpec scatter = BuildChart(negative);
        QCOMPARE(GetTypeName(scatter.eType), GetTypeName(EChartType::SCATTER));
        QCOMPARE(FormatNumbers(scatter.xAxis.vecTicks), FormatNumbers(kNegativeXTicks));
        QCOMPARE(scatter.vecYAxes.size(), 1);
        QCOMPARE(FormatNumbers(scatter.vecYAxes[0].vecTicks), FormatNumbers(kNegativeYTicks));
        QCOMPARE(scatter.xAxis.dMax, 0.0);
        QCOMPARE(scatter.vecYAxes[0].dMax, 0.0);
        QStringList vecProblems = CheckSpec(scatter);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));

        // 같은 표를 선으로: y 축도 위 끝이 0
        const ChartSpec line = BuildChart(negative, EChartType::LINE);
        QCOMPARE(GetTypeName(line.eType), GetTypeName(EChartType::LINE));
        QCOMPARE(line.vecYAxes.size(), 1);
        QCOMPARE(FormatNumbers(line.vecYAxes[0].vecTicks), FormatNumbers(kNegativeYTicks));
        vecProblems = CheckSpec(line);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));

        // 0 을 건너는 x 는 양쪽에 여유(-3–7 → -4–8)
        const KnowledgeTable mixed = MakeTable({ MakeColumn("x"), MakeColumn("y") }, { { "-3", "1" }, { "7", "2" } }, "x", { "y" });
        const ChartSpec mixedSpec = BuildChart(mixed);
        QCOMPARE(FormatNumbers(mixedSpec.xAxis.vecTicks), FormatNumbers({ -4, -2, 0, 2, 4, 6, 8 }));
        vecProblems = CheckSpec(mixedSpec);
        QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));
    }

    // 눈금 고르기: 여러 범위에서 5–8개 · 1 · 2 · 5 × 10ⁿ · 값을 덮음. 몇 경우는 값을 못박는다
    void niceTicksCoverRanges()
    {
        struct TickCase
        {
            QVector<double> vecValues;
            bool bIncludeZero;
            QVector<double> vecTicks;
        };
        const QVector<TickCase> vecAnchors {
            { { 0.1, 14.9 }, true, { 0, 5, 10, 15, 20 } },         // 2 간격이면 9개, 5 간격이면 4개라 하나를 더한다
            { { -14.9, -0.1 }, true, { -20, -15, -10, -5, 0 } },   // 음수뿐이면 아래로 더한다
            { { 5, 5 }, false, { 4, 4.5, 5, 5.5, 6 } },            // 범위가 0 이면 ±1
            { {}, false, { -1, -0.5, 0, 0.5, 1 } },                // 값이 없으면 0 의 ±1
            { { 16.0, 73.2 }, false, { 10, 20, 30, 40, 50, 60, 70, 80 } },
            { { 63.4, 78.6 }, true, { 0, 20, 40, 60, 80 } },
            { { 0.5, 155 }, false, { 0, 50, 100, 150, 200 } },                 // 모두 0 이상: 아래 여유가 0 에서 멈춘다
            { { -100, -2 }, false, { -120, -100, -80, -60, -40, -20, 0 } },    // 모두 0 이하: 위 여유가 0 에서 멈춘다
            { { -3, 7 }, false, { -4, -2, 0, 2, 4, 6, 8 } },                   // 0 을 건너는 값은 양쪽 여유
            { { 0.5, 0.5 }, false, { 0, 0.5, 1, 1.5, 2 } },                    // 범위가 0 이어도(±1) 0 에서 멈춘다
            { { -0.5, -0.5 }, false, { -2, -1.5, -1, -0.5, 0 } },
            { { 0, 0 }, false, { 0, 0.2, 0.4, 0.6, 0.8, 1 } },                 // 모두 0 이면 0 이상으로 본다
            { { 0, 0 }, true, { 0, 0.2, 0.4, 0.6, 0.8, 1 } },                  // 막대도 같다(±1 이 0 아래로 가지 않는다)
        };
        for (const TickCase& tickCase : vecAnchors)
        {
            const ChartBuilderCommon::ChartScale scale = ChartBuilderCommon::ComputeScale(tickCase.vecValues, tickCase.bIncludeZero);
            QCOMPARE(FormatNumbers(scale.vecTicks), FormatNumbers(tickCase.vecTicks));
            QCOMPARE(scale.dMin, tickCase.vecTicks.first());
            QCOMPARE(scale.dMax, tickCase.vecTicks.last());
        }

        const QVector<double> vecLows { -1234.5, -10, -7, -0.3, 0, 0.1, 1, 13.14, 99.5 };
        const QVector<double> vecSpans { 0.001, 0.37, 1, 2.5, 9.6, 14.8, 57.2, 154.5, 1000, 4900, 1e6 };
        for (const double dLow : vecLows)
        {
            for (const double dSpan : vecSpans)
            {
                for (const bool bIncludeZero : { false, true })
                {
                    const QVector<double> vecValues { dLow, dLow + dSpan };
                    const ChartBuilderCommon::ChartScale scale = ChartBuilderCommon::ComputeScale(vecValues, bIncludeZero);
                    const QString strCase = QStringLiteral("%1 + %2%3").arg(FormatNumber(dLow), FormatNumber(dSpan), bIncludeZero ? " (0 포함)" : "");
                    QStringList vecProblems = CheckScale(strCase, scale.dMin, scale.dMax, scale.vecTicks, vecValues);
                    if (bIncludeZero && (scale.dMin > 0 || scale.dMax < 0))
                        vecProblems << strCase + QStringLiteral(": 0 이 없다");
                    if (!bIncludeZero)
                        vecProblems << CheckMargin(strCase, scale.dMin, scale.dMax, vecValues);
                    QVERIFY2(vecProblems.isEmpty(), qUtf8Printable(vecProblems.join(QStringLiteral(" | "))));
                }
            }
        }
    }

private:
    const KnowledgeTable& GetTable(const char* title) const
    {
        const auto it = m_mapTables.constFind(DecodeUtf8(title));
        Q_ASSERT(it != m_mapTables.cend());
        return it.value();
    }

    static int FindColumn(const KnowledgeTable& table, const QString& name)
    {
        for (int iColumn = 0; iColumn < table.vecColumns.size(); ++iColumn)
            if (!name.isEmpty() && table.vecColumns[iColumn].strName == name)
                return iColumn;
        return -1;
    }

private:
    KnowledgeIndex m_index;                       // showcase wiki 를 읽은 색인
    QMap<QString, KnowledgeTable> m_mapTables;    // 표 제목 → 표(showcase 의 제목은 겹치지 않는다)
};

std::unique_ptr<QObject> CreateKnowledgeChartBuilderTest() { return std::make_unique<KnowledgeChartBuilderTest>(); }

#include "KnowledgeChartBuilderTest.moc"
