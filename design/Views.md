# 보기 설계(5 · 6단계)

- 작성일: 2026-10-24
- 상태: 끝(2026-10-24, §10 의 1–6). "제안" 으로 표시한 결정은 사용자가 읽으며 고칠 수 있다.
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
│   │   └── Strategy/NetworkLayoutCPU · LineageLayoutCPU · TreeLayoutCPU · TimelineLayoutCPU · MatrixLayoutCPU · BipartiteLayoutCPU
│   └── ChartBuilder/                  지식 안 차트 설정
│       ├── KnowledgeChartBuilder.h/.cpp
│       └── Common/ChartTypes.h
└── UI/                                4층(VisualMarkdownView 타깃, QtWidgets)
    ├── VisualMarkdownTheme.h/.cpp
    ├── KnowledgeGraphView.h/.cpp      관계망 · 계보도 · 분류 트리 · 타임라인 · 문서 ↔ 개념
    ├── KnowledgeMatrixView.h/.cpp     관계 행렬
    ├── KnowledgeChartView.h/.cpp      차트
    └── ViewCommon.h/.cpp              세 위젯이 함께 쓰는 도움 함수(쓰는 쪽에 내놓는 머리글이 아니다)
tools/VisualMarkdownViewer/            보기 앱(§8). main.cpp 를 뺀 소스는 VisualMarkdownViewerLib 로 묶어 시험이 링크한다
├── VisualMarkdownViewerModel          화면 로직(QtCore): 색인 · 보기 6개의 배치 · 표마다의 차트 설정
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
enum class EGraphLayoutType { NETWORK, LINEAGE, TREE, TIMELINE, MATRIX, BIPARTITE };

struct LayoutNode       { QString strId; QString strLabel; QString strKind; QPointF center; bool bDocument = false; };
struct LayoutEdge       { int iFrom = -1; int iTo = -1; QString strKind; ERelationStatus eStatus; bool bDirected = true; int iGraphEdge = -1; int iWeight = 1; };
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

showcase 에서 나와야 하는 것(테스트로 확인한다)
- 관계망: 점 31 · 선 36, 덩어리 6개의 상자가 겹치지 않는다, 점 상자끼리 겹치지 않는다. 기획서 덩어리는 연구 덩어리 오른쪽 열에, 선 없는 점 4개는 그 아래 같은 열에 있다. 배치 범위의 가로는 세로의 1.7배 이하다.
- 계보도: 점 8 · 선 5, 갈래 3개(R-CNN 갈래 3점 · YOLO 갈래 3점 · 결제 모듈 갈래 2점). 개선 선마다 from(새것)이 to(옛것)보다 오른쪽.
- 분류 트리: 점 14 · 선 12, 나무 2개(12점 · 2점). 검출기 아래 자식 3(1단계 검출기 · 2단계 검출기 · DETR), 1단계 검출기 아래 5, 2단계 검출기 아래 3, 결제 모듈 아래 1. 자식은 부모보다 아래.
- 타임라인: 놓인 점 16, 연도 없는 점 15. 갈래 3개를 이 순서로: 검출기 9점 · 결제 모듈 1점 · 그 밖 6점(COCO · RPN · RoI 풀링 · 배치 정규화 · Darknet-19 · Focal Loss). 같은 갈래 안에서 해가 늦을수록 오른쪽. DPM 은 없다.
- 관계 행렬: 31 × 31, 채워진 칸 40개(방향 있는 선 31개 중 두 개가 한 칸을 나눠 30칸 + 방향 없는 선 5개 × 2칸). 종류가 둘인 칸은 (결제 모듈 2판, 결제 모듈) 하나.
- 문서 ↔ 개념: 문서 6, 문서마다 선 YOLO 5 · YOLO9000 7 · YOLOv3 5(개념 파일 없는 Darknet-53 은 뺀다) · 2단계 노트 7 · Focal Loss 4 · 결제 기획서 9.

## 6. 차트 설정

