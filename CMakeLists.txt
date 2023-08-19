cmake_minimum_required(VERSION 3.1)

project(android_ninja_to_compdb)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set (CMAKE_CXX_STANDARD 14)

add_executable(an-compdb  android_ninja_to_compdb.cpp cmd_out.cpp)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(an-compdb PRIVATE Threads::Threads)
