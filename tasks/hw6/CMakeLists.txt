cmake_minimum_required(VERSION 3.25)

project(hw6_coroutine_dfs LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED COMPONENTS context)

add_executable(hw6_main main.cpp)
target_link_libraries(hw6_main PRIVATE Boost::context)

include(FetchContent)

FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.17.0
)
FetchContent_MakeAvailable(googletest)

add_executable(hw6_test test.cpp)
target_link_libraries(hw6_test PRIVATE Boost::context GTest::gtest GTest::gtest_main)