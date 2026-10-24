# VisualMarkdown: 보기 앱(VisualMarkdownViewer)의 스냅샷 모드를 실행 파일 그대로 시험한다(design/Views.md §8 · §9).
# 사용: cmake -DVIEWER_EXE=<VisualMarkdownViewer 실행 파일> -DWIKI_DIR=<showcase wiki> -DEXPECTED_TSV=<tests/viewer/showcase-snapshot.tsv>
#             -DOUTPUT_DIR=<빌드 폴더 안의 새 폴더> -P SnapshotTest.cmake   (CTest: VisualMarkdownViewerSnapshotTest)
#       Windows 에서 손으로 돌리면 Qt bin 폴더를 PATH 에 둔다(CTest 는 CMakeLists.txt 가 정한다). 화면은 없어도 된다(앱이 offscreen 으로 돈다).
cmake_minimum_required(VERSION 3.24)

foreach(requiredVariable IN ITEMS VIEWER_EXE WIKI_DIR EXPECTED_TSV OUTPUT_DIR)
    if(NOT DEFINED ${requiredVariable})
        message(FATAL_ERROR "SnapshotTest.cmake: -D${requiredVariable}=... is required")
    endif()
endforeach()

set_property(GLOBAL PROPERTY VISUALMARKDOWN_SNAPSHOT_FAILURES "")

# 실행 파일을 인자와 함께 돌려 끝 코드와 출력(표준 출력과 표준 오류를 합친 것)을 받는다.
function(visualmarkdown_run outExit outLog)
    execute_process(COMMAND "${VIEWER_EXE}" ${ARGN} RESULT_VARIABLE exitCode OUTPUT_VARIABLE log ERROR_VARIABLE log)
    set(${outExit} "${exitCode}" PARENT_SCOPE)
    set(${outLog} "${log}" PARENT_SCOPE)
endfunction()

function(visualmarkdown_report caseName passed detail)
    if(passed)
        message(STATUS "PASS ${caseName}")
    else()
        message(STATUS "FAIL ${caseName}: ${detail}")
        set_property(GLOBAL APPEND PROPERTY VISUALMARKDOWN_SNAPSHOT_FAILURES "${caseName}")
    endif()
endfunction()

# PNG 머리(IHDR)의 가로 · 세로: 16 바이트째부터 big endian 4 바이트씩
function(visualmarkdown_png_size path outSize)
    if(NOT EXISTS "${path}")
        set(${outSize} "missing" PARENT_SCOPE)
        return()
    endif()
    file(READ "${path}" header OFFSET 16 LIMIT 8 HEX)
    string(SUBSTRING "${header}" 0 8 widthHex)
    string(SUBSTRING "${header}" 8 8 heightHex)
    math(EXPR width "0x${widthHex}")
    math(EXPR height "0x${heightHex}")
    set(${outSize} "${width}x${height}" PARENT_SCOPE)
endfunction()

# 스냅샷 폴더를 확인한다: PNG 가 기대 목록의 파일 19개 그대로이고, 그림 크기가 size 이며, snapshot.tsv 가 기대 파일과 바이트로 같다
function(visualmarkdown_check_snapshot caseName folder size)
    file(STRINGS "${EXPECTED_TSV}" expectedLines ENCODING UTF-8)
    list(REMOVE_AT expectedLines 0)   # 머리 줄
    set(missing "")
    foreach(line IN LISTS expectedLines)
        string(REGEX MATCH "^[^\t]+" fileName "${line}")
        if(NOT EXISTS "${folder}/${fileName}")
            list(APPEND missing "${fileName}")
        endif()
    endforeach()
    file(GLOB pngs "${folder}/*.png")
    list(LENGTH pngs pngCount)
    set(passed FALSE)
    if(pngCount EQUAL 19 AND NOT missing)
        set(passed TRUE)
    endif()
    visualmarkdown_report("${caseName}.files" ${passed} "${pngCount} PNG, missing: ${missing}")

    set(wrongSizes "")
    foreach(png IN LISTS pngs)
        visualmarkdown_png_size("${png}" pngSize)
        if(NOT pngSize STREQUAL size)
            get_filename_component(pngName "${png}" NAME)
            list(APPEND wrongSizes "${pngName}=${pngSize}")
        endif()
    endforeach()
    set(passed FALSE)
    if(pngs AND NOT wrongSizes)
        set(passed TRUE)
    endif()
    visualmarkdown_report("${caseName}.size" ${passed} "expected ${size}: ${wrongSizes}")

    execute_process(COMMAND "${CMAKE_COMMAND}" -E compare_files "${folder}/snapshot.tsv" "${EXPECTED_TSV}" RESULT_VARIABLE different)
    set(passed FALSE)
    if(different EQUAL 0)
        set(passed TRUE)
    endif()
    visualmarkdown_report("${caseName}.list" ${passed} "${folder}/snapshot.tsv differs from ${EXPECTED_TSV}")
