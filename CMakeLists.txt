cmake_minimum_required(VERSION 3.10)

project(Motor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(fmt CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(DevIL REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# Todo el código del motor está en la carpeta src/
set(MOTOR_SOURCES
    src/Main.cpp
    src/Window.h
    src/Window.cpp
    src/Texture.h
    src/Texture.cpp
    src/Engine.h
    src/Engine.cpp
    src/Renderer.h
    src/Renderer.cpp
    src/Geometria.h
    src/Geometria.cpp
    src/Shader.h
    src/Shader.cpp
    src/ModelObject.h
    src/Log.h
    src/Log.cpp
    src/GameObject.h
    src/Component.h
)

add_executable(Motor ${MOTOR_SOURCES})

# Para que los #include "X.h" funcionen desde cualquier sitio
target_include_directories(Motor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# En Visual Studio, mostrar los ficheros dentro de la carpeta "src"
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX "src" FILES ${MOTOR_SOURCES})

target_link_libraries(Motor PRIVATE fmt::fmt SDL3::SDL3 glad::glad DevIL::IL DevIL::ILU assimp::assimp imgui::imgui glm::glm)

add_custom_command(
    TARGET Motor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:Motor>/assets
)