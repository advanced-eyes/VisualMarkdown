#include "VisualMarkdownViewer/VisualMarkdownViewerWindow.h"

#include "VisualMarkdown/UI/KnowledgeGraphView.h"
#include "VisualMarkdown/UI/KnowledgeMatrixView.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerChartPage.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerMatrixPage.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerPalette.h"

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <optional>

namespace
{
constexpr int kDefaultWidth = 1280;
constexpr int kDefaultHeight = 800;
constexpr int kTreeWidth = 300;          // 나무 처음 너비(나머지는 보기 쪽)
constexpr int kDocumentPageMargin = 32;
constexpr int kNoticeIconSize = 32;
constexpr int kNoticeTextWidth = 360;    // 알림 글이 너무 좁게 접히지 않게
constexpr int kStatusGap = 12;           // 상태 줄 수 사이 틈

// 나무 항목의 종류와 자리(data 역할)
enum class EItemKind { CATEGORY, VIEW, DOCUMENT, TABLE, NOTICE };
constexpr int kKindRole = Qt::UserRole;          // EItemKind
constexpr int kFirstRole = Qt::UserRole + 1;     // 보기: EGraphLayoutType 값. 문서 · 표: 문서 자리
constexpr int kSecondRole = Qt::UserRole + 2;    // 표: 문서 안 표 자리
constexpr int kBetweenCategory = 0;              // 머리 항목의 자리(BuildTree 가 이 순서로 만든다): 지식 사이
constexpr int kWithinCategory = 1;               // 지식 안

const QString kWindowTitle = QStringLiteral("VisualMarkdownViewer");
const QString kBetweenTitle = QStringLiteral("지식 사이");
const QString kWithinTitle = QStringLiteral("지식 안");
const QString kNoTableText = QStringLiteral("고를 표가 없다");
const QString kNoticeName = QStringLiteral("VisualMarkdownViewerNotice");

EItemKind GetKind(const QTreeWidgetItem* item)
{
    return static_cast<EItemKind>(item->data(0, kKindRole).toInt());
}

QTreeWidgetItem* MakeItem(QTreeWidgetItem* parent, const QString& text, EItemKind kind, int first = -1, int second = -1)
{
    QTreeWidgetItem* pItem = new QTreeWidgetItem(parent, QStringList { text });
    pItem->setData(0, kKindRole, static_cast<int>(kind));
    pItem->setData(0, kFirstRole, first);
    pItem->setData(0, kSecondRole, second);
    return pItem;
}

// 머리 항목(지식 사이 · 지식 안): 굵게, 고르지 않는다(펼치고 접기만)
QTreeWidgetItem* MakeCategory(QTreeWidget* tree, const QString& text)
{
    QTreeWidgetItem* pItem = new QTreeWidgetItem(tree, QStringList { text });
    pItem->setData(0, kKindRole, static_cast<int>(EItemKind::CATEGORY));
    pItem->setFlags(Qt::ItemIsEnabled);
    QFont fontCategory = pItem->font(0);
    fontCategory.setBold(true);
    pItem->setFont(0, fontCategory);
    return pItem;
}
}

