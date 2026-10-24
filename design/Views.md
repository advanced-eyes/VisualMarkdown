# 보기 설계(5 · 6단계)

- 작성일: 2026-10-24
- 상태: 진행(2026-10-24 사용자 "진행해줘"). "제안" 으로 표시한 결정은 사용자가 읽으며 고칠 수 있다.
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
    └── KnowledgeChartView.h/.cpp      차트
tools/VisualMarkdownViewer/            보기 앱
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
| 관계망 `NETWORK` | 보이는 점 전부, 선 전부 | 덩어리마다 힘 배치(Fruchterman–Reingold). 처음 자리는 id 순으로 원 위에 두고, 반복 횟수를 정해 결정적으로 한다. 덩어리는 큰 것부터 왼쪽에서 오른쪽으로 놓고, 선 없는 점은 끝에 한 줄로 둔다. 점끼리 상자가 겹치지 않게 마지막에 민다 |
| 계보도 `LINEAGE` | `improves` 선과 그 양 끝 | 개선 선으로 이어진 묶음 하나가 갈래 하나다. 갈래 안에서 옛것을 왼쪽, 새것을 오른쪽에 두고(개선 깊이 순), 화살표는 새것 → 옛것. 갈래 이름은 가장 옛 점의 이름 |
| 분류 트리 `TREE` | `isA` 선과 그 양 끝 | 뿌리(나가는 종류 선이 없는 점)를 위에, 하위를 아래에 둔다. 잎은 이름 열쇠 순으로 고르게, 부모는 자식들의 가운데. 나무가 여럿이면 큰 것부터 옆으로. 고리가 있으면 이미 놓은 점에서 멈춘다 |
| 타임라인 `TIMELINE` | 연도 있는 보이는 점, 그 사이의 `isA` · `improves` 선 | x 는 연도(해마다 같은 간격, 눈금은 있는 해마다). 갈래는 점이 속한 분류 나무의 뿌리(없으면 개선 묶음, 그것도 없으면 "그 밖")다. 갈래 순서는 점이 많은 것부터, "그 밖" 은 늘 마지막이다. 같은 갈래 · 같은 해의 점은 세로로 쌓는다. 연도 없는 점은 `vecUnplaced` 로 |
| 관계 행렬 `MATRIX` | 보이는 점 전부 | 순서는 덩어리(큰 것부터) 안에서 이름 열쇠 순. 칸: 방향 있는 선은 (from 행, to 열), 방향 없는 선은 두 칸 모두. 한 칸에 종류가 여럿이면 모두 담는다. 대각선은 비운다 |
| 문서 ↔ 개념 `BIPARTITE` | 문서 전부, 문서가 가리키는 개념(개념 파일이 있는 것) | 왼쪽 열에 문서(문서 목록 순), 오른쪽 열에 개념. 개념은 가리키는 문서 자리의 평균(무게 중심) 순으로 놓아 엇갈림을 줄인다. 선 무게는 그 문서가 그 개념을 가리킨 횟수(카드 링크 · 관계 끝 · 표 `concepts`) |

showcase 에서 나와야 하는 것(테스트로 확인한다)
- 관계망: 점 31 · 선 36, 덩어리 6개의 상자가 겹치지 않는다, 점 상자끼리 겹치지 않는다.
- 계보도: 점 8 · 선 5, 갈래 3개(R-CNN 갈래 3점 · YOLO 갈래 3점 · 결제 모듈 갈래 2점). 개선 선마다 from(새것)이 to(옛것)보다 오른쪽.
- 분류 트리: 점 14 · 선 12, 나무 2개(12점 · 2점). 검출기 아래 자식 3(1단계 검출기 · 2단계 검출기 · DETR), 1단계 검출기 아래 5, 2단계 검출기 아래 3, 결제 모듈 아래 1. 자식은 부모보다 아래.
- 타임라인: 놓인 점 16, 연도 없는 점 15. 갈래 3개를 이 순서로: 검출기 9점 · 결제 모듈 1점 · 그 밖 6점(COCO · RPN · RoI 풀링 · 배치 정규화 · Darknet-19 · Focal Loss). 같은 갈래 안에서 해가 늦을수록 오른쪽. DPM 은 없다.
- 관계 행렬: 31 × 31, 채워진 칸 40개(방향 있는 선 31개 중 두 개가 한 칸을 나눠 30칸 + 방향 없는 선 5개 × 2칸). 종류가 둘인 칸은 (결제 모듈 2판, 결제 모듈) 하나.
- 문서 ↔ 개념: 문서 6, 문서마다 선 YOLO 5 · YOLO9000 7 · YOLOv3 5(개념 파일 없는 Darknet-53 은 뺀다) · 2단계 노트 7 · Focal Loss 4 · 결제 기획서 9.

## 6. 차트 설정

