#include "VisualMarkdown/KnowledgeMarkdownReader.h"

#include "VisualMarkdown/KnowledgeMarkdownLayout.h"
#include "VisualMarkdown/KnowledgeVocabulary.h"

#include <QRegularExpression>
#include <QUrl>

namespace
{
using UnreadLines = QHash<QString, QStringList>;

constexpr int kMdSuffixLength = 3;          // ".md"
constexpr int kDashPrefixLength = 2;        // "— "
constexpr int kHeadingPrefixLength = 4;     // "### "
constexpr int kQuoteMarkCount = 2;          // 앞뒤 "
constexpr int kMaxTableIndent = 3;          // 표 줄 앞 공백
constexpr int kFirstFormat = 1;             // 형식 판은 1부터다. format 이 없거나 잘못 적혔으면 1 로 읽는다
constexpr int kLinkNameGroup = 1;           // MakeLinkExpression 의 [이름]
constexpr int kLinkPathGroup = 2;           // MakeLinkExpression 의 (경로)

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

QString TrimRight(const QString& line)
{
    int iEnd = line.size();
    while (iEnd > 0 && line[iEnd - 1].isSpace())
        --iEnd;
    return line.left(iEnd);
}

QString UnescapeLinkName(const QString& name)
{
    QString strResult;
    for (int iIndex = 0; iIndex < name.size(); ++iIndex)
    {
        if (name[iIndex] == QLatin1Char('\\') && iIndex + 1 < name.size())
            ++iIndex;
        strResult += name[iIndex];
    }
    return strResult;
}

// 다른 편집기가 %XX 로 바꾼 경로를 되돌린다. 올바른 %XX 가 아닌 % 는 그대로 둔다(Ocean 이 만든 id 에 % 가 들어갈 수 있다).
// 되돌린 글이 UTF-8 이 아니거나 제어 문자가 생기면 원래 경로를 쓴다.
QString DecodePath(const QString& path)
{
    static const QRegularExpression reEscape(QStringLiteral("%[0-9A-Fa-f]{2}"));
    if (!reEscape.match(path).hasMatch())
        return path;
    static const QRegularExpression reBarePercent(QStringLiteral("%(?![0-9A-Fa-f]{2})"));
    QString strEncoded = path;
    strEncoded.replace(reBarePercent, QStringLiteral("%25"));
    const QString strDecoded = QUrl::fromPercentEncoding(strEncoded.toUtf8());
    for (const QChar ch : strDecoded)
        if (ch == QChar::ReplacementCharacter || ch.category() == QChar::Other_Control)
            return path;
    return strDecoded;
}

QString ExtractConceptId(const QString& path)
{
    QString strId = DecodePath(path.trimmed());
    const int iSlash = strId.lastIndexOf(QLatin1Char('/'));
    if (iSlash >= 0)
        strId = strId.mid(iSlash + 1);
    if (strId.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive))
        strId.chop(kMdSuffixLength);
    return strId;
}

ConceptLink MakeLink(const QRegularExpressionMatch& match, int nameGroup, int pathGroup)
{
    ConceptLink link;
    link.strName = UnescapeLinkName(match.captured(nameGroup)).simplified();
    link.strConceptId = ExtractConceptId(match.captured(pathGroup));
    return link;
}

QRegularExpression MakeLinkExpression()
{
    return QRegularExpression(QStringLiteral(R"re(\[((?:\\.|[^\]\\])*)\]\(([^)]+)\))re"));
}

QVector<ConceptLink> FindLinks(const QString& text)
{
    QVector<ConceptLink> vecLinks;
    static const QRegularExpression reLink = MakeLinkExpression();
    QRegularExpressionMatchIterator itMatch = reLink.globalMatch(text);
    while (itMatch.hasNext())
        vecLinks.push_back(MakeLink(itMatch.next(), kLinkNameGroup, kLinkPathGroup));
    return vecLinks;
}

