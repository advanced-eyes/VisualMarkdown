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
// 행 · 열 이름표를 누르면 그 개념을 고르고 시그널을 낸다. 고른 개념은 행 · 열 띠를 옅게 깔고(칸 색 아래) 이름표를 굵게 쓴다.
class KnowledgeMatrixView : public QWidget
{
    Q_OBJECT

public:
    explicit KnowledgeMatrixView(QWidget* parent = nullptr);
    ~KnowledgeMatrixView() override;

public:
    // 고른 것은 지운다(시그널 없음)
    void SetGraph(const KnowledgeGraph& graph, const GraphLayoutOutput& layout);
    void SetTheme(const VisualMarkdownTheme& theme);   // 고른 것은 그대로 둔다
    // 밖에서 개념을 고른다: 그 개념의 이름표를 누른 것과 같이 고르되(행 · 열 강조) 시그널을 내지 않는다.
    // 빈 글 · 모르는 id 면 고른 것을 지운다. 이미 고른 id 면 그대로 둔다. 스크롤 · 칸 자리는 바꾸지 않는다
    void SelectConcept(const QString& conceptId);

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
    QString GetSelectedId() const;                         // 고른 개념 id. 없으면 빈 글
    double GetCellSize() const;
    // 가장 작은 칸(kMinCellSize)에서 이름표 · 칸 · 범례가 모두 위젯 안에 드는 크기. 이보다 작은 자리(예: 스크롤 영역)에서는 행렬을 스크롤로 본다.
    // 그릴 점이 없으면 비었다(QSize())
    QSize minimumSizeHint() const override;

public:
    static constexpr double kMinCellSize = 14;   // 칸이 이보다 작아지면 읽기 어렵다. 위젯이 작으면 행렬이 위젯 밖으로 나간다
    // 점이 적어도 칸을 배치의 칸(확대 1배)보다 크게 하지 않는다(design/Views.md §5: 배치 단위는 화면 점 1개)
    static constexpr double kMaxCellSize = GraphLayoutSize::kMatrixCellSize;

signals:
    // 사용자가 행 · 열 이름표를 눌러(끌기가 아닌 누르기) 그 개념을 고른 뒤 낸다. 이미 고른 개념을 다시 눌러도 낸다
    void ConceptActivated(const QString& conceptId);
    // 사용자가 빈 곳(이름표 · 칸 · 범례 판 밖)을 눌러(끌기가 아닌 누르기) 고른 것을 지운 뒤 낸다. 고른 것이 없었어도 낸다.
    // SelectConcept · SetGraph 로 지울 때, 칸 · 범례 판 안을 누를 때(고른 것을 그대로 둔다)는 내지 않는다
    void SelectionCleared();

protected:
    bool event(QEvent* event) override;                    // QEvent::ToolTip: 마우스 올림 설명
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    // 두 번째 누르기는 누르기가 아니다(첫 누르기에서 이미 고르고 시그널을 냈다)
    void mouseDoubleClickEvent(QMouseEvent* event) override;

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

    // 이름표가 차지하는 칸(위젯 크기와 상관없다): 행 이름표 너비, 열 이름표 높이, 마지막 열 이름표가 칸 오른쪽으로 넘치는 길이
    struct LabelExtent
    {
        double dRowLabelWidth = 0;
        double dColumnLabelHeight = 0;
        double dOverhang = 0;
    };

private:
    void RebuildCells();
    void SelectNode(int node);                                // 고른다(-1 이면 지운다). 시그널을 내지 않는다
    void HandleClick(const QPoint& pos);
    MatrixGeometry ComputeGeometry() const;
    LabelExtent MeasureLabels() const;
    QVector<int> FindCellEdges(int row, int column) const;   // 그 칸에 떨어지는 배치 선
    QString MakeCellToolTip(int row, int column) const;
    QString MakeLabelToolTip(int node) const;
    QString GetNodeName(int node) const;
    int FindLabelAt(const QPoint& pos) const;                 // 그 자리 이름표의 행 · 열(vecNodes 자리). 이름표 밖이면 -1
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

    QString m_strSelectedId;
    int m_iSelectedNode;                         // 고른 행 · 열(vecNodes 자리). 없으면 -1

    bool m_bPressed;                             // 왼쪽 단추를 눌렀다(놓기 전)
    bool m_bDragging;                            // 누른 뒤 kDragThreshold 이상 움직였다: 누르기가 아니다
    QPoint m_ptPress;
};
