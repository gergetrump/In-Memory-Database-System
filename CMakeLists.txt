# ---------------------------------------------------------------------------
# IMLAB
# ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.7)
project(imlab)

# ---------------------------------------------------------------------------
# Environment
# ---------------------------------------------------------------------------

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -fsanitize=address")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-march=native" COMPILER_SUPPORTS_MARCH_NATIVE)
if (COMPILER_SUPPORTS_MARCH_NATIVE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native")
endif ()

# Unfortunately, macOS (<= High Sierra) ships with a too old bison version.
# You need to manually install bison and flex via homebrew.
# (It's a good thing to have homebrew installed anyway)
# https://brew.sh/index_de.html
# - brew update
# - brew install bison flex
#
# As homebrew won't override the system's default bison, we need to tell
# cmake where it should look.
if (APPLE)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/bison)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/flex)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/bison)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/flex)
endif (APPLE)

# ---------------------------------------------------------------------------
# Dependencies
# ---------------------------------------------------------------------------

find_package(Threads REQUIRED)
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
find_package(GTest REQUIRED)
find_package(TBB REQUIRED)


FLEX_TARGET(Scanner ${CMAKE_SOURCE_DIR}/imlab/parser/scanner.l ${CMAKE_BINARY_DIR}/imlab/parser/gen/scanner.cc)
BISON_TARGET(Parser ${CMAKE_SOURCE_DIR}/imlab/parser/parser.y ${CMAKE_BINARY_DIR}/imlab/parser/gen/parser.cc)

ADD_FLEX_BISON_DEPENDENCY(Scanner Parser)

set(generated_cc
        "${CMAKE_BINARY_DIR}/imlab/parser/gen/scanner.cc"
        "${CMAKE_BINARY_DIR}/imlab/parser/gen/parser.cc"
        imlab/parser/AST.cc
)

add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/parser.cc
        COMMAND ${BISON_EXECUTABLE} --defines=${CMAKE_CURRENT_BINARY_DIR}/parser.h -o ${CMAKE_CURRENT_BINARY_DIR}/parser.cc ${CMAKE_CURRENT_SOURCE_DIR}/parser.y
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/parser.y
)


set(THREADS_PREFER_PTHREAD_FLAG ON)

include("${CMAKE_SOURCE_DIR}/cmake/clang-tidy.cmake")
include("${CMAKE_SOURCE_DIR}/vendor/googletest.cmake")
include("${CMAKE_SOURCE_DIR}/vendor/gflags.cmake")
include("${CMAKE_SOURCE_DIR}/vendor/fmt.cmake")

include("${CMAKE_SOURCE_DIR}/imlab/parser/local.cmake")

include("${CMAKE_SOURCE_DIR}/data/foo/local.cmake")

# ---------------------------------------------------------------------------
# Includes
# ---------------------------------------------------------------------------

include_directories(
        ${CMAKE_SOURCE_DIR}
        ${FMT_INCLUDE_DIR}
        ${GTEST_INCLUDE_DIR}
        ${GMOCK_INCLUDE_DIR}
        ${GFLAGS_INCLUDE_DIR}
)

include_directories(${CMAKE_BINARY_DIR}/imlab/parser/gen)


# ---------------------------------------------------------------------------
# Sources
# ---------------------------------------------------------------------------

set(INCLUDE_H
        "${CMAKE_SOURCE_DIR}/imlab/algebra/BinaryExpression.h"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/Const.h"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/Expression.h"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/InnerJoin.h"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/IU.h"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/IURef.h"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/Operator.h"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/Print.h"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/Select.h"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/TableScan.h"
        "${CMAKE_SOURCE_DIR}/imlab/infra/Bits.h"
        "${CMAKE_SOURCE_DIR}/imlab/infra/Hash.h"
        "${CMAKE_SOURCE_DIR}/imlab/infra/HashTable.h"
        "${CMAKE_SOURCE_DIR}/imlab/infra/Template.h"
        "${CMAKE_SOURCE_DIR}/imlab/optimizer/OptimizerPass.h"
        "${CMAKE_SOURCE_DIR}/imlab/parser/AST.h"
        "${CMAKE_SOURCE_DIR}/imlab/parser/ParseContext.h"
        "${CMAKE_SOURCE_DIR}/imlab/runtime/RuntimeException.h"
        "${CMAKE_SOURCE_DIR}/imlab/semana/SemanticAnalysis.h"
        "${CMAKE_SOURCE_DIR}/imlab/statement/CreateTableStatement.h"
        "${CMAKE_SOURCE_DIR}/imlab/statement/CopyTableStatement.h"
        "${CMAKE_SOURCE_DIR}/imlab/statement/QueryStatement.h"
        "${CMAKE_SOURCE_DIR}/imlab/statement/Statement.h"
        "${CMAKE_SOURCE_DIR}/imlab/types/Types.h"
        "${CMAKE_SOURCE_DIR}/imlab/Database.h"
        "${CMAKE_SOURCE_DIR}/test/tpcc/Data.h"
        "${CMAKE_SOURCE_DIR}/tools/statictpcc/TPCC.h"
)