VisualMarkdownViewerWindow::VisualMarkdownViewerWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_model()
    , m_theme(VisualMarkdownTheme::MakeLight())
    , m_bDark(false)
    , m_pTree(new QTreeWidget)
    , m_pStack(new QStackedWidget)
    , m_mapGraphViews()
    , m_pMatrixPage(new VisualMarkdownViewerMatrixPage)
    , m_pChartPage(new VisualMarkdownViewerChartPage)
    , m_pDocumentPage(new QLabel)
    , m_setFilledViews()
    , m_iCurrentDocument(-1)
    , m_iCurrentTable(-1)
    , m_pCountsLabel(new QLabel)
    , m_pWarningsLabel(new QLabel)
    , m_pConceptLabel(new QLabel)
    , m_pOpenAction(new QAction(QStringLiteral("폴더 열기"), this))
    , m_pDarkAction(new QAction(QStringLiteral("어둠"), this))
    , m_pFitAction(new QAction(QStringLiteral("맞춤"), this))
{
    setWindowTitle(kWindowTitle);
    resize(kDefaultWidth, kDefaultHeight);

    // 오른쪽 쪽들: 그래프 보기 6개 · 행렬 쪽 · 차트 쪽 · 문서 쪽
    for (const EGraphLayoutType eType : VisualMarkdownViewerModel::ListViews())
    {
        if (eType == EGraphLayoutType::MATRIX)
        {
            m_pStack->addWidget(m_pMatrixPage);
            continue;
        }
        KnowledgeGraphView* pView = new KnowledgeGraphView;
        m_mapGraphViews[eType] = pView;
        m_pStack->addWidget(pView);
        connect(pView, &KnowledgeGraphView::ConceptActivated, this, &VisualMarkdownViewerWindow::HandleConceptActivated);
        connect(pView, &KnowledgeGraphView::DocumentActivated, this, &VisualMarkdownViewerWindow::HandleDocumentActivated);
    }
    m_pStack->addWidget(m_pChartPage);
    m_pStack->addWidget(m_pDocumentPage);
    connect(m_pChartPage, &VisualMarkdownViewerChartPage::ChartTypeChosen, this, &VisualMarkdownViewerWindow::HandleChartTypeChosen);
    m_pDocumentPage->setTextFormat(Qt::PlainText);
    m_pDocumentPage->setAlignment(Qt::AlignCenter);
    m_pDocumentPage->setWordWrap(true);
    m_pDocumentPage->setMargin(kDocumentPageMargin);

    // 왼쪽 나무
    m_pTree->setHeaderHidden(true);
    m_pTree->setColumnCount(1);
    m_pTree->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_pTree, &QTreeWidget::currentItemChanged, this, &VisualMarkdownViewerWindow::HandleCurrentItemChanged);

    QSplitter* pSplitter = new QSplitter(Qt::Horizontal);
    pSplitter->addWidget(m_pTree);
    pSplitter->addWidget(m_pStack);
    pSplitter->setStretchFactor(0, 0);
    pSplitter->setStretchFactor(1, 1);
    pSplitter->setSizes({ kTreeWidth, kDefaultWidth - kTreeWidth });
    setCentralWidget(pSplitter);

    // 도구 막대: 폴더 열기 · 어둠(밝음/어둠) · 맞춤
    QToolBar* pToolBar = addToolBar(QStringLiteral("도구"));
    pToolBar->setMovable(false);
    m_pDarkAction->setCheckable(true);
    pToolBar->addAction(m_pOpenAction);
    pToolBar->addAction(m_pDarkAction);
    pToolBar->addAction(m_pFitAction);
    connect(m_pOpenAction, &QAction::triggered, this, &VisualMarkdownViewerWindow::ChooseFolder);
    connect(m_pDarkAction, &QAction::toggled, this, &VisualMarkdownViewerWindow::SetDarkTheme);
    connect(m_pFitAction, &QAction::triggered, this, &VisualMarkdownViewerWindow::FitCurrentView);

    // 상태 줄: 왼쪽에 고른 개념, 오른쪽에 수
    statusBar()->addWidget(m_pConceptLabel, 1);
    statusBar()->addPermanentWidget(m_pCountsLabel);
    statusBar()->addPermanentWidget(m_pWarningsLabel);
    m_pWarningsLabel->setContentsMargins(kStatusGap, 0, 0, 0);

    BuildTree();
    UpdateStatus();
    ApplyTheme();
    SelectView(EGraphLayoutType::NETWORK);
}

VisualMarkdownViewerWindow::~VisualMarkdownViewerWindow() = default;

