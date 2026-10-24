#pragma once

#include "VisualMarkdown/Interface/IStrategy.h"
#include "VisualMarkdown/KnowledgeTypes.h"

#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>

class KnowledgeGraph;

// 지식 사이 보기 6가지(design/Views.md §5)
enum class EGraphLayoutType { NETWORK, LINEAGE, TREE, TIMELINE, MATRIX, BIPARTITE };

struct LayoutNode
{
    QString strId;          // 개념 id. 문서면 문서 id
    QString strLabel;       // 개념 이름. 문서면 문서 제목
    QString strKind;        // 개념 종류 코드. 문서는 비운다
    QPointF center;         // 점 상자의 가운데
    bool bDocument = false;
};

struct LayoutEdge
{
    int iFrom = -1;         // vecNodes 자리
    int iTo = -1;
    QString strKind;        // 관계 종류 코드. 문서 ↔ 개념 선은 비운다
    ERelationStatus eStatus = ERelationStatus::CONFIRMED;   // 같은 입력이면 같은 출력이 되도록 기본값을 둔다
    bool bDirected = true;
    int iGraphEdge = -1;    // KnowledgeGraph::GetEdges 자리. 문서 ↔ 개념 선은 -1
    int iWeight = 1;        // 문서 ↔ 개념 선의 무게(가리킨 횟수)
};

struct LayoutTick
{
    double dPosition = 0;   // 타임라인 연도의 x
    QString strLabel;
};

struct LayoutLane
{
    QString strLabel;
    double dTop = 0;
    double dBottom = 0;
};

struct LayoutMatrixCell
{
    int iRow = -1;          // vecMatrixOrder 자리
    int iColumn = -1;
    QStringList vecKinds;   // 이 칸에 떨어지는 관계 종류(어휘 순)
};

struct GraphLayoutOutput
{
    QVector<LayoutNode> vecNodes;
    QVector<LayoutEdge> vecEdges;
    QVector<LayoutTick> vecTicks;
    QVector<LayoutLane> vecLanes;
    QStringList vecMatrixOrder;            // 행렬의 행 · 열 순서(개념 id)
    QVector<LayoutMatrixCell> vecCells;
    QStringList vecUnplaced;               // 배치에 들지 못한 점(타임라인의 연도 없는 점)
    QRectF bounds;                         // 점 상자 · 갈래를 모두 담는 사각형. 점이 없으면 빈 사각형
};

struct GraphLayoutInput
{
    const KnowledgeGraph* pGraph = nullptr;   // 배치할 그래프(맡지 않는다)
};

using IGraphLayoutStrategy = VisualMarkdown::IStrategy<GraphLayoutInput, GraphLayoutOutput, EGraphLayoutType>;

// 배치 크기. 단위는 화면 점 1개(확대 1배)다. 글자 크기는 재지 않고 점 상자를 이 크기로 본다(design/Views.md §3 · §5).
namespace GraphLayoutSize
{
constexpr double kNodeWidth = 120;
constexpr double kNodeHeight = 36;
constexpr double kNodeGapX = 24;                 // 점 상자 사이 가로 틈
constexpr double kNodeGapY = 16;                 // 점 상자 사이 세로 틈(쌓을 때)
constexpr double kComponentGap = 80;             // 관계망: 덩어리 상자 사이 틈(힘 배치 값은 NetworkLayoutCPU.cpp)
constexpr double kLineageColumnGap = 64;         // 계보도: 개선 깊이 열 사이 틈
constexpr double kTreeLevelGap = 60;             // 분류 트리: 깊이 사이 틈
constexpr double kTreeGap = 80;                  // 분류 트리: 나무 사이 틈(점 틈에 더한다)
constexpr double kTimelineYearGap = 160;         // 타임라인: 한 해의 가로 간격
constexpr double kLanePadding = 12;              // 계보도 · 타임라인: 갈래 띠 안쪽 위아래 여백
constexpr double kLaneGap = 16;                  // 계보도 · 타임라인: 갈래 띠 사이 틈
constexpr double kMatrixCellSize = 36;           // 관계 행렬: 칸 한 변
constexpr double kBipartiteColumnGap = 240;      // 문서 ↔ 개념: 두 열 상자 사이 틈
}
