#include "VisualMarkdown/UI/VisualMarkdownTheme.h"

#include <algorithm>
#include <initializer_list>

namespace
{
constexpr int kPanelAlpha = 238;            // 범례 판: 뒤의 그림이 아주 조금 비친다
constexpr int kLightHaloAlpha = 210;
constexpr int kDarkHaloAlpha = 190;
constexpr int kLinkAlpha = 150;             // 문서 ↔ 개념 선: 겹친 곳이 보이게 반쯤 비친다

QColor MakeAlpha(const char* hex, int alpha)
{
    QColor color(QString::fromLatin1(hex));
    color.setAlpha(alpha);
    return color;
}

void SetConcept(VisualMarkdownTheme* theme, const char* code, const char* fill, const char* border)
{
    theme->hashConceptColors.insert(QString::fromLatin1(code), { QColor(QString::fromLatin1(fill)), QColor(QString::fromLatin1(border)) });
}

void SetRelation(VisualMarkdownTheme* theme, const char* code, const char* color)
{
    theme->hashRelationColors.insert(QString::fromLatin1(code), QColor(QString::fromLatin1(color)));
}

void SetSeries(VisualMarkdownTheme* theme, std::initializer_list<const char*> colors)
{
    theme->vecSeriesColors.clear();
    for (const char* pColor : colors)
        theme->vecSeriesColors << QColor(QString::fromLatin1(pColor));
}
}

// 색이 아닌 값(흐림 · 점선 무늬 · 불투명도 · 글꼴 크기)은 머리글의 기본값 그대로다. 여기서는 색만 정한다
VisualMarkdownTheme VisualMarkdownTheme::MakeLight()
{
    VisualMarkdownTheme theme;
    theme.background = QColor(0xfc, 0xfc, 0xfb);
    theme.text = QColor(0x1d, 0x1d, 0x1b);
    theme.mutedText = QColor(0x5c, 0x5b, 0x57);
    theme.grid = QColor(0xe4, 0xe3, 0xdc);
    theme.axis = QColor(0xb9, 0xb8, 0xae);
    theme.panelBackground = MakeAlpha("#ffffff", kPanelAlpha);
    theme.panelBorder = QColor(0xdc, 0xdb, 0xd3);
    theme.laneBand = QColor(0xf3, 0xf2, 0xed);
    theme.laneBandAlternate = QColor(0xee, 0xf1, 0xf4);
    theme.selectionHalo = MakeAlpha("#ffd43b", kLightHaloAlpha);
    theme.selectionRing = QColor(0x1d, 0x1d, 0x1b);
    theme.nodeText = QColor(0x1d, 0x1d, 0x1b);

    // 옅은 채움(글자는 어두운 색) + 같은 색조의 진한 테두리
    SetConcept(&theme, "method", "#d8e7fb", "#2a78d6");
    SetConcept(&theme, "component", "#d3efe3", "#16966a");
    SetConcept(&theme, "task", "#fde3d6", "#d95926");
    SetConcept(&theme, "data", "#faeec4", "#b98900");
    SetConcept(&theme, "metric", "#f9dae8", "#c2408a");
    SetConcept(&theme, "system", "#e4defb", "#6250d6");
    SetConcept(&theme, "idea", "#e6f0cc", "#6a8f1f");
    theme.unknownConcept = { QColor(0xe7, 0xe6, 0xe1), QColor(0x8a, 0x89, 0x84) };
    theme.document = { QColor(0xf3, 0xef, 0xe6), QColor(0x8a, 0x81, 0x70) };

    // 개선 · 사용 · 해결은 색맹 모의에서도 서로 갈리는 세 색, 종류는 중립 회색(나무의 뼈대), 대안은 화살표가 없어 모양으로도 갈린다
    SetRelation(&theme, "improves", "#d95926");
    SetRelation(&theme, "uses", "#2a78d6");
    SetRelation(&theme, "isA", "#5c5b57");
    SetRelation(&theme, "solves", "#16966a");
    SetRelation(&theme, "alternative", "#c2408a");
    theme.unknownRelation = QColor(0x8f, 0x8e, 0x88);
    theme.confirmedSample = QColor(0x5c, 0x5b, 0x57);
    theme.bipartiteLink = MakeAlpha("#8f8e88", kLinkAlpha);

    SetSeries(&theme, { "#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#e87ba4", "#008300", "#6250d6", "#e34948" });
    theme.stepStart = QColor(0x7d, 0x7c, 0x76);
    theme.stepIncrease = QColor(0x2a, 0x78, 0xd6);
    theme.stepDecrease = QColor(0xeb, 0x68, 0x34);
    return theme;
}