bool VisualMarkdownViewerWindow::OpenWiki(const QString& wikiRoot)
{
    // 지금 고른 항목을 기억해 둔다(새 나무에서 같은 보기 · 같은 문서의 표를 다시 고른다)
    const QTreeWidgetItem* pCurrent = m_pTree->currentItem();
    const EItemKind eKind = pCurrent != nullptr ? GetKind(pCurrent) : EItemKind::VIEW;
    const int iFirst = pCurrent != nullptr ? pCurrent->data(0, kFirstRole).toInt() : static_cast<int>(EGraphLayoutType::NETWORK);
    const int iSecond = pCurrent != nullptr ? pCurrent->data(0, kSecondRole).toInt() : -1;
    const KnowledgeDocument* pDocument = eKind == EItemKind::DOCUMENT || eKind == EItemKind::TABLE ? m_model.FindDocument(iFirst) : nullptr;
    const QString strDocumentId = pDocument != nullptr ? pDocument->strId : QString();

    if (!m_model.Load(wikiRoot))
    {
        ShowNotice(m_model.GetLastError());   // "wiki 를 읽지 못했다: <경로> (까닭)"
        return false;
    }

    m_setFilledViews.clear();
    m_iCurrentDocument = -1;
    m_iCurrentTable = -1;
    m_pConceptLabel->clear();
    setWindowTitle(kWindowTitle + QStringLiteral(" — ") + wikiRoot);
    BuildTree();
    UpdateStatus();

    const int iDocument = strDocumentId.isEmpty() ? -1 : m_model.FindDocumentIndex(strDocumentId);
    if (eKind == EItemKind::TABLE && m_model.FindTable(iDocument, iSecond) != nullptr)
        SelectTable(iDocument, iSecond);
    else if ((eKind == EItemKind::DOCUMENT || eKind == EItemKind::TABLE) && iDocument >= 0)
        SelectDocument(iDocument);
    else if (eKind == EItemKind::VIEW)
        SelectView(static_cast<EGraphLayoutType>(iFirst));
    else
        SelectView(EGraphLayoutType::NETWORK);
    return true;
}

void VisualMarkdownViewerWindow::SetDarkTheme(bool dark)
{
    m_bDark = dark;
    m_theme = dark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
    if (m_pDarkAction->isChecked() != dark)
    {
        const QSignalBlocker blocker(m_pDarkAction);
        m_pDarkAction->setChecked(dark);
    }
    ApplyTheme();
}

void VisualMarkdownViewerWindow::SelectView(EGraphLayoutType type)
{
    QTreeWidgetItem* pItem = FindViewItem(type);
    if (pItem == nullptr)
        return;
    if (m_pTree->currentItem() == pItem)
        ShowItem(pItem);
    else
        m_pTree->setCurrentItem(pItem);
}

void VisualMarkdownViewerWindow::SelectDocument(int document)
{
    QTreeWidgetItem* pItem = FindDocumentItem(document);
    if (pItem == nullptr)
        return;
    if (m_pTree->currentItem() == pItem)
        ShowItem(pItem);
    else
        m_pTree->setCurrentItem(pItem);
}

void VisualMarkdownViewerWindow::SelectTable(int document, int table)
{
    QTreeWidgetItem* pItem = FindTableItem(document, table);
    if (pItem == nullptr)
        return;
    if (m_pTree->currentItem() == pItem)
        ShowItem(pItem);
    else
        m_pTree->setCurrentItem(pItem);
}

void VisualMarkdownViewerWindow::FitCurrentView()
{
    KnowledgeGraphView* pView = qobject_cast<KnowledgeGraphView*>(m_pStack->currentWidget());
    if (pView != nullptr)
        pView->FitToView();
}

QTreeWidgetItem* VisualMarkdownViewerWindow::FindViewItem(EGraphLayoutType type) const
{
    QTreeWidgetItem* pBetween = m_pTree->topLevelItem(kBetweenCategory);
    for (int iChild = 0; pBetween != nullptr && iChild < pBetween->childCount(); ++iChild)
    {
        QTreeWidgetItem* pItem = pBetween->child(iChild);
        if (GetKind(pItem) == EItemKind::VIEW && pItem->data(0, kFirstRole).toInt() == static_cast<int>(type))
            return pItem;
    }
    return nullptr;
}

