cmake_minimum_required(VERSION 3.16)

project(Terminal)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FONTCONFIG REQUIRED fontconfig)


file(GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    Freetype::Freetype
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    util
    ${FONTCONFIG_LIBRARIES}
)

# target_compile_options(${PROJECT_NAME} PRIVATE ${FONTCONFIG_CFLAGS_OTHER})