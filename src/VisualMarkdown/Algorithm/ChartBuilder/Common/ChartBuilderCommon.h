#pragma once

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"
#include "VisualMarkdown/KnowledgeTypes.h"

#include <QString>
#include <QStringList>
#include <QVector>

#include <optional>

// 차트 전략들이 함께 쓰는 것: 열 찾기, 숫자 읽기, 행을 x 에 놓기, 계열 · y 축 만들기, 눈금(design/Views.md §6).
namespace ChartBuilderCommon
{
// 차트에 쓰는 열 자리
struct ChartColumns
{
    int iX = -1;
    QVector<int> vecY;     // 그릴 y 열(수치인 것만, y 순서)
    int iSeries = -1;      // series 열. 없으면 -1
};

// 차트에 놓인 행 하나
struct ChartRow
{
    int iRow = -1;         // KnowledgeTable::vecRows 자리
    double dX = 0;         // 점의 dX(범주 자리 또는 x 값)
    QString strLabel;      // x 칸 글
};

// 수치 축의 범위와 눈금
struct ChartScale
{
    double dMin = 0;
    double dMax = 0;
    QVector<double> vecTicks;
};

// IsQuantityText 가 숫자로 보는 칸만 읽는다(천 단위 쉼표 · 부호 · 소수 · 지수). 그 밖 · 빈 칸 · 유한하지 않은 값은 비었다
std::optional<double> ParseQuantity(const QString& cell);
// 이름이 같은 첫 열의 자리. 없으면 -1
int FindColumn(const KnowledgeTable& table, const QString& name);
// condition · where 를 " · " 로 잇는다(빈 것은 뺀다)
QString MakeCaption(const KnowledgeTable& table);
// x · y · series 열을 찾는다. 그릴 수 없으면(모르는 열, 수치인 y 가 없음) false 다. 뺀 y 와 그 까닭은 warnings 에 더한다
bool ResolveColumns(const KnowledgeTable& table, ChartColumns* columns, QStringList* warnings);

// spec 을 비우고 제목 · 밑글을 채운다. 그릴 수 있으면 종류 · x 축 이름을 채우고 true,
// 그릴 수 없거나 type 이 NONE 이면 NONE 과 그 까닭(경고)을 남기고 false 다
bool BeginChart(const KnowledgeTable& table, EChartType type, ChartSpec* spec, ChartColumns* columns);
// 범주 x 축: 행 순서로 x 칸 글을 범주로 둔다(series 열이 있으면 같은 글은 한 번, 처음 나온 순서). dX 는 범주 자리다
QVector<ChartRow> PlaceOnCategories(const KnowledgeTable& table, const ChartColumns& columns, ChartSpec* spec);
// 수치 x 축: dX 는 x 값이다. x 가 비었거나 숫자가 아닌 행은 경고를 남기고 뺀다. x 축 범위 · 눈금은 값 범위에 여유를 둔다
QVector<ChartRow> PlaceOnNumbers(const KnowledgeTable& table, const ChartColumns& columns, ChartSpec* spec);
// y 마다(series 열이 있으면 그 값마다) 계열을 만들고, y 단위마다 축을 둔다(첫 단위 왼쪽 · 둘째 오른쪽 · 셋째부터 오른쪽과 경고)
void BuildSeries(const KnowledgeTable& table, const ChartColumns& columns, const QVector<ChartRow>& rows, ChartSpec* spec);
// y 축마다 그 축 계열의 값으로 범위 · 눈금을 정한다
void ScaleYAxes(ChartSpec* spec, bool includeZero);

// 값의 범위 · 눈금. includeZero 면 0 을 넣고, 아니면 값 범위의 양쪽에 5% 여유를 둔다(범위가 0 이면 ±1).
// 여유는 0 을 넘지 않는다: 값이 모두 0 이상이면 아래 끝, 모두 0 이하면 위 끝이 0 에서 멈춘다(모두 0 이면 0–1)
ChartScale ComputeScale(const QVector<double>& values, bool includeZero);
// low 부터 high 까지를 덮는 1 · 2 · 5 × 10ⁿ 간격의 눈금 5–8개. 범위는 바깥 눈금에 맞춘다
ChartScale ComputeNiceTicks(double low, double high);
}
