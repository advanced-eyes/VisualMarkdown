#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/Algorithm/GraphLayout/Common/KnowledgeGraph.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"

#include <QFont>
#include <QPoint>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSet>
#include <QSize>
#include <QString>
#include <QVector>
#include <QWidget>

class QPainter;

// 지식 사이 보기(관계망 · 이웃 · 계보도 · 분류 트리 · 타임라인 · 문서 ↔ 개념)를 그리는 위젯(design/Views.md §7).
// 이웃 보기의 초점 점(bFocus)은 테두리를 두 배로 · 이름을 굵게, 둘레 선(bContext)은 테마의 dContextOpacity 로 흐리게 그린다.
// 그래프와 배치 결과만 받아 그린다. 파일 · 색인을 읽지 않는다(Ocean Qt.md §3: 뷰는 그리기 · 입력 · 배치만).
// 휠로 커서 둘레를 확대하고, 끌어서 옮기고, 두 번 누르면 맞춘다. 점을 누르면 그 점과 이웃을 강조하고 시그널을 낸다.
// 좌표: 위젯 = 배치 × 확대 + 옮김. 점 상자 · 선 · 글자는 위젯 좌표에서 그려 선 굵기와 글자가 너무 작아지지 않게 한다.
class KnowledgeGraphView : public QWidget
{
    Q_OBJECT

public:
    explicit KnowledgeGraphView(QWidget* parent = nullptr);
    ~KnowledgeGraphView() override;

public:
    // 그래프는 점 정보(이름 · 종류 · 연도 · 정의 · 문서 수)와 선 정보(상태 · 출처 · 설명)를 찾는 데 쓴다. 고른 것은 지운다
    void SetGraph(const KnowledgeGraph& graph, const GraphLayoutOutput& layout, EGraphLayoutType type);
    void SetTheme(const VisualMarkdownTheme& theme);
    void FitToView();        // 그림 전체가 위젯(오른쪽 판 왼쪽)에 들어오게 확대와 옮김을 정한다
    void ClearSelection();
    // 밖에서 개념을 고른다: 그 개념 점(또는 연도 없음 목록 줄)을 누른 것과 같이 고르되 시그널을 내지 않는다.
    // 빈 글 · 모르는 id · 문서 id 면 고른 것을 지운다. 이미 고른 id 면 그대로 둔다. 확대 · 옮김은 바꾸지 않는다
    void SelectConcept(const QString& conceptId);
    // 범례 판을 그릴지(기본 참). 거짓이면 판 자리를 맞춤 영역에 돌린다(연도 없음 목록 판은 그대로). SetTheme · SetGraph 는 이 값을 바꾸지 않는다
    void SetLegendVisible(bool visible);

public:
    // 위치로 찾기(위젯 좌표). 보기 앱과 시험이 쓴다
    QString FindConceptAt(const QPoint& pos) const;     // 개념 점(타임라인의 연도 없음 목록 포함). 없으면 빈 글
    QString FindDocumentAt(const QPoint& pos) const;    // 문서 점. 없으면 빈 글
    int FindEdgeAt(const QPoint& pos) const;            // 배치 선 자리(vecEdges). 없으면 -1
    QString GetToolTipAt(const QPoint& pos) const;      // 그 자리의 마우스 올림 설명. 없으면 빈 글
    QString GetEdgeToolTip(int edge) const;             // 배치 선 하나의 설명(선 위에 올렸을 때 보이는 글). 없는 자리면 빈 글
    QPointF MapToWidget(const QPointF& layoutPoint) const;
    QRectF GetNodeRect(int node) const;                 // 배치 점(vecNodes 자리)의 상자. 없는 자리면 빈 사각형
    QPointF GetEdgeMidpoint(int edge) const;            // 그린 선 길이의 가운데. 그리지 않는 선이면 (NaN, NaN)
    QRectF GetLaneLabelRect(int lane) const;            // 갈래 이름을 쓰는 칸(점 상자 왼쪽 여백)
    QRectF GetUnplacedItemRect(int index) const;        // 연도 없음 목록의 줄(vecUnplaced 자리). 보이지 않으면 빈 사각형
    QString GetUnplacedTitle() const;                   // 연도 없음 목록의 제목("연도 없음 (n)"). 목록이 없으면 빈 글
    QRectF GetLegendRect() const;                       // 범례 판. 그릴 것이 없거나 범례 판을 끄면 빈 사각형
    QRectF GetFitRect() const;                          // 맞춤 영역: 위젯에서 여백 · 오른쪽 판 · 갈래 이름 칸 · 연도 머리를 뺀 칸
    QPolygonF GetEdgeLine(int edge) const;              // 그린 선의 꺾은선(위젯 좌표, 히트 테스트도 이 경로를 쓴다). 그리지 않는 선이면 비었다
    QPolygonF GetEdgeArrow(int edge) const;             // 화살촉 세모(첫 점이 촉 끝). 방향 없는 선 · 그리지 않는 선이면 비었다

public:
    QString GetSelectedId() const;                      // 고른 개념 · 문서 id. 없으면 빈 글
    double GetZoom() const;
    bool IsLegendVisible() const;

signals:
    // 사용자가 개념 점(연도 없음 목록 줄 포함) · 문서 점을 눌러(끌기가 아닌 누르기) 그 점을 고른 뒤 낸다.
    // id 는 복사해 싣는다: 받는 쪽이 신호 안에서 SetGraph 를 불러도 뒤에 받는 쪽이 누른 id 를 받는다
    void ConceptActivated(const QString& conceptId);
    void DocumentActivated(const QString& documentId);
    // 사용자가 빈 곳(점 · 연도 없음 목록 줄 · 범례 판 · 목록 판 밖)을 눌러(끌기가 아닌 누르기) 고른 것을 지운 뒤 낸다. 고른 것이 없었어도 낸다.
    // ClearSelection · SelectConcept · SetGraph 로 지울 때는 내지 않는다
    void SelectionCleared();

protected:
    bool event(QEvent* event) override;                 // QEvent::ToolTip: 마우스 올림 설명
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    enum class ELegendRow { HEADER, DOCUMENT, CONCEPT, RELATION, LINK, CONFIRMED, PROPOSED };

