#pragma once

#include "VisualMarkdown/Algorithm/GraphLayout/Common/GraphLayoutTypes.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerModel.h"

#include <QMainWindow>
#include <QSet>
#include <QString>

#include <map>

class KnowledgeGraphView;
class QAction;
class QLabel;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class VisualMarkdownViewerChartPage;
class VisualMarkdownViewerMatrixPage;

// 보기 앱 창(design/Views.md §8). 왼쪽 나무(지식 사이 보기 6개 · 지식 안 문서 → 표)에서 고른 보기를 오른쪽에 보인다.
// 도구 막대: 폴더 열기 · 어둠(밝음/어둠) · 맞춤. 상태 줄: 점 · 선 · 문서 · 빠진 관계 · 경고 수(경고는 마우스 올림으로 목록), 고른 개념.
// 화면 로직은 VisualMarkdownViewerModel 이 맡고, 창은 고른 것을 쪽에 넣고 보인다.
class VisualMarkdownViewerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit VisualMarkdownViewerWindow(QWidget* parent = nullptr);
    ~VisualMarkdownViewerWindow() override;

public:
    // wiki 를 다시 읽는다. 읽지 못하면 알림 창(막지 않는 QDialog, 그 경로와 까닭)을 띄우고 앞의 wiki 를 그대로 둔다
    bool OpenWiki(const QString& wikiRoot);
    void SetDarkTheme(bool dark);   // 모든 보기의 테마와 창 팔레트
    void SelectView(EGraphLayoutType type);
    void SelectDocument(int document);
    void SelectTable(int document, int table);
    void FitCurrentView();          // 지금 그래프 보기의 FitToView

public:
    // 나무 항목 찾기. 없으면 nullptr
    QTreeWidgetItem* FindViewItem(EGraphLayoutType type) const;
    QTreeWidgetItem* FindDocumentItem(int document) const;
    QTreeWidgetItem* FindTableItem(int document, int table) const;

public:
    const VisualMarkdownViewerModel& GetModel() const;
    const VisualMarkdownTheme& GetTheme() const;
    bool IsDarkTheme() const;
    QTreeWidget* GetTree() const;
    QStackedWidget* GetStack() const;
    KnowledgeGraphView* GetGraphView(EGraphLayoutType type) const;   // MATRIX 는 nullptr(GetMatrixPage)
    VisualMarkdownViewerMatrixPage* GetMatrixPage() const;
    VisualMarkdownViewerChartPage* GetChartPage() const;
    QLabel* GetDocumentPage() const;
    QLabel* GetCountsLabel() const;      // 점 · 선 · 문서 · 빠진 관계
    QLabel* GetWarningsLabel() const;    // 경고 수. 마우스 올림 설명이 경고 목록이다
    QLabel* GetConceptLabel() const;     // 고른 개념의 이름 · 종류 · 연도
    QAction* GetOpenAction() const;
    QAction* GetDarkAction() const;
    QAction* GetFitAction() const;

private:
    void BuildTree();
    void UpdateStatus();
    void UpdateTableItem(int document, int table);
    void ShowItem(QTreeWidgetItem* item);
    void ShowView(EGraphLayoutType type);
    void ShowDocument(int document);
    void ShowTable(int document, int table);
    void ApplyTheme();
    void ShowNotice(const QString& text);

private slots:
    void ChooseFolder();
    void HandleCurrentItemChanged(QTreeWidgetItem* current);   // 나무에서 고른 항목의 쪽을 보인다
    void HandleConceptActivated(const QString& conceptId);
    void HandleDocumentActivated(const QString& documentId);
    void HandleChartTypeChosen(int choice);

private:
    VisualMarkdownViewerModel m_model;
    VisualMarkdownTheme m_theme;
    bool m_bDark;

    QTreeWidget* m_pTree;
    QStackedWidget* m_pStack;
    std::map<EGraphLayoutType, KnowledgeGraphView*> m_mapGraphViews;
    VisualMarkdownViewerMatrixPage* m_pMatrixPage;
    VisualMarkdownViewerChartPage* m_pChartPage;
    QLabel* m_pDocumentPage;
    QSet<int> m_setFilledViews;          // 지금 wiki 의 배치를 넣은 보기(EGraphLayoutType 값)
    int m_iCurrentDocument;              // 차트 쪽에 보인 표
    int m_iCurrentTable;

    QLabel* m_pCountsLabel;
    QLabel* m_pWarningsLabel;
    QLabel* m_pConceptLabel;
    QAction* m_pOpenAction;
    QAction* m_pDarkAction;
    QAction* m_pFitAction;
};
