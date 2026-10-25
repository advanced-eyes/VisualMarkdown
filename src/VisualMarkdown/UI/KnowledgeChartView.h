#pragma once

#include "VisualMarkdown/Algorithm/ChartBuilder/Common/ChartTypes.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"

#include <QFont>
#include <QPair>
#include <QPoint>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QPainter;

// 지식 안 차트(ChartSpec)를 그리는 위젯(design/Views.md §6 · §7). QPainter 로 그린다(Qt Charts 는 GPLv3 라 쓰지 않는다).
// 제목 · 밑글, 축(왼쪽 · 오른쪽 y) · 눈금 · 격자, 막대(여러 계열이면 나란히) · 단계 막대 · 선(빈 값에서 끊는다) · 점, 범례.
// 히트맵(design/MoreViews.md §4.3)은 색 범례 아래 칸 격자(행 = 범주, 열 = 계열)를 그린다. 칸 색은 그 계열 축(색 눈금)의 자리 색이다.
// 도넛(design/MoreViews.md §5.3)은 왼쪽에 고리(첫 계열의 점마다 조각, 12시에서 시계 방향), 오른쪽에 범례 목록을 그린다. 가운데에 y 열 이름과 합.
// NONE 이면 가운데에 그리지 않는 까닭(경고)을 쓴다(히트맵은 모든 칸이 비어도, 도넛은 0 보다 큰 값이 없어도). 범위가 0 이거나 유한하지 않은 축은 그 값 둘레 ±1 로 넓혀 그린다.
class KnowledgeChartView : public QWidget
{
    Q_OBJECT

public:
    explicit KnowledgeChartView(QWidget* parent = nullptr);
    ~KnowledgeChartView() override;

public:
    void SetSpec(const ChartSpec& spec);
    void SetTheme(const VisualMarkdownTheme& theme);
    // 제목 · 밑글 줄을 그릴지(기본 참). 거짓이면 설정에 제목 · 밑글이 있어도 그 자리를 두지 않고, 그 아래 것(범례 · 그림 칸 · NONE 의 까닭 글)이
    // 위 여백에서 바로 시작한다. GetSpec 은 받은 그대로다
    void SetHeaderVisible(bool visible);

public:
    // 위치로 찾기(위젯 좌표). 보기 앱과 시험이 쓴다
    QPair<int, int> FindPointAt(const QPoint& pos) const;   // (계열, 점). 없으면 (-1, -1). 히트맵은 그 칸의 점, 도넛은 각 · 반지름으로 찾은 조각(0, 점)
    // 계열 이름 · x 이름표 · 값과 단위. 히트맵 칸은 "행 이름 · 열 이름: 값 단위"(빈 칸은 "값 없음"), 행 · 열 이름 위는 그 이름 전부.
    // 도넛 조각은 "범주: 값 단위 (전체의 n%)"(값이 곧 몫이면, 곧 단위 % · 합 100 ± 1 이면 "범주: 값 단위"). 없으면 빈 글
    QString GetToolTipAt(const QPoint& pos) const;
    // 막대 또는 점 표시. 값이 없거나 그리지 않으면 빈 사각형. 히트맵은 칸(값이 없는 칸도 테두리와 "—" 로 그리므로 칸을 준다).
    // 도넛은 조각 가운데 점(고리 띠 가운데 반지름 · 조각 가운데 각) 둘레의 작은 사각형이다(각이 0 인 조각 · 조각이 아닌 점은 빈 사각형)
    QRectF GetMarkRect(int series, int point) const;
    QRectF GetPlotRect() const;                             // 그림 칸(히트맵은 칸 격자, 도넛은 고리를 담는 정사각형). NONE 이면 빈 사각형
    QRectF GetLegendRect() const;                           // 범례. 계열이 하나뿐인 차트(단계 막대 · 히트맵 · 도넛 빼고)는 빈 사각형. 히트맵은 색 범례, 도넛은 범례 목록
    QString GetMessageText() const;                         // NONE · 그릴 값이 없을 때(히트맵은 모든 칸이 빔, 도넛은 0 보다 큰 값이 없음) 가운데에 쓰는 글. 차트를 그리면 빈 글

public:
    const ChartSpec& GetSpec() const;
    bool IsHeaderVisible() const;

protected:
    bool event(QEvent* event) override;                     // QEvent::ToolTip: 마우스 올림 설명
    void paintEvent(QPaintEvent* event) override;

private:
    // 그리는 축 범위(범위가 0 · 유한하지 않으면 넓힌 것)와 눈금
    struct AxisScale
    {
        double dMin = 0;
        double dMax = 1;
        QVector<double> vecTicks;
        bool bValid = false;    // 축이 있는지(y 축이 하나뿐이면 오른쪽 축은 false)
    };

