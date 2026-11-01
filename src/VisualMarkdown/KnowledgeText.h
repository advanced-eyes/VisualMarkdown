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

// 카드 근거(여러 줄 마크다운)를 파일에 쓰고 다시 읽었을 때의 글로 만든다(format/DocumentFile.md §5.1).
QString NormalizeEvidence(const QString& evidence);
// 정리한 근거가 여러 줄이고 첫 줄이 블록(목록 · 울타리 · $$ · 표 · 제목 · 인용 · 들여 쓴 코드 …)을 여는가.
// 그런 근거는 `  - 근거:` 뒤를 비우고 빈 줄 하나 다음에 첫 줄부터 쓴다(format/DocumentFile.md §5.1).
bool EvidenceStartsWithBlock(const QString& evidence);
}
