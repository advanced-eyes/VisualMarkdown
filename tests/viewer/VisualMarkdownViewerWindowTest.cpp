#include "VisualMarkdown/UI/KnowledgeChartView.h"
#include "VisualMarkdown/UI/KnowledgeGraphView.h"
#include "VisualMarkdown/UI/KnowledgeMatrixView.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerChartPage.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerMatrixPage.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerWindow.h"
#include "ViewTestSupport.h"
#include "ViewerTestSupport.h"

#include <QAction>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QScrollBar>
#include <QSignalSpy>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

#include <memory>

namespace
{
const QSize kWindowSize(1600, 1000);
const QSize kSmallWindowSize(720, 520);   // 관계 행렬(31 × 31)이 가장 작은 칸에서도 들어가지 않는 창

// 나무(문서 목록 순: learned 늦은 것부터)
constexpr const char* kBetweenTitle = "지식 사이";
constexpr const char* kWithinTitle = "지식 안";
const QStringList kViewLabels { "관계망", "계보도", "분류 트리", "타임라인", "관계 행렬", "문서 ↔ 개념" };
const QStringList kDocumentTitles { "결제 서비스 개편 기획서", "Focal Loss for Dense Object Detection", "2단계 검출기의 흐름",
                                    "YOLOv3: An Incremental Improvement", "YOLO9000: Better, Faster, Stronger",
                                    "You Only Look Once: Unified, Real-Time Object Detection" };
const QVector<int> kTableCounts { 4, 0, 2, 3, 3, 1 };
constexpr const char* kNoTableText = "고를 표가 없다";

constexpr int kPaymentDocument = 0;
constexpr int kFocalLossDocument = 1;
constexpr int kYolo9000Document = 4;
constexpr const char* kYolo9000Id = "5e2d8b10-7c4a-4f3e-b9d6-2a1f0c9e8d72";
constexpr int kMonthlyTable = 0;     // 월별 응답 시간: 순서 x → 선, p95 에 빈 칸
constexpr int kLoadTable = 2;        // 동시 사용자와 응답 시간: 수치 x → 산점도
constexpr int kPriorityTable = 3;    // 기능 우선순위: y 가 범주 → 그리지 않음
constexpr const char* kMonthlyTitle = "월별 응답 시간";
constexpr const char* kYoloFile = "documents/you-only-look-once-unified-real-time-object-detection.md";

const QString kCountsText = QStringLiteral("점 31 · 선 36 · 문서 6 · 빠진 관계 1");
const QString kNoWarningsText = QStringLiteral("경고 0");

QString DecodeUtf8(const char* text) { return QString::fromUtf8(text); }

int FindLayoutNode(const GraphLayoutOutput& layout, const QString& id, bool document)
{
    for (int iNode = 0; iNode < layout.vecNodes.size(); ++iNode)
        if (layout.vecNodes[iNode].strId == id && layout.vecNodes[iNode].bDocument == document)
            return iNode;
    return -1;
}

// 위젯 왼쪽 위 점의 색(모든 보기는 여백에 바탕만 칠한다)
QColor GetCornerColor(QWidget& widget)
{
    return ViewTestSupport::Render(widget).pixelColor(1, 1);
}

// 창이 이미 계산하게 한 배치(시험이 계산하지 않는다). 아직 없으면 빈 배치
const GraphLayoutOutput& GetOpenedLayout(const VisualMarkdownViewerWindow& window, EGraphLayoutType type)
{
    static const GraphLayoutOutput kEmptyLayout;
    const GraphLayoutOutput* pLayout = window.GetModel().FindLayout(type);
    return pLayout != nullptr ? *pLayout : kEmptyLayout;
}
}

