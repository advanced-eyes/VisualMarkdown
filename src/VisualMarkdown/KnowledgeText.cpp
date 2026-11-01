#include "VisualMarkdown/KnowledgeText.h"

#include "VisualMarkdown/KnowledgeMarkdownLayout.h"

#include <QHash>
#include <QRandomGenerator>
#include <QRegularExpression>

namespace
{
constexpr int kMaxSlugLength = 60;
constexpr ushort kFirstPrintableCode = 0x20;
constexpr ushort kDeleteCode = 0x7f;
constexpr int kCardIdDigits = 8;
constexpr int kHexBase = 16;
constexpr int kFirstDuplicateSuffix = 2;   // 겹치면 붙이는 첫 번호(id 는 "-2", 열 이름은 " 2")
constexpr int kFenceMarkLength = 3;        // 닫는 울타리 ``` · ~~~
constexpr int kTabStop = 4;                // 줄 앞 탭은 다음 4의 배수 칸까지(CommonMark)
constexpr int kCodeIndent = 4;             // 이만큼 들인 줄은 들여 쓴 코드 블록이다(CommonMark)

bool IsReservedName(const QString& slug)
{
    static const QRegularExpression reReserved(
        QStringLiteral("^(con|prn|aux|nul|com[1-9]|lpt[1-9])$"), QRegularExpression::CaseInsensitiveOption);
    return reReserved.match(slug).hasMatch();
}

QString TrimHyphens(QString text)
{
    while (text.startsWith(QLatin1Char('-')))
        text.remove(0, 1);
    while (text.endsWith(QLatin1Char('-')))
        text.chop(1);
    return text;
}

QString TrimRight(const QString& line)
{
    int iEnd = line.size();
    while (iEnd > 0 && line[iEnd - 1].isSpace())
        --iEnd;
    return line.left(iEnd);
}

QString GetLeadingWhitespace(const QString& line)
{
    int iEnd = 0;
    while (iEnd < line.size() && line[iEnd].isSpace())
        ++iEnd;
    return line.left(iEnd);
}

// lines[first] 부터 울타리를 파일 훑기(KnowledgeMarkdownLayout::Scan)와 같은 규칙으로 짝짓는다:
// 울타리 밖의 울타리 줄이 열고, 같은 글자로 시작하는 울타리 줄이 닫는다. 끝까지 닫히지 않았으면
// 여는 줄의 들여쓰기 + 같은 글자 셋으로 닫는 줄을 더한다(파일의 뒤 절이 코드 안으로 들어가지 않게).
void CloseOpenFence(QStringList* lines, int first)
{
    int iOpen = -1;
    QChar chFence;
    for (int iLine = first; iLine < lines->size(); ++iLine)
    {
        const QString& strLine = lines->at(iLine);
        if (!KnowledgeMarkdownLayout::IsFenceLine(strLine))
            continue;
        const QChar chThis = strLine.trimmed().at(0);
        if (iOpen < 0)
        {
            iOpen = iLine;
            chFence = chThis;
        }
        else if (chThis == chFence)
            iOpen = -1;
    }
    if (iOpen >= 0)
        *lines << GetLeadingWhitespace(lines->at(iOpen)) + QString(kFenceMarkLength, chFence);
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

// 줄이 블록을 여는가(CommonMark · GFM): 들여 쓴 코드(4칸 이상), 울타리, 목록 표지, 제목, 인용, $$ 수식, 표, 가로줄, HTML.
// 표는 `|` 로 시작하거나, `|` 가 든 줄 다음 줄이 구분 줄(`--- | --:`)일 때다.
bool OpensBlock(const QString& line, const QString& nextLine)
{
    static const QRegularExpression reMarker(QStringLiteral(
        R"re(^(?:[-*+](?:[ \t]|$)|\d{1,9}[.)](?:[ \t]|$)|#{1,6}(?:[ \t]|$)|>|\$\$|\||<[A-Za-z/!?]))re"));
    static const QRegularExpression reBreak(QStringLiteral(R"re(^(?:(?:-[ \t]*){3,}|(?:\*[ \t]*){3,}|(?:_[ \t]*){3,})$)re"));
    static const QRegularExpression reDelimiterRow(QStringLiteral(R"re(^[ \t|:-]*-[ \t|:-]*$)re"));
    int iCharacters = 0;
    if (KnowledgeText::MeasureIndent(line, &iCharacters) >= kCodeIndent)
        return true;
    const QString strBody = line.mid(iCharacters);
    if (KnowledgeMarkdownLayout::IsFenceLine(strBody) || reMarker.match(strBody).hasMatch() || reBreak.match(strBody).hasMatch())
        return true;
    return strBody.contains(QLatin1Char('|')) && nextLine.contains(QLatin1Char('|')) && reDelimiterRow.match(nextLine).hasMatch();
}

// 정리한 근거 줄들(둘 이상)의 첫 줄이 블록을 열면 첫 줄도 이어지는 줄처럼 쓴다(접지 않는다).
// 접은 첫 줄이 블록을 열게 되는 경우(앞의 U+00A0 등)도 블록으로 본다. 그래야 정리를 두 번 해도 같다.
bool StartsWithBlock(const QStringList& lines)
{
    return lines.size() > 1 && (OpensBlock(lines[0], lines[1]) || OpensBlock(lines[0].simplified(), lines[1]));
}
}

namespace KnowledgeText
{
QString MakeSlug(const QString& name, const QString& fallback)
{
    QString strSlug = name.normalized(QString::NormalizationForm_C).toLower();
    static const QRegularExpression reSpaces(QStringLiteral("\\s+"));
    strSlug.replace(reSpaces, QStringLiteral("-"));

    static const QString strForbidden = QStringLiteral("\\/:*?\"<>|#[](),%");   // ',' 는 표 주석 concepts 의 구분자, '%' 는 링크 주소의 퍼센트 부호화와 겹친다
    QString strKept;
    for (const QChar ch : strSlug)
    {
        if (strForbidden.contains(ch) || ch.unicode() < kFirstPrintableCode || ch.unicode() == kDeleteCode)
            continue;
        strKept.append(ch);
    }
    static const QRegularExpression reDashes(QStringLiteral("-{2,}"));
    strKept.replace(reDashes, QStringLiteral("-"));
    strKept = TrimHyphens(strKept);
    if (strKept.size() > kMaxSlugLength)
        strKept = strKept.left(kMaxSlugLength);
    strKept = TrimHyphens(strKept);
    if (strKept.isEmpty())
        strKept = fallback;
    if (IsReservedName(strKept))
        strKept += QStringLiteral("-1");
    return strKept;
}

QString MakeUniqueId(const QString& slug, const QSet<QString>& taken)
{
    if (!taken.contains(slug))
        return slug;
    for (int iSuffix = kFirstDuplicateSuffix;; ++iSuffix)
    {
        const QString strCandidate = slug + QLatin1Char('-') + QString::number(iSuffix);
        if (!taken.contains(strCandidate))
            return strCandidate;
    }
}

QString MakeNameKey(const QString& name)
{
    QString strKey = name.normalized(QString::NormalizationForm_C).toLower();
    static const QRegularExpression reSeparators(QStringLiteral("[\\s-]+"));
    strKey.remove(reSeparators);
    return strKey;
}

bool IsQuantityText(const QString& cell)
{
    static const QRegularExpression reNumber(
        QStringLiteral("^[+-]?(\\d{1,3}(,\\d{3})+|\\d+)(\\.\\d+)?([eE][+-]?\\d+)?$"));
    return reNumber.match(cell.trimmed()).hasMatch();
}

QStringList ExtractNumbers(const QString& text)
{
    static const QRegularExpression reNumber(QStringLiteral("\\d[\\d,]*(\\.\\d+)?"));
    QStringList vecNumbers;
    QRegularExpressionMatchIterator itMatch = reNumber.globalMatch(text);
    while (itMatch.hasNext())
    {
        QString strNumber = itMatch.next().captured(0);
        strNumber.remove(QLatin1Char(','));
        vecNumbers << strNumber;
    }
    return vecNumbers;
}

QString MakeCardId(const QSet<QString>& taken)
{
    for (;;)
    {
        const QString strId = QStringLiteral("%1").arg(QRandomGenerator::global()->generate(), kCardIdDigits, kHexBase, QLatin1Char('0'));
        if (!taken.contains(strId))
            return strId;
    }
}

EColumnKind ClassifyColumn(const KnowledgeTable& table, int column)
{
    if (column < 0 || column >= table.vecColumns.size())
        return EColumnKind::NOMINAL;
    if (table.vecOrdered.contains(table.vecColumns[column].strName))
        return EColumnKind::ORDINAL;
    bool bAnyCell = false;
    for (const QStringList& vecRow : table.vecRows)
    {
        if (column >= vecRow.size())
            continue;
        const QString strCell = vecRow[column].trimmed();
        if (strCell.isEmpty())
            continue;
        if (!IsQuantityText(strCell))
            return EColumnKind::NOMINAL;
        bAnyCell = true;
    }
    return bAnyCell ? EColumnKind::QUANTITY : EColumnKind::NOMINAL;
}

// 파일에서 쉼표로 나뉘는 자리(aliases · y · ordered)에 들어가는 이름의 쉼표를 " · " 로 바꾼다.
QString CleanName(const QString& name)
{
    static const QRegularExpression reComma(QStringLiteral("\\s*,\\s*"));
    QString strName = name;
    strName.replace(reComma, QStringLiteral(" · "));
    return strName.simplified();
}

// 이름마다 CleanName 을 하고, 비게 된 이름은 뺀다(별칭 목록).
QStringList CleanNames(const QStringList& names)
{
    QStringList vecNames;
    for (const QString& strName : names)
    {
        const QString strCleaned = CleanName(strName);
        if (!strCleaned.isEmpty())
            vecNames << strCleaned;
    }
    return vecNames;
}

// 열 이름 · 단위를 파일 머리 칸 `이름 (단위)` 를 다시 읽었을 때와 같게 만든다.
// 단위의 괄호는 뺀다. 단위가 비고 이름이 ` (…)` 로 끝나면 그 괄호 안을 단위로 옮긴다(읽기가 그렇게 나눈다).
// x · y · series · ordered 는 바뀐 열 이름을 따라간다.
void CleanColumns(KnowledgeTable* table)
{
    static const QRegularExpression reUnitInName(QStringLiteral(R"re(^(.*) \(([^()]*)\)$)re"));
    QHash<QString, QString> hashRenamed;
    QSet<QString> setUsed;
    for (KnowledgeColumn& column : table->vecColumns)
    {
        const QString strOriginal = column.strName;
        column.strName = CleanName(column.strName);
        column.strUnit.remove(QLatin1Char('('));
        column.strUnit.remove(QLatin1Char(')'));
        column.strUnit = column.strUnit.simplified();
        const QRegularExpressionMatch match = reUnitInName.match(column.strName);
        if (column.strUnit.isEmpty() && match.hasMatch())
        {
            column.strName = match.captured(1).simplified();
            column.strUnit = match.captured(2).simplified();
        }
        // 정리한 이름이 앞의 열과 겹치면 ` 2`, ` 3` … 을 붙인다(파일 문법에 안전하다: 끝이 괄호가 아니고 쉼표가 없다).
        if (setUsed.contains(column.strName))
        {
            int iSuffix = kFirstDuplicateSuffix;
            while (setUsed.contains(QStringLiteral("%1 %2").arg(column.strName).arg(iSuffix)))
                ++iSuffix;
            column.strName = QStringLiteral("%1 %2").arg(column.strName).arg(iSuffix);
        }
        setUsed.insert(column.strName);
        if (!hashRenamed.contains(strOriginal))
            hashRenamed.insert(strOriginal, column.strName);
    }
    const auto funcRename = [&hashRenamed](const QString& name)
    {
        return hashRenamed.contains(name) ? hashRenamed.value(name) : CleanName(name);
    };
    if (!table->strX.isEmpty())
        table->strX = funcRename(table->strX);
    if (!table->strSeries.isEmpty())
        table->strSeries = funcRename(table->strSeries);
    for (QString& strY : table->vecY)
        strY = funcRename(strY);
    for (QString& strOrdered : table->vecOrdered)
        strOrdered = funcRename(strOrdered);
}

// 앞뒤 공백 · 빈 줄을 빼고, 줄 끝 공백을 뺀다. 한 줄이면 지금처럼 이어진 공백을 하나로 접는다.
// 여러 줄이면 첫 줄만 접고(`- 근거: ` 뒤에 쓴다) 둘째 줄부터는 들여쓰기 · 안쪽 공백을 지킨다(코드 · 목록).
// 첫 줄이 블록을 열면 첫 줄도 이어지는 줄로 쓰므로 접지 않는다. 이어지는 줄의 울타리가 닫히지 않았으면 닫는 줄을 더한다.
QString NormalizeEvidence(const QString& evidence)
{
    QString strText = evidence;
    strText.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    strText.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    QStringList vecLines;
    for (const QString& strLine : strText.split(QLatin1Char('\n')))
        vecLines << TrimRight(strLine);
    while (!vecLines.isEmpty() && vecLines.first().isEmpty())
        vecLines.removeFirst();
    while (!vecLines.isEmpty() && vecLines.last().isEmpty())
        vecLines.removeLast();
    if (vecLines.isEmpty())
        return QString();
    if (vecLines.size() == 1)
        return vecLines.first().simplified();
    const bool bBlockFirst = StartsWithBlock(vecLines);
    if (!bBlockFirst)
        vecLines.first() = vecLines.first().simplified();
    CloseOpenFence(&vecLines, bBlockFirst ? 0 : 1);
    return vecLines.join(QLatin1Char('\n'));
}

bool EvidenceStartsWithBlock(const QString& evidence)
{
    return StartsWithBlock(evidence.split(QLatin1Char('\n')));
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
}
