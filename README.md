# VisualMarkdown

마크다운 파일을 그래프로 볼 수 있게 하는 **파일 규칙**과, 그 규칙대로 읽고 쓰는 **Qt5 C++ 라이브러리**. Ocean 의 지식 형식(형식 1판)에서 떼어 낸다. Ocean 은 이 저장소를 git 서브모듈로 붙여 쓴다.

- 작성일: 2026-10-23
- 상태: 2단계(1층 · 1+층을 라이브러리로 옮기기)까지 했다. 3단계(Ocean 에 붙이기)부터는 아직이다(§7).
- 바탕: Ocean [지식 형식 스펙](../Ocean/docs/superpowers/specs/2026-10-22-knowledge-format-design.md) §1–§4(계획 ② 반영본)와 Ocean `main` 6d539cc 의 `src/Document/Knowledge/*`. 스펙과 코드가 다른 곳은 코드를 따랐다.

## 1. 무엇인가

- 보통 마크다운이다. VS Code · GitHub · Obsidian 에서 그냥 읽히고, 링크를 눌러 옮겨 다닌다.
- 정해진 자리(머리글 · 정해진 절 · 줄 문법 · HTML 주석)를 읽으면 그래프가 된다. 시각화는 두 층이다.
  - **지식 사이**: 개념 파일 = 점, 관계 줄 = 선 → 관계망 · 계보도 · 관계 행렬
  - **지식 안**: `ocean:data` 주석이 붙은 표 → 막대 · 선 · 산점도
- 데이터와 보기를 떼어 놓는다. 점 위치 · 색 · 고른 차트는 파일에 넣지 않는다. 보기는 늘어나도 데이터 규칙은 하나다.
- 손으로 고쳐도 된다. 모르는 글 · 절 · 머리글 키는 다시 쓸 때 그대로 남는다.

## 2. 폴더

```
VisualMarkdown/
├── README.md              이 문서: 무엇을 · 어떻게 정리하는지
├── format/                파일 규칙. 이 라이브러리의 약속이다                        (1단계)
│   ├── Folder.md          wiki 폴더 · 파일 이름 · 개념 id · 링크
│   ├── ConceptFile.md     개념 파일 — 그래프의 점
│   ├── DocumentFile.md    문서 파일 — 머리글 · 정해진 절 · 차례 · 핵심 숫자 · 카드 · 하위 항목
│   ├── Relation.md        관계 줄 — 그래프의 선
│   ├── DataTable.md       데이터 표 — ocean:data 주석 · 표 · 열 종류
│   ├── Vocabulary.md      어휘(개념 · 관계 · 열 종류 · 상태)와 판
│   ├── ReadWrite.md       읽기 · 쓰기 · 손 편집 · 이스케이프 · digest
│   └── Visualization.md   파일에서 그래프로: 색인 · 보기가 읽는 칸 · 정하지 않은 것
├── resources/
│   ├── vocabulary.json    어휘의 기계판. Ocean resources/knowledge/vocabulary.json 과 같다  (1단계)
│   └── VisualMarkdown.qrc vocabulary.json 을 :/visualmarkdown/ 에 넣는다                    (2단계)
├── examples/wiki/         정준 예시(Ocean 이 쓰는 모양 그대로). 테스트 픽스처로도 쓴다       (1단계)
├── docs/                  코드 규칙. Ocean docs/ 에서 복사                              (1단계)
│   ├── CppConvention.md
│   ├── Qt.md
│   └── Patterns.md
├── CMakeLists.txt         라이브러리 · 테스트 타깃                                       (2단계)
├── src/VisualMarkdown/    라이브러리 코드. .h 와 .cpp 를 한 폴더에 둔다(Ocean 과 같게)       (2 · 4단계)
└── tests/                 VisualMarkdownTests(Qt Test)와 실행기 시험 TestMainTest.cmake   (2 · 4단계)
```

- 최상위 폴더는 Ocean 처럼 소문자, 문서 파일 이름은 `docs/` 처럼 PascalCase 다.
- `format/` 이 기준이다. 코드를 고쳐 형식이 바뀌면 같은 커밋에서 `format/` 도 고친다.

## 3. 읽는 순서

