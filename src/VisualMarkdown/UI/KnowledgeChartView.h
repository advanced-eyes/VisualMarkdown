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
// NONE 이면 가운데에 그리지 않는 까닭(경고)을 쓴다. 범위가 0 이거나 유한하지 않은 축은 그 값 둘레 ±1 로 넓혀 그린다.
class KnowledgeChartView : public QWidget
{
    Q_OBJECT

public:
    explicit KnowledgeChartView(QWidget* parent = nullptr);
    ~KnowledgeChartView() override;

public:
    void SetSpec(const ChartSpec& spec);
    void SetTheme(const VisualMarkdownTheme& theme);

public:
    // 위치로 찾기(위젯 좌표). 보기 앱과 시험이 쓴다
    QPair<int, int> FindPointAt(const QPoint& pos) const;   // (계열, 점). 없으면 (-1, -1)
    QString GetToolTipAt(const QPoint& pos) const;          // 계열 이름 · x 이름표 · 값과 단위. 없으면 빈 글
    QRectF GetMarkRect(int series, int point) const;        // 막대 또는 점 표시. 값이 없거나 그리지 않으면 빈 사각형
    QRectF GetPlotRect() const;                             // 그림 칸. NONE 이면 빈 사각형
    QRectF GetLegendRect() const;                           // 범례. 계열이 하나뿐인 차트(단계 막대 빼고)는 빈 사각형
    QString GetMessageText() const;                         // NONE 일 때 가운데에 쓰는 글. 차트를 그리면 빈 글

public:
    const ChartSpec& GetSpec() const;

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

private:
    ChartSpec m_spec;
    VisualMarkdownTheme m_theme;
};
