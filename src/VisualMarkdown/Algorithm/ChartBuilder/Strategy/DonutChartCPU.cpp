#include "VisualMarkdown/Algorithm/ChartBuilder/Strategy/DonutChartCPU.h"

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartBuilderCommon.h"

#include <optional>

namespace
{
using ChartBuilderCommon::ChartColumns;
using ChartBuilderCommon::ChartRow;

constexpr int kSumDecimals = 6;          // 경고에 쓰는 합의 소수 자리(끝의 0 은 뺀다)

// 경고에 쓰는 합: 소수 6자리까지 쓰고 끝의 0 과 소수점은 뺀다(512.7 · 40)
QString FormatSum(double sum)
{
    QString strSum = QString::number(sum, 'f', kSumDecimals);
    if (strSum.contains(QLatin1Char('.')))
    {
        while (strSum.endsWith(QLatin1Char('0')))
            strSum.chop(1);
        if (strSum.endsWith(QLatin1Char('.')))
            strSum.chop(1);
    }
    return strSum;
}

// 계열이 둘 이상이면 첫 계열만 두고 나머지를 빼며 경고를 단다(뺀 계열 이름은 따옴표로 묶어 쉼표로 잇는다). 남은 계열의 축은 0
void KeepFirstSeries(ChartSpec* spec)
{
    if (spec->vecSeries.size() > 1)
    {
        QStringList vecDropped;
        for (int iSeries = 1; iSeries < spec->vecSeries.size(); ++iSeries)
            vecDropped << QStringLiteral("\"%1\"").arg(spec->vecSeries[iSeries].strName);
        spec->vecWarnings << QStringLiteral("도넛은 계열 하나만 그린다: \"%1\" 만 그리고 %2 은 뺀다")
                                 .arg(spec->vecSeries.first().strName, vecDropped.join(QStringLiteral(", ")));
        spec->vecSeries.resize(1);
    }
    if (!spec->vecSeries.isEmpty())
        spec->vecSeries.first().iAxis = 0;
}

// 첫 계열의 점마다 표 행(KnowledgeTable::vecRows 자리). BuildSeries 와 같은 순서다: series 열이 있으면 첫 값의 행만
QVector<int> ListFirstSeriesRows(const KnowledgeTable& table, const ChartColumns& columns, const QVector<ChartRow>& rows)
{
    const QString strGroup = ChartBuilderCommon::ListSeriesGroups(table, columns).value(0);
    QVector<int> vecRows;
    for (const ChartRow& row : rows)
        if (columns.iSeries < 0 || table.vecRows[row.iRow].value(columns.iSeries).trimmed() == strGroup)
            vecRows << row.iRow;
    return vecRows;
}
}

bool DonutChartCPU::Execute(const ChartBuildInput& in, ChartSpec& out)
{
    out = ChartSpec();
    if (in.pTable == nullptr)
        return false;
    const KnowledgeTable& table = *in.pTable;
    ChartColumns columns;
    if (!ChartBuilderCommon::BeginChart(table, EChartType::DONUT, &out, &columns))
        return true;
    // 행 놓기 · 계열은 막대와 같다. 축은 하나라 y 열마다(BY_COLUMN) 만들어 규칙 4 경고 없이 첫 축(첫 계열의 y 열 이름 · 단위)만 남긴다
    const QVector<ChartRow> vecRows = ChartBuilderCommon::PlaceOnCategories(table, columns, &out);
    ChartBuilderCommon::BuildSeries(table, columns, vecRows, &out, ChartBuilderCommon::EYAxisGrouping::BY_COLUMN);
    KeepFirstSeries(&out);
    out.vecYAxes.resize(1);

    // 조각은 값이 0 이상인 점이다. 값이 없거나 0 보다 작은 점은 남기고 행마다 경고를 단다. 전체는 0 보다 큰 값의 합이다
    const QVector<int> vecPointRows = ListFirstSeriesRows(table, columns, vecRows);
    int iSlices = 0;
    double dTotal = 0;
    if (!out.vecSeries.isEmpty())
    {
        const QVector<ChartPoint>& vecPoints = out.vecSeries.first().vecPoints;
        for (int iPoint = 0; iPoint < vecPoints.size(); ++iPoint)
        {
            const int iRowNumber = vecPointRows.value(iPoint, -1) + 1;
            const std::optional<double>& optY = vecPoints[iPoint].optY;
            if (!optY)
            {
                out.vecWarnings << QStringLiteral("%1행: 값이 없어 도넛에 넣지 않는다").arg(iRowNumber);
                continue;
            }
            if (*optY < 0)
            {
                out.vecWarnings << QStringLiteral("%1행: 0 보다 작아 도넛에 넣지 않는다").arg(iRowNumber);
                continue;
            }
            ++iSlices;
            dTotal += *optY;
        }
    }
    ChartAxis& axis = out.vecYAxes.first();
    axis.dMin = 0;
    axis.dMax = dTotal;
    axis.vecTicks.clear();
    if (ChartShare::IsPercentUnit(axis.strUnit) && !ChartShare::IsWholePercent(dTotal))
        out.vecWarnings << QStringLiteral("단위가 % 인데 합이 %1 이다(전체의 몫은 합에 대한 비율로 그린다)").arg(FormatSum(dTotal));
    if (iSlices > ChartBuilderCommon::kMaxShareSlices)
        out.vecWarnings << QStringLiteral("조각이 %1개라 읽기 어렵다(막대가 낫다)").arg(iSlices);
    return true;
}

EChartType DonutChartCPU::GetType() const
{
    return EChartType::DONUT;
}