| 하려는 일 | 읽을 문서 |
| --- | --- |
| 파일을 손으로 쓴다 | Folder → ConceptFile → DocumentFile → Relation → DataTable → Vocabulary |
| 파일을 읽는 프로그램 · 보기를 만든다 | 위 전부 + ReadWrite → Visualization |
| 라이브러리 코드를 고친다 | 위 전부 + `docs/` 세 문서 |

예시는 [examples/wiki](examples/wiki/)에 있다. 문서 파일 하나(관계 셋 · 표 둘)와 개념 파일 다섯이다.

## 4. 출처 대응

| 문서 | Ocean 스펙 | Ocean 코드(옮길 것) |
| --- | --- | --- |
| [Folder.md](format/Folder.md) | §3.1 · §3.2 | `KnowledgeText.*`(`MakeSlug` · `MakeUniqueId`), `KnowledgeMarkdownReader.cpp`(링크 경로) |
| [ConceptFile.md](format/ConceptFile.md) | §3.4 · §5.7(이름 열쇠) | `KnowledgeMarkdownReader/Writer.*`, `KnowledgeText::MakeNameKey` |
| [DocumentFile.md](format/DocumentFile.md) | §3.3 | `KnowledgeMarkdownReader/Writer.*` |
| [Relation.md](format/Relation.md) | §3.3 · §3.6 · §4.2 | `KnowledgeMarkdownReader/Writer.*`, `KnowledgeBase::RebuildIndex` |
| [DataTable.md](format/DataTable.md) | §3.3 · §4.3 | `KnowledgeMarkdownReader.cpp`(`ReadTableBlock`), `KnowledgeText::ClassifyColumn`, `KnowledgeExtractionValidator.cpp`(이름 정리 부분, §5.1) |
| [Vocabulary.md](format/Vocabulary.md) | §4 | `KnowledgeVocabulary.*`, `resources/knowledge/vocabulary.json` |
| [ReadWrite.md](format/ReadWrite.md) | §3.3(이스케이프) · §3.5 | `KnowledgeMarkdownLayout.*`, `KnowledgeMarkdownWriter.*` |
| [Visualization.md](format/Visualization.md) | §1 · §2 · §3.6 | `KnowledgeBase.*` |

옮기지 않은 것: AI 추출 규칙(스펙 §5 — 요청 · 스키마 · 지침 · 검사기), 학습 화면, 백업. 파일을 **만드는** 한 가지 방법일 뿐이라 Ocean 에 남는다.

## 5. 라이브러리 범위

원칙: **`format/` 에 적힌 규칙을 코드로 지키는 데 필요한 만큼이 라이브러리다.** 파일 형식을 아는 코드는 라이브러리에 두고, 파일을 만드는 방법(AI 추출)과 보여 주는 방법(화면)은 밖에 둔다.

### 5.1. 층

| 층 | 내용 | 의존 | 언제 |
| --- | --- | --- | --- |
| 1. 형식 | `KnowledgeTypes.h` · `KnowledgeVocabulary.*`(+ `vocabulary.json`) · `KnowledgeMarkdownLayout.*` · `KnowledgeText.*` · `KnowledgeMarkdownReader.*` · `KnowledgeMarkdownWriter.*`. 로직은 그대로 옮긴다 | QtCore | 2단계 |
| 1+. 이름 정리 | 기록을 파일 문법에 맞추는 규칙: 이름 · 별칭 · 열 이름의 쉼표 → ` · `, 단위의 괄호 빼기, 이름 끝 ` (…)` 를 단위로 옮기기, 겹친 열 이름에 ` 2` · ` 3`(`x` · `y` · `series` · `ordered` 가 따라간다). `KnowledgeText::CleanName` · `CleanNames` · `CleanColumns`. Ocean `KnowledgeExtractionValidator.cpp` 53–60 · 100–158줄에서 옮겼다 | QtCore | 2단계 |
| 2. 저장소 · 색인 | `KnowledgeBase` 의 AI 와 상관없는 부분(§5.3) | QtCore | 4단계 |
| 3. 보기 모델 | 그래프 모델(점 · 선 걸러 내기), 차트 설정(열 종류 → 축 · 계열 · 추천 차트) | QtCore | 5단계, 보기 스펙과 함께 |
| 4. 위젯 | 관계망 · 차트 위젯(`QPainter`) | QtWidgets | 6단계, 별도 타깃 `VisualMarkdownView` |

