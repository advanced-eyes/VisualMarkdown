#pragma once

#include "VisualMarkdown/UI/VisualMarkdownTheme.h"

#include <QPalette>

// 창 · 쪽의 팔레트를 보기 테마의 바탕 · 글자에 맞춘다(밝음/어둠). 창과 스냅샷이 같은 팔레트를 쓴다
namespace VisualMarkdownViewerPalette
{
QPalette Make(const VisualMarkdownTheme& theme);
}