QStringList SplitList(const QString& text)
{
    QStringList vecResult;
    for (const QString& strPart : text.split(QLatin1Char(',')))
    {
        const QString strTrimmed = strPart.simplified();
        if (!strTrimmed.isEmpty())
            vecResult << strTrimmed;
    }
    return vecResult;
}

// aliases: YAML 목록(블록 · 흐름)이면 그 항목, 한 값이면 쉼표로 나눈다.
QStringList ReadAliases(const KnowledgeLayout& layout)
{
    const KnowledgeHeaderLine* pHeader = layout.FindHeader(QStringLiteral("aliases"));
    if (pHeader == nullptr)
        return QStringList();
    if (pHeader->eStyle != EHeaderStyle::BLOCK_LIST && pHeader->eStyle != EHeaderStyle::FLOW_LIST)
        return SplitList(pHeader->strValue);
    QStringList vecAliases;
    for (const QString& strItem : pHeader->vecItems)
    {
        const QString strAlias = strItem.simplified();
        if (!strAlias.isEmpty())
            vecAliases << strAlias;
    }
    return vecAliases;
}

int ReadFormat(const KnowledgeLayout& layout)
{
    bool bOk = false;
    const int iFormat = layout.FindHeaderValue(QStringLiteral("format")).toInt(&bOk);
    return bOk && iFormat >= kFirstFormat ? iFormat : kFirstFormat;
}

bool CheckHeader(const KnowledgeLayout& layout, const QString& type, QString* error)
{
    if (!layout.bHasFrontMatter)
    {
        *error = QStringLiteral("front matter is missing");
        return false;
    }
    if (!layout.bFrontMatterClosed)
    {
        *error = QStringLiteral("front matter is not closed");
        return false;
    }
    if (layout.FindHeaderValue(QStringLiteral("type")) != type)
    {
        *error = QStringLiteral("type is not ") + type;
        return false;
    }
    if (layout.FindHeaderValue(QStringLiteral("id")).isEmpty())
    {
        *error = QStringLiteral("id is missing");
        return false;
    }
    return true;
}

// 소절 하나의 본문 줄(머리 줄 다음부터 마지막 비지 않은 줄까지)
QStringList GetSectionLines(const KnowledgeLayout& layout, const KnowledgeSectionRange& section)
{
    QStringList vecLines;
    for (int iLine = section.iHeadingLine + 1; iLine <= section.iLastLine && iLine < layout.vecLines.size(); ++iLine)
        vecLines << TrimRight(layout.vecLines[iLine]);
    return vecLines;
}

// 하위 항목 `  - 글`. raw 는 "- " 뒤의 글 그대로(앞뒤 공백만 뺌)다. `키: 값` 이면 key · value 를 채우고, 콜론이 없으면 key 가 빈다.
// 아는 키가 아니면(콜론이 없어도) raw 를 그 항목에 보관한다.
bool ParseSubItem(const QString& line, QString* key, QString* value, QString* raw)
{
    static const QRegularExpression reSub(QStringLiteral(R"re(^(?:[ \t]{2,}|\t)- (.*)$)re"));
    const QRegularExpressionMatch match = reSub.match(line);
    if (!match.hasMatch())
        return false;
    *raw = match.captured(1).trimmed();
    if (raw->isEmpty())
        return false;
    const int iColon = raw->indexOf(QLatin1Char(':'));
    *key = iColon > 0 ? raw->left(iColon).trimmed() : QString();
    *value = iColon > 0 ? raw->mid(iColon + 1).simplified() : QString();
    return true;
}