- 1+ 를 라이브러리에 두는 까닭: AI 검사기에만 있으면, 다른 경로로 만든 기록(손 도구 · 다른 앱)은 쓰고 다시 읽으면 달라진다(별칭의 쉼표에서 둘로 나뉘는 등). Ocean 검사기는 3단계부터 라이브러리의 이 함수를 부른다.
- Writer 는 이름 정리를 하지 않는다(옮기기라 동작을 그대로 두었다). 기록을 만드는 쪽이 쓰기 전에 부른다. 4단계의 저장 API 는 안에서 부른다(§5.3).
- 3층을 지금 만들지 않는 까닭: 보기를 아직 정하지 않았다(§10). 지금 만들면 추측이다.
- 4층을 따로 두는 까닭: 화면이 없는 쓰임(색인 · 변환 도구)이 Widgets 를 끌어오지 않게 하려는 것이다.
- 1–3층은 Qt5 Core 만 쓴다. Ocean 의 `Observers` · `SystemStatus` · `pch.h` 에 기대지 않고, `QObject` · 시그널도 없다. 바뀜 알림은 Ocean 이 라이브러리를 부른 뒤 `Observers` 로 한다.
- 타입 · 함수 이름은 그대로 둔다(`Knowledge*`). Ocean 쪽 고침을 include 줄로 줄이려는 것이다.

### 5.2. Ocean 에 남는 것

- AI: 요청 ② · 스키마 · 지침(`resources/knowledge/*-instructions.md`) · `analysis.json`, 검사기의 AI 검사(개수 자르기, `note` 의 숫자가 `quote` 에 있는지, `ref` · `match` 확인)
- `KnowledgeBase.cpp` 에서 추출 결과를 다루는 부분. 4단계에서 Ocean 의 어댑터(이름은 예: `KnowledgeSaveBuilder`)로 옮긴다:
  - `ExtractedConcept` 를 `ConceptDraft`(§5.3)로 바꾸기: `match` → 자기 이름 → 별칭 순서의 이름 목록, 종류 · 연도 · 정의. 지금 `ConceptResolver::Resolve`(142–182줄)가 AI 타입을 읽는 부분이다
  - 가리켜지는 `ref` 모으기 `CollectReferencedRefs`(254–275줄)
  - `ref` 를 링크로 바꿔 문서 기록 만들기: 카드의 개념 연결(349–362줄), 관계 · 표(384–417줄). 두 `ref` 가 한 개념으로 이어진 관계와 한쪽 `ref` 를 잇지 못한 관계는 여기서 뺀다
  - ② 가 실패한 분석이면 지금 파일의 관계 · 표와 카드의 개념 연결을 넣기(366–368 · 376–382줄)
- 학습 흐름(`LearnLibrary` · `LearnService`), `Observers` 알림, 화면, 백업

### 5.3. 저장소 · 색인(2층)과 저장 API

라이브러리 `KnowledgeBase` 가 갖는 것:
- 읽기 · 색인: `Reload` · `ReloadIfChanged` · `ListDocuments` · `FindDocument` · `ListConcepts` · `FindConcept` · `FindConceptIdByName` · `ListRelations` · `ListKnownConcepts` · `IsReadOnly` · `GetDocumentFileName` · `GetLastError`, 그리고 지금 파일을 새로 읽는 `ReadCurrentDocument`(지금은 private. 어댑터가 ② 실패 때 쓴다)
- 쓰기: 아래 저장 API · `NeedsOverwriteWarning` · `UpdateCards` · `RemoveDocument`
- 그 안의 규칙(지금 Ocean `KnowledgeBase.cpp`):
  - 이름 열쇠로 개념 잇기 · 새 개념 id 만들기 · 별칭 더하기 · 빈 칸만 채우기: `ConceptResolver`(123–252줄)에서 AI 타입을 뺀 부분, `ConceptPatch` · `ApplyPatch`(83–119줄)
  - 다시 저장 때 관계 이어 받기 `MergeRelations`(277–318줄), 같은 카드 id 의 옛 카드에서 모르는 하위 항목 이어 받기(343–348 · 363–372줄, ② 실패 때의 366–368줄은 빼고)
  - 문서가 가리키는 개념만 쓰기 `CollectConceptIds`(422–437줄), 개념 파일 먼저 → 문서 파일(`SaveDocument` 857–941줄에서 `ref` 처리를 뺀 부분), 정해진 부분만 다시 쓰기 · `digest`