    enum class ELabelMode { HORIZONTAL, WRAPPED, ROTATED };

    struct LegendItem
    {
        QString strText;
        QColor color;
        int iSeries = -1;       // 계열 견본(모양 · 선). 단계 막대의 늘어남 · 줄어듦은 -1
        QRectF rect;            // 위젯 좌표
    };

    // 히트맵 자리(위젯 좌표). 칸 (행 r, 열 c) = [vecColumnEdges[c], vecColumnEdges[c + 1] − dGapX) × [vecRowEdges[r], vecRowEdges[r + 1] − dGapY)
    struct HeatmapGeometry
    {
        QVector<double> vecColumnEdges;   // 열 왼쪽 끝(열 수 + 1, 마지막은 격자 오른쪽 + 틈). 정수 화소
        QVector<double> vecRowEdges;      // 행 위 끝(행 수 + 1, 마지막은 격자 아래 + 틈). 정수 화소
        double dGapX = 0;                 // 칸 사이 바탕 색 틈(칸이 좁으면 0)
        double dGapY = 0;
        QRectF rowLabels;                 // 행 이름 칸(격자 왼쪽, 오른쪽 맞춤)
        QRectF columnLabels;              // 열 이름 칸(격자 위)
        int iHeaderLines = 1;             // 열 이름 줄 수(한 줄에 들어가지 않으면 둘로 접는다)
        QRectF corner;                    // 행 이름 칸 위: x 축 이름
        QRectF legendTitle;               // 색 범례: [이름] 낮음 [띠] 높음 [y 열마다 따로 칠한다]
        QRectF legendLow;
        QRectF legendBar;
        QRectF legendHigh;
        QRectF legendNote;
        QString strLegendTitle;
        QString strLegendLow;
        QString strLegendHigh;
        QString strLegendNote;
    };

    // 도넛 자리(위젯 좌표, design/MoreViews.md §5.3). 고리는 center 둘레 반지름 dInner … dOuter(그림 칸 위에 붙고 지름은 480px 까지),
    // 범례 목록은 고리 오른쪽이다. 범례 줄 하나: [견본] [범주 글] [값과 단위(오른쪽 맞춤)] [몫(오른쪽 맞춤, 값이 곧 몫이면 없다)]
    struct DonutGeometry
    {
        QPointF center;
        double dOuter = 0;
        double dInner = 0;
        QVector<QRectF> vecLegendRows;   // 첫 계열의 점마다 범례 줄. 아래로 넘치는 줄은 빈 사각형(쓰지 않는다)
        double dLabelWidth = 0;          // 범주 글 칸(넘치는 글은 줄인다)
        double dValueWidth = 0;          // 값 칸
        double dShareWidth = 0;          // 몫 칸(값이 곧 몫이면 0, 칸이 없다)
    };

    // 도넛 조각(첫 계열의 점마다). 각은 12시가 0 이고 시계 방향으로 는다(도)
    struct DonutSlice
    {
        bool bSlice = false;    // 조각이 되는 점(값이 0 이상인 유한한 수). 아니면(값 없음 · 0 보다 작음) 범례에만 남고 견본을 비운다
        double dShare = 0;      // 전체(0 보다 큰 값의 합)에 대한 몫(0 … 1)
        double dStart = 0;
        double dSweep = 0;      // 각. 조각이 아니거나 0 인 점은 0
    };