    struct LegendRow
    {
        ELegendRow eType = ELegendRow::HEADER;
        QString strText;
        QString strKind;          // 개념 · 관계 종류 코드
        bool bDirected = true;
    };

    // 선 하나의 모양(위젯 좌표): 꺾은선과 화살촉(방향 없는 선은 비었다)
    struct EdgeShape
    {
        QPolygonF line;
        QPolygonF arrow;
    };

private:
    void RebuildScene();
    void BuildLegend();
    void ApplyFit();
    void SelectNode(int node);
    void SelectUnplaced(int index);
    void HandleClick(const QPoint& pos);
    void CancelDrag();   // 누르기 · 끌기 상태를 지우고, 끄는 중이었으면 커서를 되돌린다

    void DrawLanes(QPainter& painter) const;
    void DrawTicks(QPainter& painter) const;
    void DrawEdges(QPainter& painter) const;
    void DrawNodes(QPainter& painter) const;
    void DrawLegend(QPainter& painter) const;
    void DrawUnplaced(QPainter& painter) const;
    void DrawEmptyMessage(QPainter& painter) const;

    QPointF MapToLayout(const QPointF& widgetPoint) const;
    QRectF MapRect(const QRectF& layoutRect) const;
    EdgeShape BuildEdgeShape(int edge) const;
    int FindNodeAt(const QPoint& pos) const;
    int FindUnplacedAt(const QPoint& pos) const;
    bool IsOnPanel(const QPoint& pos) const;
    QRectF GetLaneBandRect(int lane) const;
    QRectF GetTickHeaderRect() const;
    QRectF GetUnplacedPanelRect() const;
    int GetSideWidth() const;
    int GetVisibleUnplacedCount() const;
    QFont MakeFont(int pixelSize, bool bold = false) const;
    int GetNodeFontPx() const;
    QString MakeConceptToolTip(int graphNode) const;
    QString MakeDocumentToolTip(int node) const;

private:
    KnowledgeGraph m_graph;
    GraphLayoutOutput m_layout;
    EGraphLayoutType m_eType;
    VisualMarkdownTheme m_theme;

    // 배치 좌표의 장면
    QVector<QRectF> m_vecNodeBoxes;        // 배치 점 상자(문서는 개념보다 크다)
    QVector<int> m_vecGraphNodes;          // 배치 점 → 그래프 점 번호(문서 · 그래프에 없는 점은 -1)
    QVector<int> m_vecParallelIndex;       // 배치 선 → 같은 두 점 사이 선 가운데 자리
    QVector<int> m_vecParallelCount;       // 배치 선 → 같은 두 점 사이 선 수
    QRectF m_rectLaneArea;                 // 갈래 띠의 점 상자 쪽(이름 칸은 위젯 좌표로 그 왼쪽에 붙는다). 갈래가 없으면 빈 사각형
    QRectF m_rectScene;                    // 맞출 때 담는 범위
    double m_dLaneLabelWidth;              // 갈래 이름 칸 너비(위젯 좌표)

    // 위젯 좌표의 판(범례 · 연도 없음)
    QVector<LegendRow> m_vecLegendRows;
    QSize m_sizeLegend;
    int m_iUnplacedWidth;
    bool m_bLegendVisible;                 // 범례 판을 그릴지(SetLegendVisible)

    double m_dZoom;
    QPointF m_ptOffset;
    bool m_bAutoFit;                       // 사용자가 확대 · 옮기기 전에는 크기가 바뀌면 다시 맞춘다
    QSize m_sizeFitted;

    QString m_strSelectedId;
    int m_iSelectedNode;                   // 고른 배치 점. 연도 없음 목록에서 고르면 -1
    int m_iSelectedUnplaced;
    QSet<int> m_setNeighborNodes;
    QSet<int> m_setSelectedEdges;

    bool m_bPressed;
    bool m_bDragging;
    QPoint m_ptPress;
    QPointF m_ptPressOffset;
};
