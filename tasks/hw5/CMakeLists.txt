cmake_minimum_required(VERSION 3.25)

project(hw5_process_pool LANGUAGES CXX)

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

add_executable(hw5_test test.cpp)
target_link_libraries(hw5_test PRIVATE Threads::Threads rt GTest::gtest GTest::gtest_main)