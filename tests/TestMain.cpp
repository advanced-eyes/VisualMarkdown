#include <QCoreApplication>
#include <QTest>

#include <cstdio>
#include <memory>
#include <vector>

constexpr int kClassNameArgument = 1;      // argv[1]: 돌릴 클래스 이름
constexpr int kFirstQTestArgument = 2;     // argv[2] 부터는 QTest 에 넘긴다
constexpr int kUnknownClassExitCode = 1;   // 첫 인자와 맞는 클래스가 없을 때(QTest 가 모르는 함수 이름에 내는 값과 같다)

// 테스트 클래스를 한 실행 파일에서 차례로 돌린다. 첫 인자가 클래스 이름이면 그 클래스만 돈다.
// 맞는 클래스가 없으면 아는 이름을 알리고 실패로 끝난다. 0 으로 끝나면 아무것도 안 돌았는데 통과로 보인다.
#define VISUALMARKDOWN_TEST_CLASSES(X) \
    X(KnowledgeTextTest) \
    X(KnowledgeTextCleanTest) \
    X(KnowledgeVocabularyTest) \
    X(KnowledgeMarkdownReaderTest) \
    X(KnowledgeMarkdownWriterTest) \
    X(KnowledgeIndexTest) \
    X(KnowledgeGraphLayoutTest) \
    X(ExampleWikiTest) \
    X(ShowcaseWikiTest)

// 테스트 파일마다 이 선언대로 Create<클래스>() 를 둔다. GCC 는 반환 형이 달라도 링크하므로(링크 이름에 반환 형이 없다) 꼭 맞춘다.
#define VISUALMARKDOWN_DECLARE_TEST(Name) std::unique_ptr<QObject> Create##Name();
VISUALMARKDOWN_TEST_CLASSES(VISUALMARKDOWN_DECLARE_TEST)

int main(int argc, char* argv[])
{
    // 라이브러리는 QtCore 만 쓴다. 화면이 없으니 QCoreApplication 으로 돈다.
    QCoreApplication app(argc, argv);

    const QString strOnly = argc > kClassNameArgument ? QString::fromLocal8Bit(argv[kClassNameArgument]) : QString();
    // 클래스 이름 뒤의 인자는 QTest 에 넘긴다(예: -o out.txt,txt — MinGW Qt 는 콘솔에 결과를 쓰지 않아 파일로 받는다).
    std::vector<char*> vecArgs { argv[0] };
    for (int iArg = kFirstQTestArgument; iArg < argc; ++iArg)
    {
        vecArgs.push_back(argv[iArg]);
    }
    int iFailures = 0;
    int iRunClasses = 0;
#define VISUALMARKDOWN_RUN_TEST(Name) \
    if (strOnly.isEmpty() || strOnly == QStringLiteral(#Name)) \
    { \
        const std::unique_ptr<QObject> upTest = Create##Name(); \
        iFailures += QTest::qExec(upTest.get(), static_cast<int>(vecArgs.size()), vecArgs.data()); \
        ++iRunClasses; \
    }
    VISUALMARKDOWN_TEST_CLASSES(VISUALMARKDOWN_RUN_TEST)
    if (!strOnly.isEmpty() && iRunClasses == 0)
    {
        // QTest 처럼 표준 오류에 바로 쓴다. Qt 메시지 처리기를 거치면 콘솔이 없을 때 보이지 않는다.
        std::fprintf(stderr,
                     "Unknown test class: '%s'. The first argument is a test class name; QTest options go after it.\n"
                     "Test classes:\n",
                     argv[kClassNameArgument]);
#define VISUALMARKDOWN_PRINT_TEST(Name) std::fprintf(stderr, "  %s\n", #Name);
        VISUALMARKDOWN_TEST_CLASSES(VISUALMARKDOWN_PRINT_TEST)
        return kUnknownClassExitCode;
    }
    return iFailures;
}
