#include "VisualMarkdownViewer/VisualMarkdownViewerSnapshot.h"

#include "VisualMarkdown/UI/KnowledgeGraphView.h"
#include "VisualMarkdown/UI/KnowledgeMatrixView.h"
#include "VisualMarkdown/UI/VisualMarkdownTheme.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerChartPage.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerMatrixPage.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerModel.h"
#include "VisualMarkdownViewer/VisualMarkdownViewerPalette.h"

#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPalette>
#include <QWidget>

#include <memory>

namespace
{
const QString kListFileName = QStringLiteral("snapshot.tsv");
const QString kGraphFileName = QStringLiteral("graph-%1.png");
const QString kChartFileName = QStringLiteral("chart-%1-%2.png");
const QString kChartView = QStringLiteral("chart");
const QStringList kListHeader { QStringLiteral("파일"), QStringLiteral("보기"), QStringLiteral("문서 id"), QStringLiteral("표 제목"), QStringLiteral("차트 종류") };
constexpr int kNumberWidth = 2;        // chart-DD-TT 의 자리 수
constexpr int kDecimalBase = 10;
constexpr const char* kImageFormat = "PNG";

// 목록 칸: 탭 · 줄 바꿈이 칸을 가르지 않게 빈칸으로 바꾼다
QString MakeField(QString text)
{
    for (const QChar cBreak : { QLatin1Char('\t'), QLatin1Char('\n'), QLatin1Char('\r') })
        text.replace(cBreak, QLatin1Char(' '));
    return text;
}

QString MakeRow(const QStringList& fields)
{
    QStringList vecFields;
    for (const QString& strField : fields)
        vecFields << MakeField(strField);
    return vecFields.join(QLatin1Char('\t'));
}

// 쪽을 size 로 화면에 띄우지 않고 보인 뒤(배치 · 크기 이벤트를 마치게) 그 크기의 그림으로 그린다. 창이 보이는 쪽과 같은 위젯이다
QImage RenderPage(QWidget& page, const QSize& size, const VisualMarkdownTheme& theme)
{
    page.setPalette(VisualMarkdownViewerPalette::Make(theme));
    page.resize(size);
    if (!page.isVisible())
    {
        page.setAttribute(Qt::WA_DontShowOnScreen, true);
        page.show();
    }
    QApplication::processEvents();
    QImage image(size, QImage::Format_ARGB32);
    if (image.isNull())
        return image;
    image.fill(theme.background);
    page.render(&image);
    return image;
}

bool SaveImage(const QImage& image, const QString& path, QStringList* errors)
{
    if (!image.isNull() && image.save(path, kImageFormat))
        return true;
    *errors << QStringLiteral("파일을 쓰지 못했다: %1").arg(path);
    return false;
}
}

int VisualMarkdownViewerSnapshot::Run(const VisualMarkdownViewerOptions& options, QStringList* errors)
{
    VisualMarkdownViewerModel model;
    if (!model.Load(options.strWikiRoot))
    {
        *errors << model.GetLastError();
        return VisualMarkdownViewerExit::kWikiNotRead;
    }
    if (!QDir().mkpath(options.strSnapshotDir) || !QFileInfo(options.strSnapshotDir).isDir())
    {
        *errors << QStringLiteral("스냅샷 폴더를 만들지 못했다: %1").arg(options.strSnapshotDir);
        return VisualMarkdownViewerExit::kWriteFailed;
    }
    const QDir dirOutput(options.strSnapshotDir);
    const VisualMarkdownTheme theme = options.bDark ? VisualMarkdownTheme::MakeDark() : VisualMarkdownTheme::MakeLight();
    QStringList vecRows { MakeRow(kListHeader) };
    bool bWritten = true;

    // 지식 사이 보기 6개: 그래프 보기 5개는 KnowledgeGraphView, 관계 행렬은 행렬 쪽(스크롤 영역)
    for (const EGraphLayoutType eType : VisualMarkdownViewerModel::ListViews())
    {
        const GraphLayoutOutput& layout = model.GetLayout(eType);
        std::unique_ptr<QWidget> upPage;
        if (eType == EGraphLayoutType::MATRIX)
        {
            auto upMatrix = std::make_unique<VisualMarkdownViewerMatrixPage>();
            upMatrix->GetMatrixView()->SetTheme(theme);
            upMatrix->GetMatrixView()->SetGraph(model.GetGraph(), layout);
            upPage = std::move(upMatrix);
        }
        else
        {
            auto upGraph = std::make_unique<KnowledgeGraphView>();
            upGraph->SetTheme(theme);
            upGraph->SetGraph(model.GetGraph(), layout, eType);
            upPage = std::move(upGraph);
        }
        const QString strFile = MakeGraphFileName(eType);
        bWritten = SaveImage(RenderPage(*upPage, options.size, theme), dirOutput.filePath(strFile), errors) && bWritten;
        vecRows << MakeRow({ strFile, VisualMarkdownViewerModel::GetViewKey(eType), QString(), QString(), QString() });
    }

    // 지식 안: 표마다 차트 쪽(차트와 아래 표). 창처럼 쪽 하나에 표를 차례로 넣는다
    VisualMarkdownViewerChartPage chartPage;
    chartPage.SetTheme(theme);
    for (int iDocument = 0; iDocument < model.GetDocumentCount(); ++iDocument)
    {
        for (int iTable = 0; iTable < model.GetTableCount(iDocument); ++iTable)
        {
            const ChartSpec spec = model.BuildChart(iDocument, iTable);
            chartPage.SetTable(*model.FindTable(iDocument, iTable), model.GetRecommendedChart(iDocument, iTable), model.GetChosenChart(iDocument, iTable), spec);
            const QString strFile = MakeChartFileName(iDocument, iTable);
            bWritten = SaveImage(RenderPage(chartPage, options.size, theme), dirOutput.filePath(strFile), errors) && bWritten;
            vecRows << MakeRow({ strFile, kChartView, model.FindDocument(iDocument)->strId, model.FindTable(iDocument, iTable)->strTitle,
                                 VisualMarkdownViewerModel::GetChartTypeKey(spec.eType) });
        }
    }

    // 목록: UTF-8, LF(Text 모드로 열지 않는다), 머리 줄 있음, 탭 구분
    const QString strListPath = dirOutput.filePath(GetListFileName());
    const QByteArray baList = (vecRows.join(QLatin1Char('\n')) + QLatin1Char('\n')).toUtf8();
    QFile fileList(strListPath);
    const bool bListWritten = fileList.open(QIODevice::WriteOnly | QIODevice::Truncate) && fileList.write(baList) == baList.size() && fileList.flush();
    fileList.close();
    if (!bListWritten)
    {
        *errors << QStringLiteral("파일을 쓰지 못했다: %1").arg(strListPath);
        bWritten = false;
    }
    return bWritten ? VisualMarkdownViewerExit::kOk : VisualMarkdownViewerExit::kWriteFailed;
}

QString VisualMarkdownViewerSnapshot::MakeGraphFileName(EGraphLayoutType type)
{
    return kGraphFileName.arg(VisualMarkdownViewerModel::GetViewKey(type));
}

QString VisualMarkdownViewerSnapshot::MakeChartFileName(int document, int table)
{
    return kChartFileName.arg(document + 1, kNumberWidth, kDecimalBase, QLatin1Char('0')).arg(table + 1, kNumberWidth, kDecimalBase, QLatin1Char('0'));
}

QString VisualMarkdownViewerSnapshot::GetListFileName()
{
    return kListFileName;
}