QTreeWidgetItem* VisualMarkdownViewerWindow::FindDocumentItem(int document) const
{
    QTreeWidgetItem* pWithin = m_pTree->topLevelItem(kWithinCategory);
    if (pWithin == nullptr || document < 0 || document >= pWithin->childCount())
        return nullptr;
    QTreeWidgetItem* pItem = pWithin->child(document);
    return GetKind(pItem) == EItemKind::DOCUMENT && pItem->data(0, kFirstRole).toInt() == document ? pItem : nullptr;
}

QTreeWidgetItem* VisualMarkdownViewerWindow::FindTableItem(int document, int table) const
{
    QTreeWidgetItem* pDocument = FindDocumentItem(document);
    for (int iChild = 0; pDocument != nullptr && iChild < pDocument->childCount(); ++iChild)
    {
        QTreeWidgetItem* pItem = pDocument->child(iChild);
        if (GetKind(pItem) == EItemKind::TABLE && pItem->data(0, kSecondRole).toInt() == table)
            return pItem;
    }
    return nullptr;
}

const VisualMarkdownViewerModel& VisualMarkdownViewerWindow::GetModel() const
{
    return m_model;
}

const VisualMarkdownTheme& VisualMarkdownViewerWindow::GetTheme() const
{
    return m_theme;
}

bool VisualMarkdownViewerWindow::IsDarkTheme() const
{
    return m_bDark;
}

QTreeWidget* VisualMarkdownViewerWindow::GetTree() const
{
    return m_pTree;
}

QStackedWidget* VisualMarkdownViewerWindow::GetStack() const
{
    return m_pStack;
}

KnowledgeGraphView* VisualMarkdownViewerWindow::GetGraphView(EGraphLayoutType type) const
{
    const auto it = m_mapGraphViews.find(type);
    return it == m_mapGraphViews.end() ? nullptr : it->second;
}

VisualMarkdownViewerMatrixPage* VisualMarkdownViewerWindow::GetMatrixPage() const
{
    return m_pMatrixPage;
}

VisualMarkdownViewerChartPage* VisualMarkdownViewerWindow::GetChartPage() const
{
    return m_pChartPage;
}

QLabel* VisualMarkdownViewerWindow::GetDocumentPage() const
{
    return m_pDocumentPage;
}

QLabel* VisualMarkdownViewerWindow::GetCountsLabel() const
{
    return m_pCountsLabel;
}

QLabel* VisualMarkdownViewerWindow::GetWarningsLabel() const
{
    return m_pWarningsLabel;
}

QLabel* VisualMarkdownViewerWindow::GetConceptLabel() const
{
    return m_pConceptLabel;
}

QAction* VisualMarkdownViewerWindow::GetOpenAction() const
{
    return m_pOpenAction;
}

QAction* VisualMarkdownViewerWindow::GetDarkAction() const
{
    return m_pDarkAction;
}

QAction* VisualMarkdownViewerWindow::GetFitAction() const
{
    return m_pFitAction;
}

