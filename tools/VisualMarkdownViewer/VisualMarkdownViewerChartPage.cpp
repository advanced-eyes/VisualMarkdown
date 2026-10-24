#include "VisualMarkdownViewer/VisualMarkdownViewerChartPage.h"

#include "VisualMarkdown/KnowledgeText.h"
#include "VisualMarkdown/UI/KnowledgeChartView.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerModel.h"

#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr int kPageMargin = 12;
constexpr int kPageSpacing = 8;
constexpr int kChartStretch = 3;        // 차트 : 표 = 3 : 2. 표는 내용 높이를 넘지 않고 남는 자리는 차트가 쓴다
constexpr int kTableStretch = 2;
constexpr int kMinColumnWidth = 80;
constexpr double kTitleFontScale = 1.4;  // NONE 의 표 제목 글자: 쪽 글자의 이만큼

const QString kChartTypeTitle = QStringLiteral("차트 종류");
const QString kRecommendedText = QStringLiteral("추천(%1)");
const QString kNoneTitle = QStringLiteral("차트로 그리지 않는 표");
const QString kWarningTitle = QStringLiteral("경고");
const QString kJoin = QStringLiteral(" · ");
constexpr EChartType kChoosableTypes[] = { EChartType::BAR, EChartType::STEP_BAR, EChartType::LINE, EChartType::SCATTER, EChartType::HEATMAP, EChartType::DONUT };
constexpr int kRecommendedItem = 0;     // 상자 첫 항목: 추천(그 종류 이름)

// 머리 칸: 열 이름과 단위
QString MakeHeaderText(const KnowledgeColumn& column)
{
    return column.strUnit.isEmpty() ? column.strName : QStringLiteral("%1 (%2)").arg(column.strName, column.strUnit);
}
}

VisualMarkdownViewerChartPage::VisualMarkdownViewerChartPage(QWidget* parent)
    : QWidget(parent)
    , m_pChartTypeBox(new QComboBox(this))
    , m_pTitleLabel(new QLabel(this))
    , m_pMessageLabel(new QLabel(this))
    , m_pChartView(new KnowledgeChartView(this))
    , m_pTableWidget(new QTableWidget(this))
{
    // 차트 종류 상자: 추천(그 종류 이름) · 막대 · 단계 막대 · 선 · 산점도 · 히트맵 · 도넛
    m_pChartTypeBox->addItem(kRecommendedText.arg(VisualMarkdownViewerModel::GetChartTypeLabel(EChartType::NONE)), kRecommendedChoice);
    for (const EChartType eType : kChoosableTypes)
        m_pChartTypeBox->addItem(VisualMarkdownViewerModel::GetChartTypeLabel(eType), static_cast<int>(eType));
    m_pChartTypeBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(m_pChartTypeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &VisualMarkdownViewerChartPage::HandleCurrentIndexChanged);

    // NONE 의 표 제목(차트를 그리면 차트가 제목을 쓴다)
    m_pTitleLabel->setTextFormat(Qt::PlainText);
    QFont fontTitle = m_pTitleLabel->font();
    fontTitle.setBold(true);
    if (fontTitle.pointSizeF() > 0)
        fontTitle.setPointSizeF(fontTitle.pointSizeF() * kTitleFontScale);
    m_pTitleLabel->setFont(fontTitle);
    m_pTitleLabel->hide();

    // 까닭 · 경고는 한 줄로. 긴 글이 쪽을 넓히지 않게 하고 전부는 마우스 올림으로 본다
    m_pMessageLabel->setTextFormat(Qt::PlainText);
    m_pMessageLabel->setWordWrap(false);
    m_pMessageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_pMessageLabel->hide();

    // 원래 표: 읽기 전용, 행 번호 없음
    m_pTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_pTableWidget->verticalHeader()->hide();
    m_pTableWidget->setWordWrap(false);

    QHBoxLayout* pTopLayout = new QHBoxLayout;
    pTopLayout->addWidget(new QLabel(kChartTypeTitle, this));
    pTopLayout->addWidget(m_pChartTypeBox);
    pTopLayout->addStretch(1);
    QVBoxLayout* pLayout = new QVBoxLayout(this);
    pLayout->setContentsMargins(kPageMargin, kPageMargin, kPageMargin, kPageMargin);
    pLayout->setSpacing(kPageSpacing);
    pLayout->addLayout(pTopLayout);
    pLayout->addWidget(m_pTitleLabel);
    pLayout->addWidget(m_pMessageLabel);
    pLayout->addWidget(m_pChartView, kChartStretch);
    pLayout->addWidget(m_pTableWidget, kTableStretch);
}

VisualMarkdownViewerChartPage::~VisualMarkdownViewerChartPage() = default;

