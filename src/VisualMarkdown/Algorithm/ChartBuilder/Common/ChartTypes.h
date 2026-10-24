#pragma once

#include "VisualMarkdown/Interface/IStrategy.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

struct KnowledgeTable;

// 지식 안 차트 종류(design/Views.md §6). NONE 은 그리지 않는다(보기 앱은 차트 대신 표를 보인다).
enum class EChartType { NONE, BAR, STEP_BAR, LINE, SCATTER };

// 점 하나. 범주 x(막대 · 단계 막대 · 선)는 dX 가 범주 자리(0, 1, …)이고, 수치 x(산점도)는 x 값이다.
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
    int iAxis = 0;                // vecYAxes 자리(0 왼쪽, 1 오른쪽)
    QVector<ChartPoint> vecPoints;
};

struct ChartAxis
{
    QString strTitle;             // x 축은 x 열 이름, y 축은 그 단위의 y 열 이름을 " · " 로 이은 것
    QString strUnit;
    double dMin = 0;              // 수치 축은 바깥 눈금. 범주 축은 -0.5 부터 범주 수 - 0.5 까지
    double dMax = 0;
    QVector<double> vecTicks;     // 수치 축의 눈금(범주 축은 비었다)
    QStringList vecCategories;    // 범주 축의 이름표(수치 축은 비었다)
};

struct ChartSpec
{
    EChartType eType = EChartType::NONE;
    QString strTitle;                // 표 제목
    QString strCaption;              // condition · where
    ChartAxis xAxis;                 // 범주 · 순서 x 는 vecCategories, 수치 x 는 범위와 눈금
    QVector<ChartAxis> vecYAxes;     // 단위가 다르면 둘(왼쪽 · 오른쪽). NONE 이면 비었다
    QVector<ChartSeries> vecSeries;  // NONE 이면 비었다
    QStringList vecWarnings;         // 뺀 열 · 행과 그리지 않은 까닭
};

struct ChartBuildInput
{
    const KnowledgeTable* pTable = nullptr;   // 차트로 그릴 표(맡지 않는다)
};

using IChartBuilderStrategy = VisualMarkdown::IStrategy<ChartBuildInput, ChartSpec, EChartType>;
