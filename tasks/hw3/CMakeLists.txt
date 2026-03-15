cmake_minimum_required(VERSION 3.25)

project(hw3_futex_mutex LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(googletest)

add_executable(hw3_test test.cpp)
target_link_libraries(hw3_test PRIVATE Threads::Threads GTest::gtest GTest::gtest_main)