// 읽지 못한 줄(절 줄 번호, 오름차순)을 글로 모은다. keepBlankLines 면 덩어리 안의 빈 줄도 빈 글자로 넣는다.
// 덩어리 = 사이에 빈 줄만 있는(읽은 줄이 끼지 않은) 읽지 못한 줄들. 덩어리 앞뒤의 빈 줄은 넣지 않는다.
QStringList CollectUnread(const QStringList& lines, const QVector<int>& unread, bool keepBlankLines)
{
    QStringList vecResult;
    int iPrevious = -1;
    for (const int iLine : unread)
    {
        if (keepBlankLines && iPrevious >= 0)
        {
            bool bOnlyBlanks = true;
            for (int iBetween = iPrevious + 1; iBetween < iLine && bOnlyBlanks; ++iBetween)
                bOnlyBlanks = lines[iBetween].isEmpty();
            if (bOnlyBlanks)
                for (int iBetween = iPrevious + 1; iBetween < iLine; ++iBetween)
                    vecResult << QString();
        }
        vecResult << lines[iLine];
        iPrevious = iLine;
    }
    return vecResult;
}

void ReadOutline(const QStringList& lines, KnowledgeDocument* doc, QVector<int>* unread)
{
    static const QRegularExpression reItem(QStringLiteral(R"re(^(\s*)- (\S+) (.+)$)re"));
    for (int iLine = 0; iLine < lines.size(); ++iLine)
    {
        const QString& strLine = lines[iLine];
        if (strLine.isEmpty())
            continue;
        const QRegularExpressionMatch match = reItem.match(strLine);
        if (!match.hasMatch())
        {
            *unread << iLine;
            continue;
        }
        KnowledgeOutlineEntry entry;
        entry.strNumber = match.captured(2);
        entry.strTitle = match.captured(3).simplified();
        if (match.captured(1).isEmpty())
            doc->vecOutline.push_back(entry);
        else if (!doc->vecOutline.isEmpty())
            doc->vecOutline.last().vecChildren.push_back(entry);
        else
            *unread << iLine;
    }
}

void ReadKeyNumbers(const QStringList& lines, KnowledgeDocument* doc, QVector<int>* unread)
{
    static const QRegularExpression reItem(QStringLiteral("^- \\*\\*(.+?)\\*\\*(?: (.+?))?(?: — (.+))?$"));
    const QString strDash = QStringLiteral("—");
    for (int iLine = 0; iLine < lines.size(); ++iLine)
    {
        const QString& strLine = lines[iLine];
        if (strLine.isEmpty())
            continue;
        const QRegularExpressionMatch match = reItem.match(strLine);
        if (!match.hasMatch())
        {
            *unread << iLine;
            continue;
        }
        KnowledgeKeyNumber number;
        number.strValue = match.captured(1).simplified();
        number.strUnit = match.captured(2).simplified();
        number.strLabel = match.captured(3).simplified();
        if (number.strLabel.isEmpty() && number.strUnit.startsWith(strDash + QLatin1Char(' ')))
        {
            number.strLabel = number.strUnit.mid(kDashPrefixLength).simplified();
            number.strUnit.clear();
        }
        else if (number.strLabel.isEmpty() && number.strUnit == strDash)
            number.strUnit.clear();
        doc->vecKeyNumbers.push_back(number);
    }
}

// 들여 쓰지 않은 줄이 항목의 첫 줄이다. 들여 쓴 줄(`- ` 없이 이어 쓴 글)은 읽지 못해도 항목을 끝내지 않는다.
bool IsHeadLine(const QString& line)
{
    return !line.isEmpty() && !line.at(0).isSpace();
}

