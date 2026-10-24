#pragma once

#include "VisualMarkdown/Interface/IStrategy.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <cmath>
#include <optional>

struct KnowledgeTable;

// 지식 안 차트 종류(design/Views.md §6). NONE 은 그리지 않는다(보기 앱은 차트 대신 표를 보인다).
// 값은 끝에만 더한다(있는 값의 수를 바꾸지 않는다). HEATMAP: 행 = x 범주, 열 = 계열, 칸 색 = 값(design/MoreViews.md §4).
// DONUT: 조각 = 첫 계열의 점(행 순서), 각 = 값 / 전체 × 360°(design/MoreViews.md §5)
enum class EChartType { NONE, BAR, STEP_BAR, LINE, SCATTER, HEATMAP, DONUT };

// 점 하나. 범주 x(막대 · 단계 막대 · 선 · 히트맵 · 도넛)는 dX 가 범주 자리(0, 1, …)이고, 수치 x(산점도)는 x 값이다. 히트맵은 범주 자리가 행이다.
// 도넛은 첫 계열의 점이 조각이다(값이 없거나 0 보다 작은 점은 조각이 되지 않고 남는다)
struct ChartPoint
{
    double dX = 0;
    std::optional<double> optY;   // 빈 칸이면 비었다. 선은 여기서 끊고, 막대 · 점은 그리지 않는다
    double dBase = 0;             // 막대가 시작하는 값. 막대는 0, 단계 막대는 앞 막대의 값(첫 막대는 0). 선 · 점은 쓰지 않는다
    QString strLabel;             // x 칸 글
};

struct ChartSeries
{
    QString strName;
    QString strUnit;
    int iAxis = 0;                // vecYAxes 자리(0 왼쪽, 1 오른쪽). 히트맵은 그 계열 y 열의 자리(색 눈금), 도넛은 0
    QVector<ChartPoint> vecPoints;
};

struct ChartAxis
{
    QString strTitle;             // x 축은 x 열 이름, y 축은 그 단위의 y 열 이름을 " · " 로 이은 것(히트맵은 그 y 열 이름, 도넛은 첫 계열의 y 열 이름)
    QString strUnit;
    double dMin = 0;              // 수치 축은 바깥 눈금. 범주 축은 -0.5 부터 범주 수 - 0.5 까지. 히트맵 색 눈금은 값의 가장 작은 값 · 가장 큰 값.
    double dMax = 0;              // 도넛 축은 0 부터 0 보다 큰 값의 합(전체)까지
    QVector<double> vecTicks;     // 수치 축의 눈금(범주 축 · 히트맵 색 눈금 · 도넛 축은 비었다)
    QStringList vecCategories;    // 범주 축의 이름표(수치 축은 비었다)
};

struct ChartSpec
{
    EChartType eType = EChartType::NONE;
    QString strTitle;                // 표 제목
    QString strCaption;              // condition · where
    ChartAxis xAxis;                 // 범주 · 순서 x 는 vecCategories, 수치 x 는 범위와 눈금
    QVector<ChartAxis> vecYAxes;     // 단위가 다르면 둘(왼쪽 · 오른쪽). 히트맵은 y 열마다 하나(색 눈금, y 순서). 도넛은 하나(전체). NONE 이면 비었다
    QVector<ChartSeries> vecSeries;  // 도넛은 첫 계열 하나. NONE 이면 비었다
    QStringList vecWarnings;         // 뺀 열 · 행과 그리지 않은 까닭
};

struct ChartBuildInput
{
    const KnowledgeTable* pTable = nullptr;   // 차트로 그릴 표(맡지 않는다)
};

using IChartBuilderStrategy = VisualMarkdown::IStrategy<ChartBuildInput, ChartSpec, EChartType>;

// 몫(design/MoreViews.md §5.2 · §5.3): 단위가 % 이고 합이 100 ± 1 이면 값이 곧 몫이다.
// 도넛 추천(몫 표) · % 합 경고(ChartBuilderCommon · DonutChartCPU)와 위젯의 몫 칸 · 설명이 이 한 곳의 규칙을 쓴다.
namespace ChartShare
{
constexpr double kWholePercent = 100;   // 몫의 합
constexpr double kWholeTolerance = 1;   // 합 100 ± 1 은 받아 준다(반올림한 몫의 합)
constexpr double kSumEpsilon = 1e-9;    // 소수 칸을 더한 오차로 99.0 · 101.0 이 경계 밖으로 밀리지 않게

// 단위가 % 인지
inline bool IsPercentUnit(const QString& unit)
{
    return unit == QStringLiteral("%");
}

// 합이 100 ± 1 안인지
inline bool IsWholePercent(double sum)
{
    return std::abs(sum - kWholePercent) <= kWholeTolerance + kSumEpsilon;
}
}