void VisualMarkdownViewerWindow::BuildTree()
{
    const QSignalBlocker blocker(m_pTree);
    m_pTree->clear();
    QTreeWidgetItem* pBetween = MakeCategory(m_pTree, kBetweenTitle);
    for (const EGraphLayoutType eType : VisualMarkdownViewerModel::ListViews())
        MakeItem(pBetween, m_model.GetViewTitle(eType), EItemKind::VIEW, static_cast<int>(eType));   // 이웃은 "이웃 · {초점 이름}"

    // 지식 안: 문서(문서 목록 순)와 그 아래 표. 표가 없는 문서는 고를 수 없는 알림 항목을 단다
    QTreeWidgetItem* pWithin = MakeCategory(m_pTree, kWithinTitle);
    for (int iDocument = 0; iDocument < m_model.GetDocumentCount(); ++iDocument)
    {
        const KnowledgeDocument* pDocument = m_model.FindDocument(iDocument);
        const QString strTitle = pDocument->strTitle.isEmpty() ? pDocument->strId : pDocument->strTitle;
        QTreeWidgetItem* pDocumentItem = MakeItem(pWithin, strTitle, EItemKind::DOCUMENT, iDocument);
        const int iTables = m_model.GetTableCount(iDocument);
        pDocumentItem->setToolTip(0, strTitle + QLatin1Char('\n') + (iTables > 0 ? QStringLiteral("표 %1개").arg(iTables) : kNoTableText));
        if (iTables == 0)
        {
            QTreeWidgetItem* pNotice = MakeItem(pDocumentItem, kNoTableText, EItemKind::NOTICE, iDocument);
            pNotice->setFlags(Qt::NoItemFlags);
        }
        for (int iTable = 0; iTable < iTables; ++iTable)
        {
            MakeItem(pDocumentItem, QString(), EItemKind::TABLE, iDocument, iTable);
            UpdateTableItem(iDocument, iTable);
        }
    }
    m_pTree->expandAll();
}

void VisualMarkdownViewerWindow::UpdateStatus()
{
    m_pCountsLabel->setText(QStringLiteral("점 %1 · 선 %2 · 문서 %3 · 빠진 관계 %4")
                                .arg(m_model.GetNodeCount())
                                .arg(m_model.GetEdgeCount())
                                .arg(m_model.GetDocumentCount())
                                .arg(m_model.GetDroppedRelationCount()));
    const QStringList vecDropped = m_model.DescribeDroppedRelations();
    m_pCountsLabel->setToolTip(vecDropped.isEmpty() ? QStringLiteral("빠진 관계 없음")
                                                    : QStringLiteral("빠진 관계(개념 파일이 없다)\n") + vecDropped.join(QLatin1Char('\n')));
    const QStringList vecWarnings = m_model.GetWarnings();
    m_pWarningsLabel->setText(QStringLiteral("경고 %1").arg(vecWarnings.size()));
    m_pWarningsLabel->setToolTip(vecWarnings.isEmpty() ? QStringLiteral("경고 없음") : vecWarnings.join(QLatin1Char('\n')));
}

void VisualMarkdownViewerWindow::UpdateTableItem(int document, int table)
{
    QTreeWidgetItem* pItem = FindTableItem(document, table);
    const KnowledgeTable* pTable = m_model.FindTable(document, table);
    if (pItem == nullptr || pTable == nullptr)
        return;
    // 표 제목, 뒤에 지금 차트 종류(고른 것, 없으면 추천)
    pItem->setText(0, QStringLiteral("%1 (%2)").arg(pTable->strTitle, VisualMarkdownViewerModel::GetChartTypeLabel(m_model.GetChartType(document, table))));
}

void VisualMarkdownViewerWindow::ShowItem(QTreeWidgetItem* item)
{
    if (item == nullptr)
        return;
    switch (GetKind(item))
    {
    case EItemKind::VIEW:
        ShowView(static_cast<EGraphLayoutType>(item->data(0, kFirstRole).toInt()));
        break;
    case EItemKind::DOCUMENT:
        ShowDocument(item->data(0, kFirstRole).toInt());
        break;
    case EItemKind::TABLE:
        ShowTable(item->data(0, kFirstRole).toInt(), item->data(0, kSecondRole).toInt());
        break;
    case EItemKind::CATEGORY:
    case EItemKind::NOTICE:
        break;   // 쪽을 바꾸지 않는다
    }
}

void VisualMarkdownViewerWindow::ShowView(EGraphLayoutType type)
{
    // 배치는 처음 열 때 계산해 넣고, wiki 를 다시 읽을 때까지 그대로 둔다
    if (!m_setFilledViews.contains(static_cast<int>(type)))
    {
        const GraphLayoutOutput& layout = m_model.GetLayout(type);
        if (type == EGraphLayoutType::MATRIX)
            m_pMatrixPage->GetMatrixView()->SetGraph(m_model.GetGraph(), layout);
        else
            m_mapGraphViews[type]->SetGraph(m_model.GetGraph(), layout, type);
        m_setFilledViews.insert(static_cast<int>(type));
    }
    if (type == EGraphLayoutType::MATRIX)
        m_pStack->setCurrentWidget(m_pMatrixPage);
    else
        m_pStack->setCurrentWidget(m_mapGraphViews[type]);
    m_pFitAction->setEnabled(type != EGraphLayoutType::MATRIX);
}

