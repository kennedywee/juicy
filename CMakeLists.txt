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

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets)

add_executable(juicy
  src/main.cpp
)

target_link_libraries(juicy PRIVATE Qt6::Widgets)

target_compile_options(juicy PRIVATE
  -Wall
  -Wextra
  -Wpedantic
  -Wconversion
  -Wsign-conversion
)

install(TARGETS juicy RUNTIME DESTINATION bin)

