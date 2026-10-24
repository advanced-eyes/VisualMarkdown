#include "VisualMarkdown/KnowledgeVocabulary.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"
#include "ViewTestSupport.h"

#include <QSet>
#include <QTest>

#include <algorithm>
#include <cmath>
#include <memory>

namespace
{
// WCAG 2 대비 기준: 본문 글자 4.5:1, 큰 글자 · 그림 요소(선 · 테두리) 3:1. 바탕 위 본문 글자는 그보다 넉넉하게 7:1
constexpr double kBodyTextContrast = 7.0;
constexpr double kTextContrast = 4.5;
constexpr double kGraphicContrast = 3.0;
constexpr int kConceptKindCount = 7;             // 어휘의 개념 종류(design/Views.md §7)
constexpr int kRelationKindCount = 5;            // 어휘의 관계 종류
constexpr int kSeriesCount = 8;
constexpr const char* kUnknownKind = "모르는-종류";
constexpr double kNeutralSaturation = 0.15;      // 이 채도 아래면 회색 계열이다
constexpr double kDistinctColorDistance = 80;    // RGB 거리가 이만큼 넘으면 눈으로 갈린다
constexpr int kContrastDecimals = 2;             // 실패 글에 쓰는 대비의 소수 자리

// 크기 눈금(design/MoreViews.md §4.4): 밝은 테마는 파랑 100 → 700(낮음 → 높음), 어두운 테마는 같은 색을 거꾸로. 채움 위 글자는 검정 · 흰색
const QStringList kLightSequentialRamp { "#cde2fb", "#9ec5f4", "#6da7ec", "#3987e5", "#256abf", "#184f95", "#0d366b" };
constexpr const char* kFillTextDark = "#000000";
constexpr const char* kFillTextLight = "#ffffff";
constexpr int kRampSamples = 101;                // 눈금을 고르게 나눈 자리(0, 0.01, …, 1)
constexpr double kRampMiddle = 0.5;              // 눈금 7개의 가운데(넷째 색)
constexpr double kStepMiddle = 0.5;              // 이웃한 두 색 사이의 가운데(한 칸의 반)
constexpr double kMixTolerance = 0.5;            // 섞은 채널 값의 반올림 오차

// WCAG 2 휘도 · 대비(차트 시험과 함께 쓴다)
using ViewTestSupport::GetContrast;
using ViewTestSupport::GetLuminance;

QString Describe(const QString& what, const QColor& a, const QColor& b)
{
    return QStringLiteral("%1: %2 / %3 = %4").arg(what, a.name(), b.name()).arg(GetContrast(a, b), 0, 'f', kContrastDecimals);
}

VisualMarkdownTheme MakeTheme(bool dark)
{
    return dark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
}

// 크기 눈금을 고르게 나눈 자리의 색(위젯이 칸을 칠하는 것과 같은 보간)
QVector<QColor> SampleRamp(const VisualMarkdownTheme& theme)
{
    QVector<QColor> vecColors;
    for (int iSample = 0; iSample < kRampSamples; ++iSample)
        vecColors << theme.GetRampColor(static_cast<double>(iSample) / (kRampSamples - 1));
    return vecColors;
}
}

