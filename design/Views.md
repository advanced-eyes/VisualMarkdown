# 보기 설계(5 · 6단계)

- 작성일: 2026-10-24
- 상태: 끝(2026-10-24, §10 의 1–6). "제안" 으로 표시한 결정은 사용자가 읽으며 고칠 수 있다. 7단계에서 이웃 보기와 히트맵 · 도넛 차트를 더했다(조사 · 설계는 [MoreViews.md](MoreViews.md), 이 문서에는 규칙을 함께 적었다).
- 바탕: [format/Visualization.md](../format/Visualization.md)(보기가 읽는 칸), [examples/showcase](../examples/showcase/)(기대값), [README](../README.md) §5.1(층), Ocean `docs/Qt.md` §3(뷰와 화면 로직을 나눈다).

## 1. 목표와 범위

**목표**: showcase 의 모든 보기를 화면에 그린다. 지식 사이 보기 6가지와 지식 안 차트를 보기 앱으로 열어 본다.

**범위 안**
- 2층 읽기: wiki 폴더를 읽어 색인을 만드는 `KnowledgeIndex`
- 3층 보기 모델(QtCore): 그래프 배치 6가지, 차트 설정
- 4층 위젯(QtWidgets, `VisualMarkdownView` 타깃): `QPainter` 로 그리는 그래프 · 행렬 · 차트 위젯, 테마
- 보기 앱 `VisualMarkdownViewer`: 폴더 열기, 보기 고르기, 모든 보기를 PNG 로 남기는 스냅샷 모드
- 테스트: showcase README 의 기대값을 색인 · 배치 · 차트 모델로 확인, 위젯 그리기 확인

**범위 밖**
- 2층 저장 API(4단계). Ocean 의 `KnowledgeBase` 는 그대로 둔다.
- Ocean 화면에 붙이기(Ocean 의 Presenter, Ocean 테마를 `VisualMarkdownTheme` 으로 바꾸는 mapper). 붙일 때 Ocean 쪽에서 한다.
- AI 제안 관계를 확인 · 고치는 화면, 지식 편집
- 보기 정보 저장(점 위치 · 고른 차트). 배치는 열 때마다 계산한다(Visualization.md §5).
- 여러 문서의 표 행을 모아 비교하기

## 2. 결정

| 항목 | 결정 | 정한 쪽 |
| --- | --- | --- |
| 색인 | 라이브러리에 읽기 전용 `KnowledgeIndex` 를 둔다. Ocean `KnowledgeBase` 와 이름이 겹치지 않게 한다. 4단계에서 `KnowledgeBase` 의 읽기 부분이 이것을 쓴다 | 제안 |
| 배치 | 열 때마다 계산한다. 같은 입력이면 같은 위치가 나온다(결정적). 파일에 쓰지 않는다 | 사용자(Visualization.md §5) |
| 차트 고르기 | Visualization.md §4 의 시작점을 규칙으로 정한다(§6) | 제안 |
| 그리기 | `QPainter`. Qt Charts 는 쓰지 않는다(GPLv3 전용) | 라이선스 |
| 위젯 | 위젯은 그릴 값(보기 모델)만 받고, 파일 · 색인을 직접 읽지 않는다. 뷰는 그리기 · 입력 · 배치만 한다 | Ocean Qt.md §3 |
| 색 | `VisualMarkdownTheme` 구조체로 받는다. 밝은 · 어두운 기본값을 둔다. Ocean 은 자기 테마를 이것으로 바꾸는 mapper 를 둔다(TerminalViewer 의 `TerminalTheme` 와 같은 방식) | 제안 |
| 상호작용 | 휠로 확대, 끌어서 이동, 두 번 누르면 맞춤. 점 위에 올리면 이름 · 종류 · 연도 · 정의, 선 위에 올리면 종류 · 상태 · 설명 · 출처를 보인다. 점을 누르면 이웃을 강조하고 시그널을 낸다 | 제안 |
| 보기 앱 | `VisualMarkdownViewer`. 이 저장소를 따로 빌드할 때만 만든다. `--snapshot <폴더>` 로 모든 보기를 PNG 로 남긴다 | 제안 |

## 3. 구조

```
src/VisualMarkdown/
├── (1층 · 1+층 파일은 그대로)
├── KnowledgeIndex.h/.cpp              2층 읽기: wiki 폴더 → 색인
├── Algorithm/
│   ├── GraphLayout/                   지식 사이 배치(Patterns.md §1, 전략 패턴)
│   │   ├── KnowledgeGraphLayout.h/.cpp        EGraphLayoutType 으로 전략을 고른다
│   │   ├── Common/KnowledgeGraph.h/.cpp       색인 → 그래프(점 · 선 · 덩어리)
│   │   ├── Common/GraphLayoutTypes.h          배치 결과
│   │   └── Strategy/NetworkLayoutCPU · LineageLayoutCPU · TreeLayoutCPU · TimelineLayoutCPU · MatrixLayoutCPU · BipartiteLayoutCPU · EgoLayoutCPU
│   └── ChartBuilder/                  지식 안 차트 설정
│       ├── KnowledgeChartBuilder.h/.cpp
│       └── Common/ChartTypes.h
└── UI/                                4층(VisualMarkdownView 타깃, QtWidgets)
    ├── VisualMarkdownTheme.h/.cpp
    ├── KnowledgeGraphView.h/.cpp      관계망 · 이웃 · 계보도 · 분류 트리 · 타임라인 · 문서 ↔ 개념
    ├── KnowledgeMatrixView.h/.cpp     관계 행렬
    ├── KnowledgeChartView.h/.cpp      차트
    └── ViewCommon.h/.cpp              세 위젯이 함께 쓰는 도움 함수(쓰는 쪽에 내놓는 머리글이 아니다)
tools/VisualMarkdownViewer/            보기 앱(§8). main.cpp 를 뺀 소스는 VisualMarkdownViewerLib 로 묶어 시험이 링크한다
├── VisualMarkdownViewerModel          화면 로직(QtCore): 색인 · 보기 7개의 배치 · 이웃의 초점 · 표마다의 차트 설정
├── VisualMarkdownViewerWindow         창: 나무 · 쪽 · 도구 막대 · 상태 줄
├── VisualMarkdownViewerChartPage · MatrixPage · Palette   차트 쪽(차트 + 원래 표) · 행렬 쪽(스크롤) · 테마 → 창 팔레트
├── VisualMarkdownViewerSnapshot       스냅샷 모드
└── VisualMarkdownViewerOptions        인자 · 끝 코드
tests/view/                            VisualMarkdownViewTests(위젯)
tests/viewer/                          VisualMarkdownViewerTests(보기 앱) · 스냅샷 시험(SnapshotTest.cmake · showcase-snapshot.tsv)
```

