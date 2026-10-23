#include "VisualMarkdown/KnowledgeText.h"

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
}
