cmake_minimum_required(VERSION 3.25)

project(lecture_examples LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

include_directories(include)

add_subdirectory(cache)
add_subdirectory(futex)
add_subdirectory(processes_and_threads)
add_subdirectory(synchronization_primitives)
add_subdirectory(tasks/hw1)
