#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartBuilderCommon.h"

#include "VisualMarkdown/KnowledgeText.h"

#include <algorithm>
#include <cmath>

namespace ChartBuilderCommon
{
namespace
{
const QString kJoin = QStringLiteral(" · ");    // 밑글 · 계열 이름 · 축 이름을 잇는 글
constexpr int kMinTicks = 5;
constexpr int kMaxTicks = 8;
constexpr int kNiceMantissas[] = { 1, 2, 5 };     // 눈금 간격은 1 · 2 · 5 × 10ⁿ
constexpr int kFirstExponentBelowSpan = 1;        // 첫 간격 10ⁿ 의 n 은 ⌊log₁₀ 범위⌋ 보다 이만큼 작다(범위의 1/100–1/10)
constexpr int kExponentTries = 4;                 // 첫 간격(범위의 1/100–1/10)에서 10ⁿ 을 이만큼 키우면 눈금이 8개 아래로 준다
constexpr double kMaxTickUnits = 1e15;            // 눈금 번호를 double 로 정확히 셀 수 있는 크기
constexpr int kDecimalBase = 10;
constexpr double kMarginRatio = 0.05;             // 선 · 산점도: 값 범위 양쪽 여유
constexpr double kZeroRangePadding = 1;           // 범위가 0 이면 ±1
constexpr double kCategoryHalfWidth = 0.5;        // 범주 축은 -0.5 부터 범주 수 - 0.5 까지
constexpr int kRightAxis = 1;
constexpr int kMaxYAxes = 2;                      // 왼쪽 · 오른쪽

// 칸 글(앞뒤 공백을 뺀다). 칸이 모자란 행은 빈 칸으로 본다
QString GetCell(const KnowledgeTable& table, int row, int column)
{
    if (column < 0)
        return QString();
    return table.vecRows[row].value(column).trimmed();
}

bool HasAnyCell(const KnowledgeTable& table, int column)
{
    for (int iRow = 0; iRow < table.vecRows.size(); ++iRow)
        if (!GetCell(table, iRow, column).isEmpty())
            return true;
    return false;
}

// series 열이 있는데 그 칸이 비었으면 경고를 남기고 false 다(그 행은 어느 계열에도 넣지 않는다)
bool CheckSeriesCell(const KnowledgeTable& table, const ChartColumns& columns, int row, QStringList* warnings)
{
    if (columns.iSeries < 0 || !GetCell(table, row, columns.iSeries).isEmpty())
        return true;
    *warnings << QStringLiteral("%1행: series 칸이 비어 이 행을 뺀다").arg(row + 1);
    return false;
}

// 10ⁿ(n ≥ 0). 곱해 가며 만들어 n ≤ 22 까지 정확하다
double MakePowerOfTen(int exponent)
{
    double dValue = 1;
    for (int iPower = 0; iPower < exponent; ++iPower)
        dValue *= kDecimalBase;
    return dValue;
}

// 눈금 값 units × mantissa × 10ⁿ. n 이 음수면 나눠서 소수 눈금(0.1 등)이 그 소수에 가장 가까운 double 이 되게 한다
double MakeTick(long long units, int mantissa, int exponent)
{
    const double dUnits = static_cast<double>(units) * mantissa;
    return exponent >= 0 ? dUnits * MakePowerOfTen(exponent) : dUnits / MakePowerOfTen(-exponent);
}
}

std::optional<double> ParseQuantity(const QString& cell)
{
    if (!KnowledgeText::IsQuantityText(cell))
        return std::nullopt;
    QString strNumber = cell.trimmed();
    strNumber.remove(QLatin1Char(','));
    bool bOk = false;
    const double dValue = strNumber.toDouble(&bOk);
    if (!bOk || !std::isfinite(dValue))
        return std::nullopt;
    return dValue;
}

int FindColumn(const KnowledgeTable& table, const QString& name)
{
    if (name.isEmpty())
        return -1;
    for (int iColumn = 0; iColumn < table.vecColumns.size(); ++iColumn)
        if (table.vecColumns[iColumn].strName == name)
            return iColumn;
    return -1;
}

QString MakeCaption(const KnowledgeTable& table)
{
    QStringList vecParts;
    for (const QString& strPart : { table.strCondition, table.strWhere })
        if (!strPart.trimmed().isEmpty())
            vecParts << strPart.trimmed();
    return vecParts.join(kJoin);
}

bool ResolveColumns(const KnowledgeTable& table, ChartColumns* columns, QStringList* warnings)
{
    *columns = ChartColumns();
    bool bChartable = true;
    if (table.strX.isEmpty())
    {
        *warnings << QStringLiteral("x 열이 정해지지 않았다");
        bChartable = false;
    }
    else
    {
        columns->iX = FindColumn(table, table.strX);
        if (columns->iX < 0)
        {
            *warnings << QStringLiteral("x 열 \"%1\": 표에 없다").arg(table.strX);
            bChartable = false;
        }
    }

    // y: 표에 없으면 그리지 않는다. 수치가 아닌 열은 빼고 경고를 단다
    if (table.vecY.isEmpty())
    {
        *warnings << QStringLiteral("y 열이 정해지지 않았다");
        bChartable = false;
    }
    for (const QString& strY : table.vecY)
    {
        const int iY = FindColumn(table, strY);
        if (iY < 0)
        {
            *warnings << QStringLiteral("y 열 \"%1\": 표에 없다").arg(strY);
            bChartable = false;
            continue;
        }
        const EColumnKind eKind = KnowledgeText::ClassifyColumn(table, iY);
        if (eKind == EColumnKind::QUANTITY)
        {
            columns->vecY << iY;
            continue;
        }
        if (eKind == EColumnKind::ORDINAL)
            *warnings << QStringLiteral("y 열 \"%1\": 순서 열이라 뺀다").arg(strY);
        else if (HasAnyCell(table, iY))
            *warnings << QStringLiteral("y 열 \"%1\": 수치가 아니라 뺀다").arg(strY);
        else
            *warnings << QStringLiteral("y 열 \"%1\": 값이 없어 뺀다").arg(strY);
    }

    if (!table.strSeries.isEmpty())
    {
        columns->iSeries = FindColumn(table, table.strSeries);
        if (columns->iSeries < 0)
        {
            *warnings << QStringLiteral("series 열 \"%1\": 표에 없다").arg(table.strSeries);
            bChartable = false;
        }
    }
    if (columns->vecY.isEmpty() && !table.vecY.isEmpty())
    {
        *warnings << QStringLiteral("수치인 y 열이 없어 차트를 그리지 않는다");
        bChartable = false;
    }
    return bChartable;
}

bool BeginChart(const KnowledgeTable& table, EChartType type, ChartSpec* spec, ChartColumns* columns)
{
    *spec = ChartSpec();
    spec->strTitle = table.strTitle;
    spec->strCaption = MakeCaption(table);
    const bool bChartable = ResolveColumns(table, columns, &spec->vecWarnings);
    if (!bChartable || type == EChartType::NONE)
    {
        if (bChartable)
            spec->vecWarnings << QStringLiteral("차트 종류를 NONE 으로 정해 그리지 않는다");
        spec->eType = EChartType::NONE;
        return false;
    }
    spec->eType = type;
    const KnowledgeColumn& xColumn = table.vecColumns[columns->iX];
    spec->xAxis.strTitle = xColumn.strName;
    spec->xAxis.strUnit = xColumn.strUnit;
    return true;
}

QVector<ChartRow> PlaceOnCategories(const KnowledgeTable& table, const ChartColumns& columns, ChartSpec* spec)
{
    QVector<ChartRow> vecRows;
    QStringList& vecCategories = spec->xAxis.vecCategories;
    for (int iRow = 0; iRow < table.vecRows.size(); ++iRow)
    {
        if (!CheckSeriesCell(table, columns, iRow, &spec->vecWarnings))
            continue;
        // series 열이 없으면 행마다 범주 하나, 있으면 같은 x 글은 한 범주(처음 나온 순서)
        const QString strLabel = GetCell(table, iRow, columns.iX);
        int iCategory = columns.iSeries >= 0 ? vecCategories.indexOf(strLabel) : -1;
        if (iCategory < 0)
        {
            iCategory = vecCategories.size();
            vecCategories << strLabel;
        }
        vecRows.push_back({ iRow, static_cast<double>(iCategory), strLabel });
    }
    spec->xAxis.dMin = -kCategoryHalfWidth;
    spec->xAxis.dMax = std::max(vecCategories.size(), 1) - kCategoryHalfWidth;
    return vecRows;
}

QVector<ChartRow> PlaceOnNumbers(const KnowledgeTable& table, const ChartColumns& columns, ChartSpec* spec)
{
    QVector<ChartRow> vecRows;
    QVector<double> vecValues;
    for (int iRow = 0; iRow < table.vecRows.size(); ++iRow)
    {
        if (!CheckSeriesCell(table, columns, iRow, &spec->vecWarnings))
            continue;
        const QString strLabel = GetCell(table, iRow, columns.iX);
        if (strLabel.isEmpty())
        {
            spec->vecWarnings << QStringLiteral("%1행: x 칸이 비어 이 행을 뺀다").arg(iRow + 1);
            continue;
        }
        const std::optional<double> optX = ParseQuantity(strLabel);
        if (!optX)
        {
            spec->vecWarnings << QStringLiteral("%1행 x 칸 \"%2\": 숫자가 아니라 이 행을 뺀다").arg(iRow + 1).arg(strLabel);
            continue;
        }
        vecRows.push_back({ iRow, *optX, strLabel });
        vecValues << *optX;
    }
    const ChartScale scale = ComputeScale(vecValues, false);
    spec->xAxis.dMin = scale.dMin;
    spec->xAxis.dMax = scale.dMax;
    spec->xAxis.vecTicks = scale.vecTicks;
    return vecRows;
}

void BuildSeries(const KnowledgeTable& table, const ChartColumns& columns, const QVector<ChartRow>& rows, ChartSpec* spec)
{
    // series 값: 빈 칸이 아닌 글, 처음 나온 순서. series 열이 없으면 y 마다 계열 하나
    QStringList vecGroups;
    if (columns.iSeries >= 0)
    {
        for (int iRow = 0; iRow < table.vecRows.size(); ++iRow)
        {
            const QString strGroup = GetCell(table, iRow, columns.iSeries);
            if (!strGroup.isEmpty() && !vecGroups.contains(strGroup))
                vecGroups << strGroup;
        }
    }
    else
        vecGroups << QString();

    // y 단위마다 축(빈 단위도 단위 하나). 첫 단위는 왼쪽, 둘째부터는 오른쪽이다
    QStringList vecUnits;
    for (const int iY : columns.vecY)
        if (!vecUnits.contains(table.vecColumns[iY].strUnit))
            vecUnits << table.vecColumns[iY].strUnit;
    for (int iAxis = 0; iAxis < std::min(vecUnits.size(), kMaxYAxes); ++iAxis)
    {
        ChartAxis axis;
        axis.strUnit = vecUnits[iAxis];
        QStringList vecNames;
        for (const int iY : columns.vecY)
            if (table.vecColumns[iY].strUnit == axis.strUnit)
                vecNames << table.vecColumns[iY].strName;
        axis.strTitle = vecNames.join(kJoin);
        spec->vecYAxes << axis;
    }

    const bool bSeveralY = columns.vecY.size() > 1;
    for (const int iY : columns.vecY)
    {
        const KnowledgeColumn& yColumn = table.vecColumns[iY];
        const int iUnit = vecUnits.indexOf(yColumn.strUnit);
        if (iUnit >= kMaxYAxes)
            spec->vecWarnings << QStringLiteral("y 열 \"%1\"(단위 \"%2\"): 셋째 이후 단위라 오른쪽 축에 둔다").arg(yColumn.strName, yColumn.strUnit);
        for (const QString& strGroup : vecGroups)
        {
            ChartSeries series;
            if (columns.iSeries < 0)
                series.strName = yColumn.strName;
            else
                series.strName = bSeveralY ? yColumn.strName + kJoin + strGroup : strGroup;
            series.strUnit = yColumn.strUnit;
            series.iAxis = std::min(iUnit, kRightAxis);
            for (const ChartRow& row : rows)
            {
                if (columns.iSeries >= 0 && GetCell(table, row.iRow, columns.iSeries) != strGroup)
                    continue;
                ChartPoint point;
                point.dX = row.dX;
                point.strLabel = row.strLabel;
                // 빈 칸은 값 없음. 수치 열이라 빈 칸이 아니면 숫자다(double 로 담지 못하는 것만 빼고)
                const QString strCell = GetCell(table, row.iRow, iY);
                if (!strCell.isEmpty())
                {
                    point.optY = ParseQuantity(strCell);
                    if (!point.optY)
                        spec->vecWarnings << QStringLiteral("%1행 y 열 \"%2\" 칸 \"%3\": 숫자로 읽지 못해 값 없음으로 둔다")
                                                 .arg(row.iRow + 1)
                                                 .arg(yColumn.strName, strCell);
                }
                series.vecPoints << point;
            }
            spec->vecSeries << series;
        }
    }
}

void ScaleYAxes(ChartSpec* spec, bool includeZero)
{
    for (int iAxis = 0; iAxis < spec->vecYAxes.size(); ++iAxis)
    {
        QVector<double> vecValues;
        for (const ChartSeries& series : spec->vecSeries)
        {
            if (series.iAxis != iAxis)
                continue;
            for (const ChartPoint& point : series.vecPoints)
                if (point.optY)
                    vecValues << *point.optY;
        }
        const ChartScale scale = ComputeScale(vecValues, includeZero);
        ChartAxis& axis = spec->vecYAxes[iAxis];
        axis.dMin = scale.dMin;
        axis.dMax = scale.dMax;
        axis.vecTicks = scale.vecTicks;
    }
}

ChartScale ComputeScale(const QVector<double>& values, bool includeZero)
{
    // 값이 없으면 0 하나로 본다
    double dLow = 0;
    double dHigh = 0;
    if (!values.isEmpty())
    {
        const auto pairMinMax = std::minmax_element(values.cbegin(), values.cend());
        dLow = *pairMinMax.first;
        dHigh = *pairMinMax.second;
    }
    const double dValueLow = dLow;
    const double dValueHigh = dHigh;
    if (includeZero)
    {
        dLow = std::min(dLow, 0.0);
        dHigh = std::max(dHigh, 0.0);
    }
    else
    {
        const double dMargin = (dHigh - dLow) * kMarginRatio;
        dLow -= dMargin;
        dHigh += dMargin;
    }
    if (!(dHigh > dLow))
    {
        dLow -= kZeroRangePadding;
        dHigh += kZeroRangePadding;
    }
    // 여유(±1 도)는 0 을 넘지 않는다: 값이 모두 0 이상이면 아래 끝, 모두 0 이하면 위 끝을 0 에서 멈춘다.
    // 모두 0 이면 0 이상으로 본다(0–1). 값이 없으면 멈추지 않는다
    if (!values.isEmpty())
    {
        if (dValueLow >= 0)
            dLow = std::max(dLow, 0.0);
        else if (dValueHigh <= 0)
            dHigh = std::min(dHigh, 0.0);
    }
    return ComputeNiceTicks(dLow, dHigh);
}

ChartScale ComputeNiceTicks(double low, double high)
{
    ChartScale scale;
    const double dSpan = high - low;
    if (!(dSpan > 0) || !std::isfinite(dSpan))
    {
        // 눈금을 셀 수 없는 범위(값이 아주 크다): 두 끝만 둔다
        scale.dMin = low;
        scale.dMax = high;
        scale.vecTicks = { low, high };
        return scale;
    }

    // 간격은 범위의 1/100–1/10 인 10ⁿ 부터 1 · 2 · 5 × 10ⁿ 차례로 키운다. 눈금이 8개 이하가 되는 첫 간격을 쓴다
    const int iFirstExponent = static_cast<int>(std::floor(std::log10(dSpan))) - kFirstExponentBelowSpan;
    for (int iExponent = iFirstExponent; iExponent <= iFirstExponent + kExponentTries; ++iExponent)
    {
        for (const int iMantissa : kNiceMantissas)
        {
            const double dStep = MakeTick(1, iMantissa, iExponent);
            const double dLowUnits = std::floor(low / dStep);
            const double dHighUnits = std::ceil(high / dStep);
            if (std::fabs(dLowUnits) > kMaxTickUnits || std::fabs(dHighUnits) > kMaxTickUnits)
                continue;
            // 나눗셈 오차는 눈금 값으로 바로잡는다: 눈금(low 번호) ≤ low, high ≤ 눈금(high 번호), 그 안쪽 눈금은 범위 안
            long long llLow = static_cast<long long>(dLowUnits);
            long long llHigh = static_cast<long long>(dHighUnits);
            while (MakeTick(llLow, iMantissa, iExponent) > low)
                --llLow;
            while (MakeTick(llLow + 1, iMantissa, iExponent) <= low)
                ++llLow;
            while (MakeTick(llHigh, iMantissa, iExponent) < high)
                ++llHigh;
            while (MakeTick(llHigh - 1, iMantissa, iExponent) >= high)
                --llHigh;
            if (llHigh - llLow + 1 > kMaxTicks)
                continue;
            // 앞 간격이 9개 이상이면 이 간격은 4개일 수 있다. 0 이 아닌 쪽으로 하나씩 더한다(모두 0 이하면 아래로)
            while (llHigh - llLow + 1 < kMinTicks)
            {
                if (MakeTick(llHigh, iMantissa, iExponent) <= 0 && MakeTick(llLow, iMantissa, iExponent) < 0)
                    --llLow;
                else
                    ++llHigh;
            }
            for (long long llUnits = llLow; llUnits <= llHigh; ++llUnits)
                scale.vecTicks << MakeTick(llUnits, iMantissa, iExponent);
            scale.dMin = scale.vecTicks.first();
            scale.dMax = scale.vecTicks.last();
            return scale;
        }
    }
    // 닿지 않는 자리(유한한 범위면 위에서 끝난다)
    scale.dMin = low;
    scale.dMax = high;
    scale.vecTicks = { low, high };
    return scale;
}
}