// 밝은 · 어두운 기본 테마(design/Views.md §7): 어휘의 모든 종류에 색이 있고, 글자는 모든 바탕 · 점 색 위에서 읽힌다.
class VisualMarkdownThemeTest : public QObject
{
    Q_OBJECT

private slots:
    void themes_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }

    // 바탕 위 글자, 점 색 위 점 글자, 바탕 위 선 · 테두리
    void textAndLinesAreReadable_data() { themes_data(); }
    void textAndLinesAreReadable()
    {
        QFETCH(bool, dark);
        const VisualMarkdownTheme theme = MakeTheme(dark);
        QVERIFY2(GetContrast(theme.text, theme.background) >= kBodyTextContrast, qUtf8Printable(Describe("text", theme.text, theme.background)));
        QVERIFY2(GetContrast(theme.mutedText, theme.background) >= kTextContrast,
                 qUtf8Printable(Describe("mutedText", theme.mutedText, theme.background)));
        QVERIFY2(GetContrast(theme.text, theme.laneBand) >= kBodyTextContrast, qUtf8Printable(Describe("lane", theme.text, theme.laneBand)));
        QVERIFY2(GetContrast(theme.text, theme.laneBandAlternate) >= kBodyTextContrast,
                 qUtf8Printable(Describe("lane2", theme.text, theme.laneBandAlternate)));

        QVector<VisualMarkdownNodeColors> vecFills { theme.unknownConcept, theme.document };
        for (const QString& strCode : KnowledgeVocabulary::Get().GetConceptCodes())
            vecFills << theme.GetConceptColors(strCode);
        for (const VisualMarkdownNodeColors& colors : vecFills)
        {
            QVERIFY2(GetContrast(theme.nodeText, colors.fill) >= kTextContrast, qUtf8Printable(Describe("node text", theme.nodeText, colors.fill)));
            QVERIFY2(GetContrast(colors.border, theme.background) >= kGraphicContrast,
                     qUtf8Printable(Describe("node border", colors.border, theme.background)));
        }
        for (const QString& strCode : KnowledgeVocabulary::Get().GetRelationCodes())
        {
            const QColor color = theme.GetRelationColor(strCode);
            QVERIFY2(GetContrast(color, theme.background) >= kGraphicContrast, qUtf8Printable(Describe(strCode, color, theme.background)));
        }
        QVERIFY2(GetContrast(theme.stepIncrease, theme.background) >= kGraphicContrast,
                 qUtf8Printable(Describe("stepIncrease", theme.stepIncrease, theme.background)));
        QVERIFY2(GetContrast(theme.stepDecrease, theme.background) >= kGraphicContrast,
                 qUtf8Printable(Describe("stepDecrease", theme.stepDecrease, theme.background)));
        QVERIFY2(GetContrast(theme.stepStart, theme.background) >= kGraphicContrast,
                 qUtf8Printable(Describe("stepStart", theme.stepStart, theme.background)));
        QVERIFY2(GetContrast(theme.selectionRing, theme.background) >= kGraphicContrast,
                 qUtf8Printable(Describe("selectionRing", theme.selectionRing, theme.background)));
    }

    // 어휘의 종류마다 제 색이 있고 서로 다르다. 모르는 종류는 따로 정한 색이다
    void everyKindHasItsOwnColor_data() { themes_data(); }
    void everyKindHasItsOwnColor()
    {
        QFETCH(bool, dark);
        const VisualMarkdownTheme theme = MakeTheme(dark);
        const QStringList vecConceptCodes = KnowledgeVocabulary::Get().GetConceptCodes();
        QCOMPARE(vecConceptCodes.size(), kConceptKindCount);
        QSet<QRgb> setFills;
        for (const QString& strCode : vecConceptCodes)
        {
            QVERIFY2(theme.hashConceptColors.contains(strCode), qUtf8Printable(strCode));
            setFills.insert(theme.GetConceptColors(strCode).fill.rgb());
        }
        setFills.insert(theme.unknownConcept.fill.rgb());
        setFills.insert(theme.document.fill.rgb());
        QCOMPARE(setFills.size(), vecConceptCodes.size() + 2);

        const QStringList vecRelationCodes = KnowledgeVocabulary::Get().GetRelationCodes();
        QCOMPARE(vecRelationCodes.size(), kRelationKindCount);
        QSet<QRgb> setLines;
        for (const QString& strCode : vecRelationCodes)
        {
            QVERIFY2(theme.hashRelationColors.contains(strCode), qUtf8Printable(strCode));
            setLines.insert(theme.GetRelationColor(strCode).rgb());
        }
        setLines.insert(theme.unknownRelation.rgb());
        QCOMPARE(setLines.size(), vecRelationCodes.size() + 1);

        QCOMPARE(theme.GetConceptColors(QString::fromUtf8(kUnknownKind)).fill, theme.unknownConcept.fill);
        QCOMPARE(theme.GetRelationColor(QString::fromUtf8(kUnknownKind)), theme.unknownRelation);
    }

    // 차트 계열 색 8개(서로 다르다), 단계 막대의 늘어남 · 줄어듦은 다른 색. 9번째 계열은 처음 색으로 돌아간다
    void seriesPalette_data() { themes_data(); }
    void seriesPalette()
    {
        QFETCH(bool, dark);
        const VisualMarkdownTheme theme = MakeTheme(dark);
        QCOMPARE(theme.vecSeriesColors.size(), kSeriesCount);
        QSet<QRgb> setColors;
        for (const QColor& color : theme.vecSeriesColors)
            setColors.insert(color.rgb());
        QCOMPARE(setColors.size(), kSeriesCount);
        QCOMPARE(theme.GetSeriesColor(kSeriesCount), theme.GetSeriesColor(0));
        QCOMPARE(theme.GetSeriesColor(-1), theme.GetSeriesColor(kSeriesCount - 1));
        QVERIFY(theme.stepIncrease != theme.stepDecrease);
    }

    // 단계 막대의 시작 막대 색: 중립 회색이고, 늘어남 · 줄어듦 색과 눈으로 갈린다
    void stepStartIsNeutralAndDistinct_data() { themes_data(); }
    void stepStartIsNeutralAndDistinct()
    {
        QFETCH(bool, dark);
        const VisualMarkdownTheme theme = MakeTheme(dark);
        QVERIFY(theme.stepStart.isValid());
        QVERIFY2(theme.stepStart.hsvSaturationF() <= kNeutralSaturation, qUtf8Printable(theme.stepStart.name()));
        for (const QColor& other : { theme.stepIncrease, theme.stepDecrease })
            QVERIFY2(ViewTestSupport::GetColorDistance(theme.stepStart, other) >= kDistinctColorDistance,
                     qUtf8Printable(theme.stepStart.name() + QStringLiteral(" / ") + other.name()));
    }

    // AI 제안 색은 바탕 쪽으로 섞여 원래 색보다 바탕과 덜 대비된다(흐리게)
    void proposedIsLighter_data() { themes_data(); }
    void proposedIsLighter()
    {
        QFETCH(bool, dark);
        const VisualMarkdownTheme theme = MakeTheme(dark);
        QVERIFY(!theme.vecProposedDash.isEmpty());
        QCOMPARE(theme.vecProposedDash.size() % 2, 0);
        for (const QString& strCode : KnowledgeVocabulary::Get().GetRelationCodes())
        {
            const QColor color = theme.GetRelationColor(strCode);
            const QColor proposed = theme.MakeProposed(color);
            QVERIFY2(GetContrast(proposed, theme.background) < GetContrast(color, theme.background), qUtf8Printable(strCode));
        }
    }

    // 크기 눈금(히트맵 칸): 밝은 테마는 파랑 100 → 700, 어두운 테마는 같은 색을 거꾸로(낮은 값이 어두운 바탕 쪽으로 물러난다).
    // 눈금 끝 · 가운데는 그 색 그대로이고, 범위 밖 자리는 끝 색이다. 채움 위 글자는 검정 · 흰색이다
    void sequentialRampMatchesDesign_data() { themes_data(); }
    void sequentialRampMatchesDesign()
    {
        QFETCH(bool, dark);
        const VisualMarkdownTheme theme = MakeTheme(dark);
        QStringList vecExpected = kLightSequentialRamp;
        if (dark)
            std::reverse(vecExpected.begin(), vecExpected.end());
        QStringList vecActual;
        for (const QColor& color : theme.vecSequentialRamp)
            vecActual << color.name();
        QCOMPARE(vecActual, vecExpected);
        QCOMPARE(theme.fillTextDark, QColor(QString::fromLatin1(kFillTextDark)));
        QCOMPARE(theme.fillTextLight, QColor(QString::fromLatin1(kFillTextLight)));

        QCOMPARE(theme.GetRampColor(0).name(), vecExpected.first());
        QCOMPARE(theme.GetRampColor(1).name(), vecExpected.last());
        QCOMPARE(theme.GetRampColor(kRampMiddle).name(), vecExpected[vecExpected.size() / 2]);
        QCOMPARE(theme.GetRampColor(-1).name(), vecExpected.first());
        QCOMPARE(theme.GetRampColor(2).name(), vecExpected.last());
        // 이웃한 두 색 사이는 선형으로 섞는다: 첫 두 색의 가운데 자리는 두 색의 가운데 색이다
        const double dHalfStep = kStepMiddle / (vecExpected.size() - 1);
        const QColor first(vecExpected[0]);
        const QColor second(vecExpected[1]);
        const QColor middle = theme.GetRampColor(dHalfStep);
        QVERIFY2(std::abs(middle.red() - (first.red() + second.red()) / 2.0) <= kMixTolerance
                     && std::abs(middle.green() - (first.green() + second.green()) / 2.0) <= kMixTolerance
                     && std::abs(middle.blue() - (first.blue() + second.blue()) / 2.0) <= kMixTolerance,
                 qUtf8Printable(middle.name()));
    }

    // 채움(히트맵 칸 · 계열 색 8개) 위 글자: fillTextDark · fillTextLight 가운데 대비가 큰 쪽을 고르고, 어떤 채움에서도 4.5:1 이상이다
    void fillTextIsReadableOnFills_data() { themes_data(); }
    void fillTextIsReadableOnFills()
    {
        QFETCH(bool, dark);
        const VisualMarkdownTheme theme = MakeTheme(dark);
        QVector<QColor> vecFills = SampleRamp(theme);
        vecFills << theme.vecSeriesColors;
        QCOMPARE(vecFills.size(), kRampSamples + kSeriesCount);
        for (const QColor& fill : vecFills)
        {
            const QColor chosen = theme.GetFillTextColor(fill);
            QVERIFY2(chosen == theme.fillTextDark || chosen == theme.fillTextLight, qUtf8Printable(fill.name() + QStringLiteral(" → ") + chosen.name()));
            const QColor other = chosen == theme.fillTextDark ? theme.fillTextLight : theme.fillTextDark;
            QVERIFY2(GetContrast(chosen, fill) >= GetContrast(other, fill), qUtf8Printable(Describe("chosen", chosen, fill)));
            QVERIFY2(GetContrast(chosen, fill) >= kTextContrast, qUtf8Printable(Describe("fill text", chosen, fill)));
        }
    }

    // 눈금의 밝기가 한쪽으로 고르게 바뀐다: 밝은 테마는 높은 값일수록 어두워지고, 어두운 테마는 밝아진다
    void sequentialRampIsMonotonic_data() { themes_data(); }
    void sequentialRampIsMonotonic()
    {
        QFETCH(bool, dark);
        const VisualMarkdownTheme theme = MakeTheme(dark);
        const QVector<QColor> vecSamples = SampleRamp(theme);
        for (int iSample = 1; iSample < vecSamples.size(); ++iSample)
        {
            const double dBefore = GetLuminance(vecSamples[iSample - 1]);
            const double dAfter = GetLuminance(vecSamples[iSample]);
            QVERIFY2(dark ? dAfter > dBefore : dAfter < dBefore,
                     qUtf8Printable(QStringLiteral("%1: %2 → %3").arg(iSample).arg(vecSamples[iSample - 1].name(), vecSamples[iSample].name())));
        }
        // 낮은 값은 바탕 쪽이다: 낮은 끝이 높은 끝보다 바탕과 덜 대비된다
        QVERIFY(GetContrast(vecSamples.first(), theme.background) < GetContrast(vecSamples.last(), theme.background));
    }

    // 기본 생성한 테마의 색이 아닌 값(AI 제안 흐림 · 점선 무늬 · 흐린 불투명도 · 글꼴 크기)은 내장 테마와 같다.
    // 색은 비어 있다(쓰는 쪽은 내장 테마에서 시작해 바꾼다)
    void defaultConstructedMatchesBuiltIn_data() { themes_data(); }
    void defaultConstructedMatchesBuiltIn()
    {
        QFETCH(bool, dark);
        const VisualMarkdownTheme defaults {};
        const VisualMarkdownTheme builtIn = MakeTheme(dark);
        QCOMPARE(defaults.dProposedFade, builtIn.dProposedFade);
        QCOMPARE(defaults.vecProposedDash, builtIn.vecProposedDash);
        QCOMPARE(defaults.dDimOpacity, builtIn.dDimOpacity);
        QCOMPARE(defaults.iNodeFontPx, builtIn.iNodeFontPx);
        QCOMPARE(defaults.iLabelFontPx, builtIn.iLabelFontPx);
        QCOMPARE(defaults.iSmallFontPx, builtIn.iSmallFontPx);
        QCOMPARE(defaults.iTitleFontPx, builtIn.iTitleFontPx);
        QCOMPARE(defaults.iCaptionFontPx, builtIn.iCaptionFontPx);
        QCOMPARE(defaults.strFontFamily, builtIn.strFontFamily);
        QVERIFY(!defaults.background.isValid());
        QVERIFY(defaults.vecSeriesColors.isEmpty());
        QVERIFY(defaults.vecSequentialRamp.isEmpty());
        QVERIFY(!defaults.fillTextDark.isValid());
        QVERIFY(!defaults.fillTextLight.isValid());
    }

    // 두 테마는 바탕이 다르고 글자 색이 뒤바뀐다(밝은 바탕에 어두운 글자, 어두운 바탕에 밝은 글자)
    void lightAndDarkDiffer()
    {
        const VisualMarkdownTheme light = VisualMarkdownTheme::MakeLight();
        const VisualMarkdownTheme dark = VisualMarkdownTheme::MakeDark();
        QVERIFY(light.background != dark.background);
        QVERIFY(GetLuminance(light.background) > GetLuminance(light.text));
        QVERIFY(GetLuminance(dark.background) < GetLuminance(dark.text));
    }
};

std::unique_ptr<QObject> CreateVisualMarkdownThemeTest() { return std::make_unique<VisualMarkdownThemeTest>(); }

#include "VisualMarkdownThemeTest.moc"