set(SRC_CC
        "${CMAKE_SOURCE_DIR}/imlab/algebra/BinaryExpression.cc"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/Const.cc"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/Expression.cc"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/GroupBy.cc"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/InnerJoin.cc"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/IU.cc"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/IURef.cc"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/Print.cc"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/Select.cc"
        "${CMAKE_SOURCE_DIR}/imlab/algebra/TableScan.cc"
        "${CMAKE_SOURCE_DIR}/imlab/infra/Types.cc"
        "${CMAKE_SOURCE_DIR}/imlab/parser/AST.cc"
        "${CMAKE_SOURCE_DIR}/imlab/parser/ParseContext.cc"
        "${CMAKE_SOURCE_DIR}/imlab/runtime/RuntimeException.cc"
        "${CMAKE_SOURCE_DIR}/imlab/semana/SemanticAnalysis.cc"
        "${CMAKE_SOURCE_DIR}/imlab/statement/CreateTableStatement.cc"
        "${CMAKE_SOURCE_DIR}/imlab/statement/CopyTableStatement.cc"
        "${CMAKE_SOURCE_DIR}/imlab/statement/QueryStatement.cc"
        "${CMAKE_SOURCE_DIR}/imlab/statement/Statement.cc"
        "${CMAKE_SOURCE_DIR}/imlab/types/Types.cc"
        "${CMAKE_SOURCE_DIR}/imlab/Database.cc"
)

# keep the generated files of bison and flex separated, as otherwise these are linted too
set(generated_cc
        "${CMAKE_SOURCE_DIR}/imlab/parser/gen/scanner.cc"
        "${CMAKE_SOURCE_DIR}/imlab/parser/gen/parser.cc"
        imlab/parser/AST.cc
        test/own_tests/test_generated_code.cc
        test/own_tests/test.cpp
        imlab/infra/HashTable.h
        imlab/algebra/SubqueryScan.cc
        imlab/algebra/SubqueryScan.h
        imlab/statement/CodeGenHelper.h
)

set(TEST_CC
        "${CMAKE_SOURCE_DIR}/test/tpcc/Data.cc"
        "${CMAKE_SOURCE_DIR}/test/tpcc/DatabaseTest.cc"
        "${CMAKE_SOURCE_DIR}/test/optimizer/OptimizerTest.cc"
        "${CMAKE_SOURCE_DIR}/test/parser/QueryParserTest.cc"
        "${CMAKE_SOURCE_DIR}/test/parser/SchemaParserTest.cc"
        "${CMAKE_SOURCE_DIR}/test/semana/SemanticAnalysisTest.cc"
        "${CMAKE_SOURCE_DIR}/test/query/AggregateAndBuildSideTest.cc"
)

set(TPCC_TOOLS_CC
        "${CMAKE_SOURCE_DIR}/tools/statictpcc/TPCC.cc"
)

set(TOOLS_CC
        "${CMAKE_SOURCE_DIR}/tools/tpcc.cc"
        "${CMAKE_SOURCE_DIR}/tools/imlabdb.cc"
        "${CMAKE_SOURCE_DIR}/tools/bench.cc"
)

# ---------------------------------------------------------------------------
# Targets
# ---------------------------------------------------------------------------