```cpp
enum class EChartType { NONE, BAR, STEP_BAR, LINE, SCATTER };

struct ChartPoint  { double dX = 0; std::optional<double> optY; double dBase = 0; QString strLabel; };   // dBase: 막대가 시작하는 값
struct ChartSeries { QString strName; QString strUnit; int iAxis = 0; QVector<ChartPoint> vecPoints; };
struct ChartAxis   { QString strTitle; QString strUnit; double dMin = 0; double dMax = 0; QVector<double> vecTicks; QStringList vecCategories; };
struct ChartSpec
{
    EChartType eType = EChartType::NONE;
    QString strTitle;
    QString strCaption;              // condition · where
    ChartAxis xAxis;                 // 범주 · 순서 x 는 vecCategories, 수치 x 는 범위와 눈금
    QVector<ChartAxis> vecYAxes;     // 단위가 다르면 둘(왼쪽 · 오른쪽)
    QVector<ChartSeries> vecSeries;
    QStringList vecWarnings;
};
```

규칙
1. y 열 가운데 수치(quantity)가 하나도 없으면 `NONE` 이고 경고를 단다. 보기 앱은 차트 대신 표를 보인다. 수치가 아닌 y 열은 빼고 경고를 단다.
2. x 열 종류: 범주 → `BAR`, 순서 + `cumulative` → `STEP_BAR`, 순서 → `LINE`, 수치 → `SCATTER`.
3. y 가 여럿이면 y 마다 계열이다. `series` 열이 있으면 그 값마다 계열을 나눈다(y 가 여럿이면 y × 값). 계열 순서는 처음 나온 순서다.
4. y 단위가 둘이면 y 축 둘(첫 단위 왼쪽 · 둘째 오른쪽). 셋 이상이면 셋째부터는 오른쪽 축에 두고 경고를 단다.
5. 빈 칸은 값 없음이다. 선은 그 자리에서 끊고, 막대와 점은 그리지 않는다.
6. 숫자는 `IsQuantityText` 와 같은 꼴로 읽는다(천 단위 쉼표 · 부호 · 지수).
7. 축 범위: 막대 · 단계 막대는 0 을 넣는다. 선 · 산점도는 값 범위의 양쪽에 5% 여유를 두되, 값이 모두 0 이상(또는 모두 0 이하)이면 여유가 0 을 넘지 않는다. 눈금은 1 · 2 · 5 × 10ⁿ 간격으로 5–8개이고, 범위 끝은 바깥 눈금에 맞춘다. 범주 x 축은 −0.5 … 범주 수 − 0.5 이고 눈금이 없다.
8. 단계 막대: 첫 막대는 0 부터, 그다음은 앞 값(빈 칸을 건너뛴 앞 값)에서 이 값까지. 시작 값은 `dBase` 에 담는다. 값이 있는 첫 막대는 시작 값이라 중립 색(회색)으로, 그다음은 늘어남과 줄어듦을 다른 색으로 칠한다(파랑 · 주황, 빨강 · 초록만으로 나누지 않는다). 범례는 시작 · 늘어남 · 줄어듦.
9. 제목은 표 제목, 밑글은 `condition · where`.

showcase 의 표 12개가 README §3 의 시작 차트와 같게 나와야 한다(테스트로 확인한다).

## 7. 위젯

- `VisualMarkdownTheme`: 바탕 · 글자 · 흐린 글자 · 격자, 개념 종류 7개(+ 모르는 종류)의 점 색, 관계 종류 5개의 선 색, AI 제안 선(점선 · 흐리게), 강조, 차트 계열 색 8개, 단계 막대의 시작 · 늘어남 · 줄어듦. 내장 기본값은 `MakeLight()` · `MakeDark()` 다. 기본 생성한 테마는 색이 비어 있으니, 쓰는 쪽(Ocean 의 mapper)은 내장 테마에서 시작해 바꾼다. 글자는 바탕 · 점 색 위에서 4.5:1, 선 · 막대는 바탕 위에서 3:1 이상의 대비를 갖는다(`VisualMarkdownThemeTest` 가 잰다).
- `KnowledgeGraphView`(`QWidget`): 배치 결과와 점 정보(이름 · 종류 · 연도 · 정의)를 받아 그린다. 선(방향 있는 선은 화살표, AI 제안은 점선, 종류마다 색), 점(종류마다 색의 둥근 상자, 문서는 다른 모양), 타임라인 눈금 · 갈래, 연도 없는 점 목록, 범례. 확대 · 이동 · 맞춤, 마우스 올림 설명, 누르면 이웃 강조. 시그널: `ConceptActivated(id)`, `DocumentActivated(id)`.
  - 맞춤은 1배보다 키우지 않는다(배치 단위가 화면 점 1개라, 작은 그래프가 부풀어 갈래 이름과 상자 글자 크기가 어긋나지 않게). 휠로는 더 키울 수 있다.
  - 같은 열에 붙어 쌓인 두 상자(타임라인의 같은 해) 사이 선은 상자 오른쪽으로 휘어 나가는 곡선으로 그려 화살촉이 보이게 한다.
  - 개념 점 설명에는 그 개념이 개선한 것(개선 선의 대상과 `설명`)을 함께 쓴다. B 를 볼 때 A 와 무엇을 고쳤는지가 보이게 하려는 것이다(Visualization.md §3, showcase §2.2 계보도).
