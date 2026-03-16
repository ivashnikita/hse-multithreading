cmake_minimum_required(VERSION 3.25)

project(hw4_mpsc_queue LANGUAGES CXX)

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

add_executable(hw4_test test.cpp)
target_link_libraries(hw4_test PRIVATE Threads::Threads rt GTest::gtest GTest::gtest_main)

add_executable(producer_demo producer_demo.cpp)
target_link_libraries(producer_demo PRIVATE rt)

add_executable(consumer_demo consumer_demo.cpp)
target_link_libraries(consumer_demo PRIVATE Threads::Threads rt)