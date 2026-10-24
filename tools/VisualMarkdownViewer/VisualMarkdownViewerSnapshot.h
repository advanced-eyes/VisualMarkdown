#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerOptions.h"

#include <QString>
#include <QStringList>

// 스냅샷 모드(design/Views.md §8): 보기 7개와 표마다의 차트를 PNG 로 남기고, 그 목록을 snapshot.tsv 로 쓴다.
// 창이 보이는 것과 같은 쪽 위젯(그래프 보기 · 행렬 쪽 · 차트 쪽)을 --size 크기로 그린다. 차트 쪽은 차트와 아래 표를 함께 그린다.
// 파일: graph-<보기>.png(보기 순), chart-DD-TT.png(DD 문서 목록 자리, TT 문서 안 표 자리, 1부터 두 자리), snapshot.tsv
// snapshot.tsv: UTF-8 · LF · 탭 구분, 머리 줄 "파일 · 보기 · 문서 id · 표 제목 · 차트 종류". 그래프 줄의 뒤 세 칸은 비었다.
class VisualMarkdownViewerSnapshot
{
public:
    // options 의 wiki · 스냅샷 폴더 · 테마 · 크기로 찍는다. 끝 코드(VisualMarkdownViewerExit)를 돌려주고, 실패 까닭은 errors 에 더한다
    static int Run(const VisualMarkdownViewerOptions& options, QStringList* errors);

public:
    static QString MakeGraphFileName(EGraphLayoutType type);    // graph-network.png …
    static QString MakeChartFileName(int document, int table);  // 0부터 받은 자리 → chart-01-01.png …
    static QString GetListFileName();                           // snapshot.tsv
};
