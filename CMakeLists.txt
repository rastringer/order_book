cmake_minimum_required(VERSION 3.16)
project(order_book_engine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Engine sources reused by demo and tests
set(ENGINE_SOURCES
    src/order_book.cpp
    src/matching_engine.cpp
)

# Main executable
add_executable(order_book_demo
    src/main.cpp
    ${ENGINE_SOURCES}
)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(order_book_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------
# Tests (doctest, fetched at configure time). Disable with
#   cmake -DBUILD_TESTING=OFF ..
# ---------------------------------------------------------------------
option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.11
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(doctest)

    enable_testing()
    add_executable(order_book_tests
        test/test_order_book.cpp
        ${ENGINE_SOURCES}
    )
    target_include_directories(order_book_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(order_book_tests PRIVATE doctest::doctest)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(order_book_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME unit_tests COMMAND order_book_tests)
endif()
