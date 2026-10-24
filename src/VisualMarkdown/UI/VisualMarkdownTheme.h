#pragma once

#include <QColor>
#include <QHash>
#include <QString>
#include <QVector>

// 점 상자 하나의 색: 채움과 테두리
struct VisualMarkdownNodeColors
{
    QColor fill;
    QColor border;
};

// 보기 위젯(KnowledgeGraphView · KnowledgeMatrixView · KnowledgeChartView)이 쓰는 색과 글꼴(design/Views.md §7).
// 위젯은 이 값만 보고 그린다. Ocean 은 자기 테마를 이것으로 바꾸는 mapper 를 둔다(TerminalViewer 의 TerminalTheme 와 같은 방식).
// 글자 색(text · nodeText)은 모든 바탕 · 점 색 위에서 읽혀야 한다(VisualMarkdownThemeTest 가 대비를 잰다).
// 기본 생성한 테마는 색이 비어 있다. 쓰는 쪽(Ocean 의 mapper)은 내장 테마(MakeLight · MakeDark)에서 시작해 바꾼다.
// 색이 아닌 값(흐림 · 점선 무늬 · 불투명도 · 글꼴 크기)은 여기 기본값 한 곳에만 두고 내장 테마도 그대로 쓴다.
struct VisualMarkdownTheme
{
public:
    static VisualMarkdownTheme MakeLight();
    static VisualMarkdownTheme MakeDark();

public:
    QColor MakeProposed(const QColor& color) const;                         // AI 제안: 바탕 쪽으로 dProposedFade 만큼 섞은 색

public:
    VisualMarkdownNodeColors GetConceptColors(const QString& kind) const;   // 모르는 종류는 unknownConcept
    QColor GetRelationColor(const QString& kind) const;                     // 모르는 종류는 unknownRelation
    QColor GetSeriesColor(int series) const;                                // 8개를 넘으면 처음부터 되풀이한다(모양으로 가른다)
    // 크기 눈금의 t 자리(0 낮음 … 1 높음, 밖은 끝으로, 숫자가 아니면 가운데) 색. 이웃한 두 색 사이는 채널마다 선형으로 섞는다. 눈금이 비었으면 text
    QColor GetRampColor(double t) const;
    // 채움(히트맵 칸 · 도넛 조각) 위 글자 색: fillTextDark · fillTextLight 가운데 채움과 WCAG 대비가 큰 쪽(같으면 fillTextDark)
    QColor GetFillTextColor(const QColor& fill) const;

public:
    // 바탕 · 글자 · 격자
    QColor background;
    QColor text;
    QColor mutedText;          // 눈금 · 밑글 · 갈래 이름처럼 덜 앞에 나서는 글자
    QColor grid;               // 격자 · 눈금 선(가는 실선)
    QColor axis;               // 축 · 행렬 덩어리 경계
    QColor panelBackground;    // 범례 · 연도 없음 목록 판
    QColor panelBorder;

    // 갈래 띠(계보도 · 타임라인). 번갈아 칠한다
    QColor laneBand;
    QColor laneBandAlternate;

    // 고른 점: 둘레 띠와 테두리. 고른 점 · 이웃이 아닌 것은 이 불투명도로 흐리게 그린다
    QColor selectionHalo;
    QColor selectionRing;
    double dDimOpacity = 0.2;

    // 점: 개념 종류 코드(어휘의 conceptKinds) → 색. 어휘에 없는 종류는 unknownConcept. 문서 점은 document
    QColor nodeText;
    QHash<QString, VisualMarkdownNodeColors> hashConceptColors;
    VisualMarkdownNodeColors unknownConcept;
    VisualMarkdownNodeColors document;

    // 선: 관계 종류 코드(어휘의 relationKinds) → 색. AI 제안은 바탕 쪽으로 섞어 흐리게, 점선으로 그린다
    QHash<QString, QColor> hashRelationColors;
    QColor unknownRelation;
    QColor confirmedSample;          // 범례에서 확인됨 · AI 제안 선 모양을 보이는 중립 색
    double dProposedFade = 0.45;                 // 0 이면 그대로, 1 이면 바탕 색
    QVector<qreal> vecProposedDash { 4, 3 };     // 점선 무늬(선 굵기 단위): 4 칠하고 3 비운다
    QColor bipartiteLink;            // 문서 ↔ 개념 선(중립 회색, 굵기가 무게)

    // 차트: 계열 색 8개(고정 순서, 색맹 구분 검사를 거친 순서), 단계 막대의 시작(값이 있는 첫 막대) · 늘어남 · 줄어듦
    QVector<QColor> vecSeriesColors;
    QColor stepStart;                // 시작 값 막대: 늘어난 값이 아니라 출발점이라 중립 회색으로 따로 보인다
    QColor stepIncrease;
    QColor stepDecrease;

    // 크기 눈금(히트맵 칸 색, 낮음 → 높음): 한 색조의 밝음 → 어두움(어두운 테마는 거꾸로, 낮은 값이 바탕 쪽)
    QVector<QColor> vecSequentialRamp;
    // 채움(히트맵 칸 · 도넛 조각) 위 글자: 둘 중 채움과 대비가 큰 쪽을 쓴다(GetFillTextColor). 검정 · 흰색이면 어떤 채움에서도 4.5:1 이상이다
    QColor fillTextDark;
    QColor fillTextLight;

    // 글꼴. 비우면 위젯 글꼴을 쓴다. 크기는 화면 점(px) 단위(확대 1배)
    QString strFontFamily;
    int iNodeFontPx = 13;            // 점 이름
    int iLabelFontPx = 12;           // 갈래 · 눈금 · 범례 · 행렬 이름표
    int iSmallFontPx = 11;           // 범례 제목 · 보조 글
    int iTitleFontPx = 18;           // 차트 제목
    int iCaptionFontPx = 13;         // 차트 밑글
};