- `KnowledgeMatrixView`: 행 · 열 이름, 칸 색(종류가 둘이면 칸을 대각선으로 나눈다), 마우스 올림 설명. 칸 크기는 위젯에 맞추되 14px 보다 작게, 배치의 칸 크기(36)보다 크게 하지 않는다.
- `KnowledgeChartView`: `ChartSpec` 을 그린다. 축 · 눈금 · 격자, 막대(여러 계열이면 나란히), 단계 막대, 선(빈 값에서 끊음), 점(계열마다 색), 범례, 제목 · 밑글. `NONE` 이면 가운데에 그 까닭을 쓴다.
- 위젯은 Ocean 에 기대지 않는다. 그리기 결과는 `render()` 로 `QImage` 에 남길 수 있다.

## 8. 보기 앱

- `VisualMarkdownViewer [wiki 폴더] [--theme light|dark] [--size 가로x세로]`. 폴더를 주지 않으면 이 저장소의 `examples/showcase/wiki` 를 연다. `--size` 를 주지 않으면 창은 1600×1000 을 화면의 85% 안으로 줄인 크기다.
- 왼쪽 나무: 지식 사이(관계망 · 계보도 · 분류 트리 · 타임라인 · 관계 행렬 · 문서 ↔ 개념), 지식 안(문서 → 표, 표 이름 뒤에 차트 종류). 문서를 고르면 문서 쪽(제목 · 표 목록)을 보이고, 표가 없는 문서는 그렇다고 알린다.
- 오른쪽: 고른 보기. 관계 행렬은 가장 작은 칸에서도 창보다 크면 스크롤로 본다(`KnowledgeMatrixView::minimumSizeHint`).
- 지식 안: 차트 아래에 원래 표(읽기 전용, 머리 줄은 열 이름과 단위, 칸은 원문 글 그대로)를 보인다. 차트 종류 상자(추천 · 막대 · 단계 막대 · 선 · 산점도)로 바꿀 수 있다(`KnowledgeChartBuilder::SetStrategy` · 추천이면 `ResetStrategy`, 표마다 기억한다). 차트로 그리지 않는 표는 제목 · 까닭 한 줄 · 표를 보이고 상자를 끈다.
- 도구: 폴더 열기(읽지 못하면 알리고 앞의 wiki 를 그대로 둔다), 밝음/어둠(창 팔레트도 따른다), 맞춤. 상태 줄: 점 · 선 · 문서 · 빠진 관계 · 경고 수(마우스 올림으로 목록).
- 점을 누르면(`ConceptActivated`) 상태 줄에 그 개념의 이름 · 종류 · 연도를 보인다. 문서 ↔ 개념에서 문서를 누르면(`DocumentActivated`) 나무에서 그 문서를 고른다.
- 배치는 보기를 처음 열 때 계산하고 wiki 를 다시 읽을 때까지 둔다. 차트 설정은 표를 고를 때 만든다.
- `--snapshot <폴더> [--theme light|dark] [--size 가로x세로]`(기본 light · 1600×1000): 화면 없이(offscreen) 창과 같은 쪽 위젯을 그려 PNG 로 남기고 끝난다.
  - 파일: `graph-network.png` · `graph-lineage.png` · `graph-tree.png` · `graph-timeline.png` · `graph-matrix.png` · `graph-bipartite.png`, 표마다 `chart-DD-TT.png`(DD 는 문서 목록 순 · TT 는 문서 안 표 순, 1부터 두 자리)
  - `snapshot.tsv`(UTF-8 · LF, 머리 줄 있음): 파일 · 보기(network … bipartite · chart) · 문서 id · 표 제목 · 차트 종류(`EChartType` 이름)
  - 끝 코드: 0 성공, 2 잘못된 인자(모르는 인자 · 같은 인자 두 번 · `--size` 한 변 1–8192 밖 · 모르는 `--theme`), 3 wiki 를 읽지 못함, 4 파일을 못 씀. 까닭은 표준 오류에 쓴다.
