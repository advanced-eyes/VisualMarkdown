#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"

#include <QFont>
#include <QPair>
#include <QPoint>
#include <QPolygonF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QFontMetricsF;
class QPainter;

// 관계 행렬(MATRIX 배치)을 그리는 위젯(design/Views.md §7). 행 · 열은 vecMatrixOrder 순이고, 칸은 그 칸에 떨어지는 관계 종류의 색이다.
// 종류가 둘이면 칸을 대각선으로 나누고, 셋 이상이면 같은 너비의 띠로 나눈다. AI 제안 관계는 옅게 칠한다.
// 위젯 크기에 맞춰 칸 크기를 정하되, 읽을 수 있는 가장 작은 칸보다 작게 하지 않는다.
class KnowledgeMatrixView : public QWidget
{
    Q_OBJECT

public:
    explicit KnowledgeMatrixView(QWidget* parent = nullptr);
    ~KnowledgeMatrixView() override;

public:
    void SetGraph(const KnowledgeGraph& graph, const GraphLayoutOutput& layout);
    void SetTheme(const VisualMarkdownTheme& theme);

public:
    // 위치로 찾기(위젯 좌표). 보기 앱과 시험이 쓴다
    QPair<int, int> FindCellAt(const QPoint& pos) const;   // (행, 열). 칸 밖이면 (-1, -1)
    QString GetToolTipAt(const QPoint& pos) const;         // 채워진 칸: 행 → 열, 종류 · 상태. 행 · 열 이름표: 개념 이름
    QRectF GetCellRect(int row, int column) const;         // 없는 칸이면 빈 사각형
    QRectF GetGridRect() const;
    QRectF GetLegendRect() const;                          // 그릴 종류가 없으면 빈 사각형
    // 열 이름표의 맞힘 칸: 기울인 네모(이름표 시작 위 · 끝 위 · 끝 아래 · 시작 아래, 위젯 좌표). 이름 글 너비에 작은 여유를 더한 길이다.
    // 없는 열이면 비었다
    QPolygonF GetColumnLabelArea(int column) const;

public:
    double GetCellSize() const;

public:
    static constexpr double kMinCellSize = 14;   // 칸이 이보다 작아지면 읽기 어렵다. 위젯이 작으면 행렬이 위젯 밖으로 나간다
    // 점이 적어도 칸을 배치의 칸(확대 1배)보다 크게 하지 않는다(design/Views.md §5: 배치 단위는 화면 점 1개)
    static constexpr double kMaxCellSize = GraphLayoutSize::kMatrixCellSize;

protected:
    bool event(QEvent* event) override;                    // QEvent::ToolTip: 마우스 올림 설명
    void paintEvent(QPaintEvent* event) override;

private:
    // 칸 하나에 그리는 종류 · 상태(종류마다 확인됨이 하나라도 있으면 확인됨)
    struct CellKind
    {
        QString strKind;
        bool bConfirmed = false;
    };

    // 위젯 크기에서 정한 자리(위젯 좌표)
    struct MatrixGeometry
    {
        double dCell = 0;
        QRectF grid;
        double dRowLabelWidth = 0;
        QRectF legend;
    };

private:
    void RebuildCells();
    MatrixGeometry ComputeGeometry() const;
    QVector<int> FindCellEdges(int row, int column) const;   // 그 칸에 떨어지는 배치 선
    QString MakeCellToolTip(int row, int column) const;
    QString MakeLabelToolTip(int node) const;
    QString GetNodeName(int node) const;
    int GetLabelFontPx(double cell) const;                    // 행 · 열 이름표 글자 크기(칸 크기에 맞춘다)
    QPolygonF MakeColumnLabelArea(const MatrixGeometry& geometry, const QFontMetricsF& metrics, int column) const;
    QStringList ListLegendKinds() const;
    QFont MakeFont(int pixelSize, bool bold = false) const;
    void DrawCell(QPainter& painter, const QRectF& rect, const QVector<CellKind>& kinds) const;
    void DrawLegend(QPainter& painter, const QRectF& rect) const;

private:
    KnowledgeGraph m_graph;
    GraphLayoutOutput m_layout;
    VisualMarkdownTheme m_theme;
    QVector<QVector<CellKind>> m_vecCellKinds;   // 행 × 열 → 그 칸의 종류(빈 칸은 비었다)
    QVector<int> m_vecBlockEnds;                 // 덩어리 경계: 그 앞에서 덩어리가 끝나는 행 · 열 자리
    QSize m_sizeLegend;
};