저장 API 는 AI 결과(`DocumentSaveRequest` · `ExtractionResult`) 대신 기록만 받는다(**제안**, 4단계 전에 정한다). 지금 코드처럼 개념은 저장 하나 안에 모아 두었다가, 문서 기록이 가리키는 것만 쓴다.

```cpp
struct ConceptDraft
{
    QStringList vecNames;    // 찾는 순서. 새 개념이면 첫 이름이 대표 이름, 나머지는 별칭
    QString strKind;
    QString strYear;
    QString strSummary;
};

// 저장 하나. 개념을 잇거나 새로 만들어 모아 둔다(아직 파일을 쓰지 않는다)
class KnowledgeSave
{
public:
    explicit KnowledgeSave(const KnowledgeBase& base);

public:
    // 이름 열쇠로 있는 개념에 잇거나(별칭 더하기, 빈 칸만 채우기) 새 개념을 만든다. 개념 id 를 돌려준다
    QString UpsertConcept(const ConceptDraft& draft);

public:
    QString GetConceptName(const QString& conceptId) const;   // 링크 글자
};

// 문서 기록이 가리키는 개념만 쓰고(개념 파일 먼저) 문서를 쓴다. 다시 저장이면 지금 파일에서 확인된 관계와 모르는 하위 항목을 이어 받는다
SaveResult KnowledgeBase::SaveDocument(const KnowledgeSave& save, const KnowledgeDocument& document);
```

- `UpsertConcept` 은 `vecNames` 를 앞에서부터 이름 열쇠로 찾아 처음 찾은 개념에 잇는다. 없으면 첫 이름으로 새 개념을 만든다. 한 저장 안에서 먼저 만든 개념 · 더한 별칭은 다음 호출에서 바로 찾힌다. 이름은 안에서 `CleanName` · `CleanNames` 로 정리한다.
- Ocean 어댑터의 흐름: 가리켜지는 개념마다 `vecNames` = `match` → 자기 이름 → 별칭으로 `UpsertConcept` → 받은 id 와 `GetConceptName` 으로 카드 · 관계 · 표의 링크를 채운 문서 기록 → `SaveDocument`. 자기 자신을 잇는 관계 · 한쪽을 잇지 못한 관계는 기록에서 빠지므로, 그런 관계만 가리키던 개념은 쓰이지 않는다(지금 동작과 같다).
- ② 가 실패한 분석이면 어댑터가 `ReadCurrentDocument` 로 지금 파일을 읽어 옛 관계 · 표와 카드의 개념 연결을 넣는다. 색인이 아니라 파일에서 읽는다(지금 동작과 같다).
- 손 도구 · 다른 앱도 AI 없이 같은 저장 규칙을 쓸 수 있다.

### 5.4. 1–2층이 끝났다고 보는 기준

1. Ocean 의 Reader · Writer · Text · Vocabulary · Base 테스트가 단언을 바꾸지 않고 라이브러리에서 통과한다. `KnowledgeBaseSaveTest` 는 저장 규칙 부분을 라이브러리로, AI 결과 → 기록 바꾸기를 Ocean 어댑터 테스트로 나눈다.
2. `examples/wiki` 를 읽고 다시 쓰면 바이트가 같다.
3. 라이브러리의 어떤 파일도 Ocean 헤더를 include 하지 않는다(QtCore 만).
4. 서브모듈로 붙인 뒤 전체 `OceanTests` 가 통과하고 동작이 바뀌지 않는다.

## 6. Ocean 에 붙이는 모양(3단계)

```
Ocean/
├── external/VisualMarkdown/   git 서브모듈
└── CMakeLists.txt             add_subdirectory(external/VisualMarkdown)
                               target_link_libraries(OceanCore PUBLIC VisualMarkdown::VisualMarkdown)
```