void VisualMarkdownViewerChartPage::SetTable(const KnowledgeTable& table, EChartType recommended, std::optional<EChartType> chosen, const ChartSpec& spec)
{
    {
        // 상자를 이 표에 맞춘다(ChartTypeChosen 을 내지 않는다). 그리지 못하는 표(추천이 NONE)는 어떤 종류도 그리지 못해 상자를 끈다
        const QSignalBlocker blocker(m_pChartTypeBox);
        m_pChartTypeBox->setItemText(kRecommendedItem, kRecommendedText.arg(VisualMarkdownViewerModel::GetChartTypeLabel(recommended)));
        const int iChosen = chosen ? m_pChartTypeBox->findData(static_cast<int>(*chosen)) : -1;
        m_pChartTypeBox->setCurrentIndex(std::max(kRecommendedItem, iChosen));
        m_pChartTypeBox->setEnabled(recommended != EChartType::NONE);
    }

    // NONE: 차트 대신 까닭 한 줄, 표를 크게. 그리는 차트도 경고(뺀 열 · 행)가 있으면 한 줄로 보인다
    const bool bChart = spec.eType != EChartType::NONE;
    QString strMessage;
    if (!bChart)
        strMessage = spec.vecWarnings.isEmpty() ? kNoneTitle : kNoneTitle + QStringLiteral(": ") + spec.vecWarnings.join(kJoin);
    else if (!spec.vecWarnings.isEmpty())
        strMessage = kWarningTitle + QStringLiteral(": ") + spec.vecWarnings.join(kJoin);
    m_pMessageLabel->setText(strMessage);
    m_pMessageLabel->setToolTip(strMessage);
    m_pMessageLabel->setVisible(!strMessage.isEmpty());
    m_pTitleLabel->setText(bChart ? QString() : table.strTitle);
    m_pTitleLabel->setVisible(!bChart);

    // 차트를 그리면 표는 내용 크기만큼(틀이 열을 감싼다), 그리지 않으면 표가 쪽을 채운다(열을 늘인다)
    m_pChartView->SetSpec(spec);
    m_pChartView->setVisible(bChart);
    FillTable(table, !bChart);
    m_pTableWidget->setMaximumWidth(bChart ? ComputeTableWidth() : QWIDGETSIZE_MAX);
    m_pTableWidget->setMaximumHeight(bChart ? ComputeTableHeight() : QWIDGETSIZE_MAX);
}

void VisualMarkdownViewerChartPage::SetTheme(const VisualMarkdownTheme& theme)
{
    m_pChartView->SetTheme(theme);
}

KnowledgeChartView* VisualMarkdownViewerChartPage::GetChartView() const
{
    return m_pChartView;
}

QTableWidget* VisualMarkdownViewerChartPage::GetTableWidget() const
{
    return m_pTableWidget;
}

QComboBox* VisualMarkdownViewerChartPage::GetChartTypeBox() const
{
    return m_pChartTypeBox;
}

QLabel* VisualMarkdownViewerChartPage::GetTitleLabel() const
{
    return m_pTitleLabel;
}

QLabel* VisualMarkdownViewerChartPage::GetMessageLabel() const
{
    return m_pMessageLabel;
}

void VisualMarkdownViewerChartPage::FillTable(const KnowledgeTable& table, bool fillPage)
{
    m_pTableWidget->clear();
    m_pTableWidget->setColumnCount(table.vecColumns.size());
    m_pTableWidget->setRowCount(table.vecRows.size());
    QStringList vecHeaders;
    for (const KnowledgeColumn& column : table.vecColumns)
        vecHeaders << MakeHeaderText(column);
    m_pTableWidget->setHorizontalHeaderLabels(vecHeaders);
    for (int iColumn = 0; iColumn < table.vecColumns.size(); ++iColumn)
    {
        // 수치 열은 오른쪽, 그 밖은 왼쪽에 맞춘다
        const bool bQuantity = KnowledgeText::ClassifyColumn(table, iColumn) == EColumnKind::QUANTITY;
        const Qt::Alignment alignment = (bQuantity ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter;
        for (int iRow = 0; iRow < table.vecRows.size(); ++iRow)
        {
            const QStringList& vecCells = table.vecRows[iRow];
            QTableWidgetItem* pItem = new QTableWidgetItem(iColumn < vecCells.size() ? vecCells[iColumn] : QString());
            pItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            pItem->setTextAlignment(static_cast<int>(alignment));
            m_pTableWidget->setItem(iRow, iColumn, pItem);
        }
    }
    // 쪽을 채우면 열이 쪽 너비를 나눠 갖고, 아니면 열은 내용 너비(정한 너비 이상)다
    QHeaderView* pHeader = m_pTableWidget->horizontalHeader();
    pHeader->setSectionResizeMode(fillPage ? QHeaderView::Stretch : QHeaderView::Interactive);
    if (fillPage)
        return;
    m_pTableWidget->resizeColumnsToContents();
    for (int iColumn = 0; iColumn < m_pTableWidget->columnCount(); ++iColumn)
        m_pTableWidget->setColumnWidth(iColumn, std::max(kMinColumnWidth, m_pTableWidget->columnWidth(iColumn)));
}

int VisualMarkdownViewerChartPage::ComputeTableWidth() const
{
    // 테두리 + 모든 열. 행이 넘쳐 세로 막대가 나와도 마지막 열이 가리지 않게 그 너비를 더한다
    int iWidth = 2 * m_pTableWidget->frameWidth();
    for (int iColumn = 0; iColumn < m_pTableWidget->columnCount(); ++iColumn)
        iWidth += m_pTableWidget->columnWidth(iColumn);
    return iWidth + m_pTableWidget->verticalScrollBar()->sizeHint().width();
}

int VisualMarkdownViewerChartPage::ComputeTableHeight() const
{
    // 머리 줄 + 모든 행 + 테두리. 열이 넓어 가로 막대가 나와도 마지막 행이 가리지 않게 그 높이를 더한다
    int iHeight = 2 * m_pTableWidget->frameWidth() + m_pTableWidget->horizontalHeader()->sizeHint().height();
    for (int iRow = 0; iRow < m_pTableWidget->rowCount(); ++iRow)
        iHeight += m_pTableWidget->rowHeight(iRow);
    return iHeight + m_pTableWidget->horizontalScrollBar()->sizeHint().height();
}

void VisualMarkdownViewerChartPage::HandleCurrentIndexChanged(int index)
{
    if (index >= 0)
        emit ChartTypeChosen(m_pChartTypeBox->itemData(index).toInt());
}
