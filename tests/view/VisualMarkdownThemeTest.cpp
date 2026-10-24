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

// WCAG 2 상대 휘도(sRGB): 채널 값 → 선형 값 → 휘도. 대비는 (밝은 휘도 + 0.05) / (어두운 휘도 + 0.05)
constexpr double kChannelMax = 255.0;
constexpr double kLinearThreshold = 0.04045;
constexpr double kLinearSlope = 12.92;
constexpr double kGammaOffset = 0.055;
constexpr double kGammaScale = 1.055;
constexpr double kGamma = 2.4;
constexpr double kRedWeight = 0.2126;
constexpr double kGreenWeight = 0.7152;
constexpr double kBlueWeight = 0.0722;
constexpr double kContrastOffset = 0.05;

double ToLinear(int channel)
{
    const double dValue = channel / kChannelMax;
    return dValue <= kLinearThreshold ? dValue / kLinearSlope : std::pow((dValue + kGammaOffset) / kGammaScale, kGamma);
}

double GetLuminance(const QColor& color)
{
    return kRedWeight * ToLinear(color.red()) + kGreenWeight * ToLinear(color.green()) + kBlueWeight * ToLinear(color.blue());
}

double GetContrast(const QColor& a, const QColor& b)
{
    const double dLighter = std::max(GetLuminance(a), GetLuminance(b));
    const double dDarker = std::min(GetLuminance(a), GetLuminance(b));
    return (dLighter + kContrastOffset) / (dDarker + kContrastOffset);
}

QString Describe(const QString& what, const QColor& a, const QColor& b)
{
    return QStringLiteral("%1: %2 / %3 = %4").arg(what, a.name(), b.name()).arg(GetContrast(a, b), 0, 'f', kContrastDecimals);
}

VisualMarkdownTheme MakeTheme(bool dark)
{
    return dark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
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
