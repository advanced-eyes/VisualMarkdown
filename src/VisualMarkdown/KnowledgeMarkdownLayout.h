#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// 머리글 값의 YAML 모양. 다시 쓸 때 그 키의 모양을 지킨다.
enum class EHeaderStyle { PLAIN, DOUBLE_QUOTED, SINGLE_QUOTED, BLOCK_LIST, FLOW_LIST };

// 줄 번호는 모두 0부터. vecLines 는 정규화한 글(\r\n -> \n, 맨 앞 BOM 제거)을 \n 으로 나눈 것이다.
// 끝 줄바꿈이 있으면 마지막 원소는 빈 글자다. 없는 줄 번호는 -1.
// 머리글이 닫히지 않았으면 첫 줄부터 본문으로도 훑는다(제목 · 절 범위가 채워진다).
struct KnowledgeHeaderLine
{
    QString strKey;
    QString strValue;          // 앞뒤 공백 · 감싼 따옴표를 뺀 값(줄바꿈은 공백). BLOCK_LIST 면 항목을 ", " 로 이은 것, FLOW_LIST 면 [..] 원문
    QStringList vecItems;      // 목록(BLOCK_LIST · FLOW_LIST)의 항목(따옴표를 뺀 것, 빈 항목은 뺀다)
    EHeaderStyle eStyle = EHeaderStyle::PLAIN;
    QString strItemIndent;     // BLOCK_LIST 항목 줄의 "-" 앞 들여쓰기
    int iLine = -1;
    int iLastLine = -1;        // 그 키가 차지한 마지막 줄(BLOCK_LIST 면 마지막 항목 줄)
};

struct KnowledgeSectionRange
{
    QString strName;
    int iHeadingLine = -1;
    int iLastLine = -1;   // 마지막 비지 않은 줄(내용이 없으면 머리 줄)
};

struct KnowledgeLayout
{
    QString FindHeaderValue(const QString& key) const;
    const KnowledgeHeaderLine* FindHeader(const QString& key) const;
    bool HasHeaderKey(const QString& key) const;

    QStringList vecLines;
    bool bHasFrontMatter = false;     // 첫 줄이 ---
    bool bFrontMatterClosed = false;
    int iFrontMatterStart = -1;       // 여는 --- 줄
    int iFrontMatterEnd = -1;         // 닫는 --- 줄
    QVector<KnowledgeHeaderLine> vecHeader;
    int iTitleLine = -1;
    QString strTitle;
    int iSummaryFirst = -1;           // 한 줄(첫 문단)의 줄 범위
    int iSummaryLast = -1;
    QString strSummary;               // 줄들을 공백 하나로 이은 것. 맨 앞의 EscapeParagraphStart 이스케이프 \ 하나는 뺀다
    QVector<KnowledgeSectionRange> vecSections;
};

namespace KnowledgeMarkdownLayout
{
QString Normalize(const QString& text);
KnowledgeLayout Scan(const QString& text);
bool IsFenceLine(const QString& line);
// 한 줄 문단으로 쓸 글(한 줄 · 정의)이 블록(그림 · 인용 · 목록 · 표 · HTML · 울타리 · 제목 · 구분선)을 여는 줄로
// 읽히지 않게 GFM 이스케이프 \ 하나를 넣는다(`\<` `\-` `\#` …, 번호 목록은 `1\.`). Scan 은 한 줄을 읽을 때 그 \ 하나를 뺀다.
QString EscapeParagraphStart(const QString& text);
}
