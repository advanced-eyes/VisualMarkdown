#pragma once

#include "VisualMarkdown/KnowledgeTypes.h"

// 지식 파일을 쓴다. existingText 가 있으면 정해진 부분(머리글의 아는 키 · 제목 · 한 줄 · 정해진 절)만 갈아 끼우고
// 나머지 줄은 글자 그대로 둔다. 결과는 늘 LF, BOM 없음, 끝 줄바꿈 하나다.
namespace KnowledgeMarkdownWriter
{
QString WriteDocument(const KnowledgeDocument& document, const QString& existingText = QString());   // digest 를 새로 계산해 머리글에 쓴다
QString WriteConcept(const KnowledgeConcept& concept, const QString& existingText = QString());       // 머리글 아는 키 · 제목 · 한 줄만. `## 관계` 는 만들지 않고, 있으면 그대로 둔다
QString ComputeDigest(const KnowledgeDocument& document);   // 정준 글(제목 줄부터 마지막 정해진 절까지)의 SHA-256, 소문자 16진수 64자리
}
