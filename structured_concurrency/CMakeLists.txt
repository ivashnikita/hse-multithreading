add_executable(future future.cpp)
target_link_libraries(future PRIVATE Threads::Threads)

add_executable(shared_future shared_future.cpp)
target_link_libraries(shared_future PRIVATE Threads::Threads)

add_executable(async async.cpp)
target_link_libraries(async PRIVATE Threads::Threads)

add_executable(packaged_task packaged_task.cpp)
target_link_libraries(packaged_task PRIVATE Threads::Threads)

include(FetchContent)
FetchContent_Declare(
    stdexec
    GIT_REPOSITORY https://github.com/NVIDIA/stdexec.git
    GIT_TAG main
)
FetchContent_MakeAvailable(stdexec)

add_executable(stdexec_intro stdexec_intro.cpp)
target_link_libraries(stdexec_intro PRIVATE Threads::Threads STDEXEC::stdexec)

add_executable(continuations continuations.cpp)
target_link_libraries(continuations PRIVATE Threads::Threads STDEXEC::stdexec)