```cpp
enum class EChartType { NONE, BAR, STEP_BAR, LINE, SCATTER };

struct ChartPoint  { double dX = 0; std::optional<double> optY; QString strLabel; };
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
7. 축 범위: 막대 · 단계 막대는 0 을 넣는다. 선 · 산점도는 값 범위에 여유를 둔다. 눈금은 1 · 2 · 5 × 10ⁿ 간격으로 5–8개.
8. 단계 막대: 첫 막대는 0 부터, 그다음은 앞 값에서 이 값까지. 늘어남과 줄어듦을 다른 색으로(파랑 · 주황, 빨강 · 초록만으로 나누지 않는다).
9. 제목은 표 제목, 밑글은 `condition · where`.

showcase 의 표 12개가 README §3 의 시작 차트와 같게 나와야 한다(테스트로 확인한다).

## 7. 위젯

- `VisualMarkdownTheme`: 바탕 · 글자 · 흐린 글자 · 격자, 개념 종류 7개(+ 모르는 종류)의 점 색, 관계 종류 5개의 선 색, AI 제안 선(점선 · 흐리게), 강조, 차트 계열 색 8개, 늘어남 · 줄어듦. `Light()` · `Dark()` 기본값.
- `KnowledgeGraphView`(`QWidget`): 배치 결과와 점 정보(이름 · 종류 · 연도 · 정의)를 받아 그린다. 선(방향 있는 선은 화살표, AI 제안은 점선, 종류마다 색), 점(종류마다 색의 둥근 상자, 문서는 다른 모양), 타임라인 눈금 · 갈래, 연도 없는 점 목록, 범례. 확대 · 이동 · 맞춤, 마우스 올림 설명, 누르면 이웃 강조. 시그널: `ConceptActivated(id)`, `DocumentActivated(id)`.
- `KnowledgeMatrixView`: 행 · 열 이름, 칸 색(종류가 둘이면 칸을 대각선으로 나눈다), 마우스 올림 설명.
- `KnowledgeChartView`: `ChartSpec` 을 그린다. 축 · 눈금 · 격자, 막대(여러 계열이면 나란히), 단계 막대, 선(빈 값에서 끊음), 점(계열마다 색), 범례, 제목 · 밑글. `NONE` 이면 가운데에 그 까닭을 쓴다.
- 위젯은 Ocean 에 기대지 않는다. 그리기 결과는 `render()` 로 `QImage` 에 남길 수 있다.

## 8. 보기 앱

- `VisualMarkdownViewer [wiki 폴더]`. 폴더를 주지 않으면 이 저장소의 `examples/showcase/wiki` 를 연다.
- 왼쪽: 지식 사이(관계망 · 계보도 · 분류 트리 · 타임라인 · 관계 행렬 · 문서 ↔ 개념), 지식 안(문서 → 표). 오른쪽: 고른 보기. 지식 안에서는 차트 아래에 원래 표를 함께 보인다.
- 도구: 폴더 열기, 밝음/어둠, 맞춤. 상태 줄: 점 · 선 · 문서 수와 경고 수.
- `--snapshot <폴더> [--theme light|dark] [--size 가로x세로]`: 보기 6개와 표마다의 차트를 PNG 로 남기고 끝난다. 화면이 없어도 돈다(offscreen).
- 빌드: `VISUALMARKDOWN_BUILD_VIEWER`(기본: 이 저장소가 최상위일 때만).

## 9. 테스트

- `KnowledgeIndexTest`: showcase README §2.1 의 기대값을 진짜 색인으로 확인한다. `ShowcaseWikiTest` 안에 있던 색인 계산은 `KnowledgeIndex` 로 바꾼다(기대값은 그대로).
- `KnowledgeGraphLayoutTest`: §5 의 showcase 기대값과 배치 조건(겹침 없음 · 방향 · 깊이 · 순서), 같은 입력이면 같은 배치.
- `KnowledgeChartBuilderTest`: 표 12개가 README §3 과 같은 차트 · 계열 · 축 · 빈 값을 내는지, 숫자 읽기.
- `VisualMarkdownViewTests`(offscreen): 보기마다 그려지는지(비지 않음, 두 번 그리면 같음), 위치로 점 · 칸 찾기, 테마 바꾸기.
- 보기 앱 스냅샷이 PNG 를 모두 남기는지(ctest).

## 10. 순서

작업마다 구현 에이전트 → 리뷰 에이전트(CLAUDE.md)다. 메인 세션은 스냅샷 PNG 를 직접 보고 확인한다.

1. 색인 `KnowledgeIndex`, `ShowcaseWikiTest` 가 색인을 쓰게
2. 그래프 모델 · 배치 6가지
3. 차트 설정
4. 테마 · 위젯 3개 · 위젯 테스트
5. 보기 앱 · 스냅샷 · 문서 반영(README · Visualization.md)

## 11. 남은 위험

- 힘 배치는 단순하고 결정적이라 보기 좋지 않을 수 있다. 스냅샷을 보고 다듬는다.
- 점이 수천 개면 관계망 배치가 느리다(반복마다 점 쌍 전부). 지금은 showcase 크기에 맞춘다.
- Ocean 에 붙일 때 Ocean 의 Presenter · 테마 mapper 가 필요하다.
