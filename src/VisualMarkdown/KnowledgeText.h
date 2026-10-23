#pragma once

#include "VisualMarkdown/KnowledgeTypes.h"

#include <QSet>
#include <QString>
#include <QStringList>

namespace KnowledgeText
{
QString MakeSlug(const QString& name, const QString& fallback);
QString MakeUniqueId(const QString& slug, const QSet<QString>& taken);
QString MakeNameKey(const QString& name);
bool IsQuantityText(const QString& cell);
QStringList ExtractNumbers(const QString& text);
QString MakeCardId(const QSet<QString>& taken);
EColumnKind ClassifyColumn(const KnowledgeTable& table, int column);

// 이름 정리: 기록을 파일 문법에 맞춰, 쓰고 다시 읽어도 같은 기록이 되게 한다(format/DataTable.md §5 · ConceptFile.md §1).
QString CleanName(const QString& name);
QStringList CleanNames(const QStringList& names);
void CleanColumns(KnowledgeTable* table);
}