- 빌드: `VISUALMARKDOWN_BUILD_VIEWER`(기본: 이 저장소가 최상위일 때만). 콘솔 실행 파일이다(스냅샷 오류를 콘솔에 쓴다).

## 9. 테스트

- `KnowledgeIndexTest`: showcase README §2.1 의 기대값을 진짜 색인으로 확인한다. `ShowcaseWikiTest` 안에 있던 색인 계산은 `KnowledgeIndex` 로 바꾼다(기대값은 그대로).
- `KnowledgeGraphLayoutTest`: §5 의 showcase 기대값과 배치 조건(겹침 없음 · 방향 · 깊이 · 순서), 같은 입력이면 같은 배치.
- `KnowledgeChartBuilderTest`: 표 12개가 README §3 과 같은 차트 · 계열 · 축 · 빈 값을 내는지, 숫자 읽기.
- `VisualMarkdownViewTests`(offscreen): 보기마다 그려지는지(비지 않음, 두 번 그리면 같음), 위치로 점 · 칸 찾기, 테마 바꾸기, 색 대비, 설명 글, 상호작용. `VISUALMARKDOWN_SNAPSHOT_DIR` 을 정하면 그린 그림을 PNG 로 남긴다.
- `VisualMarkdownViewerTests`(offscreen): 인자 · 끝 코드, 화면 로직(보기 6개의 점 · 선 수, 표 12개의 차트 종류, 종류 바꾸기 · 되돌리기, 읽기 실패 때 앞 상태 지키기), 창(나무 → 쪽, `DocumentActivated` → 나무, 테마, 차트 종류 상자, 그리지 않는 표, 상태 줄), 스냅샷.
- `VisualMarkdownViewerSnapshotTest`(ctest, `cmake -P`): 실행 파일로 showcase 스냅샷을 찍어 끝 코드 0 · PNG 18개 · 그림 크기를 보고, `snapshot.tsv` 가 `tests/viewer/showcase-snapshot.tsv` 와 바이트로 같은지 본다. 잘못된 인자 · 없는 wiki · 못 쓰는 파일의 끝 코드도 본다.

## 10. 순서

작업마다 구현 에이전트 → 리뷰 에이전트(CLAUDE.md)다. 메인 세션은 스냅샷 PNG 를 직접 보고 확인한다.

1. 색인 `KnowledgeIndex`, `ShowcaseWikiTest` 가 색인을 쓰게 — 끝
2. 그래프 모델 · 배치 6가지 — 끝
3. 차트 설정 — 끝
4. 테마 · 위젯 3개 · 위젯 테스트 — 끝
5. 보기 앱 · 스냅샷 · 문서 반영(README · Visualization.md) — 끝
6. 관계망 덩어리를 세로 열로 쌓기(보기 앱 창에서 점 이름이 잘리는 것을 보고 더했다) — 끝

## 11. 남은 위험

- 힘 배치는 단순하고 결정적이라 보기 좋지 않을 수 있다. 스냅샷을 보고 다듬는다. 관계망은 덩어리 상자를 세로 열로 쌓아, showcase 를 1600×1000 에 맞추면 확대가 약 0.95 다(한 줄로 늘어놓던 때는 0.67). 두 점 덩어리처럼 낮은 덩어리만 많으면 열 높이 상한이 낮아 여전히 가로로 길다. 1400×900 창에서는 긴 이름 두 개가 '…' 로 잘린다(휠로 키워 본다).
- 차트 값 글은 수를 다시 쓴 것이다. 원문의 `70.0` 은 `70` 으로 보인다(`ChartPoint` 에 원문 글이 없다).
- 점이 수천 개면 관계망 배치가 느리다(반복마다 점 쌍 전부). 지금은 showcase 크기에 맞춘다.
- Ocean 에 붙일 때 Ocean 의 Presenter · 테마 mapper 가 필요하다.
- 차트 축: double 끝(1e308 근처)이나 2^53 를 넘는 한 값은 축이 무한대 · 폭 0 이 될 수 있다. 위젯은 폭 0 인 축을 ±1 로 넓혀 그린다. 실제 표에서는 나오지 않는 값이라 차트 설정은 그대로 둔다.