VisualMarkdownTheme VisualMarkdownTheme::MakeDark()
{
    VisualMarkdownTheme theme;
    theme.background = QColor(0x1a, 0x1a, 0x19);
    theme.text = QColor(0xf0, 0xef, 0xec);
    theme.mutedText = QColor(0xb4, 0xb3, 0xab);
    theme.grid = QColor(0x2e, 0x2e, 0x2c);
    theme.axis = QColor(0x4a, 0x49, 0x45);
    theme.panelBackground = MakeAlpha("#242422", kPanelAlpha);
    theme.panelBorder = QColor(0x3a, 0x3a, 0x37);
    theme.laneBand = QColor(0x22, 0x22, 0x21);
    theme.laneBandAlternate = QColor(0x1f, 0x23, 0x27);
    theme.selectionHalo = MakeAlpha("#e0a800", kDarkHaloAlpha);
    theme.selectionRing = QColor(0xf0, 0xef, 0xec);
    theme.nodeText = QColor(0xf0, 0xef, 0xec);

    // 짙은 채움(글자는 밝은 색) + 같은 색조의 밝은 테두리
    SetConcept(&theme, "method", "#1d3a5e", "#3987e5");
    SetConcept(&theme, "component", "#163f31", "#199e70");
    SetConcept(&theme, "task", "#4e2a19", "#d95926");
    SetConcept(&theme, "data", "#45391a", "#c98500");
    SetConcept(&theme, "metric", "#4a2035", "#d55181");
    SetConcept(&theme, "system", "#2f2a5a", "#9085e9");
    SetConcept(&theme, "idea", "#333f1b", "#8fb33c");
    theme.unknownConcept = { QColor(0x34, 0x33, 0x30), QColor(0x8a, 0x89, 0x84) };
    theme.document = { QColor(0x2d, 0x2b, 0x26), QColor(0xa3, 0x9b, 0x88) };

    SetRelation(&theme, "improves", "#d95926");
    SetRelation(&theme, "uses", "#3987e5");
    SetRelation(&theme, "isA", "#a9a89f");
    SetRelation(&theme, "solves", "#199e70");
    SetRelation(&theme, "alternative", "#d55181");
    theme.unknownRelation = QColor(0x7a, 0x79, 0x73);
    theme.confirmedSample = QColor(0xb4, 0xb3, 0xab);
    theme.bipartiteLink = MakeAlpha("#8f8e88", kLinkAlpha);

    SetSeries(&theme, { "#3987e5", "#d95926", "#199e70", "#c98500", "#d55181", "#008300", "#9085e9", "#e66767" });
    theme.stepStart = QColor(0x8a, 0x89, 0x84);
    theme.stepIncrease = QColor(0x39, 0x87, 0xe5);
    theme.stepDecrease = QColor(0xd9, 0x59, 0x26);
    return theme;
}

QColor VisualMarkdownTheme::MakeProposed(const QColor& color) const
{
    const double dFade = std::clamp(dProposedFade, 0.0, 1.0);
    const auto funcMix = [dFade](int from, int to)
    {
        return static_cast<int>(from + (to - from) * dFade + 0.5);
    };
    return QColor(funcMix(color.red(), background.red()), funcMix(color.green(), background.green()),
                  funcMix(color.blue(), background.blue()), color.alpha());
}

VisualMarkdownNodeColors VisualMarkdownTheme::GetConceptColors(const QString& kind) const
{
    return hashConceptColors.value(kind, unknownConcept);
}

QColor VisualMarkdownTheme::GetRelationColor(const QString& kind) const
{
    return hashRelationColors.value(kind, unknownRelation);
}

QColor VisualMarkdownTheme::GetSeriesColor(int series) const
{
    if (vecSeriesColors.isEmpty())
        return text;
    const int iCount = vecSeriesColors.size();
    return vecSeriesColors[((series % iCount) + iCount) % iCount];
}