- 타깃: `add_library(VisualMarkdown STATIC …)` + 별칭 `VisualMarkdown::VisualMarkdown`. C++17, `Qt5::Core`, `target_include_directories(VisualMarkdown PUBLIC src)`.
- 테스트는 이 저장소가 최상위 프로젝트일 때만 만든다(`VISUALMARKDOWN_BUILD_TESTS`: 최상위면 ON, 서브모듈이면 OFF). Ocean 빌드에 라이브러리 테스트가 끼지 않는다.
- include: Ocean 에게 이 라이브러리는 외부 라이브러리다. `docs/CppConvention.md` §9 대로 `<>` 로 쓰고 외부 라이브러리 묶음에 둔다.

```cpp
#include "Document/LearnLibrary.h"

#include "Core/Observers.h"

#include <VisualMarkdown/KnowledgeBase.h>
#include <VisualMarkdown/KnowledgeTypes.h>
#include <QString>

#include <optional>
```

- 리소스: 정적 라이브러리의 qrc 는 저절로 붙지 않는다. `KnowledgeVocabulary::Get()` 이 처음 불릴 때 `Q_INIT_RESOURCE(VisualMarkdown)` 를 부른다. 경로는 `:/visualmarkdown/vocabulary.json`(Ocean 의 `:/knowledge/` 와 겹치지 않게). Ocean 의 `resources/knowledge/vocabulary.json` 은 지운다.
- Ocean 에서 지울 것. Ocean 은 소스 · 테스트를 glob 으로 모으므로 `CMakeLists.txt` 에 파일마다의 줄은 없다.
  - 3단계: `src/Document/Knowledge/` 의 1층 파일, `resources/knowledge/vocabulary.json` 과 `resources/Ocean.qrc` 의 그 줄, 옮긴 테스트(`KnowledgeMarkdownReaderTest` · `KnowledgeMarkdownWriterTest` · `KnowledgeTextTest` · `KnowledgeVocabularyTest` · `KnowledgeTestData.h`)와 `tests/TestMain.cpp` 의 그 줄, 검사기의 이름 정리 코드(라이브러리 함수를 부르게 바꾼다)
  - 4단계: `KnowledgeBase` 의 2층 부분과 `KnowledgeBaseTest`, `KnowledgeBaseSaveTest` 의 저장 규칙 부분(§5.4)
- 라이브러리 테스트의 생성 함수는 `std::unique_ptr<QObject>` 를 돌려준다(`tests/TestMain.cpp`). 4단계에서 옮기는 테스트도 이 꼴로 바꾼다. `QObject*` 를 돌려주는 옛 정의가 남으면 링크는 되지만 실행이 깨진다.
- 받기: `git clone --recurse-submodules`, 이미 받았으면 `git submodule update --init`. Ocean `README.md` 의 빌드 절에 적는다.

## 7. 순서

| 단계 | 할 일 | 끝난 기준 |
| --- | --- | --- |
| 1 | 파일 규칙(`format/`) · 어휘(`resources/`) · 예시(`examples/`) · 코드 규칙(`docs/`) · 이 README | 끝 |
| 2 | 라이브러리 뼈대(`CMakeLists.txt` · `tests/`)를 만들고 1층 · 1+층(§5.1)과 그 테스트를 옮긴다. `examples/wiki` 를 읽고 다시 쓰면 바이트가 같은지 보는 테스트를 더한다. Ocean 은 아직 그대로다 | `VisualMarkdownTests` 통과 — 끝(6개 클래스, 82개 통과) |
| 3 | Ocean 에 서브모듈로 붙이고 1층 사본을 지운다. 검사기는 라이브러리의 이름 정리를 부른다(§6) | 전체 `OceanTests` 통과, 동작 바뀜 없음 |
| 4 | 2층: `KnowledgeBase` 를 나눠 라이브러리로 옮기고 저장 API 를 바꾼다. Ocean 에 어댑터를 만든다(§5.3) | 두 쪽 테스트 통과, §5.4 기준 |
| 5(나중) | 3층 보기 모델 | 보기 스펙에서 정한다 |
| 6(나중) | 4층 위젯, 별도 타깃 `VisualMarkdownView` | 보기 스펙에서 정한다 |

