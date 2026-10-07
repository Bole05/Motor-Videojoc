cmake_minimum_required(VERSION 3.10)

project(Motor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(DevIL REQUIRED)
find_package(assimp CONFIG REQUIRED)

add_executable(Motor
    Main.cpp
    Window.h
    Window.cpp
    Texture.h
    Texture.cpp
    Engine.h
    Engine.cpp
    Renderer.h
    Renderer.cpp
    Geometria.h
    Geometria.cpp
    Log.h
    Log.cpp
)

target_link_libraries(Motor PRIVATE fmt::fmt SDL3::SDL3 glad::glad DevIL::IL DevIL::ILU assimp::assimp)

add_custom_command(
    TARGET Motor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:Motor>/assets
)