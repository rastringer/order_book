cmake_minimum_required(VERSION 3.16)
project(order_book_engine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Main executable
add_executable(order_book_demo
    src/main.cpp
    src/order_book.cpp
    src/matching_engine.cpp
)

# Enable warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(order_book_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()