- 서브모듈 붙이기(3)를 `KnowledgeBase` 나누기(4)보다 먼저 한다. Ocean 어댑터가 라이브러리의 `UpsertConcept` · `SaveDocument` 를 부르려면 라이브러리가 먼저 붙어 있어야 한다. 단계마다 두 쪽 테스트가 모두 통과한 채로 끝난다.
- 2–4단계 코드는 [`../CLAUDE.md`](../CLAUDE.md)의 순서를 따른다: 구현 에이전트(테스트 → 옮기기 → 통과) → 리뷰 에이전트(`docs/` 규칙으로 정리 → 통과). 옮기기는 동작을 바꾸지 않는다.
- VisualMarkdown 은 아직 git 저장소가 아니다. 2단계 전에 `git init` 하고 원격(서브모듈 URL)을 정한다.
- 학습 메뉴 ② 가 아직 진행 중이다(Ocean `main` 6d539cc 은 그 "진행분" 병합). 그 작업이 `KnowledgeBase` · Reader · Writer 를 쓰므로, 3–4단계는 학습 메뉴 ② 가 끝난 뒤에 하거나 그 작업과 맞춰서 한다.

## 8. 코드 규칙

- 이 저장소의 코드는 [docs/CppConvention.md](docs/CppConvention.md) · [docs/Qt.md](docs/Qt.md) · [docs/Patterns.md](docs/Patterns.md) 를 따른다. Ocean `docs/` 의 같은 파일을 복사했다. 다른 점: `Qt.md` 에는 §3 UI Architecture 가 없다(라이브러리에는 아직 화면이 없다. 4층 위젯을 만들 때 Ocean 의 그 절을 다시 본다). 그 밖에는 한쪽을 고치면 다른 쪽도 고친다.
- 요점: PascalCase, 타입 접두 이름(`strName` · `vecRows` · `bCumulative`), 매개변수는 접두 없음, 클래스 멤버 `m_`, enum 은 `E` 로 시작하고 값은 대문자, 함수 이름은 동사로 시작, include 순서(대응 헤더 → 이 저장소 → 외부 → 표준), 매직 넘버는 `constexpr` 상수.
- Ocean 과 다른 점: `pch.h` 가 없다. `.cpp` 의 맨 앞은 대응 헤더다.
- 라이선스(Ocean `docs/LicenseCompliance.md`): Qt5 LGPLv3 로 상용 배포한다. Core · Gui · Widgets · Svg 같은 LGPL 모듈만 쓴다. **Qt Charts · Qt Data Visualization 은 오픈소스판이 GPLv3 전용이라 쓰지 않는다.** 차트 위젯(4층, 6단계)은 `QPainter` 로 그린다. GPL · AGPL · 출처 불명 코드는 들이지 않는다.

## 9. 바꾸는 규칙

- 문법이나 어휘가 바뀌면(종류를 더하기만 해도) `format` 판을 올린다. `resources/vocabulary.json` · [Vocabulary.md](format/Vocabulary.md) · 바뀐 문법 문서를 함께 고친다.
- `examples/wiki/` 는 Ocean 이 쓰는 정준 모양이다. 문서 파일의 정해진 부분을 고치면 `digest` 를 다시 계산한다([ReadWrite.md §6](format/ReadWrite.md#6-digest)).
- 옮기기 전에는 Ocean 코드가 기준이다. Ocean 쪽 지식 형식이 바뀌면 이 폴더도 고친다. 옮긴 뒤에는 이 저장소가 기준이다(1층은 3단계부터, 2층은 4단계부터).

## 10. 정하지 않은 것

- 보기: 어떤 보기를 만들지, 열 종류로 차트를 추천하는 규칙([Visualization.md](format/Visualization.md)에 시작점 제안만 있다), 보기 정보(점 위치 · 색 · 고른 차트)를 둘 곳
- 여러 문서의 표 행을 모아 비교하는 규칙
- Obsidian 위키 링크(`[[YOLO]]`). 지금은 표준 링크만 읽는다. 카드의 `개념:` 줄에 쓰면 다시 쓸 때 사라진다.
- Ocean 계획 ② 의 남은 백로그: 표 머리 줄 뒤의 `ocean:data` 주석, 주석만 있는 `###` 블록, 인용 속 공백으로 묶은 숫자, `#태그` 로 시작하는 한 줄, 여러 줄 `<!--` 주석([ReadWrite.md §8](format/ReadWrite.md#8-알려진-한계))
- 이 라이브러리 자체의 라이선스, 원격 저장소 주소