set(IMLAB_CODEGEN_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
if (CMAKE_BUILD_TYPE)
    string(TOUPPER "${CMAKE_BUILD_TYPE}" IMLAB_BUILD_TYPE_UPPER)
    set(IMLAB_CODEGEN_CXX_FLAGS "${IMLAB_CODEGEN_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${IMLAB_BUILD_TYPE_UPPER}}")
endif ()

set(IMLAB_TBB_TARGET tbb)
if (NOT TARGET tbb AND TARGET TBB::tbb)
    set(IMLAB_TBB_TARGET TBB::tbb)
endif ()
get_target_property(IMLAB_TBB_INCLUDES ${IMLAB_TBB_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
if (IMLAB_TBB_INCLUDES)
    foreach (IMLAB_TBB_INCLUDE ${IMLAB_TBB_INCLUDES})
        string(APPEND IMLAB_CODEGEN_CXX_FLAGS " -I${IMLAB_TBB_INCLUDE}")
    endforeach ()
endif ()

set(IMLAB_TBB_LIB "")
get_target_property(IMLAB_TBB_LIB ${IMLAB_TBB_TARGET} IMPORTED_LOCATION)
if (NOT IMLAB_TBB_LIB)
    get_target_property(IMLAB_TBB_LIB ${IMLAB_TBB_TARGET} IMPORTED_LOCATION_RELEASE)
endif ()
if (NOT IMLAB_TBB_LIB)
    get_target_property(IMLAB_TBB_LIB ${IMLAB_TBB_TARGET} IMPORTED_LOCATION_DEBUG)
endif ()
if (IMLAB_TBB_LIB)
    get_filename_component(IMLAB_TBB_LIBDIR "${IMLAB_TBB_LIB}" DIRECTORY)
    set(IMLAB_CODEGEN_LINK_FLAGS "-L${IMLAB_TBB_LIBDIR} -ltbb")
else ()
    set(IMLAB_CODEGEN_LINK_FLAGS "-ltbb")
endif ()

configure_file(
        ${CMAKE_SOURCE_DIR}/imlab/statement/CodeGenConfig.h.in
        ${CMAKE_BINARY_DIR}/imlab/statement/CodeGenConfig.h
        @ONLY
)
add_compile_definitions("IMLAB_DATA_DIR=\"${CMAKE_SOURCE_DIR}/data\"")

add_library(imlab STATIC ${SRC_CC} ${generated_cc})
add_dependencies(imlab parser)
target_include_directories(imlab PRIVATE ${CMAKE_BINARY_DIR})
target_link_libraries(imlab fmt Threads::Threads TBB::tbb)

add_executable(tpcc ${TPCC_TOOLS_CC} tools/tpcc.cc)
target_link_libraries(tpcc imlab gflags Threads::Threads)

add_executable(imlabdb tools/imlabdb.cc)
target_link_libraries(imlabdb imlab)

add_executable(bench tools/bench.cc)
target_link_libraries(bench imlab)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

add_executable(tester test/tester.cc ${TEST_CC} ${TPCC_TOOLS_CC})
target_link_libraries(tester imlab GTest::GTest GTest::Main Threads::Threads)
enable_testing()
add_test(imlab tester)

add_executable(test_generated_code
        test/own_tests/test_generated_code.cc
)

target_link_libraries(test_generated_code imlab GTest::GTest GTest::Main Threads::Threads)



# ---------------------------------------------------------------------------
# Linting
# ---------------------------------------------------------------------------

add_clang_tidy_target(src_linting "${SRC_CC}")
add_clang_tidy_target(include_linting "${INCLUDE_H}")
add_clang_tidy_target(tpcc_tools_linting "${TPCC_TOOLS_CC}")
add_clang_tidy_target(tools_linting "${TOOLS_CC}")
add_clang_tidy_target(test_linting "${TEST_CC}")

add_custom_target(lint)
list(APPEND lint_targets include_linting src_linting tpcc_tools_linting tools_linting test_linting)
add_dependencies(lint ${lint_targets})

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

message(STATUS "[IMLAB] settings")
message(STATUS "    GFLAGS_INCLUDE_DIR          = ${GFLAGS_INCLUDE_DIR}")
message(STATUS "    GFLAGS_LIBRARY_PATH         = ${GFLAGS_LIBRARY_PATH}")
message(STATUS "    FMT_INCLUDE_DIR             = ${FMT_INCLUDE_DIR}")
message(STATUS "    FMT_LIBRARY_PATH            = ${FMT_LIBRARY_PATH}")
message(STATUS "[TEST] settings")
message(STATUS "    GTEST_INCLUDE_DIR           = ${GTEST_INCLUDE_DIR}")
message(STATUS "    GTEST_LIBRARY_PATH          = ${GTEST_LIBRARY_PATH}")
message(STATUS "    GMOCK_INCLUDE_DIR           = ${GMOCK_INCLUDE_DIR}")
message(STATUS "    GMOCK_LIBRARY_PATH          = ${GMOCK_LIBRARY_PATH}")