endfunction()

# 실패해야 하는 실행: 0 이 아닌 끝 코드와, 까닭에 mention 이 있다
function(visualmarkdown_expect_failure caseName mention)
    visualmarkdown_run(exitCode log ${ARGN})
    set(passed FALSE)
    if(NOT exitCode EQUAL 0)
        string(FIND "${log}" "${mention}" mentionAt)
        if(mentionAt GREATER_EQUAL 0)
            set(passed TRUE)
        endif()
    endif()
    visualmarkdown_report("${caseName}" ${passed} "exit ${exitCode}: ${log}")
endfunction()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

# 1) showcase(기본: light · 1600x1000): 끝 코드 0, PNG 19개(그래프 6 + 차트 13), snapshot.tsv 가 기대 파일과 바이트로 같다
set(lightDir "${OUTPUT_DIR}/light")
visualmarkdown_run(exitCode log "${WIKI_DIR}" --snapshot "${lightDir}")
set(passed FALSE)
if(exitCode EQUAL 0)
    set(passed TRUE)
endif()
visualmarkdown_report(light.exit ${passed} "exit ${exitCode}: ${log}")
visualmarkdown_check_snapshot(light "${lightDir}" "1600x1000")

# 2) 어둠 · 다른 크기: 그림이 그 크기이고 목록은 같다
set(darkDir "${OUTPUT_DIR}/dark")
visualmarkdown_run(exitCode log "${WIKI_DIR}" --snapshot "${darkDir}" --theme dark --size 800x500)
set(passed FALSE)
if(exitCode EQUAL 0)
    set(passed TRUE)
endif()
visualmarkdown_report(dark.exit ${passed} "exit ${exitCode}: ${log}")
visualmarkdown_check_snapshot(dark "${darkDir}" "800x500")

# 3) 0 이 아니고 까닭을 표준 오류에 쓰는 경우
visualmarkdown_expect_failure(missingWiki "no-such-wiki" "${OUTPUT_DIR}/no-such-wiki" --snapshot "${OUTPUT_DIR}/missing-out")
if(EXISTS "${OUTPUT_DIR}/missing-out")
    visualmarkdown_report(missingWiki.noOutput FALSE "${OUTPUT_DIR}/missing-out was created")
endif()
visualmarkdown_expect_failure(unknownArgument "--bogus-option" "${WIKI_DIR}" --snapshot "${OUTPUT_DIR}/bogus-out" --bogus-option)
visualmarkdown_expect_failure(unknownArgumentWindow "--bogus-option" --bogus-option)   # 창 모드도 창을 띄우기 전에 끝난다
visualmarkdown_expect_failure(badSize "0x500" "${WIKI_DIR}" --snapshot "${OUTPUT_DIR}/size-out" --size 0x500)
visualmarkdown_expect_failure(badSizeText "800by500" "${WIKI_DIR}" --snapshot "${OUTPUT_DIR}/size-out" --size 800by500)
visualmarkdown_expect_failure(badTheme "sepia" "${WIKI_DIR}" --snapshot "${OUTPUT_DIR}/theme-out" --theme sepia)
file(MAKE_DIRECTORY "${OUTPUT_DIR}/blocked/graph-network.png")   # PNG 자리에 폴더가 있어 그 파일을 쓰지 못한다
visualmarkdown_expect_failure(unwritableFile "graph-network.png" "${WIKI_DIR}" --snapshot "${OUTPUT_DIR}/blocked")

get_property(failures GLOBAL PROPERTY VISUALMARKDOWN_SNAPSHOT_FAILURES)
if(failures)
    message(FATAL_ERROR "VisualMarkdownViewerSnapshotTest failed: ${failures}")
endif()
