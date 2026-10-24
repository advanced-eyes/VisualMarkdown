#include <QApplication>
#include <QByteArray>
#include <QDir>
#include <QFont>
#include <QTest>

#include <cstdio>
#include <memory>
#include <vector>

constexpr int kClassNameArgument = 1;      // argv[1]: 돌릴 클래스 이름
constexpr int kFirstQTestArgument = 2;     // argv[2] 부터는 QTest 에 넘긴다
constexpr int kUnknownClassExitCode = 1;   // 첫 인자와 맞는 클래스가 없을 때(QTest 가 모르는 함수 이름에 내는 값과 같다)

#ifdef Q_OS_WIN
constexpr int kWindowsUiFontPointSize = 9;   // Windows 메시지 글꼴 크기(맑은 고딕 9pt)
#endif

// 위젯 시험 클래스를 한 실행 파일에서 차례로 돌린다. 첫 인자가 클래스 이름이면 그 클래스만 돈다.
// 맞는 클래스가 없으면 아는 이름을 알리고 실패로 끝난다. 0 으로 끝나면 아무것도 안 돌았는데 통과로 보인다.
#define VISUALMARKDOWN_VIEW_TEST_CLASSES(X) \
    X(VisualMarkdownThemeTest) \
    X(KnowledgeGraphViewTest) \
    X(KnowledgeMatrixViewTest) \
    X(KnowledgeChartViewTest)

// 시험 파일마다 이 선언대로 Create<클래스>() 를 둔다. GCC 는 반환 형이 달라도 링크하므로(링크 이름에 반환 형이 없다) 꼭 맞춘다.
#define VISUALMARKDOWN_DECLARE_VIEW_TEST(Name) std::unique_ptr<QObject> Create##Name();
VISUALMARKDOWN_VIEW_TEST_CLASSES(VISUALMARKDOWN_DECLARE_VIEW_TEST)

int main(int argc, char* argv[])
{
    // 화면이 없어도 돌게 정하지 않았으면 offscreen 으로 둔다. QApplication 을 만들기 전에 정해야 한다
    if (!qEnvironmentVariableIsSet("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
#ifdef Q_OS_WIN
    // Windows 의 offscreen 은 글꼴 폴더에서 글꼴을 읽는다. 정하지 않았으면 Windows 글꼴 폴더를 알려 준다(한글이 그려지게)
    if (!qEnvironmentVariableIsSet("QT_QPA_FONTDIR") && qEnvironmentVariableIsSet("WINDIR"))
        qputenv("QT_QPA_FONTDIR", QDir::fromNativeSeparators(qEnvironmentVariable("WINDIR") + QStringLiteral("/Fonts")).toUtf8());
#endif
    QApplication app(argc, argv);
#ifdef Q_OS_WIN
    // Windows 의 offscreen 은 시스템 글꼴을 몰라 Qt 의 대체 기본값을 쓴다. 실제 windows 플랫폼처럼 맑은 고딕을 기본 글꼴로 둔다(Ocean 과 같다)
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        QApplication::setFont(QFont(QStringLiteral("Malgun Gothic"), kWindowsUiFontPointSize));
#endif

    const QString strOnly = argc > kClassNameArgument ? QString::fromLocal8Bit(argv[kClassNameArgument]) : QString();
    // 클래스 이름 뒤의 인자는 QTest 에 넘긴다(예: -o out.txt,txt — MinGW Qt 는 콘솔에 결과를 쓰지 않아 파일로 받는다).
    std::vector<char*> vecArgs { argv[0] };
    for (int iArg = kFirstQTestArgument; iArg < argc; ++iArg)
        vecArgs.push_back(argv[iArg]);
    int iFailures = 0;
    int iRunClasses = 0;
#define VISUALMARKDOWN_RUN_VIEW_TEST(Name) \
    if (strOnly.isEmpty() || strOnly == QStringLiteral(#Name)) \
    { \
        const std::unique_ptr<QObject> upTest = Create##Name(); \
        iFailures += QTest::qExec(upTest.get(), static_cast<int>(vecArgs.size()), vecArgs.data()); \
        ++iRunClasses; \
    }
    VISUALMARKDOWN_VIEW_TEST_CLASSES(VISUALMARKDOWN_RUN_VIEW_TEST)
    if (!strOnly.isEmpty() && iRunClasses == 0)
    {
        // QTest 처럼 표준 오류에 바로 쓴다. Qt 메시지 처리기를 거치면 콘솔이 없을 때 보이지 않는다.
        std::fprintf(stderr,
                     "Unknown test class: '%s'. The first argument is a test class name; QTest options go after it.\n"
                     "Test classes:\n",
                     argv[kClassNameArgument]);
#define VISUALMARKDOWN_PRINT_VIEW_TEST(Name) std::fprintf(stderr, "  %s\n", #Name);
        VISUALMARKDOWN_VIEW_TEST_CLASSES(VISUALMARKDOWN_PRINT_VIEW_TEST)
        return kUnknownClassExitCode;
    }
    return iFailures;
}
