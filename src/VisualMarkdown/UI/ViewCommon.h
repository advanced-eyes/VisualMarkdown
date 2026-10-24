#pragma once

#include "VisualMarkdown/KnowledgeTypes.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"

#include <QFont>
#include <QString>

class QHelpEvent;
class QPainter;
class QRectF;
class QWidget;

// 보기 위젯(KnowledgeGraphView · KnowledgeMatrixView · KnowledgeChartView)이 함께 쓰는 것: 값 자르기, 글꼴, 상태 이름표,
// 마우스 올림 설명, 범례 판. 위젯 .cpp 만 쓴다(쓰는 쪽에 내놓는 머리글이 아니다).
namespace ViewCommon
{
// value 를 low … high 안에 둔다(value 가 NaN 이면 high)
double Clamp(double value, double low, double high);
// 위젯 글꼴 base 에 테마의 글꼴 이름(비었으면 base 그대로) · 글자 크기(px, 1 아래는 1) · 굵기를 입힌다
QFont MakeFont(const QFont& base, const VisualMarkdownTheme& theme, int pixelSize, bool bold);
// 관계 상태의 어휘 이름표(확인됨 · AI 제안)
QString GetStatusLabel(ERelationStatus status);
// QEvent::ToolTip 에서 tip 을 보인다. tip 이 비었으면 설명을 숨기고 이벤트를 넘긴다(ignore)
void ShowToolTip(QWidget* widget, QHelpEvent* event, const QString& tip);
// 범례 · 목록 판: 테마의 판 색으로 채우고 1px 테두리를 두른 둥근 사각형
void DrawPanel(QPainter& painter, const QRectF& rect, const VisualMarkdownTheme& theme);
}
