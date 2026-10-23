#pragma once

#include "VisualMarkdown/KnowledgeTypes.h"

struct KnowledgeLayout;

namespace KnowledgeMarkdownReader
{
DocumentReadResult ReadDocument(const QString& text);
ConceptReadResult ReadConcept(const QString& text);
// 머리글을 따지지 않고 문서의 정해진 절만 읽어, 읽지 못한 줄을 절 이름별로 모은다(쓰기가 있는 글에 갈아 끼울 때).
// 읽지 못한 줄 덩어리 안의 빈 줄은 빈 글자로 들어 있다(ReadDocument 의 hashUnreadLines 에는 빈 줄이 없다).
QHash<QString, QStringList> ReadUnreadLines(const KnowledgeLayout& layout);
}
