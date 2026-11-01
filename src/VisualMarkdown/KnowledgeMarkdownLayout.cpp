#include "VisualMarkdown/KnowledgeMarkdownLayout.h"

#include <QRegularExpression>

#include <algorithm>

QString KnowledgeLayout::FindHeaderValue(const QString& key) const
{
    const KnowledgeHeaderLine* pHeader = FindHeader(key);
    return pHeader != nullptr ? pHeader->strValue : QString();
}

const KnowledgeHeaderLine* KnowledgeLayout::FindHeader(const QString& key) const
{
    for (const KnowledgeHeaderLine& line : vecHeader)
        if (line.strKey == key)
            return &line;
    return nullptr;
}

bool KnowledgeLayout::HasHeaderKey(const QString& key) const
{
    for (const KnowledgeHeaderLine& line : vecHeader)
        if (line.strKey == key)
            return true;
    return false;
}

namespace
{
constexpr QChar kByteOrderMark(0xFEFF);
constexpr int kTitlePrefixLength = 2;     // "# "
constexpr int kSectionPrefixLength = 3;   // "## "
constexpr int kQuoteMarkCount = 2;        // 앞뒤 따옴표
constexpr int kBracketCount = 2;          // 흐름 목록의 [ ]
constexpr int kTabStop = 4;               // 줄 앞 탭은 다음 4의 배수 칸까지(CommonMark)
constexpr int kCodeIndent = 4;            // 담는 곳보다 이만큼 들인 줄은 들여 쓴 코드다. ``` 줄이어도 울타리가 아니다(CommonMark)
constexpr int kMaxListMarkerSpaces = 4;   // 목록 표지 뒤 공백이 이보다 많으면 내용은 표지 뒤 한 칸부터다(CommonMark)

// YAML 따옴표 값: "…"(\" · \\ 를 되돌린다) · '…'('' 를 되돌린다). 앞뒤를 감싼 따옴표 한 쌍이 아니면 그대로 둔다.
QString UnquoteScalar(const QString& value, EHeaderStyle* style)
{
    *style = EHeaderStyle::PLAIN;
    if (value.size() < kQuoteMarkCount)
        return value;
    const QChar chQuote = value.at(0);
    if (chQuote != QLatin1Char('"') && chQuote != QLatin1Char('\''))
        return value;
    const bool bDouble = chQuote == QLatin1Char('"');
    QString strResult;
    for (int iIndex = 1; iIndex < value.size(); ++iIndex)
    {
        const QChar ch = value.at(iIndex);
        const bool bHasNext = iIndex + 1 < value.size();
        if (bDouble && ch == QLatin1Char('\\') && bHasNext
            && (value.at(iIndex + 1) == QLatin1Char('"') || value.at(iIndex + 1) == QLatin1Char('\\')))
        {
            strResult += value.at(++iIndex);
            continue;
        }
        if (ch != chQuote)
        {
            strResult += ch;
            continue;
        }
        if (!bDouble && bHasNext && value.at(iIndex + 1) == QLatin1Char('\''))
        {
            strResult += QLatin1Char('\'');
            ++iIndex;
            continue;
        }
        if (bHasNext)
            return value;   // 닫는 따옴표 뒤에 글이 더 있다
        *style = bDouble ? EHeaderStyle::DOUBLE_QUOTED : EHeaderStyle::SINGLE_QUOTED;
        return strResult;
    }
    return value;   // 닫히지 않았다
}

// 흐름 목록 `[a, "b, c", 'd']` 의 괄호 안. 따옴표 안의 쉼표는 나누지 않는다.
QStringList SplitFlowList(const QString& body)
{
    QStringList vecParts;
    QString strPart;
    QChar chQuote;
    for (int iIndex = 0; iIndex < body.size(); ++iIndex)
    {
        const QChar ch = body.at(iIndex);
        if (!chQuote.isNull())
        {
            strPart += ch;
            if (chQuote == QLatin1Char('"') && ch == QLatin1Char('\\') && iIndex + 1 < body.size())
                strPart += body.at(++iIndex);
            else if (ch == chQuote && chQuote == QLatin1Char('\'') && iIndex + 1 < body.size() && body.at(iIndex + 1) == QLatin1Char('\''))
                strPart += body.at(++iIndex);
            else if (ch == chQuote)
                chQuote = QChar();
            continue;
        }
        if ((ch == QLatin1Char('"') || ch == QLatin1Char('\'')) && strPart.trimmed().isEmpty())
            chQuote = ch;
        if (ch == QLatin1Char(','))
        {
            vecParts << strPart;
            strPart.clear();
            continue;
        }
        strPart += ch;
    }
    vecParts << strPart;

    QStringList vecItems;
    for (const QString& strRaw : vecParts)
    {
        EHeaderStyle eIgnored = EHeaderStyle::PLAIN;
        const QString strItem = UnquoteScalar(strRaw.simplified(), &eIgnored);
        if (!strItem.isEmpty())
            vecItems << strItem;
    }
    return vecItems;
}

// 블록 목록 항목 줄 `  - 값`(들여쓰기가 없어도 된다)
bool MatchBlockItem(const QString& line, QString* indent, QString* item)
{
    static const QRegularExpression reItem(QStringLiteral(R"re(^([ \t]*)-(?:[ \t]+(.*))?$)re"));
    const QRegularExpressionMatch match = reItem.match(line);
    if (!match.hasMatch())
        return false;
    *indent = match.captured(1);
    *item = match.captured(2).simplified();
    return true;
}

// 키 줄 뒤의 값을 읽는다. 값이 비고 다음 줄들이 `- 항목` 이면 블록 목록, `[…]` 면 흐름 목록, 아니면 (따옴표를 벗긴) 한 값.
void ReadHeaderValue(const QStringList& lines, const QString& value, KnowledgeHeaderLine* header)
{
    if (value.isEmpty())
    {
        QString strIndent, strItem;
        for (int iItem = header->iLine + 1; iItem < lines.size() && MatchBlockItem(lines[iItem], &strIndent, &strItem); ++iItem)
        {
            if (header->eStyle != EHeaderStyle::BLOCK_LIST)
            {
                header->eStyle = EHeaderStyle::BLOCK_LIST;
                header->strItemIndent = strIndent;
            }
            EHeaderStyle eIgnored = EHeaderStyle::PLAIN;
            const QString strValue = UnquoteScalar(strItem, &eIgnored);
            if (!strValue.isEmpty())
                header->vecItems << strValue;
            header->iLastLine = iItem;
        }
    }
    else if (value.startsWith(QLatin1Char('[')) && value.endsWith(QLatin1Char(']')))
    {
        header->eStyle = EHeaderStyle::FLOW_LIST;
        header->vecItems = SplitFlowList(value.mid(1, value.size() - kBracketCount));
        header->strValue = value;   // 목록 키가 아니면 원문 그대로 한 값이다
        return;
    }
    else
    {
        header->strValue = UnquoteScalar(value, &header->eStyle);
        return;
    }
    header->strValue = header->vecItems.join(QStringLiteral(", "));
}

bool IsBlankLine(const QString& line)
{
    return line.trimmed().isEmpty();
}

// column 칸에 있는 들여쓰기 글자 ch 다음의 칸. 공백은 한 칸, 탭은 다음 4의 배수 칸까지다(CommonMark).
// 공백 · 탭이 아니면 -1.
int AdvanceIndent(int column, QChar ch)
{
    if (ch == QLatin1Char(' '))
        return column + 1;
    if (ch == QLatin1Char('\t'))
        return column + kTabStop - column % kTabStop;
    return -1;
}

// 목록 표지 줄(`-` · `*` · `+` · `1.` · `1)` 뒤에 공백이나 줄 끝)이면 그 항목의 내용 들여쓰기(CommonMark), 아니면 -1.
// 내용 칸 = 표지 끝 칸 + 뒤 공백 칸. 뒤가 비었거나 공백이 4칸을 넘으면(들여 쓴 코드로 시작) 표지 끝 칸 + 1.
int MeasureListContentIndent(const QString& line)
{
    static const QRegularExpression reMarker(QStringLiteral(R"re(^(?:[-*+]|\d{1,9}[.)])(?=[ \t]|$))re"));
    int iIndex = 0;
    const int iIndent = KnowledgeMarkdownLayout::MeasureIndent(line, &iIndex);
    const QRegularExpressionMatch match = reMarker.match(line.mid(iIndex));
    if (!match.hasMatch())
        return -1;
    const int iMarkerEnd = iIndent + match.capturedLength();
    iIndex += match.capturedLength();
    int iColumn = iMarkerEnd;
    for (; iIndex < line.size(); ++iIndex)
    {
        const int iNextColumn = AdvanceIndent(iColumn, line[iIndex]);
        if (iNextColumn < 0)
            break;
        iColumn = iNextColumn;
    }
    if (iIndex >= line.size() || iColumn - iMarkerEnd > kMaxListMarkerSpaces)
        return iMarkerEnd + 1;
    return iColumn;
}

QString TrimLeft(const QString& line)
{
    int iStart = 0;
    while (iStart < line.size() && line[iStart].isSpace())
        ++iStart;
    return line.mid(iStart);
}

bool IsHeadingLine(const QString& line)
{
    return TrimLeft(line).startsWith(QLatin1Char('#'));
}

// 구분선 `---` · `***` · `___`
bool IsThematicBreak(const QString& line)
{
    static const QRegularExpression reBreak(QStringLiteral(R"re(^\s*(?:(?:-\s*){3,}|(?:\*\s*){3,}|(?:_\s*){3,})$)re"));
    return reBreak.match(line).hasMatch();
}

// 한 줄로 닫힌 HTML 주석 `<!-- … -->`(GFM: 주석 블록은 --> 가 있는 줄에서 끝난다)
bool IsOneLineHtmlComment(const QString& line)
{
    const QString strBody = TrimLeft(line);
    return strBody.startsWith(QStringLiteral("<!--")) && strBody.contains(QStringLiteral("-->"));
}

// 한 줄 블록(제목 · 구분선 · 한 줄 HTML 주석). 다음 줄은 새 블록이다.
bool IsSingleLineBlock(const QString& line)
{
    return IsHeadingLine(line) || IsThematicBreak(line) || IsOneLineHtmlComment(line);
}

// 보통 문단이 아닌 블록의 첫 줄: 그림 · 인용 · 목록 · 표 · HTML · 울타리 · 제목 · 구분선
bool StartsOtherBlock(const QString& line)
{
    static const QRegularExpression reListItem(QStringLiteral(R"re(^(?:[-*+]|\d{1,9}[.)])(?:[ \t]|$))re"));
    const QString strBody = TrimLeft(line);
    return strBody.startsWith(QStringLiteral("![")) || strBody.startsWith(QLatin1Char('>'))
        || strBody.startsWith(QLatin1Char('|')) || strBody.startsWith(QLatin1Char('<'))
        || reListItem.match(strBody).hasMatch() || KnowledgeMarkdownLayout::IsFenceLine(line) || IsSingleLineBlock(line);
}

int FindEscapeSlot(const QString& text);

// text 가 FindEscapeSlot 자리에 \ 하나를 넣은 꼴이면 그 \ 의 자리, 아니면 -1
int FindInsertedEscape(const QString& text)
{
    static const QRegularExpression reDigitsThenBackslash(QStringLiteral(R"re(^(\d{0,9})\\)re"));
    const QRegularExpressionMatch match = reDigitsThenBackslash.match(text);
    if (!match.hasMatch())
        return -1;
    const int iSlot = match.capturedLength(1);
    return FindEscapeSlot(QString(text).remove(iSlot, 1)) == iSlot ? iSlot : -1;
}

// 문단 첫 줄로 쓸 글이 블록을 여는 줄로 읽히지 않게 GFM 이스케이프 \ 하나를 넣을 자리. 넣지 않아도 되면 -1.
// 번호 목록(`1. ` · `1) `)은 숫자 뒤(`1\.`), 나머지는 맨 앞(`\<` `\-` `\#` …).
// 이미 그 자리에 \ 를 넣은 꼴인 글도 \ 를 하나 더 넣는다. 그래서 읽을 때 그 \ 하나만 빼면 쓴 글 그대로다.
int FindEscapeSlot(const QString& text)
{
    static const QRegularExpression reOrderedStart(QStringLiteral(R"re(^(\d{1,9})[.)](?:[ \t]|$))re"));
    if (text.isEmpty() || text.at(0).isSpace())
        return -1;
    if (StartsOtherBlock(text))
    {
        const QRegularExpressionMatch match = reOrderedStart.match(text);
        return match.hasMatch() ? match.capturedLength(1) : 0;
    }
    return FindInsertedEscape(text);
}

// EscapeParagraphStart 가 넣은 \ 하나를 뺀다
QString UnescapeParagraphStart(const QString& text)
{
    const int iSlot = FindInsertedEscape(text);
    return iSlot < 0 ? text : QString(text).remove(iSlot, 1);
}

// 울타리 블록 다음 줄(KnowledgeMarkdownLayout::FindFenceEnd 의 끝 줄 다음). 닫히지 않으면 end.
int SkipFence(const QStringList& lines, int open, int end)
{
    const int iLast = KnowledgeMarkdownLayout::FindFenceEnd(lines, open, end);
    return iLast < 0 ? end : iLast + 1;
}

// 한 줄 = 제목 뒤 첫 ## 앞에서 처음 만나는 보통 문단. 다른 블록은 건너뛴다(빈 줄까지. 울타리는 닫는 줄까지, 제목 · 구분선은 그 줄만).
// 문단은 빈 줄이나 다른 블록의 첫 줄에서 끝난다.
void FindSummary(KnowledgeLayout* layout)
{
    const QStringList& vecLines = layout->vecLines;
    const int iEnd = layout->vecSections.isEmpty() ? vecLines.size() : layout->vecSections.first().iHeadingLine;
    int iLine = layout->iTitleLine + 1;
    while (iLine < iEnd)
    {
        const QString& strLine = vecLines[iLine];
        if (IsBlankLine(strLine) || IsSingleLineBlock(strLine))
        {
            ++iLine;
            continue;
        }
        if (KnowledgeMarkdownLayout::IsFenceLine(strLine))
        {
            iLine = SkipFence(vecLines, iLine, iEnd);
            continue;
        }
        if (StartsOtherBlock(strLine))
        {
            ++iLine;
            while (iLine < iEnd && !IsBlankLine(vecLines[iLine]) && !KnowledgeMarkdownLayout::IsFenceLine(vecLines[iLine])
                   && !IsSingleLineBlock(vecLines[iLine]))
                ++iLine;
            continue;
        }

        int iLast = iLine;
        while (iLast + 1 < iEnd && !IsBlankLine(vecLines[iLast + 1]) && !StartsOtherBlock(vecLines[iLast + 1]))
            ++iLast;
        layout->iSummaryFirst = iLine;
        layout->iSummaryLast = iLast;
        QStringList vecParts;
        for (int iPart = iLine; iPart <= iLast; ++iPart)
            vecParts << vecLines[iPart].trimmed();
        layout->strSummary = UnescapeParagraphStart(vecParts.join(QLatin1Char(' ')).simplified());
        return;
    }
}
}