void VisualMarkdownViewerWindow::ShowDocument(int document)
{
    const KnowledgeDocument* pDocument = m_model.FindDocument(document);
    if (pDocument == nullptr)
        return;
    QStringList vecLines { pDocument->strTitle.isEmpty() ? pDocument->strId : pDocument->strTitle };
    if (!pDocument->strMeta.isEmpty())
        vecLines << pDocument->strMeta;
    vecLines << QString();
    const int iTables = m_model.GetTableCount(document);
    if (iTables == 0)
    {
        vecLines << kNoTableText;
    }
    else
    {
        vecLines << QStringLiteral("표 %1개: 나무에서 표를 고르면 차트와 원래 표를 본다").arg(iTables);
        for (int iTable = 0; iTable < iTables; ++iTable)
            vecLines << QStringLiteral("%1 (%2)").arg(m_model.FindTable(document, iTable)->strTitle,
                                                      VisualMarkdownViewerModel::GetChartTypeLabel(m_model.GetChartType(document, iTable)));
    }
    m_pDocumentPage->setText(vecLines.join(QLatin1Char('\n')));
    m_pStack->setCurrentWidget(m_pDocumentPage);
    m_pFitAction->setEnabled(false);
}

void VisualMarkdownViewerWindow::ShowTable(int document, int table)
{
    const KnowledgeTable* pTable = m_model.FindTable(document, table);
    if (pTable == nullptr)
        return;
    // 차트 설정은 표를 고를 때 만든다
    m_iCurrentDocument = document;
    m_iCurrentTable = table;
    m_pChartPage->SetTable(*pTable, m_model.GetRecommendedChart(document, table), m_model.GetChosenChart(document, table),
                           m_model.BuildChart(document, table));
    m_pStack->setCurrentWidget(m_pChartPage);
    m_pFitAction->setEnabled(false);
}

void VisualMarkdownViewerWindow::ApplyTheme()
{
    setPalette(VisualMarkdownViewerPalette::Make(m_theme));
    for (const auto& pairView : m_mapGraphViews)
        pairView.second->SetTheme(m_theme);
    m_pMatrixPage->GetMatrixView()->SetTheme(m_theme);
    m_pChartPage->SetTheme(m_theme);
}

void VisualMarkdownViewerWindow::ShowNotice(const QString& text)
{
    // 막지 않는 알림 창(창은 앞의 wiki 를 그대로 보인다). QMessageBox 를 쓰지 않는 까닭: Windows 의 offscreen 플랫폼(시험이 돈다)은
    // native interface 가 없어 QMessageBox::showEvent(시스템 메뉴를 찾는다)가 죽는다. QDialog 는 그렇지 않다
    QDialog* pDialog = new QDialog(this);
    pDialog->setObjectName(kNoticeName);
    pDialog->setWindowTitle(m_pOpenAction->text());
    pDialog->setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    pDialog->setAttribute(Qt::WA_DeleteOnClose);
    QLabel* pIcon = new QLabel(pDialog);
    pIcon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(kNoticeIconSize, kNoticeIconSize));
    pIcon->setAlignment(Qt::AlignTop);
    QLabel* pText = new QLabel(text, pDialog);
    pText->setTextFormat(Qt::PlainText);
    pText->setWordWrap(true);
    pText->setMinimumWidth(kNoticeTextWidth);
    pText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    QDialogButtonBox* pButtons = new QDialogButtonBox(QDialogButtonBox::Ok, pDialog);
    connect(pButtons, &QDialogButtonBox::accepted, pDialog, &QDialog::accept);
    QHBoxLayout* pMessageLayout = new QHBoxLayout;
    pMessageLayout->addWidget(pIcon);
    pMessageLayout->addWidget(pText, 1);
    QVBoxLayout* pLayout = new QVBoxLayout(pDialog);
    pLayout->addLayout(pMessageLayout);
    pLayout->addWidget(pButtons);
    pDialog->open();
}

