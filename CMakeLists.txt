cmake_minimum_required(VERSION 3.24)

project(
  juicy
  VERSION 0.1.0
  DESCRIPTION "A minimal Linux torrent video player with Anime4K"
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS OpenGLWidgets Widgets)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv)

add_executable(juicy
  src/main.cpp
  src/player/MpvVideoWidget.cpp
  src/player/MpvVideoWidget.hpp
)

target_include_directories(juicy PRIVATE src)

target_link_libraries(juicy PRIVATE
  PkgConfig::MPV
  Qt6::OpenGLWidgets
  Qt6::Widgets
)

target_compile_options(juicy PRIVATE
  -Wall
  -Wextra
  -Wpedantic
  -Wconversion
  -Wsign-conversion
)

install(TARGETS juicy RUNTIME DESTINATION bin)
