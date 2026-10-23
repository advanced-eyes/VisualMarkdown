#include "VisualMarkdown/KnowledgeMarkdownWriter.h"

#include "VisualMarkdown/KnowledgeMarkdownLayout.h"
#include "VisualMarkdown/KnowledgeMarkdownReader.h"
#include "VisualMarkdown/KnowledgeText.h"
#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <QCryptographicHash>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace
{
using UnreadLines = QHash<QString, QStringList>;

const QString kSectionOutline = QStringLiteral("차례");
const QString kSectionKeyNumbers = QStringLiteral("핵심 숫자");
const QString kSectionCards = QStringLiteral("지식");
const QString kSectionRelations = QStringLiteral("관계");
const QString kSectionData = QStringLiteral("데이터");

const QString kKeyEvidence = QStringLiteral("근거");
const QString kKeySection = QStringLiteral("절");
const QString kKeyConcepts = QStringLiteral("개념");
const QString kKeyNote = QStringLiteral("설명");
const QString kKeyWhere = QStringLiteral("위치");
const QString kKeyQuote = QStringLiteral("인용");
const QString kKeyStatus = QStringLiteral("상태");

const QString kDocumentConceptPath = QStringLiteral("../concepts/");

struct HeaderField
{
    QString strKey;
    QString strValue;
    bool bAlways = false;   // 값이 비어도 줄을 쓴다
    bool bList = false;     // 목록 키(aliases). 있는 파일이 YAML 목록으로 적었으면 그 모양으로 쓴다
    // 목록 키가 아닌 줄은 이 칸을 빼고 적는다. {} 가 있어야 그 줄들에 -Wmissing-field-initializers 경고가 나지 않는다.
    QStringList vecItems {};   // bList 의 항목(strValue 는 이것을 ", " 로 이은 것)
};

// 정해진 부분을 정준 모양으로 만든 것. 절은 머리 줄부터 마지막 줄까지(비면 빈 목록).
struct OwnedParts
{
    QString strTitle;
    QString strSummary;
    QStringList vecSectionNames;
    QVector<QStringList> vecSections;
};

// [iFirst, iLast] 줄을 vecLines 로 바꾼다. iLast == iFirst - 1 이면 iFirst 앞에 끼워 넣는다.
struct LineEdit
{
    int iFirst = 0;
    int iLast = -1;
    QStringList vecLines;
};

bool IsBlank(const QString& line)
{
    return line.trimmed().isEmpty();
}

QString EscapeLinkText(const QString& text)
{
    QString strResult;
    for (const QChar ch : text.simplified())
    {
        if (ch == QLatin1Char('\\') || ch == QLatin1Char('[') || ch == QLatin1Char(']'))
            strResult += QLatin1Char('\\');
        strResult += ch;
    }
    return strResult;
}

QString EscapeCell(const QString& text)
{
    QString strResult = text.simplified();
    strResult.replace(QLatin1Char('|'), QStringLiteral("\\|"));
    return strResult;
}

QString EscapeAttribute(const QString& text)
{
    QString strResult = text.simplified();
    strResult.replace(QLatin1Char('&'), QStringLiteral("&amp;"));
    strResult.replace(QLatin1Char('"'), QStringLiteral("&quot;"));
    strResult.replace(QLatin1Char('>'), QStringLiteral("&gt;"));
    return strResult;
}

QString MakeLink(const ConceptLink& link)
{
    return QLatin1Char('[') + EscapeLinkText(link.strName) + QStringLiteral("](") + kDocumentConceptPath
        + link.strConceptId + QStringLiteral(".md)");
}

QStringList JoinBlocks(const QVector<QStringList>& blocks)
{
    QStringList vecResult;
    for (const QStringList& vecBlock : blocks)
    {
        if (vecBlock.isEmpty())
            continue;
        if (!vecResult.isEmpty())
            vecResult << QString();
        vecResult += vecBlock;
    }
    return vecResult;
}

QString JoinText(QStringList lines)
{
    while (!lines.isEmpty() && IsBlank(lines.last()))
        lines.removeLast();
    return lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

void AppendSubItem(QStringList* lines, const QString& key, const QString& value)
{
    const QString strValue = value.simplified();
    if (!strValue.isEmpty())
        *lines << QStringLiteral("  - ") + key + QStringLiteral(": ") + strValue;
}

// 모르는 하위 항목은 읽은 글자 그대로 쓴다. 줄바꿈만 공백 하나로 바꾼다.
void AppendExtraLines(QStringList* lines, const QStringList& extraLines)
{
    static const QRegularExpression reLineBreak(QStringLiteral("[ \\t]*[\\r\\n]+[ \\t]*"));
    for (const QString& strExtra : extraLines)
    {
        QString strLine = strExtra;
        strLine.replace(reLineBreak, QStringLiteral(" "));
        strLine = strLine.trimmed();
        if (!strLine.isEmpty())
            *lines << QStringLiteral("  - ") + strLine;
    }
}

QStringList MakeSection(const QString& name, const QStringList& content)
{
    if (content.isEmpty())
        return QStringList();
    QStringList vecLines { QStringLiteral("## ") + name, QString() };
    vecLines += content;
    return vecLines;
}

QString MakeOutlineText(const KnowledgeOutlineEntry& entry)
{
    QStringList vecParts;
    for (const QString& strPart : { entry.strNumber.simplified(), entry.strTitle.simplified() })
        if (!strPart.isEmpty())
            vecParts << strPart;
    return vecParts.join(QLatin1Char(' '));
}

// 차례는 두 단계까지다. 더 깊은 항목은 둘째 단계로 펴서 쓴다.
void AppendOutlineChildren(QStringList* lines, const QVector<KnowledgeOutlineEntry>& children)
{
    for (const KnowledgeOutlineEntry& child : children)
    {
        *lines << QStringLiteral("  - ") + MakeOutlineText(child);
        AppendOutlineChildren(lines, child.vecChildren);
    }
}

QStringList BuildOutline(const QVector<KnowledgeOutlineEntry>& outline)
{
    QStringList vecLines;
    for (const KnowledgeOutlineEntry& entry : outline)
    {
        vecLines << QStringLiteral("- ") + MakeOutlineText(entry);
        AppendOutlineChildren(&vecLines, entry.vecChildren);
    }
    return vecLines;
}

QStringList BuildKeyNumbers(const QVector<KnowledgeKeyNumber>& numbers)
{
    QStringList vecLines;
    for (const KnowledgeKeyNumber& number : numbers)
    {
        QString strLine = QStringLiteral("- **") + number.strValue.simplified() + QStringLiteral("**");
        const QString strUnit = number.strUnit.simplified();
        const QString strLabel = number.strLabel.simplified();
        if (!strUnit.isEmpty())
            strLine += QLatin1Char(' ') + strUnit;
        if (!strLabel.isEmpty())
            strLine += QStringLiteral(" — ") + strLabel;
        vecLines << strLine;
    }
    return vecLines;
}

QStringList BuildCards(const QVector<KnowledgeCard>& cards)
{
    QStringList vecLines;
    for (const KnowledgeCard& card : cards)
    {
        QString strLine = QStringLiteral("- ") + card.strClaim.simplified();
        if (!card.strId.isEmpty())
            strLine += QStringLiteral(" <!-- card:") + card.strId + QStringLiteral(" -->");
        vecLines << strLine;
        AppendSubItem(&vecLines, kKeyEvidence, card.strEvidence);
        AppendSubItem(&vecLines, kKeySection, card.strSection);
        QStringList vecLinks;
        for (const ConceptLink& link : card.vecConcepts)
            vecLinks << MakeLink(link);
        AppendSubItem(&vecLines, kKeyConcepts, vecLinks.join(QStringLiteral(", ")));
        AppendExtraLines(&vecLines, card.vecExtraLines);
    }
    return vecLines;
}

QStringList BuildRelations(const QVector<KnowledgeRelation>& relations)
{
    const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
    QStringList vecLines;
    for (const KnowledgeRelation& relation : relations)
    {
        const QString strArrow = vocabulary.IsUndirected(relation.strKind) ? QStringLiteral("↔") : QStringLiteral("→");
        vecLines << QStringLiteral("- ") + MakeLink(relation.fromConcept) + QStringLiteral(" **")
                + vocabulary.ToRelationLabel(relation.strKind).simplified() + QStringLiteral("** ") + strArrow
                + QLatin1Char(' ') + MakeLink(relation.toConcept);
        AppendSubItem(&vecLines, kKeyNote, relation.strNote);
        AppendSubItem(&vecLines, kKeyWhere, relation.strWhere);
        const QString strQuote = relation.strQuote.simplified();
        if (!strQuote.isEmpty())
            AppendSubItem(&vecLines, kKeyQuote, QLatin1Char('"') + strQuote + QLatin1Char('"'));
        if (relation.eStatus == ERelationStatus::PROPOSED)
            AppendSubItem(&vecLines, kKeyStatus, vocabulary.ToStatusLabel(QStringLiteral("proposed")));
        AppendExtraLines(&vecLines, relation.vecExtraLines);
    }
    return vecLines;
}

QString MakeAttribute(const QString& name, const QString& value)
{
    return name + QStringLiteral("=\"") + EscapeAttribute(value) + QLatin1Char('"');
}

QString MakeDataComment(const KnowledgeTable& table)
{
    const QString strComma = QStringLiteral(",");
    QStringList vecAttributes;
    vecAttributes << MakeAttribute(QStringLiteral("x"), table.strX);
    vecAttributes << MakeAttribute(QStringLiteral("y"), table.vecY.join(strComma));
    if (!table.strSeries.simplified().isEmpty())
        vecAttributes << MakeAttribute(QStringLiteral("series"), table.strSeries);
    if (!table.vecOrdered.isEmpty())
        vecAttributes << MakeAttribute(QStringLiteral("ordered"), table.vecOrdered.join(strComma));
    if (table.bCumulative)
        vecAttributes << MakeAttribute(QStringLiteral("cumulative"), QStringLiteral("yes"));
    if (!table.vecConceptIds.isEmpty())
        vecAttributes << MakeAttribute(QStringLiteral("concepts"), table.vecConceptIds.join(strComma));
    if (!table.strCondition.simplified().isEmpty())
        vecAttributes << MakeAttribute(QStringLiteral("condition"), table.strCondition);
    if (!table.strWhere.simplified().isEmpty())
        vecAttributes << MakeAttribute(QStringLiteral("where"), table.strWhere);
    return QStringLiteral("<!-- ocean:data ") + vecAttributes.join(QLatin1Char(' ')) + QStringLiteral(" -->");
}

QString MakeRow(const QStringList& cells)
{
    QStringList vecCells;
    for (const QString& strCell : cells)
        vecCells << EscapeCell(strCell);
    return QStringLiteral("| ") + vecCells.join(QStringLiteral(" | ")) + QStringLiteral(" |");
}

QStringList BuildTable(const KnowledgeTable& table)
{
    QStringList vecLines { QStringLiteral("### ") + table.strTitle.simplified(), QString(), MakeDataComment(table) };
    if (table.vecColumns.isEmpty())
        return vecLines;

    QStringList vecHeader;
    QStringList vecAlignment;
    for (int iColumn = 0; iColumn < table.vecColumns.size(); ++iColumn)
    {
        const KnowledgeColumn& column = table.vecColumns[iColumn];
        const QString strUnit = column.strUnit.simplified();
        vecHeader << (strUnit.isEmpty() ? column.strName : column.strName + QStringLiteral(" (") + strUnit + QLatin1Char(')'));
        vecAlignment << (KnowledgeText::ClassifyColumn(table, iColumn) == EColumnKind::QUANTITY ? QStringLiteral("--:") : QStringLiteral("---"));
    }
    vecLines << QString() << MakeRow(vecHeader);
    vecLines << QStringLiteral("| ") + vecAlignment.join(QStringLiteral(" | ")) + QStringLiteral(" |");
    for (const QStringList& vecRow : table.vecRows)
        vecLines << MakeRow(vecRow);
    return vecLines;
}

QStringList BuildTables(const QVector<KnowledgeTable>& tables)
{
    QVector<QStringList> vecBlocks;
    for (const KnowledgeTable& table : tables)
        vecBlocks << BuildTable(table);
    return JoinBlocks(vecBlocks);
}

OwnedParts MakeDocumentParts(const KnowledgeDocument& document)
{
    OwnedParts parts;
    parts.strTitle = document.strTitle.simplified();
    parts.strSummary = document.strSummary.simplified();
    parts.vecSectionNames = QStringList { kSectionOutline, kSectionKeyNumbers, kSectionCards, kSectionRelations, kSectionData };
    parts.vecSections = {
        MakeSection(kSectionOutline, BuildOutline(document.vecOutline)),
        MakeSection(kSectionKeyNumbers, BuildKeyNumbers(document.vecKeyNumbers)),
        MakeSection(kSectionCards, BuildCards(document.vecCards)),
        MakeSection(kSectionRelations, BuildRelations(document.vecRelations)),
        MakeSection(kSectionData, BuildTables(document.vecTables)),
    };
    return parts;
}

OwnedParts MakeConceptParts(const KnowledgeConcept& concept)
{
    OwnedParts parts;
    parts.strTitle = concept.strName.simplified();
    parts.strSummary = concept.strSummary.simplified();
    return parts;
}

QStringList MakeTitleBlock(const OwnedParts& parts)
{
    return QStringList { QStringLiteral("# ") + parts.strTitle };
}

QStringList MakeSummaryBlock(const OwnedParts& parts)
{
    return parts.strSummary.isEmpty() ? QStringList() : QStringList { KnowledgeMarkdownLayout::EscapeParagraphStart(parts.strSummary) };
}

// 새 파일로 쓸 때의 제목부터 마지막 정해진 절까지
QStringList BuildCanonicalBody(const OwnedParts& parts)
{
    QVector<QStringList> vecBlocks { MakeTitleBlock(parts), MakeSummaryBlock(parts) };
    vecBlocks += parts.vecSections;
    return JoinBlocks(vecBlocks);
}

QString ComputeDigestOf(const OwnedParts& parts)
{
    const QByteArray baHash = QCryptographicHash::hash(JoinText(BuildCanonicalBody(parts)).toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(baHash.toHex());
}

bool IsWritten(const HeaderField& field)
{
    return field.bAlways || !field.strValue.isEmpty();
}

// YAML 따옴표 없는 값으로 써도 되는가: ": " · " #" 가 없고, ':' 로 끝나지 않고, 표시 글자로 시작하지 않는다.
bool IsPlainSafe(const QString& value)
{
    static const QString strIndicators = QStringLiteral("-?:,[]{}#&*!|>'\"%@`");
    if (value.isEmpty())
        return true;
    if (strIndicators.contains(value.at(0)))
        return false;
    return !value.contains(QStringLiteral(": ")) && !value.contains(QStringLiteral(" #")) && !value.endsWith(QLatin1Char(':'));
}

QString QuoteDouble(const QString& value)
{
    QString strResult = value;
    strResult.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    strResult.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + strResult + QLatin1Char('"');
}

QString QuoteSingle(const QString& value)
{
    QString strResult = value;
    strResult.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QLatin1Char('\'') + strResult + QLatin1Char('\'');
}

// 따옴표로 감쌌던 값은 그 따옴표로, 아니면 안전할 때만 그대로, 아니면 큰따옴표로
QString MakeScalar(const QString& value, EHeaderStyle style)
{
    if (value.isEmpty())
        return value;
    if (style == EHeaderStyle::DOUBLE_QUOTED)
        return QuoteDouble(value);
    if (style == EHeaderStyle::SINGLE_QUOTED)
        return QuoteSingle(value);
    return IsPlainSafe(value) ? value : QuoteDouble(value);
}

// 흐름 목록 안에서는 , [ ] { } 도 구분 글자다
QString MakeFlowItem(const QString& item)
{
    static const QRegularExpression reFlowIndicator(QStringLiteral("[,\\[\\]{}]"));
    return IsPlainSafe(item) && !reFlowIndicator.match(item).hasMatch() ? item : QuoteDouble(item);
}

QString MakeHeaderLine(const QString& key, const QString& value)
{
    return value.isEmpty() ? key + QLatin1Char(':') : key + QStringLiteral(": ") + value;
}

// 있는 키의 모양(style · 블록 목록 들여쓰기)을 지켜 줄을 만든다. 목록 키가 아닌데 목록으로 적혔으면 한 값으로 쓴다.
QStringList MakeHeaderLines(const HeaderField& field, EHeaderStyle style, const QString& itemIndent)
{
    if (field.bList && style == EHeaderStyle::BLOCK_LIST)
    {
        QStringList vecLines { field.strKey + QLatin1Char(':') };
        for (const QString& strItem : field.vecItems)
            vecLines << itemIndent + QStringLiteral("- ") + MakeScalar(strItem, EHeaderStyle::PLAIN);
        return vecLines;
    }
    if (field.bList && style == EHeaderStyle::FLOW_LIST)
    {
        QStringList vecItems;
        for (const QString& strItem : field.vecItems)
            vecItems << MakeFlowItem(strItem);
        return { field.strKey + QStringLiteral(": [") + vecItems.join(QStringLiteral(", ")) + QLatin1Char(']') };
    }
    const bool bQuoted = style == EHeaderStyle::DOUBLE_QUOTED || style == EHeaderStyle::SINGLE_QUOTED;
    return { MakeHeaderLine(field.strKey, MakeScalar(field.strValue, bQuoted ? style : EHeaderStyle::PLAIN)) };
}

QVector<HeaderField> MakeDocumentHeader(const KnowledgeDocument& document, const QString& digest)
{
    return {
        { QStringLiteral("type"), QStringLiteral("document"), true },
        { QStringLiteral("format"), QString::number(document.iFormat), true },
        { QStringLiteral("id"), document.strId.simplified(), true },
        { QStringLiteral("source"), document.strSource.simplified(), false },
        { QStringLiteral("fileInfo"), document.strFileInfo.simplified(), false },
        { QStringLiteral("meta"), document.strMeta.simplified(), false },
        { QStringLiteral("learned"), document.strLearned.simplified(), false },
        { QStringLiteral("analysis"), document.strAnalysisId.simplified(), false },
        { QStringLiteral("rules"), QString::number(document.iRules), true },
        { QStringLiteral("edited"), document.bEdited ? QStringLiteral("true") : QStringLiteral("false"), true },
        { QStringLiteral("digest"), digest, true },
    };
}

QVector<HeaderField> MakeConceptHeader(const KnowledgeConcept& concept)
{
    QStringList vecAliases;
    for (const QString& strAlias : concept.vecAliases)
    {
        const QString strTrimmed = strAlias.simplified();
        if (!strTrimmed.isEmpty())
            vecAliases << strTrimmed;
    }
    return {
        { QStringLiteral("type"), QStringLiteral("concept"), true },
        { QStringLiteral("format"), QString::number(concept.iFormat), true },
        { QStringLiteral("id"), concept.strId.simplified(), true },
        { QStringLiteral("kind"), concept.strKind.simplified(), false },
        { QStringLiteral("aliases"), vecAliases.join(QStringLiteral(", ")), false, true, vecAliases },
        { QStringLiteral("year"), concept.strYear.simplified(), false },
    };
}

QStringList BuildHeader(const QVector<HeaderField>& fields)
{
    QStringList vecLines { QStringLiteral("---") };
    for (const HeaderField& field : fields)
        if (IsWritten(field))
            vecLines += MakeHeaderLines(field, EHeaderStyle::PLAIN, QString());
    vecLines << QStringLiteral("---");
    return vecLines;
}

int FindField(const QVector<HeaderField>& fields, const QString& key)
{
    for (int iField = 0; iField < fields.size(); ++iField)
        if (fields[iField].strKey == key)
            return iField;
    return -1;
}

// 아는 키는 제자리에서 값만 바꾸고(그 키의 모양을 지킨다. 블록 목록은 항목 줄까지 통째로. 겹친 키 · 빈 선택 키는 뺀다),
// 모르는 줄은 그대로, 없던 아는 키는 닫는 --- 앞에 정준 순서로
QStringList SpliceHeader(const KnowledgeLayout& layout, const QVector<HeaderField>& fields)
{
    QHash<int, int> hashHeaderByLine;   // 줄 → layout.vecHeader 번호
    for (int iHeader = 0; iHeader < layout.vecHeader.size(); ++iHeader)
        hashHeaderByLine.insert(layout.vecHeader[iHeader].iLine, iHeader);

    QStringList vecLines { layout.vecLines[layout.iFrontMatterStart] };
    QSet<QString> setSeen;
    for (int iLine = layout.iFrontMatterStart + 1; iLine < layout.iFrontMatterEnd; ++iLine)
    {
        const int iHeader = hashHeaderByLine.value(iLine, -1);
        const int iField = iHeader < 0 ? -1 : FindField(fields, layout.vecHeader[iHeader].strKey);
        if (iField < 0)
        {
            vecLines << layout.vecLines[iLine];
            continue;
        }
        const KnowledgeHeaderLine& header = layout.vecHeader[iHeader];
        iLine = header.iLastLine;
        if (setSeen.contains(header.strKey))
            continue;
        setSeen.insert(header.strKey);
        if (IsWritten(fields[iField]))
            vecLines += MakeHeaderLines(fields[iField], header.eStyle, header.strItemIndent);
    }
    for (const HeaderField& field : fields)
        if (!setSeen.contains(field.strKey) && IsWritten(field))
            vecLines += MakeHeaderLines(field, EHeaderStyle::PLAIN, QString());
    vecLines << layout.vecLines[layout.iFrontMatterEnd];
    return vecLines;
}

// 바꿀 줄이 없으면 그 범위 뒤의 빈 줄까지 함께 지운다(앞뒤 빈 줄이 둘로 겹치지 않게).
LineEdit MakeReplace(const QStringList& lines, int first, int last, const QStringList& newLines)
{
    int iLast = last;
    if (newLines.isEmpty())
        while (iLast + 1 < lines.size() && IsBlank(lines[iLast + 1]))
            ++iLast;
    return { first, iLast, newLines };
}

// position 앞에 끼워 넣는다. 이웃한 줄이 비지 않았으면 빈 줄 하나로 띄운다.
LineEdit MakeInsert(const QStringList& lines, int position, const QStringList& block)
{
    QStringList vecLines;
    if (position > 0 && !IsBlank(lines[position - 1]))
        vecLines << QString();
    vecLines += block;
    if (position < lines.size() && !IsBlank(lines[position]))
        vecLines << QString();
    return { position, position - 1, vecLines };
}

void ApplyEdits(QStringList* lines, QVector<LineEdit> edits)
{
    std::sort(edits.begin(), edits.end(), [](const LineEdit& a, const LineEdit& b)
    {
        return a.iFirst != b.iFirst ? a.iFirst > b.iFirst : a.iLast > b.iLast;
    });
    for (const LineEdit& edit : edits)
    {
        for (int iLine = edit.iLast; iLine >= edit.iFirst; --iLine)
            lines->removeAt(iLine);
        for (int iIndex = 0; iIndex < edit.vecLines.size(); ++iIndex)
            lines->insert(edit.iFirst + iIndex, edit.vecLines[iIndex]);
    }
}

QStringList AppendUnread(const QString& name, const QStringList& section, const QStringList& unread)
{
    if (unread.isEmpty())
        return section;
    QStringList vecLines = section.isEmpty() ? QStringList { QStringLiteral("## ") + name } : section;
    vecLines << QString();
    vecLines += unread;
    return vecLines;
}

// 제목 뒤 첫 절 앞까지(머리 부분)의 마지막 비지 않은 줄 다음 자리. 한 줄 뒤에 사용자가 둔 블록이 있으면 그 뒤다.
int FindHeadEnd(const KnowledgeLayout& layout)
{
    int iEnd = layout.vecSections.isEmpty() ? layout.vecLines.size() : layout.vecSections.first().iHeadingLine;
    while (iEnd > layout.iTitleLine + 1 && IsBlank(layout.vecLines[iEnd - 1]))
        --iEnd;
    return iEnd;
}

// 제목이 없는 글에서 제목부터 넣을 자리: 첫 절 앞, 절이 없으면 본문 끝
int FindHeadPosition(const KnowledgeLayout& layout, int bodyStart)
{
    if (!layout.vecSections.isEmpty())
        return layout.vecSections.first().iHeadingLine;
    int iLast = layout.vecLines.size() - 1;
    while (iLast >= bodyStart && IsBlank(layout.vecLines[iLast]))
        --iLast;
    return std::max(iLast + 1, bodyStart);
}

QString SpliceInto(const QString& existingText, const QVector<HeaderField>& header, const OwnedParts& parts)
{
    const KnowledgeLayout layout = KnowledgeMarkdownLayout::Scan(existingText);
    QStringList vecLines = layout.vecLines;
    QVector<LineEdit> vecEdits;

    const bool bHasHeader = layout.bHasFrontMatter && layout.bFrontMatterClosed;
    if (bHasHeader)
        vecEdits.push_back({ layout.iFrontMatterStart, layout.iFrontMatterEnd, SpliceHeader(layout, header) });
    const int iBodyStart = bHasHeader ? layout.iFrontMatterEnd + 1 : 0;

    const int iSectionCount = parts.vecSectionNames.size();
    const UnreadLines hashUnread = iSectionCount > 0 ? KnowledgeMarkdownReader::ReadUnreadLines(layout) : UnreadLines();

    // 정해진 절마다 처음 나온 자리. 같은 이름이 또 나오면 지운다(내용은 첫 자리에 다시 쓴다).
    QVector<int> vecFirst(iSectionCount, -1);
    for (int iSection = 0; iSection < layout.vecSections.size(); ++iSection)
    {
        const KnowledgeSectionRange& section = layout.vecSections[iSection];
        const int iOwned = parts.vecSectionNames.indexOf(section.strName);
        if (iOwned < 0)
            continue;
        if (vecFirst[iOwned] < 0)
            vecFirst[iOwned] = iSection;
        else
            vecEdits.push_back(MakeReplace(vecLines, section.iHeadingLine, section.iLastLine, QStringList()));
    }

    // 없던 절은 정준 순서상 바로 앞에 있는 정해진 절 뒤에, 그런 절이 없으면 한 줄(없으면 제목) 뒤에 붙인다.
    QVector<QVector<QStringList>> vecFollowers(iSectionCount);
    QVector<QStringList> vecHeadFollowers;
    int iAnchor = -1;
    for (int iOwned = 0; iOwned < iSectionCount; ++iOwned)
    {
        const QStringList vecBlock = AppendUnread(parts.vecSectionNames[iOwned], parts.vecSections[iOwned],
                                                  hashUnread.value(parts.vecSectionNames[iOwned]));
        if (vecFirst[iOwned] >= 0)
        {
            iAnchor = iOwned;
            vecFollowers[iOwned].prepend(vecBlock);
        }
        else if (iAnchor >= 0)
            vecFollowers[iAnchor].push_back(vecBlock);
        else
            vecHeadFollowers.push_back(vecBlock);
    }
    for (int iOwned = 0; iOwned < iSectionCount; ++iOwned)
    {
        if (vecFirst[iOwned] < 0)
            continue;
        const KnowledgeSectionRange& section = layout.vecSections[vecFirst[iOwned]];
        vecEdits.push_back(MakeReplace(vecLines, section.iHeadingLine, section.iLastLine, JoinBlocks(vecFollowers[iOwned])));
    }

    // 한 줄은 그 문단 줄만 바꾼다. 한 줄(없으면 제목) 뒤에 넣을 절은, 그 뒤에 사용자가 둔 블록이 있으면
    // 그 블록 뒤(머리 부분 끝)에 넣는다. 블록이 새 절 안으로 들어가 기록으로 읽히지 않게.
    const QStringList vecTitle = MakeTitleBlock(parts);
    const QStringList vecSummary = MakeSummaryBlock(parts);
    const QStringList vecHeadBlock = JoinBlocks(vecHeadFollowers);
    const int iHeadEnd = layout.iTitleLine >= 0 ? FindHeadEnd(layout) : -1;
    if (layout.iTitleLine >= 0 && layout.iSummaryFirst >= 0)
    {
        vecEdits.push_back({ layout.iTitleLine, layout.iTitleLine, vecTitle });
        if (iHeadEnd == layout.iSummaryLast + 1)
            vecEdits.push_back(MakeReplace(vecLines, layout.iSummaryFirst, layout.iSummaryLast,
                                           JoinBlocks(QVector<QStringList> { vecSummary } + vecHeadFollowers)));
        else
        {
            vecEdits.push_back(MakeReplace(vecLines, layout.iSummaryFirst, layout.iSummaryLast, vecSummary));
            if (!vecHeadBlock.isEmpty())
                vecEdits.push_back(MakeInsert(vecLines, iHeadEnd, vecHeadBlock));
        }
    }
    else if (layout.iTitleLine >= 0 && iHeadEnd == layout.iTitleLine + 1)
        vecEdits.push_back({ layout.iTitleLine, layout.iTitleLine,
                             JoinBlocks(QVector<QStringList> { vecTitle, vecSummary } + vecHeadFollowers) });
    else if (layout.iTitleLine >= 0)
    {
        QStringList vecHead = JoinBlocks(QVector<QStringList> { vecTitle, vecSummary });
        if (!vecSummary.isEmpty() && !IsBlank(vecLines[layout.iTitleLine + 1]))
            vecHead << QString();
        vecEdits.push_back({ layout.iTitleLine, layout.iTitleLine, vecHead });
        if (!vecHeadBlock.isEmpty())
            vecEdits.push_back(MakeInsert(vecLines, iHeadEnd, vecHeadBlock));
    }
    else
        vecEdits.push_back(MakeInsert(vecLines, FindHeadPosition(layout, iBodyStart),
                                      JoinBlocks(QVector<QStringList> { vecTitle, vecSummary } + vecHeadFollowers)));

    ApplyEdits(&vecLines, vecEdits);
    if (!bHasHeader)
    {
        QStringList vecHeader = BuildHeader(header);
        if (!vecLines.isEmpty() && !IsBlank(vecLines.first()))
            vecHeader << QString();
        vecLines = vecHeader + vecLines;
    }
    return JoinText(vecLines);
}

QString Write(const QString& existingText, const QVector<HeaderField>& header, const OwnedParts& parts)
{
    if (!existingText.trimmed().isEmpty())
        return SpliceInto(existingText, header, parts);
    QStringList vecLines = BuildHeader(header);
    vecLines << QString();
    vecLines += BuildCanonicalBody(parts);
    return JoinText(vecLines);
}
}

namespace KnowledgeMarkdownWriter
{
QString WriteDocument(const KnowledgeDocument& document, const QString& existingText)
{
    const OwnedParts parts = MakeDocumentParts(document);
    return Write(existingText, MakeDocumentHeader(document, ComputeDigestOf(parts)), parts);
}

QString WriteConcept(const KnowledgeConcept& concept, const QString& existingText)
{
    return Write(existingText, MakeConceptHeader(concept), MakeConceptParts(concept));
}

QString ComputeDigest(const KnowledgeDocument& document)
{
    return ComputeDigestOf(MakeDocumentParts(document));
}
}