void VisualMarkdownViewerWindow::ChooseFolder()
{
    const QString strFolder = QFileDialog::getExistingDirectory(this, QStringLiteral("wiki 폴더 열기"), m_model.GetWikiRoot());
    if (!strFolder.isEmpty())
        OpenWiki(strFolder);
}

void VisualMarkdownViewerWindow::HandleCurrentItemChanged(QTreeWidgetItem* current)
{
    ShowItem(current);
}

void VisualMarkdownViewerWindow::HandleConceptActivated(const QString& conceptId)
{
    // 시그널 글은 보낸 보기의 배치 안을 가리킬 수 있으니 먼저 베껴 둔다
    const QString strConceptId = conceptId;
    m_pConceptLabel->setText(m_model.DescribeConcept(strConceptId));

    // 이웃 보기의 초점을 그 개념으로 옮긴다(design/MoreViews.md §6.2). 실제 초점이 바뀌었을 때만 배치를 버리고 나무 글을 고친다
    if (!m_model.SetFocusConcept(strConceptId))
        return;
    m_setFilledViews.remove(static_cast<int>(EGraphLayoutType::EGO));
    QTreeWidgetItem* pEgoItem = FindViewItem(EGraphLayoutType::EGO);
    if (pEgoItem != nullptr)
        pEgoItem->setText(0, m_model.GetViewTitle(EGraphLayoutType::EGO));
    // 이웃 보기를 보고 있으면 바로 다시 배치해 보인다. 이 시그널은 그 보기의 누르기 처리 안에서 오므로,
    // 누르기 처리가 끝난 뒤(다음 이벤트 차례)에 바꿔 그 보기가 지금 쓰는 배치를 처리 도중에 지우지 않게 한다
    if (m_pStack->currentWidget() == GetGraphView(EGraphLayoutType::EGO))
        QTimer::singleShot(0, this, &VisualMarkdownViewerWindow::RefreshEgoView);
}

void VisualMarkdownViewerWindow::RefreshEgoView()
{
    // 그사이 다른 쪽으로 갔으면 그대로 둔다(이웃을 다시 열 때 새 초점으로 배치한다). 이미 다시 넣었으면 또 하지 않는다
    if (m_pStack->currentWidget() != GetGraphView(EGraphLayoutType::EGO) || m_setFilledViews.contains(static_cast<int>(EGraphLayoutType::EGO)))
        return;
    ShowView(EGraphLayoutType::EGO);   // 새 배치를 넣고 맞춘다(KnowledgeGraphView::SetGraph)
}

void VisualMarkdownViewerWindow::HandleDocumentActivated(const QString& documentId)
{
    const int iDocument = m_model.FindDocumentIndex(documentId);
    if (iDocument >= 0)
        SelectDocument(iDocument);
}

void VisualMarkdownViewerWindow::HandleChartTypeChosen(int choice)
{
    if (m_model.FindTable(m_iCurrentDocument, m_iCurrentTable) == nullptr)
        return;
    const std::optional<EChartType> optType = choice == VisualMarkdownViewerChartPage::kRecommendedChoice ? std::nullopt
                                                                                                         : std::optional<EChartType>(static_cast<EChartType>(choice));
    // 고르면 SetStrategy, 추천이면 ResetStrategy 로 다시 만든다(VisualMarkdownViewerModel::BuildChart)
    m_model.ChooseChart(m_iCurrentDocument, m_iCurrentTable, optType);
    ShowTable(m_iCurrentDocument, m_iCurrentTable);
    UpdateTableItem(m_iCurrentDocument, m_iCurrentTable);
}