namespace KnowledgeMarkdownLayout
{
QString Normalize(const QString& text)
{
    QString strResult = text;
    if (strResult.startsWith(kByteOrderMark))
        strResult.remove(0, 1);
    strResult.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    return strResult;
}

bool IsFenceLine(const QString& line)
{
    const QString strTrimmed = line.trimmed();
    return strTrimmed.startsWith(QStringLiteral("```")) || strTrimmed.startsWith(QStringLiteral("~~~"));
}

int FindFenceContainer(const QStringList& lines, int open)
{
    const int iFenceIndent = MeasureIndent(lines[open]);
    int iBound = iFenceIndent;
    for (int iLine = open - 1; iLine >= 0 && iBound > 0; --iLine)
    {
        if (IsBlankLine(lines[iLine]))
            continue;
        const int iIndent = MeasureIndent(lines[iLine]);
        if (iIndent >= iBound)
            continue;
        const int iContent = MeasureListContentIndent(lines[iLine]);
        if (iContent >= 0 && iContent <= iFenceIndent)
            return iContent;
        iBound = iIndent;
    }
    return -1;
}

int FindFenceEnd(const QStringList& lines, int open, int end)
{
    const int iContainer = FindFenceContainer(lines, open);
    if (MeasureIndent(lines[open]) - std::max(iContainer, 0) >= kCodeIndent)
        return open;   // 들여 쓴 코드 · 문단 글이지 울타리가 아니다
    const QChar chFence = lines[open].trimmed().at(0);
    for (int iLine = open + 1; iLine < end; ++iLine)
    {
        const QString& strLine = lines[iLine];
        if (iContainer >= 0 && !IsBlankLine(strLine) && MeasureIndent(strLine) < iContainer)
            return iLine - 1;   // 담는 목록 항목이 끝났다
        if (IsFenceLine(strLine) && strLine.trimmed().at(0) == chFence)
            return iLine;
    }
    return -1;
}

int FindOpenFence(const QStringList& lines)
{
    int iLine = 0;
    while (iLine < lines.size())
    {
        if (!IsFenceLine(lines[iLine]))
        {
            ++iLine;
            continue;
        }
        const int iLast = FindFenceEnd(lines, iLine, lines.size());
        if (iLast < 0)
            return iLine;
        iLine = iLast + 1;
    }
    return -1;
}

int MeasureIndent(const QString& line, int* characters)
{
    int iColumns = 0;
    int iIndex = 0;
    for (; iIndex < line.size(); ++iIndex)
    {
        const int iNextColumn = AdvanceIndent(iColumns, line[iIndex]);
        if (iNextColumn < 0)
            break;
        iColumns = iNextColumn;
    }
    if (characters != nullptr)
        *characters = iIndex;
    return iColumns;
}

QString RemoveIndent(const QString& line, int columns)
{
    int iColumn = 0;
    int iIndex = 0;
    while (iIndex < line.size() && iColumn < columns)
    {
        const int iNextColumn = AdvanceIndent(iColumn, line[iIndex]);
        if (iNextColumn < 0)
            break;
        ++iIndex;
        if (iNextColumn > columns)
            return QString(iNextColumn - columns, QLatin1Char(' ')) + line.mid(iIndex);
        iColumn = iNextColumn;
    }
    return line.mid(iIndex);
}

QString EscapeParagraphStart(const QString& text)
{
    const int iSlot = FindEscapeSlot(text);
    return iSlot < 0 ? text : QString(text).insert(iSlot, QLatin1Char('\\'));
}

KnowledgeLayout Scan(const QString& text)
{
    KnowledgeLayout layout;
    layout.vecLines = Normalize(text).split(QLatin1Char('\n'));
    const QStringList& vecLines = layout.vecLines;
    const int iCount = vecLines.size();

    int iNext = 0;
    if (iCount > 0 && vecLines[0].trimmed() == QLatin1String("---"))
    {
        layout.bHasFrontMatter = true;
        layout.iFrontMatterStart = 0;
        for (int iLine = 1; iLine < iCount; ++iLine)
        {
            const QString strLine = vecLines[iLine];
            if (strLine.trimmed() == QLatin1String("---"))
            {
                layout.bFrontMatterClosed = true;
                layout.iFrontMatterEnd = iLine;
                break;
            }
            const int iColon = strLine.indexOf(QLatin1Char(':'));
            if (iColon > 0)
            {
                KnowledgeHeaderLine header;
                header.strKey = strLine.left(iColon).trimmed();
                header.iLine = iLine;
                header.iLastLine = iLine;
                ReadHeaderValue(vecLines, strLine.mid(iColon + 1).simplified(), &header);
                layout.vecHeader.push_back(header);
                iLine = header.iLastLine;
            }
        }
        // 닫히지 않은 머리글은 읽기에서 실패하지만, 쓰기가 갈아 끼울 수 있게 본문은 첫 줄부터 훑는다.
        if (layout.bFrontMatterClosed)
            iNext = layout.iFrontMatterEnd + 1;
    }

    for (int iLine = iNext; iLine < iCount; ++iLine)
    {
        const QString& strLine = vecLines[iLine];
        if (IsFenceLine(strLine))
        {
            // 울타리 블록(여는 줄부터 끝 줄까지)은 제목 · 절로 보지 않는다. 비지 않은 줄은 절 범위에 든다.
            const int iFenceEnd = SkipFence(vecLines, iLine, iCount);
            if (!layout.vecSections.isEmpty())
                for (int iFence = iLine; iFence < iFenceEnd; ++iFence)
                    if (!IsBlankLine(vecLines[iFence]))
                        layout.vecSections.last().iLastLine = iFence;
            iLine = iFenceEnd - 1;
            continue;
        }

        if (layout.iTitleLine < 0 && layout.vecSections.isEmpty() && strLine.startsWith(QStringLiteral("# ")))
        {
            layout.iTitleLine = iLine;
            layout.strTitle = strLine.mid(kTitlePrefixLength).simplified();
            continue;
        }
        if (strLine.startsWith(QStringLiteral("## ")))
        {
            KnowledgeSectionRange section;
            section.strName = strLine.mid(kSectionPrefixLength).simplified();
            section.iHeadingLine = iLine;
            section.iLastLine = iLine;
            layout.vecSections.push_back(section);
            continue;
        }
        if (!layout.vecSections.isEmpty() && !strLine.trimmed().isEmpty())
            layout.vecSections.last().iLastLine = iLine;
    }

    if (layout.iTitleLine >= 0)
        FindSummary(&layout);
    return layout;
}
}
