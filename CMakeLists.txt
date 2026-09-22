cmake_minimum_required(VERSION 3.10)

project(Motor)

find_package(fmt CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)

add_executable(Motor Main.cpp)

target_link_libraries(Motor PRIVATE fmt::fmt SDL3::SDL3 glad::glad)