- 1–3층(`VisualMarkdown` 타깃)은 QtCore 만 쓴다. `QPointF` · `QRectF` 는 QtCore 다. 글자 크기를 재지 않는다(배치는 점 상자 크기를 정해진 값으로 쓴다. 글자 줄이기는 위젯이 한다).
- 4층(`VisualMarkdownView` 타깃)은 `VisualMarkdown` 과 Qt5 Widgets 를 링크한다. 서브모듈로 붙을 때는 쓰는 쪽이 링크할 때만 빌드된다.
- 위젯 테스트는 QtWidgets 가 필요해 따로 실행 파일(`VisualMarkdownViewTests`)로 둔다. 1–3층 테스트(`VisualMarkdownTests`)는 지금처럼 QtCore 만 쓴다.

## 4. 색인(`KnowledgeIndex`)

규칙은 [Visualization.md §2](../format/Visualization.md#2-색인--파일에서-그래프로)다. 읽기 규칙은 Ocean `KnowledgeBase::Reload` · `RebuildIndex` 와 같다.

```cpp
struct KnowledgeEdge
{
    QString strFromId;
    QString strKind;
    QString strToId;
    ERelationStatus eStatus = ERelationStatus::PROPOSED;   // 하나라도 확인됨이면 확인됨
    QStringList vecDocumentIds;                             // 그 선을 말한 문서
    bool bFromConceptFile = false;
    QStringList vecNotes;                                   // 출처마다의 설명(빈 것은 뺀다)
};

struct KnowledgeDroppedRelation   // 개념 파일이 없어 빠진 관계
{
    QString strFromId;
    QString strKind;
    QString strToId;
    QString strSource;            // 문서 id. 개념 파일이면 그 개념 id
};

class KnowledgeIndex
{
public:
    bool Load(const QString& wikiRoot);   // documents/ · concepts/ 를 읽는다. 읽지 못한 파일은 건너뛰고 경고로 남긴다

    QVector<KnowledgeConcept> ListConcepts(bool includeHidden = false) const;   // 이름 열쇠 순
    std::optional<KnowledgeConcept> FindConcept(const QString& conceptId) const;
    QString FindConceptIdByName(const QString& nameOrAlias) const;
    bool IsHidden(const QString& conceptId) const;
    QVector<KnowledgeEdge> ListEdges() const;
    QVector<KnowledgeDroppedRelation> ListDroppedRelations() const;
    QVector<KnowledgeDocument> ListDocuments() const;                           // learned 늦은 것부터
    std::optional<KnowledgeDocument> FindDocument(const QString& documentId) const;
    QStringList ListDocumentIdsPointingTo(const QString& conceptId) const;     // 그 개념을 가리키는 문서
    QStringList GetWarnings() const;
};
```

- 개념 id 는 파일 이름이다. 머리글 `id` 가 다르면 경고를 남기고 파일 이름을 쓴다.
- 이름은 Ocean 과 겹치지 않게 했다(Ocean 에는 `KnowledgeBase` · `IndexedRelation` · `KnownConcept` 가 있다).

## 5. 그래프 배치

`KnowledgeGraph` 는 색인에서 만든다: 점(보이는 개념: id · 이름 · 종류 · 연도 · 다루는 문서 수 · 이어진 선 수 · 덩어리 번호), 선(`KnowledgeEdge`), 덩어리(큰 것부터, 같으면 이름 열쇠 순).

```cpp
enum class EGraphLayoutType { NETWORK, LINEAGE, TREE, TIMELINE, MATRIX, BIPARTITE, EGO };   // 값은 끝에만 더한다(이웃은 MoreViews.md §6)

struct GraphLayoutInput { const KnowledgeGraph* pGraph = nullptr; QString strFocusId; int iDepth = 2; };   // 초점 · 깊이는 이웃만 본다
struct LayoutNode       { QString strId; QString strLabel; QString strKind; QPointF center; bool bDocument = false; bool bFocus = false; };   // bFocus: 이웃의 초점만
struct LayoutEdge       { int iFrom = -1; int iTo = -1; QString strKind; ERelationStatus eStatus; bool bDirected = true; int iGraphEdge = -1; int iWeight = 1; bool bContext = false; };   // bContext: 이웃의 둘레 선만
struct LayoutTick       { double dPosition = 0; QString strLabel; };          // 타임라인 연도
struct LayoutLane       { QString strLabel; double dTop = 0; double dBottom = 0; };
struct LayoutMatrixCell { int iRow = -1; int iColumn = -1; QStringList vecKinds; };

struct GraphLayoutOutput
{
    QVector<LayoutNode> vecNodes;
    QVector<LayoutEdge> vecEdges;
    QVector<LayoutTick> vecTicks;
    QVector<LayoutLane> vecLanes;
    QStringList vecMatrixOrder;            // 행렬의 행 · 열 순서(개념 id)
    QVector<LayoutMatrixCell> vecCells;
    QStringList vecUnplaced;               // 배치에 들지 못한 점(타임라인의 연도 없는 점)
    QRectF bounds;
};
```

배치 단위는 화면 점 1개(확대 1배)다. 점 상자는 정해진 크기(가로 120 · 세로 36 안팎)로 본다.

| 보기 | 넣는 것 | 배치 |
| --- | --- | --- |
| 관계망 `NETWORK` | 보이는 점 전부, 선 전부 | 덩어리마다 힘 배치(Fruchterman–Reingold). 처음 자리는 id 순으로 원 위에 두고, 반복 횟수를 정해 결정적으로 한다. 점끼리 상자가 겹치지 않게 마지막에 민다. 덩어리 상자는 큰 것부터 세로 열로 쌓는다: 열 높이 상한은 가장 높은 덩어리 상자의 높이이고, 넘치면 오른쪽에 새 열을 연다. 선 없는 점은 덩어리 뒤에 격자로 두되 마지막 열의 남은 높이를 먼저 채운다(덩어리 안의 모양은 바꾸지 않고 옮기기만 한다) |
| 계보도 `LINEAGE` | `improves` 선과 그 양 끝 | 개선 선으로 이어진 묶음 하나가 갈래 하나다. 갈래 안에서 옛것을 왼쪽, 새것을 오른쪽에 두고(개선 깊이 순), 화살표는 새것 → 옛것. 갈래 이름은 가장 옛 점의 이름 |
| 분류 트리 `TREE` | `isA` 선과 그 양 끝 | 뿌리(나가는 종류 선이 없는 점)를 위에, 하위를 아래에 둔다. 잎은 이름 열쇠 순으로 고르게, 부모는 자식들의 가운데. 나무가 여럿이면 큰 것부터 옆으로. 고리가 있으면 이미 놓은 점에서 멈춘다 |
| 타임라인 `TIMELINE` | 연도 있는 보이는 점, 그 사이의 `isA` · `improves` 선 | x 는 연도(해마다 같은 간격, 눈금은 있는 해마다). 갈래는 점이 속한 분류 나무의 뿌리(없으면 개선 묶음, 그것도 없으면 "그 밖")다. 갈래 순서는 점이 많은 것부터, "그 밖" 은 늘 마지막이다. 같은 갈래 · 같은 해의 점은 세로로 쌓는다. 연도 없는 점은 `vecUnplaced` 로 |
| 관계 행렬 `MATRIX` | 보이는 점 전부 | 순서는 덩어리(큰 것부터) 안에서 이름 열쇠 순. 칸: 방향 있는 선은 (from 행, to 열), 방향 없는 선은 두 칸 모두. 한 칸에 종류가 여럿이면 모두 담는다. 대각선은 비운다 |
| 문서 ↔ 개념 `BIPARTITE` | 문서 전부, 문서가 가리키는 개념(개념 파일이 있는 것) | 왼쪽 열에 문서(문서 목록 순), 오른쪽 열에 개념. 개념은 가리키는 문서 자리의 평균(무게 중심) 순으로 놓아 엇갈림을 줄인다. 선 무게는 그 문서가 그 개념을 가리킨 횟수(카드 링크 · 관계 끝 · 표 `concepts`) |
| 이웃 `EGO` | 초점(`strFocusId`. 비었거나 보이는 점이 아니면 기본 초점)에서 방향을 보지 않고 깊이(`iDepth`, 기본 2, 1 아래는 1) 걸음 안의 점, 그 점들 사이의 선 전부(그래프 선 순서) | 초점을 가운데(0, 0)에, k 걸음 점을 k 번째 고리에 둔다(Eades 방사형 나무, [MoreViews.md §6.1](MoreViews.md)). 1 걸음 순서는 초점과 잇는 선의 관계 종류(어휘 순, 둘 이상이면 앞의 종류) → 나가는 선 · 들어오는 선 · 방향 없는 선 → 점 번호. 2 걸음부터는 앞 고리에서 이어진 점 가운데 순서가 가장 앞인 점이 부모이고, 한 부모의 자식은 점 번호 순이다. 쐐기는 잎 수에 비례하고 첫 1 걸음 점의 쐐기 가운데가 12시(−90°), 시계 방향이다. 반지름은 고리마다 하나: 앞 고리 + 120 에서 시작해 같은 고리 · 안쪽 고리의 점 상자와 틈(가로 48 · 세로 32)을 두고 겹치지 않는 가장 작은 값. 출력 점은 초점(`bFocus`) → 1 걸음 → 2 걸음(각 순) … . 점과 그 부모(1 걸음 점은 초점)를 잇는 선은 방향과 상관없이 모두 나무 선이고, 그 밖(같은 고리 점끼리 · 부모가 아닌 앞 고리 점과 · 제 자신으로 가는 선)은 둘레 선(`bContext`)이다 |

- 기본 초점(`KnowledgeGraphLayout::FindDefaultFocus`, 보기 앱도 쓴다): 선이 가장 많은 점, 같으면 다루는 문서가 많은 점, 같으면 점 번호(이름 열쇠) 순으로 앞의 것. 점이 없으면 빈 글이고 배치도 비었다.

showcase 에서 나와야 하는 것(테스트로 확인한다)
- 관계망: 점 31 · 선 36, 덩어리 6개의 상자가 겹치지 않는다, 점 상자끼리 겹치지 않는다. 기획서 덩어리는 연구 덩어리 오른쪽 열에, 선 없는 점 4개는 그 아래 같은 열에 있다. 배치 범위의 가로는 세로의 1.7배 이하다.
- 계보도: 점 8 · 선 5, 갈래 3개(R-CNN 갈래 3점 · YOLO 갈래 3점 · 결제 모듈 갈래 2점). 개선 선마다 from(새것)이 to(옛것)보다 오른쪽.
- 분류 트리: 점 14 · 선 12, 나무 2개(12점 · 2점). 검출기 아래 자식 3(1단계 검출기 · 2단계 검출기 · DETR), 1단계 검출기 아래 5, 2단계 검출기 아래 3, 결제 모듈 아래 1. 자식은 부모보다 아래.
- 타임라인: 놓인 점 16, 연도 없는 점 15. 갈래 3개를 이 순서로: 검출기 9점 · 결제 모듈 1점 · 그 밖 6점(COCO · RPN · RoI 풀링 · 배치 정규화 · Darknet-19 · Focal Loss). 같은 갈래 안에서 해가 늦을수록 오른쪽. DPM 은 없다.
- 관계 행렬: 31 × 31, 채워진 칸 40개(방향 있는 선 31개 중 두 개가 한 칸을 나눠 30칸 + 방향 없는 선 5개 × 2칸). 종류가 둘인 칸은 (결제 모듈 2판, 결제 모듈) 하나.
- 문서 ↔ 개념: 문서 6, 문서마다 선 YOLO 5 · YOLO9000 7 · YOLOv3 5(개념 파일 없는 Darknet-53 은 뺀다) · 2단계 노트 7 · Focal Loss 4 · 결제 기획서 9.
- 이웃: 기본 초점 Faster R-CNN. 깊이 2 는 점 12 · 선 18(나무 선 11 · 둘레 선 7), 깊이 1 은 점 7 · 선 9(나무 선 6 · 둘레 선 3). 허브 1단계 검출기를 초점으로 주면 깊이 2 는 나무 선 13 · 둘레 선 8. 1 걸음 6점 · 2 걸음 5점의 순서 · 부모 · 쐐기 가운데 각과 나무 · 둘레 선은 showcase README §2.2(MoreViews.md §7.3)와 같다. 고리마다 반지름이 같고 바깥 고리가 더 크며, 점 상자끼리 겹치지 않는다(모든 점을 초점으로 깊이 1–3 에서도). 선 없는 COCO 를 초점으로 주면 점 1 · 선 0.

## 6. 차트 설정

```cpp
enum class EChartType { NONE, BAR, STEP_BAR, LINE, SCATTER, HEATMAP, DONUT };   // 값은 끝에만 더한다

struct ChartPoint  { double dX = 0; std::optional<double> optY; double dBase = 0; QString strLabel; };   // dBase: 막대가 시작하는 값
struct ChartSeries { QString strName; QString strUnit; int iAxis = 0; QVector<ChartPoint> vecPoints; };
struct ChartAxis   { QString strTitle; QString strUnit; double dMin = 0; double dMax = 0; QVector<double> vecTicks; QStringList vecCategories; };
struct ChartSpec
{
    EChartType eType = EChartType::NONE;
    QString strTitle;
    QString strCaption;              // condition · where
    ChartAxis xAxis;                 // 범주 · 순서 x 는 vecCategories, 수치 x 는 범위와 눈금
    QVector<ChartAxis> vecYAxes;     // 단위가 다르면 둘(왼쪽 · 오른쪽). 히트맵은 y 열마다 하나(색 눈금), 도넛은 하나(0 – 전체)
    QVector<ChartSeries> vecSeries;  // 도넛은 첫 계열 하나
    QStringList vecWarnings;
};
```

규칙
1. y 열 가운데 수치(quantity)가 하나도 없으면 `NONE` 이고 경고를 단다. 보기 앱은 차트 대신 표를 보인다. 수치가 아닌 y 열은 빼고 경고를 단다.
2. x 열 종류: 범주 → `BAR`(몫 표면 `DONUT`, 규칙 11. 그 밖에 계열이 넷 이상이면 `HEATMAP`, 규칙 10), 순서 + `cumulative` → `STEP_BAR`, 순서 → `LINE`, 수치 → `SCATTER`.
3. y 가 여럿이면 y 마다 계열이다. `series` 열이 있으면 그 값마다 계열을 나눈다(y 가 여럿이면 y × 값). 계열 순서는 처음 나온 순서다.
4. y 단위가 둘이면 y 축 둘(첫 단위 왼쪽 · 둘째 오른쪽). 셋 이상이면 셋째부터는 오른쪽 축에 두고 경고를 단다. 히트맵은 y 열마다 색 눈금이라(규칙 10), 도넛은 축이 하나라(규칙 11) 이 경고가 없다.
5. 빈 칸은 값 없음이다. 선은 그 자리에서 끊고, 막대와 점은 그리지 않는다.
6. 숫자는 `IsQuantityText` 와 같은 꼴로 읽는다(천 단위 쉼표 · 부호 · 지수).
7. 축 범위: 막대 · 단계 막대는 0 을 넣는다. 선 · 산점도는 값 범위의 양쪽에 5% 여유를 두되, 값이 모두 0 이상(또는 모두 0 이하)이면 여유가 0 을 넘지 않는다. 눈금은 1 · 2 · 5 × 10ⁿ 간격으로 5–8개이고, 범위 끝은 바깥 눈금에 맞춘다. 범주 x 축은 −0.5 … 범주 수 − 0.5 이고 눈금이 없다.
8. 단계 막대: 첫 막대는 0 부터, 그다음은 앞 값(빈 칸을 건너뛴 앞 값)에서 이 값까지. 시작 값은 `dBase` 에 담는다. 값이 있는 첫 막대는 시작 값이라 중립 색(회색)으로, 그다음은 늘어남과 줄어듦을 다른 색으로 칠한다(파랑 · 주황, 빨강 · 초록만으로 나누지 않는다). 범례는 시작 · 늘어남 · 줄어듦.
9. 제목은 표 제목, 밑글은 `condition · where`.
10. 히트맵([MoreViews.md §4](MoreViews.md)): 행은 x 범주(위 → 아래), 열은 계열(왼쪽 → 오른쪽). 행 놓기 · 계열은 막대와 같다. 계열 수(수치인 y 열 수 × `series` 값 가운데 빈 칸이 아닌 것의 수, `series` 가 없으면 × 1)가 넷 이상이고 x 가 범주면 추천한다. 색 눈금(`vecYAxes`)은 y 열마다 하나(y 순서, 계열의 `iAxis` 는 그 y 열 자리)이고, 범위는 그 y 열 값의 가장 작은 값 · 가장 큰 값 그대로다(여유 · 0 · 눈금 없음, 값이 없으면 0 · 0). `series` 값으로 나뉜 계열은 한 눈금을 함께 쓴다. 한 계열에 같은 칸(같은 x 글 · 같은 `series` 값)이 둘 이상이면 첫 점만 두고 계열마다 `%1행: 같은 칸(x "%2" · 계열 "%3")이 겹쳐 이 행을 뺀다` 경고를 단다.
11. 도넛([MoreViews.md §5](MoreViews.md)): x 가 범주이고 `series` 열 없이 수치인 y 가 하나, 그 단위가 `%`, 행이 2–6개, 모든 행의 y 칸이 비지 않은 0 이상의 숫자, 합이 100 ± 1 이면 추천한다. 히트맵 규칙(규칙 10)보다 먼저 본다(도넛은 계열이 하나라 둘이 겹치지 않는다). 행 놓기 · 계열은 막대와 같고 조각은 첫 계열의 점(행 순서)이다. 계열이 둘 이상이면 첫 계열만 두고 `도넛은 계열 하나만 그린다: "%1" 만 그리고 %2 은 뺀다` 경고를 단다(%2 는 뺀 계열 이름을 따옴표로 묶어 쉼표로 이은 것). 축(`vecYAxes`)은 하나: 첫 계열의 y 열 이름 · 단위, 0 부터 0 보다 큰 값의 합(전체)까지, 눈금 없음. 값이 없거나 0 보다 작은 점은 조각이 되지 않고(점은 남는다) `%1행: 값이 없어 도넛에 넣지 않는다` · `%1행: 0 보다 작아 도넛에 넣지 않는다` 경고를 단다. 0 인 점은 각이 0 인 조각이다. 단위가 `%` 인데 합이 100 ± 1 밖이면 `단위가 % 인데 합이 %1 이다(전체의 몫은 합에 대한 비율로 그린다)`, 조각(값이 0 이상인 점)이 6개를 넘으면 `조각이 %1개라 읽기 어렵다(막대가 낫다)` 경고를 단다.

showcase 의 표 13개가 README §3 의 시작 차트와 같게 나와야 한다(테스트로 확인한다).

## 7. 위젯

- `VisualMarkdownTheme`: 바탕 · 글자 · 흐린 글자 · 격자, 개념 종류 7개(+ 모르는 종류)의 점 색, 관계 종류 5개의 선 색, AI 제안 선(점선 · 흐리게), 이웃의 둘레 선 불투명도 `dContextOpacity`(0.45), 강조, 차트 계열 색 8개, 단계 막대의 시작 · 늘어남 · 줄어듦, 크기 눈금 `vecSequentialRamp`(히트맵 칸. 밝은 테마는 파랑 100 → 700, 어두운 테마는 같은 색을 거꾸로), 채움 위 글자 `fillTextDark` · `fillTextLight`(히트맵 칸 · 도넛 조각 글. 검정 · 흰색, 채움과 대비가 큰 쪽을 쓴다). 내장 기본값은 `MakeLight()` · `MakeDark()` 다. 기본 생성한 테마는 색이 비어 있으니, 쓰는 쪽(Ocean 의 mapper)은 내장 테마에서 시작해 바꾼다. 글자는 바탕 · 점 색 위에서 4.5:1, 선 · 막대는 바탕 위에서 3:1 이상, 채움 위 글자는 크기 눈금(고르게 나눈 101곳) · 계열 색 위에서 4.5:1 이상의 대비를 갖고, 크기 눈금의 밝기는 한쪽으로 바뀐다(`VisualMarkdownThemeTest` 가 잰다).
- `KnowledgeGraphView`(`QWidget`): 배치 결과와 점 정보(이름 · 종류 · 연도 · 정의)를 받아 그린다. 선(방향 있는 선은 화살표, AI 제안은 점선, 종류마다 색), 점(종류마다 색의 둥근 상자, 문서는 다른 모양), 타임라인 눈금 · 갈래, 연도 없는 점 목록, 범례. 확대 · 이동 · 맞춤, 마우스 올림 설명, 누르면 이웃 강조. 시그널: `ConceptActivated(id)`, `DocumentActivated(id)`.
  - 맞춤은 1배보다 키우지 않는다(배치 단위가 화면 점 1개라, 작은 그래프가 부풀어 갈래 이름과 상자 글자 크기가 어긋나지 않게). 휠로는 더 키울 수 있다.
  - 같은 열에 붙어 쌓인 두 상자(타임라인의 같은 해) 사이 선은 상자 오른쪽으로 휘어 나가는 곡선으로 그려 화살촉이 보이게 한다.
  - 개념 점 설명에는 그 개념이 개선한 것(개선 선의 대상과 `설명`)을 함께 쓴다. B 를 볼 때 A 와 무엇을 고쳤는지가 보이게 하려는 것이다(Visualization.md §3, showcase §2.2 계보도).
  - 이웃 보기의 초점 점(`bFocus`)은 테두리를 그 상태(보통 · 이웃 · 고름) 굵기의 두 배로 그리고 이름을 굵게 쓴다. 둘레 선(`bContext`)은 선 · 화살촉을 테마의 `dContextOpacity`(기본 0.45) 불투명도로 흐리게 그린다. 고른 점의 강조 · 흐림 위에 곱하고, AI 제안 둘레 선은 점선 그대로다. 나무 선이 걸음의 뼈대로 또렷이 보이고, 고리를 가로지르는 긴 둘레 선이 다른 점 상자 밑을 지나도 그 점의 선으로 잘못 읽히지 않게 하려는 것이다. 범례 줄은 없고, 점은 흐리게 하지 않는다. 그 밖(선 · 점 모양 · 확대 · 맞춤 · 마우스 올림 · 누르기 · 시그널)은 관계망과 같다. `bFocus` · `bContext` 가 없는 배치는 전과 같게 그린다.
- `KnowledgeMatrixView`: 행 · 열 이름, 칸 색(종류가 둘이면 칸을 대각선으로 나눈다), 마우스 올림 설명. 칸 크기는 위젯에 맞추되 14px 보다 작게, 배치의 칸 크기(36)보다 크게 하지 않는다.
- `KnowledgeChartView`: `ChartSpec` 을 그린다. 축 · 눈금 · 격자, 막대(여러 계열이면 나란히), 단계 막대, 선(빈 값에서 끊음), 점(계열마다 색), 범례, 제목 · 밑글. `NONE` 이면 가운데에 그 까닭을 쓴다.
  - 히트맵: 색 범례(낮음 → 높음 띠. 색 눈금이 하나면 앞에 y 열 이름, 양 끝에 가장 작은 값 · 가장 큰 값, 여럿이면 낮음 · 높음과 "y 열마다 따로 칠한다") 아래 칸 격자. 범례는 위젯보다 넓어지지 않는다: 띠와 양 끝 값은 줄이지 않고, 띠가 가장 작은 너비(40px)까지 줄어든 뒤에도 넘치면 앞의 y 열 이름을 '…' 로 줄인다(이름은 범례 위 마우스 올림에 다 쓴다). 위에 열 이름, 왼쪽에 행 이름(넘치면 '…', 마우스 올림에 전부), 칸 사이 2px 바탕 틈, 행 높이는 위 한계(48px)까지. 칸 색은 그 y 열 색 눈금에서 값 자리의 색(범위가 0 이면 가운데), 칸 글은 들어갈 때만 채움과 대비가 큰 글자 색으로 쓴다. 빈 칸은 채우지 않고 격자 색 1px 테두리와 흐린 "—". 마우스 올림은 `행 이름 · 열 이름: 값 단위`(빈 칸은 `값 없음`), `FindPointAt` · `GetMarkRect` 는 칸, `GetPlotRect` 는 칸 격자, `GetLegendRect` 는 색 범례다. 모든 칸이 비면 "그릴 값이 없다" 를 쓴다.
  - 도넛: 왼쪽에 고리(그림 칸 위에 붙인다. 바깥 지름은 그림 칸에 맞추되 480px 를 넘지 않고, 안쪽 반지름은 바깥의 60 %), 오른쪽에 범례 목록(고리 가운데 높이). 조각은 12시에서 시계 방향 · 행 순서, 각은 값 / 전체 × 360°, 조각 사이 2px 바탕 틈, 색은 조각 자리의 계열 색이다. 조각 글(몫, 소수 한 자리까지 · 끝의 0 은 뺀다)은 20° 이상이고 글이 조각에 들어갈 때만 채움과 대비가 큰 글자 색으로 쓴다. 가운데에 y 열 이름과 `합 {전체}{단위}`(구멍이 작으면 작은 글자로). 범례 줄은 견본 · 범주 글 · 값과 단위 · 몫이고, 조각이 아닌 점(값 없음 · 0 보다 작음)도 견본을 비워 남긴다. 단위가 `%` 이고 합이 100 ± 1 이면 값이 곧 몫이라 범례의 몫 칸과 마우스 올림의 `(전체의 n%)` 를 쓰지 않는다. 범례 목록 옆 고리가 60px 보다 작아지는 좁은 위젯에서는 범례 목록 없이 고리만 그린다. 마우스 올림은 `범주: 값 단위 (전체의 n%)`, `FindPointAt` 은 각 · 반지름으로 조각(계열 0)을 찾고, `GetMarkRect` 는 조각 가운데 점 둘레의 작은 사각형, `GetPlotRect` 는 고리를 담는 정사각형, `GetLegendRect` 는 범례 목록이다. 0 보다 큰 값이 없으면 "그릴 값이 없다" 를 쓴다.
- 위젯은 Ocean 에 기대지 않는다. 그리기 결과는 `render()` 로 `QImage` 에 남길 수 있다.

## 8. 보기 앱

- `VisualMarkdownViewer [wiki 폴더] [--theme light|dark] [--size 가로x세로]`. 폴더를 주지 않으면 이 저장소의 `examples/showcase/wiki` 를 연다. `--size` 를 주지 않으면 창은 1600×1000 을 화면의 85% 안으로 줄인 크기다.
- 왼쪽 나무: 지식 사이(관계망 · 이웃 · 계보도 · 분류 트리 · 타임라인 · 관계 행렬 · 문서 ↔ 개념. 이웃은 `이웃 · {초점 이름}` 이고 초점이 바뀌면 고친다), 지식 안(문서 → 표, 표 이름 뒤에 차트 종류). 문서를 고르면 문서 쪽(제목 · 표 목록)을 보이고, 표가 없는 문서는 그렇다고 알린다.
- 오른쪽: 고른 보기. 관계 행렬은 가장 작은 칸에서도 창보다 크면 스크롤로 본다(`KnowledgeMatrixView::minimumSizeHint`).
- 지식 안: 차트 아래에 원래 표(읽기 전용, 머리 줄은 열 이름과 단위, 칸은 원문 글 그대로)를 보인다. 차트 종류 상자(추천 · 막대 · 단계 막대 · 선 · 산점도 · 히트맵 · 도넛)로 바꿀 수 있다(`KnowledgeChartBuilder::SetStrategy` · 추천이면 `ResetStrategy`, 표마다 기억한다). 차트로 그리지 않는 표는 제목 · 까닭 한 줄 · 표를 보이고 상자를 끈다.
- 도구: 폴더 열기(읽지 못하면 알리고 앞의 wiki 를 그대로 둔다), 밝음/어둠(창 팔레트도 따른다), 맞춤. 상태 줄: 점 · 선 · 문서 · 빠진 관계 · 경고 수(마우스 올림으로 목록).
- 점을 누르면(`ConceptActivated`) 상태 줄에 그 개념의 이름 · 종류 · 연도를 보인다. 문서 ↔ 개념에서 문서를 누르면(`DocumentActivated`) 나무에서 그 문서를 고른다.
- 이웃의 초점([MoreViews.md §6.2](MoreViews.md)): 보기 앱 모델이 초점 id 를 갖는다. 처음 · 다시 읽은 뒤에는 비었다(기본 초점). 어느 그래프 보기에서든 점을 누르면(`ConceptActivated`) 초점을 그 개념으로 바꾸고, 실제 초점이 바뀌면 이웃 배치를 버리고 나무 글을 고친다. 지금 이웃 보기를 보고 있으면 누르기 처리가 끝난 뒤(다음 이벤트 차례) 바로 다시 배치해 맞춰 보인다. 그래서 이웃 보기 안에서 점을 누르면 그 점으로 옮겨 간다. 초점 점을 다시 누르면 배치는 그대로 두고 고르기만 한다. 깊이는 2 로 둔다(고르는 칸이 없다).
- 배치는 보기를 처음 열 때 계산하고 wiki 를 다시 읽을 때까지 둔다(이웃은 초점이 바뀌어도 다시 계산한다). 차트 설정은 표를 고를 때 만든다.
- `--snapshot <폴더> [--theme light|dark] [--size 가로x세로]`(기본 light · 1600×1000): 화면 없이(offscreen) 창과 같은 쪽 위젯을 그려 PNG 로 남기고 끝난다.
  - 파일: `graph-network.png` · `graph-ego.png`(기본 초점) · `graph-lineage.png` · `graph-tree.png` · `graph-timeline.png` · `graph-matrix.png` · `graph-bipartite.png`, 표마다 `chart-DD-TT.png`(DD 는 문서 목록 순 · TT 는 문서 안 표 순, 1부터 두 자리)
  - `snapshot.tsv`(UTF-8 · LF, 머리 줄 있음): 파일 · 보기(network · ego … bipartite · chart) · 문서 id · 표 제목 · 차트 종류(`EChartType` 이름). 그래프 줄의 뒤 세 칸은 비었다
  - 끝 코드: 0 성공, 2 잘못된 인자(모르는 인자 · 같은 인자 두 번 · `--size` 한 변 1–8192 밖 · 모르는 `--theme`), 3 wiki 를 읽지 못함, 4 파일을 못 씀. 까닭은 표준 오류에 쓴다.
- 빌드: `VISUALMARKDOWN_BUILD_VIEWER`(기본: 이 저장소가 최상위일 때만). 콘솔 실행 파일이다(스냅샷 오류를 콘솔에 쓴다).

## 9. 테스트

- `KnowledgeIndexTest`: showcase README §2.1 의 기대값을 진짜 색인으로 확인한다. `ShowcaseWikiTest` 안에 있던 색인 계산은 `KnowledgeIndex` 로 바꾼다(기대값은 그대로).
- `KnowledgeGraphLayoutTest`: §5 의 showcase 기대값과 배치 조건(겹침 없음 · 방향 · 깊이 · 순서), 같은 입력이면 같은 배치. 이웃: 기본 초점(선 수 → 문서 수 → 이름 열쇠), 초점 고르기(빈 · 없는 · 숨은 id 는 기본 초점, 선 없는 점은 점 하나), 깊이 1 · 2(1 아래는 1), 1 걸음 순서 · 부모 · 쐐기 각(MoreViews.md §7.3, 합성 그래프의 종류 · 방향 · 이름 열쇠 순서), 나무 선 · 둘레 선(기본 초점 11 · 7, 깊이 1, 허브 1단계 검출기 13 · 8, 부모와 자식 사이 선 둘 · 제 자신으로 가는 선), 고리마다 같은 반지름 · 바깥이 더 큼 · 겹침 없음(모든 점을 초점으로 깊이 1–3), 다른 전략은 초점 · 깊이 칸을 보지 않고 둘레 선이 없다.
- `KnowledgeChartBuilderTest`: 표 13개가 README §3 과 같은 차트 · 계열 · 축 · 빈 값을 내는지, 숫자 읽기, 히트맵의 추천 경계 · y 열마다의 색 눈금 · 겹친 칸, 도넛의 추천 경계(단위 % · 합 100 ± 1 · 행 2–6 · 빈 칸 · 0 보다 작은 값 · `series` · y 둘) · 첫 계열 · 축 하나 · 경고.
- `VisualMarkdownViewTests`(offscreen): 보기마다 그려지는지(비지 않음, 두 번 그리면 같음), 위치로 점 · 칸 · 조각 찾기, 도넛 조각의 각 · 조각 글 · 범례 목록, 테마 바꾸기, 색 대비, 설명 글, 상호작용, 이웃 초점 점의 강조(초점 상자 둘레만 바뀌고 테두리 띠 · 글자 모두 바뀐다, 초점 표시가 없으면 관계망과 같은 그림), 둘레 선(같은 선을 나무 선으로 그린 그림보다 흐리고 둘레 선 자리만 다르다, 찾기 · 마우스 올림은 같다). `VISUALMARKDOWN_SNAPSHOT_DIR` 을 정하면 그린 그림을 PNG 로 남긴다.
- `VisualMarkdownViewerTests`(offscreen): 인자 · 끝 코드, 화면 로직(보기 7개의 순서 · 점 · 선 수, 이웃의 초점 · 나무 글 · 배치 버리기 · 다시 읽으면 기본 초점, 표 13개의 차트 종류, 종류 바꾸기 · 되돌리기, 읽기 실패 때 앞 상태 지키기), 창(나무 → 쪽, `DocumentActivated` → 나무, `ConceptActivated` → 이웃 초점(다른 보기에서 · 이웃 보기 안에서), 테마, 차트 종류 상자, 그리지 않는 표, 상태 줄), 스냅샷.
- `VisualMarkdownViewerSnapshotTest`(ctest, `cmake -P`): 실행 파일로 showcase 스냅샷을 찍어 끝 코드 0 · PNG 20개 · 그림 크기를 보고, `snapshot.tsv` 가 `tests/viewer/showcase-snapshot.tsv` 와 바이트로 같은지 본다. 잘못된 인자 · 없는 wiki · 못 쓰는 파일의 끝 코드도 본다.

## 10. 순서

작업마다 구현 에이전트 → 리뷰 에이전트(CLAUDE.md)다. 메인 세션은 스냅샷 PNG 를 직접 보고 확인한다.

1. 색인 `KnowledgeIndex`, `ShowcaseWikiTest` 가 색인을 쓰게 — 끝
2. 그래프 모델 · 배치 6가지 — 끝
3. 차트 설정 — 끝
4. 테마 · 위젯 3개 · 위젯 테스트 — 끝
5. 보기 앱 · 스냅샷 · 문서 반영(README · Visualization.md) — 끝
6. 관계망 덩어리를 세로 열로 쌓기(보기 앱 창에서 점 이름이 잘리는 것을 보고 더했다) — 끝
7. 보기 더하기: 히트맵 → 도넛 → 이웃([MoreViews.md §8](MoreViews.md)) — 끝

## 11. 남은 위험

- 힘 배치는 단순하고 결정적이라 보기 좋지 않을 수 있다. 스냅샷을 보고 다듬는다. 관계망은 덩어리 상자를 세로 열로 쌓아, showcase 를 1600×1000 에 맞추면 확대가 약 0.95 다(한 줄로 늘어놓던 때는 0.67). 두 점 덩어리처럼 낮은 덩어리만 많으면 열 높이 상한이 낮아 여전히 가로로 길다. 1400×900 창에서는 긴 이름 두 개가 '…' 로 잘린다(휠로 키워 본다).
- 차트 값 글은 수를 다시 쓴 것이다. 원문의 `70.0` 은 `70` 으로 보인다(`ChartPoint` 에 원문 글이 없다).
- 점이 수천 개면 관계망 배치가 느리다(반복마다 점 쌍 전부). 지금은 showcase 크기에 맞춘다.
- Ocean 에 붙일 때 Ocean 의 Presenter · 테마 mapper 가 필요하다.
- 차트 축: double 끝(1e308 근처)이나 2^53 를 넘는 한 값은 축이 무한대 · 폭 0 이 될 수 있다. 위젯은 폭 0 인 축을 ±1 로 넓혀 그린다. 실제 표에서는 나오지 않는 값이라 차트 설정은 그대로 둔다.