// 하위 항목은 바로 앞까지 이어진 항목에만 붙는다. 빈 줄이나 읽지 못한 첫 줄 뒤의 하위 항목은 읽지 못한 줄이다
// (손으로 적은 줄이 이웃 항목의 값을 덮지 않고, 절 끝에 다시 쓴 읽지 못한 줄도 다음 읽기에서 붙지 않는다).
void ReadCards(const QStringList& lines, KnowledgeDocument* doc, QVector<int>* unread)
{
    static const QRegularExpression reCard(QStringLiteral(R"re(^- (.*?)(?: <!-- card:([0-9a-f]{8}) -->)?$)re"));
    bool bItemOpen = false;
    for (int iLine = 0; iLine < lines.size(); ++iLine)
    {
        const QString& strLine = lines[iLine];
        if (strLine.isEmpty())
        {
            bItemOpen = false;
            continue;
        }
        QString strKey, strValue, strRaw;
        if (ParseSubItem(strLine, &strKey, &strValue, &strRaw))
        {
            if (!bItemOpen)
            {
                *unread << iLine;
                continue;
            }
            KnowledgeCard& card = doc->vecCards.last();
            if (strKey == kKeyEvidence)
                card.strEvidence = strValue;
            else if (strKey == kKeySection)
                card.strSection = strValue;
            else if (strKey == kKeyConcepts)
                card.vecConcepts = FindLinks(strValue);
            else
                card.vecExtraLines << strRaw;
            continue;
        }
        const QRegularExpressionMatch match = reCard.match(strLine);
        if (!match.hasMatch())
        {
            *unread << iLine;
            if (IsHeadLine(strLine))
                bItemOpen = false;
            continue;
        }
        KnowledgeCard card;
        card.strClaim = match.captured(1).simplified();
        card.strId = match.captured(2);
        doc->vecCards.push_back(card);
        bItemOpen = true;
    }
}

QVector<KnowledgeRelation> ReadRelationLines(const QStringList& lines, QVector<int>* unread)
{
    static const QRegularExpression reFirst(QStringLiteral(
        "^- (\\[(?:\\\\.|[^\\]\\\\])*\\]\\([^)]+\\)) \\*\\*(.+?)\\*\\* (→|↔) (\\[(?:\\\\.|[^\\]\\\\])*\\]\\([^)]+\\))$"));
    static const QRegularExpression reLink = MakeLinkExpression();
    const KnowledgeVocabulary& vocabulary = KnowledgeVocabulary::Get();
    QVector<KnowledgeRelation> vecRelations;
    bool bItemOpen = false;   // ReadCards 와 같은 규칙
    for (int iLine = 0; iLine < lines.size(); ++iLine)
    {
        const QString& strLine = lines[iLine];
        if (strLine.isEmpty())
        {
            bItemOpen = false;
            continue;
        }
        QString strKey, strValue, strRaw;
        if (ParseSubItem(strLine, &strKey, &strValue, &strRaw))
        {
            if (!bItemOpen)
            {
                *unread << iLine;
                continue;
            }
            KnowledgeRelation& relation = vecRelations.last();
            if (strKey == kKeyNote)
                relation.strNote = strValue;
            else if (strKey == kKeyWhere)
                relation.strWhere = strValue;
            else if (strKey == kKeyQuote)
            {
                if (strValue.size() >= kQuoteMarkCount && strValue.startsWith(QLatin1Char('"')) && strValue.endsWith(QLatin1Char('"')))
                    strValue = strValue.mid(1, strValue.size() - kQuoteMarkCount);
                relation.strQuote = strValue;
            }
            else if (strKey == kKeyStatus)
                relation.eStatus = vocabulary.ToStatusCode(strValue) == QStringLiteral("confirmed")
                    ? ERelationStatus::CONFIRMED : ERelationStatus::PROPOSED;
            else
                relation.vecExtraLines << strRaw;
            continue;
        }
        const QRegularExpressionMatch match = reFirst.match(strLine);
        if (!match.hasMatch())
        {
            *unread << iLine;
            if (IsHeadLine(strLine))
                bItemOpen = false;
            continue;
        }
        KnowledgeRelation relation;
        const QRegularExpressionMatch from = reLink.match(match.captured(1));
        const QRegularExpressionMatch to = reLink.match(match.captured(4));
        relation.fromConcept = MakeLink(from, kLinkNameGroup, kLinkPathGroup);
        relation.toConcept = MakeLink(to, kLinkNameGroup, kLinkPathGroup);
        relation.strKind = vocabulary.ToRelationCode(match.captured(2).simplified());
        vecRelations.push_back(relation);
        bItemOpen = true;
    }
    return vecRelations;
}

QString DecodeAttribute(QString value)
{
    value.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    value.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    value.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    return value;
}