    // 위젯 크기에서 정한 자리(위젯 좌표)
    struct ChartGeometry
    {
        QRectF title;
        QRectF caption;
        QVector<LegendItem> vecLegend;
        QRectF legend;
        QRectF plot;
        AxisScale xScale;
        AxisScale arrYScales[2];  // 왼쪽 · 오른쪽 y 축
        ELabelMode eLabelMode = ELabelMode::HORIZONTAL;
        double dLabelBand = 0;  // x 이름표 칸 높이
        QRectF message;         // NONE 의 글 칸
        HeatmapGeometry heatmap;
        DonutGeometry donut;
    };

private:
    ChartGeometry ComputeGeometry() const;
    QVector<QVector<QRectF>> ComputeMarks(const ChartGeometry& geometry) const;   // 계열 × 점 → 표시 상자
    QStringList ListCategoryLabels() const;
    int GetAxisOf(int series) const;
    double MapX(const ChartGeometry& geometry, double x) const;
    double MapY(const ChartGeometry& geometry, int axis, double y) const;
    double GetSlotWidth(const ChartGeometry& geometry) const;
    QFont MakeFont(int pixelSize, bool bold = false) const;
    QString MakeAxisTitle(const QString& title, const QString& unit) const;
    QString MakePointToolTip(int series, int point) const;
    bool IsStepStart(int series, int point) const;   // 단계 막대에서 그 계열의 값이 있는 첫 막대(시작 값)인지

    void DrawGrid(QPainter& painter, const ChartGeometry& geometry) const;
    void DrawXAxis(QPainter& painter, const ChartGeometry& geometry) const;
    void DrawBars(QPainter& painter, const ChartGeometry& geometry, const QVector<QVector<QRectF>>& marks) const;
    void DrawLines(QPainter& painter, const ChartGeometry& geometry, const QVector<QVector<QRectF>>& marks) const;
    void DrawMarker(QPainter& painter, const QPointF& center, double radius, int series) const;
    void DrawLegend(QPainter& painter, const ChartGeometry& geometry) const;
    void DrawMessage(QPainter& painter, const ChartGeometry& geometry) const;

    // 가운데 글만 쓰는지: NONE, 계열이 없음, 히트맵의 모든 칸이 빔, 도넛에 0 보다 큰 값이 없음
    bool ShowsMessage() const;

    // 히트맵
    void ComputeHeatmapGeometry(ChartGeometry* geometry, double left, double right, double top, double bottom) const;
    QVector<QVector<int>> ComputeHeatmapCells() const;   // 계열(열) × 행 → 그 칸의 점(첫 점). 없으면 -1
    int GetHeatmapRow(const ChartPoint& point, int rows) const;   // 점의 행(범주 자리). 행이 아니면 -1
    QRectF GetHeatmapCellRect(const ChartGeometry& geometry, int row, int column) const;
    QPair<int, int> FindHeatmapCellAt(const ChartGeometry& geometry, const QPointF& pos) const;   // (행, 열). 없으면 (-1, -1)
    QColor GetHeatmapColor(int series, double value) const;   // 그 계열 색 눈금에서 값 자리의 색
    QString MakeHeatmapToolTip(const ChartGeometry& geometry, const QPointF& pos) const;
    void DrawHeatmapLegend(QPainter& painter, const ChartGeometry& geometry) const;
    void DrawHeatmap(QPainter& painter, const ChartGeometry& geometry) const;

    // 도넛
    void ComputeDonutGeometry(ChartGeometry* geometry, double left, double right, double top, double bottom) const;
    QVector<DonutSlice> ComputeDonutSlices(double* total) const;   // 첫 계열의 점마다 조각. total(있으면): 0 보다 큰 값의 합
    int FindDonutSliceAt(const ChartGeometry& geometry, const QPointF& pos) const;   // 그 자리(고리 안) 조각의 점. 없으면 -1
    QPointF GetDonutSliceCenter(const ChartGeometry& geometry, const DonutSlice& slice) const;   // 고리 띠 가운데 반지름 · 조각 가운데 각
    QString MakeDonutValueText(const ChartPoint& point) const;   // 값과 단위. 값이 없으면 "값 없음"
    QString MakeDonutToolTip(int point) const;   // "범주: 값 단위 (전체의 n%)"(값이 곧 몫이면 몫 없이). 조각이 아니면 빈 글
    bool ShowsDonutShare() const;   // 범례 몫 칸 · 설명의 몫을 쓰는지. 값이 곧 몫이면(단위 % · 합 100 ± 1) 쓰지 않는다
    void DrawDonut(QPainter& painter, const ChartGeometry& geometry) const;
    void DrawDonutCenter(QPainter& painter, const ChartGeometry& geometry) const;
    void DrawDonutLegend(QPainter& painter, const ChartGeometry& geometry) const;

private:
    ChartSpec m_spec;
    VisualMarkdownTheme m_theme;
    bool m_bHeaderVisible;   // 제목 · 밑글 줄을 그릴지(SetHeaderVisible)
};
