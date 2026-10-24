cmake_minimum_required(VERSION 3.24)

project(VisualMarkdown VERSION 0.1.0 LANGUAGES CXX)

# 테스트는 이 저장소를 따로 빌드할 때만 만든다. Ocean 이 서브모듈로 붙이면 OFF 다(README §6).
option(VISUALMARKDOWN_BUILD_TESTS "Build unit tests (Qt Test)" ${PROJECT_IS_TOP_LEVEL})

################################################################################
# Dependencies (상용 배포 라이선스 정책은 README §8 참고)
#   Qt5 : LGPLv3 모듈 Core 만 쓰고 반드시 동적 링크한다.
################################################################################
find_package(Qt5 5.15 REQUIRED COMPONENTS Core)

get_target_property(VISUALMARKDOWN_QT_LIBRARY_TYPE Qt5::Core TYPE)
if(NOT VISUALMARKDOWN_QT_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
    message(FATAL_ERROR
        "Qt5::Core must be a shared library to keep LGPLv3 compliance "
        "(found: ${VISUALMARKDOWN_QT_LIBRARY_TYPE}).")
endif()

# 라이브러리와 테스트가 함께 쓰는 컴파일 옵션
if(MSVC)
    # 소스에 한국어 글자가 있어 /utf-8 이 꼭 있어야 한다. /Zc:__cplusplus: __cplusplus 를 실제 표준 값으로 둔다(기본은 199711L).
    set(VISUALMARKDOWN_COMPILE_OPTIONS /utf-8 /permissive- /W4 /Zc:__cplusplus)
else()
    set(VISUALMARKDOWN_COMPILE_OPTIONS -Wall -Wextra)
endif()

################################################################################
# VisualMarkdown : format/ 의 파일 규칙대로 지식 파일을 읽고 쓰는 정적 라이브러리
################################################################################
# 하위 폴더(Interface · Algorithm 등)까지 모은다. UI/ 는 4층(QtWidgets) 타깃 VisualMarkdownView 의 자리라
# 이 타깃(QtCore 만)에는 넣지 않는다(design/Views.md §3). 경로에 정규식 글자가 있어도 되도록 상대 경로로 걸러 낸다.
set(VISUALMARKDOWN_CORE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/VisualMarkdown)
file(GLOB_RECURSE VISUALMARKDOWN_SOURCES RELATIVE ${VISUALMARKDOWN_CORE_SOURCE_DIR} CONFIGURE_DEPENDS
    ${VISUALMARKDOWN_CORE_SOURCE_DIR}/*.h
    ${VISUALMARKDOWN_CORE_SOURCE_DIR}/*.cpp)
list(FILTER VISUALMARKDOWN_SOURCES EXCLUDE REGEX "^UI/")
list(TRANSFORM VISUALMARKDOWN_SOURCES PREPEND ${VISUALMARKDOWN_CORE_SOURCE_DIR}/)

# vocabulary.json 은 :/visualmarkdown/ 아래에 들어간다. 정적 라이브러리의 qrc 는 저절로 붙지 않아 KnowledgeVocabulary::Get() 이 등록한다.
add_library(VisualMarkdown STATIC ${VISUALMARKDOWN_SOURCES} resources/VisualMarkdown.qrc)
add_library(VisualMarkdown::VisualMarkdown ALIAS VisualMarkdown)
# Ocean 이 add_subdirectory 로 붙여도 같게 돌도록 전역 CMAKE_* 대신 타깃 속성으로 정한다.
set_target_properties(VisualMarkdown PROPERTIES
    AUTORCC ON
    CXX_EXTENSIONS OFF)
target_compile_features(VisualMarkdown PUBLIC cxx_std_17)
# 쓰는 쪽은 #include <VisualMarkdown/KnowledgeTypes.h> 로 쓴다.
target_include_directories(VisualMarkdown PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(VisualMarkdown PUBLIC Qt5::Core)
target_compile_definitions(VisualMarkdown PRIVATE QT_DISABLE_DEPRECATED_BEFORE=0x050F00)
target_compile_options(VisualMarkdown PRIVATE ${VISUALMARKDOWN_COMPILE_OPTIONS})

################################################################################
# Tests (Qt Test, 개발용 — Ocean 빌드에는 끼지 않는다)
################################################################################
if(VISUALMARKDOWN_BUILD_TESTS)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Test)
    enable_testing()
    file(GLOB VISUALMARKDOWN_TEST_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h)
    add_executable(VisualMarkdownTests ${VISUALMARKDOWN_TEST_SOURCES})
    set_target_properties(VisualMarkdownTests PROPERTIES
        AUTOMOC ON
        CXX_EXTENSIONS OFF)
    target_link_libraries(VisualMarkdownTests PRIVATE VisualMarkdown Qt5::Test)
    # Ocean 에서 테스트가 받던 것과 같게 옛 API 를 막는다. ExampleWikiTest 는 정준 예시 폴더(examples/wiki)를,
    # ShowcaseWikiTest 는 보기 시험용 예시 폴더(examples/showcase/wiki)를 읽는다.
    target_compile_definitions(VisualMarkdownTests PRIVATE
        QT_DISABLE_DEPRECATED_BEFORE=0x050F00
        VISUALMARKDOWN_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples/wiki"
        VISUALMARKDOWN_SHOWCASE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/examples/showcase/wiki")
    target_compile_options(VisualMarkdownTests PRIVATE ${VISUALMARKDOWN_COMPILE_OPTIONS})
    add_test(NAME VisualMarkdownTests COMMAND VisualMarkdownTests)
    # 첫 인자(클래스 이름) 시험: 모르는 이름이면 아무것도 돌지 않으므로 실패로 끝나야 한다(tests/TestMainTest.cmake).
    add_test(NAME VisualMarkdownTestMainTest
             COMMAND ${CMAKE_COMMAND} -DTEST_EXE=$<TARGET_FILE:VisualMarkdownTests>
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/TestMainTest.cmake)
    if(WIN32)
        # 실행 파일 옆에 DLL 이 없어도 Qt DLL 과 MinGW 런타임 DLL 을 찾게 Qt bin 폴더와 컴파일러 폴더를 PATH 앞에 둔다.
        # 콘솔 창 없이 돌면(IDE 등에서 부른 ctest) Qt Test 는 결과를 OutputDebugString 으로만 보내므로 QT_FORCE_STDERR_LOGGING 으로 출력에 남긴다.
        set(VISUALMARKDOWN_TEST_ENVIRONMENT "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt5::Core>")
        if(MINGW)
            get_filename_component(VISUALMARKDOWN_MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
            list(APPEND VISUALMARKDOWN_TEST_ENVIRONMENT "PATH=path_list_prepend:${VISUALMARKDOWN_MINGW_BIN_DIR}")
        endif()
        list(APPEND VISUALMARKDOWN_TEST_ENVIRONMENT "QT_FORCE_STDERR_LOGGING=set:1")
        set_tests_properties(VisualMarkdownTests VisualMarkdownTestMainTest
                             PROPERTIES ENVIRONMENT_MODIFICATION "${VISUALMARKDOWN_TEST_ENVIRONMENT}")
    endif()
endif()