void ReadAttributes(const QString& line, KnowledgeTable* table)
{
    static const QRegularExpression reAttribute(QStringLiteral(R"re((\w+)="([^"]*)")re"));
    QRegularExpressionMatchIterator itMatch = reAttribute.globalMatch(line);
    while (itMatch.hasNext())
    {
        const QRegularExpressionMatch match = itMatch.next();
        const QString strName = match.captured(1);
        const QString strValue = DecodeAttribute(match.captured(2));
        if (strName == QLatin1String("x"))
            table->strX = strValue.simplified();
        else if (strName == QLatin1String("y"))
            table->vecY = SplitList(strValue);
        else if (strName == QLatin1String("series"))
            table->strSeries = strValue.simplified();
        else if (strName == QLatin1String("ordered"))
            table->vecOrdered = SplitList(strValue);
        else if (strName == QLatin1String("cumulative"))
            table->bCumulative = strValue.trimmed() == QLatin1String("yes");
        else if (strName == QLatin1String("concepts"))
            table->vecConceptIds = SplitList(strValue);
        else if (strName == QLatin1String("condition"))
            table->strCondition = strValue.simplified();
        else if (strName == QLatin1String("where"))
            table->strWhere = strValue.simplified();
    }
}

QStringList SplitRow(const QString& line)
{
    QString strBody = line.trimmed();
    if (strBody.startsWith(QLatin1Char('|')))
        strBody.remove(0, 1);
    int iBackslashes = 0;
    for (int iIndex = strBody.size() - 2; iIndex >= 0 && strBody[iIndex] == QLatin1Char('\\'); --iIndex)
        ++iBackslashes;
    if (strBody.endsWith(QLatin1Char('|')) && iBackslashes % 2 == 0)
        strBody.chop(1);

    QStringList vecCells;
    QString strCell;
    for (int iIndex = 0; iIndex < strBody.size(); ++iIndex)
    {
        if (strBody[iIndex] == QLatin1Char('\\') && iIndex + 1 < strBody.size() && strBody[iIndex + 1] == QLatin1Char('|'))
        {
            strCell += QLatin1Char('|');
            ++iIndex;
        }
        else if (strBody[iIndex] == QLatin1Char('|'))
        {
            vecCells << strCell.simplified();
            strCell.clear();
        }
        else
            strCell += strBody[iIndex];
    }
    vecCells << strCell.simplified();
    return vecCells;
}

// 구분 줄의 칸은 `:?-+:?` 이다(Ocean 이 쓰는 `--:` 도 받는다).
bool IsAlignmentRow(const QStringList& cells)
{
    static const QRegularExpression reAlign(QStringLiteral("^:?-+:?$"));
    for (const QString& strCell : cells)
        if (!reAlign.match(strCell).hasMatch())
            return false;
    return !cells.isEmpty();
}

// 표 줄: 앞 공백 3칸까지, 앞에 \ 가 붙지 않은 | 가 하나라도 있다(바깥 | 는 없어도 된다).
bool IsTableLine(const QString& line)
{
    int iIndent = 0;
    while (iIndent < line.size() && line[iIndent] == QLatin1Char(' '))
        ++iIndent;
    if (iIndent > kMaxTableIndent)
        return false;
    for (int iIndex = iIndent; iIndex < line.size(); ++iIndex)
    {
        if (line[iIndex] == QLatin1Char('\\'))
            ++iIndex;
        else if (line[iIndex] == QLatin1Char('|'))
            return true;
    }
    return false;
}

// 머리 줄 바로 다음 줄이 같은 칸 수의 구분 줄이어야 표가 시작된다.
bool StartsTable(const QStringList& lines, int index, int end)
{
    if (index + 1 >= end || !IsTableLine(lines[index]) || !IsTableLine(lines[index + 1]))
        return false;
    const QStringList vecDelimiter = SplitRow(lines[index + 1]);
    return IsAlignmentRow(vecDelimiter) && vecDelimiter.size() == SplitRow(lines[index]).size();
}

void ReadColumns(const QStringList& cells, KnowledgeTable* table)
{
    static const QRegularExpression reUnit(QStringLiteral(R"re(^(.*) \(([^()]*)\)$)re"));
    for (const QString& strCell : cells)
    {
        KnowledgeColumn column;
        const QRegularExpressionMatch match = reUnit.match(strCell);
        if (match.hasMatch())
        {
            column.strName = match.captured(1).simplified();
            column.strUnit = match.captured(2).simplified();
        }
        else
            column.strName = strCell;
        table->vecColumns.push_back(column);
    }
}

// `###` 줄(begin)부터 다음 `###` 줄 앞(end)까지가 한 블록이다. 주석도 표도 없으면 표가 아니라 읽지 못한 글이다.
void ReadTableBlock(const QStringList& lines, int begin, int end, KnowledgeDocument* doc, QVector<int>* unread)
{
    KnowledgeTable table;
    table.strTitle = lines[begin].mid(kHeadingPrefixLength).simplified();
    QVector<int> vecBlockUnread;
    bool bFound = false;
    bool bHeaderSeen = false;
    bool bEnded = false;
    for (int iLine = begin + 1; iLine < end; ++iLine)
    {
        const QString& strLine = lines[iLine];
        if (strLine.isEmpty())
        {
            // 표는 머리 줄을 본 뒤 첫 빈 줄에서 끝난다(GFM 과 같다). 다음 ### 까지의 줄은 읽지 못한 줄이다.
            if (bHeaderSeen)
                bEnded = true;
            continue;
        }
        if (bEnded)
            vecBlockUnread << iLine;
        else if (strLine.startsWith(QStringLiteral("<!--")) && strLine.contains(QStringLiteral("ocean:data")))
        {
            ReadAttributes(strLine, &table);
            bFound = true;
        }
        else if (!bHeaderSeen)
        {
            if (!StartsTable(lines, iLine, end))
            {
                vecBlockUnread << iLine;
                continue;
            }
            ReadColumns(SplitRow(strLine), &table);
            bHeaderSeen = true;
            bFound = true;
            ++iLine;   // 구분 줄
        }
        else if (!IsTableLine(strLine))
            vecBlockUnread << iLine;
        else
        {
            const QStringList vecCells = SplitRow(strLine);
            if (vecCells.size() == table.vecColumns.size())
                table.vecRows.push_back(vecCells);
            else
                vecBlockUnread << iLine;
        }
    }
    if (!bFound)
    {
        for (int iLine = begin; iLine < end; ++iLine)
            if (!lines[iLine].isEmpty())
                *unread << iLine;
        return;
    }
    doc->vecTables.push_back(table);
    *unread += vecBlockUnread;
}

void ReadTables(const QStringList& lines, KnowledgeDocument* doc, QVector<int>* unread)
{
    int iLine = 0;
    while (iLine < lines.size())
    {
        if (!lines[iLine].startsWith(QStringLiteral("### ")))
        {
            if (!lines[iLine].isEmpty())
                *unread << iLine;
            ++iLine;
            continue;
        }
        int iEnd = iLine + 1;
        while (iEnd < lines.size() && !lines[iEnd].startsWith(QStringLiteral("### ")))
            ++iEnd;
        ReadTableBlock(lines, iLine, iEnd, doc, unread);
        iLine = iEnd;
    }
}

void AppendUnread(UnreadLines* hash, const QString& section, const QStringList& lines)
{
    if (!lines.isEmpty())
        (*hash)[section] += lines;
}

// keepBlankLines: 다시 쓸 때 쓰려고 덩어리 안의 빈 줄도 모은다(로그로 남기는 읽지 못한 줄에는 빈 줄이 없다).
void ReadDocumentSections(const KnowledgeLayout& layout, KnowledgeDocument* doc, UnreadLines* unread, bool keepBlankLines)
{
    for (const KnowledgeSectionRange& section : layout.vecSections)
    {
        const QStringList vecLines = GetSectionLines(layout, section);
        QVector<int> vecUnread;
        if (section.strName == kSectionOutline)
            ReadOutline(vecLines, doc, &vecUnread);
        else if (section.strName == kSectionKeyNumbers)
            ReadKeyNumbers(vecLines, doc, &vecUnread);
        else if (section.strName == kSectionCards)
            ReadCards(vecLines, doc, &vecUnread);
        else if (section.strName == kSectionRelations)
            doc->vecRelations += ReadRelationLines(vecLines, &vecUnread);
        else if (section.strName == kSectionData)
            ReadTables(vecLines, doc, &vecUnread);
        else
            continue;
        AppendUnread(unread, section.strName, CollectUnread(vecLines, vecUnread, keepBlankLines));
    }
}
}

namespace KnowledgeMarkdownReader
{
DocumentReadResult ReadDocument(const QString& text)
{
    DocumentReadResult result;
    const KnowledgeLayout layout = KnowledgeMarkdownLayout::Scan(text);
    if (!CheckHeader(layout, QStringLiteral("document"), &result.strError))
        return result;

    KnowledgeDocument& doc = result.document;
    doc.iFormat = ReadFormat(layout);
    doc.strId = layout.FindHeaderValue(QStringLiteral("id"));
    doc.strSource = layout.FindHeaderValue(QStringLiteral("source"));
    doc.strFileInfo = layout.FindHeaderValue(QStringLiteral("fileInfo"));
    doc.strMeta = layout.FindHeaderValue(QStringLiteral("meta"));
    doc.strLearned = layout.FindHeaderValue(QStringLiteral("learned"));
    doc.strAnalysisId = layout.FindHeaderValue(QStringLiteral("analysis"));
    doc.iRules = layout.FindHeaderValue(QStringLiteral("rules")).toInt();
    doc.bEdited = layout.FindHeaderValue(QStringLiteral("edited")) == QLatin1String("true");
    doc.strDigest = layout.FindHeaderValue(QStringLiteral("digest"));
    doc.strTitle = layout.strTitle;
    doc.strSummary = layout.strSummary;

    ReadDocumentSections(layout, &doc, &result.hashUnreadLines, false);
    result.bOk = true;
    return result;
}

UnreadLines ReadUnreadLines(const KnowledgeLayout& layout)
{
    KnowledgeDocument doc;
    UnreadLines hashUnread;
    ReadDocumentSections(layout, &doc, &hashUnread, true);
    return hashUnread;
}

ConceptReadResult ReadConcept(const QString& text)
{
    ConceptReadResult result;
    const KnowledgeLayout layout = KnowledgeMarkdownLayout::Scan(text);
    if (!CheckHeader(layout, QStringLiteral("concept"), &result.strError))
        return result;

    KnowledgeConcept& concept = result.concept;
    concept.iFormat = ReadFormat(layout);
    concept.strId = layout.FindHeaderValue(QStringLiteral("id"));
    concept.strKind = KnowledgeVocabulary::Get().ToConceptCode(layout.FindHeaderValue(QStringLiteral("kind")));
    concept.vecAliases = ReadAliases(layout);
    static const QRegularExpression reYear(QStringLiteral("^\\d{4}$"));
    const QString strYear = layout.FindHeaderValue(QStringLiteral("year"));
    concept.strYear = reYear.match(strYear).hasMatch() ? strYear : QString();
    concept.strName = layout.strTitle;
    concept.strSummary = layout.strSummary;

    for (const KnowledgeSectionRange& section : layout.vecSections)
    {
        if (section.strName != kSectionRelations)
            continue;
        const QStringList vecLines = GetSectionLines(layout, section);
        QVector<int> vecUnread;
        concept.vecRelations += ReadRelationLines(vecLines, &vecUnread);
        AppendUnread(&result.hashUnreadLines, section.strName, CollectUnread(vecLines, vecUnread, false));
    }
    result.bOk = true;
    return result;
}
}