// 보기 앱 창(VisualMarkdownViewerWindow)을 showcase 로 시험한다(offscreen)
class VisualMarkdownViewerWindowTest : public QObject
{
    Q_OBJECT

private slots:
    // 시험마다 새 창을 1600 × 1000 으로 띄우고 showcase 를 연다
    void init()
    {
        m_upWindow = std::make_unique<VisualMarkdownViewerWindow>();
        m_upWindow->resize(kWindowSize);
        m_upWindow->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_upWindow.get()));
        QVERIFY2(m_upWindow->OpenWiki(ViewerTestSupport::GetShowcaseDir()), qUtf8Printable(m_upWindow->GetModel().GetLastError()));
        QCoreApplication::processEvents();
    }

    void cleanup()
    {
        m_upWindow.reset();
    }

    // 나무: "지식 사이" 아래 보기 6개, "지식 안" 아래 문서(문서 목록 순)와 그 아래 표(표 제목, 뒤에 차트 종류).
    // 표가 없는 문서도 보이되 고를 표가 없다고 알린다. 머리 항목은 고르지 않는다
    void treeListsViewsDocumentsAndTables()
    {
        VisualMarkdownViewerWindow& window = *m_upWindow;
        QTreeWidget* pTree = window.GetTree();
        QCOMPARE(pTree->topLevelItemCount(), 2);
        QTreeWidgetItem* pBetween = pTree->topLevelItem(0);
        QTreeWidgetItem* pWithin = pTree->topLevelItem(1);
        QCOMPARE(pBetween->text(0), DecodeUtf8(kBetweenTitle));
        QCOMPARE(pWithin->text(0), DecodeUtf8(kWithinTitle));
        QVERIFY(!(pBetween->flags() & Qt::ItemIsSelectable));
        QVERIFY(!(pWithin->flags() & Qt::ItemIsSelectable));

        const QVector<EGraphLayoutType> vecViews = VisualMarkdownViewerModel::ListViews();
        QCOMPARE(pBetween->childCount(), kViewLabels.size());
        for (int iView = 0; iView < vecViews.size(); ++iView)
        {
            QCOMPARE(pBetween->child(iView)->text(0), kViewLabels[iView]);
            QCOMPARE(window.FindViewItem(vecViews[iView]), pBetween->child(iView));
        }

        QCOMPARE(pWithin->childCount(), kDocumentTitles.size());
        const VisualMarkdownViewerModel& model = window.GetModel();
        for (int iDocument = 0; iDocument < kDocumentTitles.size(); ++iDocument)
        {
            QTreeWidgetItem* pDocument = pWithin->child(iDocument);
            QCOMPARE(pDocument->text(0), kDocumentTitles[iDocument]);
            QCOMPARE(window.FindDocumentItem(iDocument), pDocument);
            for (int iTable = 0; iTable < kTableCounts[iDocument]; ++iTable)
            {
                QTreeWidgetItem* pTable = window.FindTableItem(iDocument, iTable);
                QVERIFY(pTable != nullptr);
                QCOMPARE(pTable->parent(), pDocument);
                const QString strText = pTable->text(0);
                QVERIFY2(strText.startsWith(model.FindTable(iDocument, iTable)->strTitle), qUtf8Printable(strText));
                QVERIFY2(strText.contains(VisualMarkdownViewerModel::GetChartTypeLabel(model.GetRecommendedChart(iDocument, iTable))), qUtf8Printable(strText));
            }
            QVERIFY(window.FindTableItem(iDocument, kTableCounts[iDocument]) == nullptr);
        }

        // 표가 없는 문서: 고를 수 없는 알림 항목 하나, 문서 쪽에도 알린다
        QTreeWidgetItem* pFocalLoss = window.FindDocumentItem(kFocalLossDocument);
        QCOMPARE(pFocalLoss->childCount(), 1);
        QVERIFY(pFocalLoss->child(0)->text(0).contains(DecodeUtf8(kNoTableText)));
        QVERIFY(!(pFocalLoss->child(0)->flags() & Qt::ItemIsSelectable));
        window.SelectDocument(kFocalLossDocument);
        QCOMPARE(window.GetStack()->currentWidget(), window.GetDocumentPage());
        QVERIFY2(window.GetDocumentPage()->text().contains(DecodeUtf8(kNoTableText)), qUtf8Printable(window.GetDocumentPage()->text()));
    }

    // 나무 항목을 고르면 그 쪽이 보인다: 그래프 보기 5개는 KnowledgeGraphView, 관계 행렬은 행렬 쪽, 표는 차트 쪽, 문서는 문서 쪽.
    // 처음에는 관계망이 보이고, 배치는 보기를 처음 열 때 계산한다. 맞춤은 그래프 보기에서만 켜진다
    void selectingItemShowsItsPage()
    {
        VisualMarkdownViewerWindow& window = *m_upWindow;
        const VisualMarkdownViewerModel& model = window.GetModel();
        QStackedWidget* pStack = window.GetStack();
        QCOMPARE(window.GetTree()->currentItem(), window.FindViewItem(EGraphLayoutType::NETWORK));
        QCOMPARE(pStack->currentWidget(), window.GetGraphView(EGraphLayoutType::NETWORK));
        for (const EGraphLayoutType eType : VisualMarkdownViewerModel::ListViews())
            QCOMPARE(model.HasLayout(eType), eType == EGraphLayoutType::NETWORK);

        for (const EGraphLayoutType eType : VisualMarkdownViewerModel::ListViews())
        {
            const QString strName = VisualMarkdownViewerModel::GetViewKey(eType);
            window.GetTree()->setCurrentItem(window.FindViewItem(eType));
            QVERIFY2(model.HasLayout(eType), qUtf8Printable(strName));
            const int iNodes = GetOpenedLayout(window, eType).vecNodes.size();
            QVERIFY(iNodes > 0);
            if (eType == EGraphLayoutType::MATRIX)
            {
                QVERIFY(window.GetGraphView(eType) == nullptr);
                QCOMPARE(pStack->currentWidget(), window.GetMatrixPage());
                QVERIFY(!window.GetMatrixPage()->GetMatrixView()->GetCellRect(iNodes - 1, iNodes - 1).isEmpty());
                QVERIFY(window.GetMatrixPage()->GetMatrixView()->GetCellRect(iNodes, iNodes).isEmpty());
                QVERIFY(!window.GetFitAction()->isEnabled());
                continue;
            }
            KnowledgeGraphView* pView = window.GetGraphView(eType);
            QVERIFY2(pView != nullptr, qUtf8Printable(strName));
            QCOMPARE(pStack->currentWidget(), pView);
            QVERIFY2(!pView->GetNodeRect(iNodes - 1).isEmpty(), qUtf8Printable(strName));   // 그 보기의 배치를 받았다
            QVERIFY2(pView->GetNodeRect(iNodes).isEmpty(), qUtf8Printable(strName));
            QVERIFY2(window.GetFitAction()->isEnabled(), qUtf8Printable(strName));
        }
        // 그래프 보기 5개는 저마다 따로 있는 위젯이다
        QSet<KnowledgeGraphView*> setViews;
        for (const EGraphLayoutType eType : VisualMarkdownViewerModel::ListViews())
            if (eType != EGraphLayoutType::MATRIX)
                setViews.insert(window.GetGraphView(eType));
        QCOMPARE(setViews.size(), VisualMarkdownViewerModel::ListViews().size() - 1);

        // 표: 차트 쪽에 그 표의 차트(고를 때 만든다)
        QCOMPARE(window.GetChartPage()->GetChartView()->GetSpec().strTitle, QString());
        window.GetTree()->setCurrentItem(window.FindTableItem(kPaymentDocument, kMonthlyTable));
        QCOMPARE(pStack->currentWidget(), window.GetChartPage());
        QCOMPARE(window.GetChartPage()->GetChartView()->GetSpec().strTitle, DecodeUtf8(kMonthlyTitle));
        QCOMPARE(window.GetChartPage()->GetChartView()->GetSpec().eType, EChartType::LINE);
        QVERIFY(!window.GetFitAction()->isEnabled());

        // 문서: 문서 쪽(제목과 표 수)
        window.GetTree()->setCurrentItem(window.FindDocumentItem(kYolo9000Document));
        QCOMPARE(pStack->currentWidget(), window.GetDocumentPage());
        QVERIFY2(window.GetDocumentPage()->text().contains(kDocumentTitles[kYolo9000Document]), qUtf8Printable(window.GetDocumentPage()->text()));

        // 고르는 함수도 같은 쪽을 보인다
        window.SelectView(EGraphLayoutType::LINEAGE);
        QCOMPARE(window.GetTree()->currentItem(), window.FindViewItem(EGraphLayoutType::LINEAGE));
        QCOMPARE(pStack->currentWidget(), window.GetGraphView(EGraphLayoutType::LINEAGE));
        window.SelectTable(kPaymentDocument, kPriorityTable);
        QCOMPARE(window.GetTree()->currentItem(), window.FindTableItem(kPaymentDocument, kPriorityTable));
        QCOMPARE(pStack->currentWidget(), window.GetChartPage());
    }

    // 문서 ↔ 개념 보기에서 문서 점을 누르면(DocumentActivated) 나무에서 그 문서를 고른다
    void documentActivatedSelectsDocument()
    {
        VisualMarkdownViewerWindow& window = *m_upWindow;
        window.SelectView(EGraphLayoutType::BIPARTITE);
        KnowledgeGraphView* pView = window.GetGraphView(EGraphLayoutType::BIPARTITE);
        QVERIFY(pView != nullptr);
        QTRY_VERIFY(pView->isVisible() && pView->width() > ViewTestSupport::kSnapshotWidth / 2);
        const int iNode = FindLayoutNode(GetOpenedLayout(window, EGraphLayoutType::BIPARTITE), DecodeUtf8(kYolo9000Id), true);
        QVERIFY(iNode >= 0);
        const QPoint ptNode = pView->GetNodeRect(iNode).center().toPoint();
        QCOMPARE(pView->FindDocumentAt(ptNode), DecodeUtf8(kYolo9000Id));

        QSignalSpy spyDocument(pView, &KnowledgeGraphView::DocumentActivated);
        QTest::mouseClick(pView, Qt::LeftButton, Qt::NoModifier, ptNode);
        QCOMPARE(spyDocument.count(), 1);
        QCOMPARE(window.GetTree()->currentItem(), window.FindDocumentItem(kYolo9000Document));
        QCOMPARE(window.GetStack()->currentWidget(), window.GetDocumentPage());
    }

    // 개념 점을 누르면(ConceptActivated) 상태 줄에 그 개념의 이름 · 종류 · 연도를 보인다
    void conceptActivatedShowsConceptInStatusBar()
    {
        VisualMarkdownViewerWindow& window = *m_upWindow;
        KnowledgeGraphView* pView = window.GetGraphView(EGraphLayoutType::NETWORK);
        QVERIFY(pView != nullptr);
        QTRY_VERIFY(pView->isVisible() && pView->width() > ViewTestSupport::kSnapshotWidth / 2);
        QCOMPARE(window.GetConceptLabel()->text(), QString());
        const int iNode = FindLayoutNode(GetOpenedLayout(window, EGraphLayoutType::NETWORK), QStringLiteral("yolo"), false);
        QVERIFY(iNode >= 0);
        QTest::mouseClick(pView, Qt::LeftButton, Qt::NoModifier, pView->GetNodeRect(iNode).center().toPoint());
        QCOMPARE(window.GetConceptLabel()->text(), QStringLiteral("YOLO · 방법 · 2016"));
        QCOMPARE(window.GetTree()->currentItem(), window.FindViewItem(EGraphLayoutType::NETWORK));   // 보기는 그대로
    }

    // 밝음/어둠: 모든 보기(그래프 5 · 행렬 · 차트)의 테마와 창 팔레트(바탕 · 글자 · 표)가 바뀐다. 아직 열지 않은 보기도, 이미 연 보기도
    void themeAppliesToEveryView()
    {
        VisualMarkdownViewerWindow& window = *m_upWindow;
        QVERIFY(!window.IsDarkTheme());
        QVERIFY(!window.GetDarkAction()->isChecked());
        window.GetDarkAction()->trigger();   // 도구 막대의 어둠
        QVERIFY(window.IsDarkTheme());
        for (const bool bDark : { true, false, true })
        {
            window.SetDarkTheme(bDark);
            const VisualMarkdownTheme theme = bDark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
            const QString strTheme = bDark ? QStringLiteral("dark") : QStringLiteral("light");
            QCOMPARE(window.IsDarkTheme(), bDark);
            QCOMPARE(window.GetDarkAction()->isChecked(), bDark);
            QCOMPARE(window.GetTheme().background, theme.background);
            QCOMPARE(window.palette().color(QPalette::Window), theme.background);
            QCOMPARE(window.palette().color(QPalette::WindowText), theme.text);
            QCOMPARE(window.palette().color(QPalette::Base), theme.background);
            QCOMPARE(window.palette().color(QPalette::Text), theme.text);
            QCOMPARE(window.GetDocumentPage()->palette().color(QPalette::WindowText), theme.text);
            QCOMPARE(window.GetChartPage()->GetTableWidget()->palette().color(QPalette::Base), theme.background);
            QCOMPARE(window.GetChartPage()->GetTableWidget()->palette().color(QPalette::Text), theme.text);
            for (const EGraphLayoutType eType : VisualMarkdownViewerModel::ListViews())
            {
                window.SelectView(eType);
                QCoreApplication::processEvents();
                QWidget* pView = eType == EGraphLayoutType::MATRIX ? static_cast<QWidget*>(window.GetMatrixPage()->GetMatrixView())
                                                                   : static_cast<QWidget*>(window.GetGraphView(eType));
                QVERIFY(pView != nullptr);
                QVERIFY2(GetCornerColor(*pView) == theme.background,
                         qUtf8Printable(strTheme + QLatin1Char(' ') + VisualMarkdownViewerModel::GetViewKey(eType) + QLatin1Char(' ') + GetCornerColor(*pView).name()));
            }
            window.SelectTable(kPaymentDocument, kMonthlyTable);
            QCoreApplication::processEvents();
            QCOMPARE(GetCornerColor(*window.GetChartPage()->GetChartView()), theme.background);
        }
    }

    // 차트 종류 상자: 추천(그 종류 이름) · 막대 · 단계 막대 · 선 · 산점도 · 히트맵. 고르면 그 종류로, 추천이면 추천 종류로 다시 만든다.
    // 나무의 표 글도 따라가고, 고른 종류는 표마다 기억한다
    void chartTypeBoxRebuildsChart()
    {
        VisualMarkdownViewerWindow& window = *m_upWindow;
        const VisualMarkdownViewerModel& model = window.GetModel();
        window.SelectTable(kPaymentDocument, kMonthlyTable);
        VisualMarkdownViewerChartPage* pPage = window.GetChartPage();
        QComboBox* pBox = pPage->GetChartTypeBox();
        const QStringList vecTexts { QStringLiteral("추천(선)"), QStringLiteral("막대"), QStringLiteral("단계 막대"), QStringLiteral("선"), QStringLiteral("산점도"),
                                     QStringLiteral("히트맵") };
        const QVector<int> vecData { VisualMarkdownViewerChartPage::kRecommendedChoice, static_cast<int>(EChartType::BAR), static_cast<int>(EChartType::STEP_BAR),
                                     static_cast<int>(EChartType::LINE), static_cast<int>(EChartType::SCATTER), static_cast<int>(EChartType::HEATMAP) };
        QCOMPARE(pBox->count(), vecTexts.size());
        for (int iItem = 0; iItem < vecTexts.size(); ++iItem)
        {
            QCOMPARE(pBox->itemText(iItem), vecTexts[iItem]);
            QCOMPARE(pBox->itemData(iItem).toInt(), vecData[iItem]);
        }
        QCOMPARE(pBox->currentIndex(), 0);
        QVERIFY(pBox->isEnabled());

        const QVector<QPair<int, EChartType>> vecChoices { { 1, EChartType::BAR }, { 2, EChartType::STEP_BAR }, { 4, EChartType::SCATTER }, { 5, EChartType::HEATMAP },
                                                           { 3, EChartType::LINE } };
        for (const QPair<int, EChartType>& pairChoice : vecChoices)
        {
            pBox->setCurrentIndex(pairChoice.first);
            QCOMPARE(pPage->GetChartView()->GetSpec().eType, pairChoice.second);
            QCOMPARE(model.GetChosenChart(kPaymentDocument, kMonthlyTable), std::optional<EChartType>(pairChoice.second));
            const QString strItem = window.FindTableItem(kPaymentDocument, kMonthlyTable)->text(0);
            QVERIFY2(strItem.endsWith(QLatin1Char('(') + VisualMarkdownViewerModel::GetChartTypeLabel(pairChoice.second) + QLatin1Char(')')), qUtf8Printable(strItem));
        }
        pBox->setCurrentIndex(0);
        QCOMPARE(pPage->GetChartView()->GetSpec().eType, EChartType::LINE);
        QVERIFY(!model.GetChosenChart(kPaymentDocument, kMonthlyTable).has_value());

        // 다른 표에 갔다 돌아오면 고른 종류 그대로
        pBox->setCurrentIndex(1);
        window.SelectTable(kPaymentDocument, kLoadTable);
        QCOMPARE(pBox->currentIndex(), 0);
        QCOMPARE(pBox->itemText(0), QStringLiteral("추천(산점도)"));
        QCOMPARE(pPage->GetChartView()->GetSpec().eType, EChartType::SCATTER);
        window.SelectTable(kPaymentDocument, kMonthlyTable);
        QCOMPARE(pBox->currentIndex(), 1);
        QCOMPARE(pPage->GetChartView()->GetSpec().eType, EChartType::BAR);
    }

    // 차트로 그리지 않는 표(NONE): 까닭을 한 줄로 보이고 차트는 숨기며 표를 크게 보인다(상자는 쓸 수 없다).
    // 표는 읽기 전용이고 머리 줄은 열 이름과 단위다. 차트를 그리는 표로 가면 차트가 다시 보이고 표는 내용 높이만큼이다
    void noneTableShowsReasonAndTable()
    {
        VisualMarkdownViewerWindow& window = *m_upWindow;
        VisualMarkdownViewerChartPage* pPage = window.GetChartPage();
        window.SelectTable(kPaymentDocument, kPriorityTable);
        QCoreApplication::processEvents();
        QVERIFY(!pPage->GetChartView()->isVisible());
        QVERIFY(pPage->GetTitleLabel()->isVisible());   // 차트가 없으니 표 제목을 따로 보인다
        QCOMPARE(pPage->GetTitleLabel()->text(), QStringLiteral("기능 우선순위"));
        QLabel* pMessage = pPage->GetMessageLabel();
        QVERIFY(pMessage->isVisible());
        QVERIFY2(pMessage->text().contains(QStringLiteral("우선순위")) && !pMessage->text().contains(QLatin1Char('\n')), qUtf8Printable(pMessage->text()));
        QCOMPARE(pPage->GetChartTypeBox()->itemText(0), QStringLiteral("추천(그리지 않음)"));
        QVERIFY(!pPage->GetChartTypeBox()->isEnabled());

        QTableWidget* pTable = pPage->GetTableWidget();
        QVERIFY(pTable->isVisible());
        QCOMPARE(pTable->rowCount(), 4);
        QCOMPARE(pTable->columnCount(), 3);
        QCOMPARE(pTable->horizontalHeaderItem(0)->text(), QStringLiteral("기능"));
        QCOMPARE(pTable->horizontalHeaderItem(1)->text(), QStringLiteral("우선순위"));
        QCOMPARE(pTable->horizontalHeaderItem(2)->text(), QStringLiteral("담당"));
        QCOMPARE(pTable->item(0, 0)->text(), QStringLiteral("비동기 결제"));
        QCOMPARE(pTable->item(3, 2)->text(), QStringLiteral("화면팀"));
        QCOMPARE(pTable->editTriggers(), QAbstractItemView::NoEditTriggers);
        QVERIFY(!(pTable->item(0, 0)->flags() & Qt::ItemIsEditable));
        QVERIFY2(pTable->height() > pPage->height() / 2, qPrintable(QStringLiteral("%1 / %2").arg(pTable->height()).arg(pPage->height())));
        QVERIFY2(pTable->width() > pPage->width() / 2, qPrintable(QStringLiteral("%1 / %2").arg(pTable->width()).arg(pPage->width())));

        window.SelectTable(kPaymentDocument, kMonthlyTable);
        QCoreApplication::processEvents();
        QVERIFY(pPage->GetChartView()->isVisible());
        QVERIFY(!pPage->GetTitleLabel()->isVisible());   // 제목은 차트가 쓴다
        QVERIFY(!pMessage->isVisible());   // 이 표는 경고가 없다
        // 표 틀은 열 너비만큼(쪽 너비까지 늘이지 않는다), 열은 다 보인다
        QVERIFY2(pTable->width() < pPage->width() / 2, qPrintable(QStringLiteral("%1 / %2").arg(pTable->width()).arg(pPage->width())));
        QCOMPARE(pTable->horizontalScrollBar()->maximum(), 0);
        QVERIFY(pPage->GetChartTypeBox()->isEnabled());
        QCOMPARE(pTable->rowCount(), 5);
        QCOMPARE(pTable->horizontalHeaderItem(0)->text(), QStringLiteral("월"));
        QCOMPARE(pTable->horizontalHeaderItem(1)->text(), QStringLiteral("p50 (ms)"));
        QCOMPARE(pTable->horizontalHeaderItem(2)->text(), QStringLiteral("p95 (ms)"));
        QCOMPARE(pTable->item(2, 2)->text(), QString());   // 7월 p95 는 빈 칸
        QVERIFY2(pPage->GetChartView()->height() > pTable->height(),
                 qPrintable(QStringLiteral("%1 / %2").arg(pPage->GetChartView()->height()).arg(pTable->height())));
        QVERIFY(pTable->verticalScrollBar()->maximum() == 0);   // 다섯 행이 다 보인다
    }

    // 상태 줄: 점 · 선 · 문서 · 빠진 관계 · 경고 수. 빠진 관계 · 경고는 마우스 올림으로 목록을 본다
    void statusBarShowsCounts()
    {
        VisualMarkdownViewerWindow& window = *m_upWindow;
        QCOMPARE(window.GetCountsLabel()->text(), kCountsText);
        QCOMPARE(window.GetWarningsLabel()->text(), kNoWarningsText);
        QVERIFY2(window.GetCountsLabel()->toolTip().contains(QStringLiteral("darknet-53")), qUtf8Printable(window.GetCountsLabel()->toolTip()));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString strCopy = dir.filePath(QStringLiteral("wiki"));
        QVERIFY(ViewerTestSupport::CopyWiki(ViewerTestSupport::GetShowcaseDir(), strCopy));
        QVERIFY(ViewerTestSupport::WriteText(strCopy + QStringLiteral("/concepts/broken.md"), QStringLiteral("# 머리글 없음\n")));
        QVERIFY(window.OpenWiki(strCopy));
        QCOMPARE(window.GetCountsLabel()->text(), kCountsText);
        QCOMPARE(window.GetWarningsLabel()->text(), QStringLiteral("경고 1"));
        QVERIFY2(window.GetWarningsLabel()->toolTip().contains(QStringLiteral("broken.md")), qUtf8Printable(window.GetWarningsLabel()->toolTip()));
    }

    // 폴더를 읽지 못하면 알림(그 경로)을 띄우고 앞의 wiki(나무 · 상태 줄 · 고른 쪽)를 그대로 둔다
    void openFailureKeepsWiki()
    {
        VisualMarkdownViewerWindow& window = *m_upWindow;
        window.SelectTable(kPaymentDocument, kMonthlyTable);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString strMissing = dir.filePath(QStringLiteral("no-such-wiki"));
        QVERIFY(window.findChild<QDialog*>() == nullptr);

        QVERIFY(!window.OpenWiki(strMissing));
        QDialog* pNotice = window.findChild<QDialog*>();
        QVERIFY(pNotice != nullptr);
        QVERIFY(pNotice->isVisible());
        QStringList vecTexts;
        for (const QLabel* pLabel : pNotice->findChildren<QLabel*>())
            vecTexts << pLabel->text();
        QVERIFY2(vecTexts.join(QLatin1Char('\n')).contains(strMissing), qUtf8Printable(vecTexts.join(QLatin1Char('\n'))));
        pNotice->close();

        QCOMPARE(window.GetModel().GetWikiRoot(), ViewerTestSupport::GetShowcaseDir());
        QCOMPARE(window.GetCountsLabel()->text(), kCountsText);
        QCOMPARE(window.GetTree()->topLevelItem(1)->childCount(), kDocumentTitles.size());
        QCOMPARE(window.GetTree()->currentItem(), window.FindTableItem(kPaymentDocument, kMonthlyTable));
        QCOMPARE(window.GetStack()->currentWidget(), window.GetChartPage());
        QCOMPARE(window.GetChartPage()->GetChartView()->GetSpec().strTitle, DecodeUtf8(kMonthlyTitle));
    }

    // 다른 wiki 를 열면 나무 · 상태 줄 · 보기가 새 wiki 를 따른다. 보던 보기는 그대로 고르고 새 배치로 다시 계산한다
    void reopenFollowsNewWiki()
    {
        VisualMarkdownViewerWindow& window = *m_upWindow;
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString strCopy = dir.filePath(QStringLiteral("wiki"));
        QVERIFY(ViewerTestSupport::CopyWiki(ViewerTestSupport::GetShowcaseDir(), strCopy));
        QVERIFY(QFile::remove(strCopy + QLatin1Char('/') + DecodeUtf8(kYoloFile)));
        window.SelectView(EGraphLayoutType::BIPARTITE);
        window.SelectView(EGraphLayoutType::TREE);

        QVERIFY(window.OpenWiki(strCopy));
        const VisualMarkdownViewerModel& model = window.GetModel();
        QCOMPARE(model.GetWikiRoot(), strCopy);
        QCOMPARE(window.GetTree()->topLevelItem(1)->childCount(), kDocumentTitles.size() - 1);
        QVERIFY2(window.GetCountsLabel()->text().contains(QStringLiteral("문서 5")), qUtf8Printable(window.GetCountsLabel()->text()));
        QCOMPARE(window.GetTree()->currentItem(), window.FindViewItem(EGraphLayoutType::TREE));
        QVERIFY(model.HasLayout(EGraphLayoutType::TREE));
        QVERIFY(!model.HasLayout(EGraphLayoutType::BIPARTITE));
        window.SelectView(EGraphLayoutType::BIPARTITE);
        const GraphLayoutOutput& layout = GetOpenedLayout(window, EGraphLayoutType::BIPARTITE);
        KnowledgeGraphView* pView = window.GetGraphView(EGraphLayoutType::BIPARTITE);
        QVERIFY(!pView->GetNodeRect(layout.vecNodes.size() - 1).isEmpty());
        QVERIFY(pView->GetNodeRect(layout.vecNodes.size()).isEmpty());   // 새 배치(문서 5)를 받았다
        int iDocuments = 0;
        for (const LayoutNode& node : layout.vecNodes)
            iDocuments += node.bDocument ? 1 : 0;
        QCOMPARE(iDocuments, kDocumentTitles.size() - 1);
    }

    // 맞춤: 지금 그래프 보기의 FitToView(휠로 키운 뒤 맞춤을 누르면 맞춘 확대로 돌아간다)
    void fitActionFitsCurrentGraph()
    {
        VisualMarkdownViewerWindow& window = *m_upWindow;
        window.SelectView(EGraphLayoutType::LINEAGE);
        KnowledgeGraphView* pView = window.GetGraphView(EGraphLayoutType::LINEAGE);
        QTRY_VERIFY(pView->isVisible() && pView->width() > ViewTestSupport::kSnapshotWidth / 2);
        const double dFitted = pView->GetZoom();
        ViewTestSupport::SendWheel(*pView, pView->rect().center(), -3);
        QVERIFY(pView->GetZoom() < dFitted);
        window.GetFitAction()->trigger();
        QCOMPARE(pView->GetZoom(), dFitted);
    }

    // 행렬이 창보다 크면(가장 작은 칸에서) 스크롤로 본다. 창이 크면 스크롤 없이 칸을 키운다
    void matrixScrollsWhenWindowIsSmall()
    {
        VisualMarkdownViewerWindow& window = *m_upWindow;
        window.SelectView(EGraphLayoutType::MATRIX);
        VisualMarkdownViewerMatrixPage* pPage = window.GetMatrixPage();
        KnowledgeMatrixView* pMatrix = pPage->GetMatrixView();
        QTRY_VERIFY(pPage->isVisible());
        QTRY_COMPARE(pPage->horizontalScrollBar()->maximum(), 0);
        QCOMPARE(pPage->verticalScrollBar()->maximum(), 0);
        QVERIFY2(pMatrix->GetCellSize() > KnowledgeMatrixView::kMinCellSize, qPrintable(QString::number(pMatrix->GetCellSize())));

        window.resize(kSmallWindowSize);
        QTRY_VERIFY(pPage->horizontalScrollBar()->maximum() > 0);
        QTRY_VERIFY(pPage->verticalScrollBar()->maximum() > 0);
        QCOMPARE(pMatrix->GetCellSize(), KnowledgeMatrixView::kMinCellSize);
        QVERIFY(pMatrix->width() >= pMatrix->minimumSizeHint().width());
        QVERIFY(pMatrix->height() >= pMatrix->minimumSizeHint().height());
    }

private:
    std::unique_ptr<VisualMarkdownViewerWindow> m_upWindow;
};

std::unique_ptr<QObject> CreateVisualMarkdownViewerWindowTest() { return std::make_unique<VisualMarkdownViewerWindowTest>(); }

#include "VisualMarkdownViewerWindowTest.moc"
