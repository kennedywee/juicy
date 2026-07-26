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

set(JUICY_DEPS_ROOT "" CACHE PATH "Optional repository-local dependency root")
find_path(
  LIBTORRENT_INCLUDE_DIR
  NAMES libtorrent/session.hpp
  HINTS "${JUICY_DEPS_ROOT}/usr/include"
)
find_library(
  LIBTORRENT_LIBRARY
  NAMES torrent-rasterbar
  HINTS "${JUICY_DEPS_ROOT}/usr/lib"
)
if(NOT LIBTORRENT_INCLUDE_DIR OR NOT LIBTORRENT_LIBRARY)
  message(FATAL_ERROR "libtorrent-rasterbar development files were not found")
endif()

add_library(Libtorrent::Rasterbar SHARED IMPORTED)
set_target_properties(Libtorrent::Rasterbar PROPERTIES
  IMPORTED_LOCATION "${LIBTORRENT_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${LIBTORRENT_INCLUDE_DIR}"
  INTERFACE_COMPILE_DEFINITIONS
    "TORRENT_LINKING_SHARED;BOOST_ASIO_ENABLE_CANCELIO;BOOST_ASIO_NO_DEPRECATED;BOOST_SYSTEM_USE_UTF8;TORRENT_ABI_VERSION=2;TORRENT_USE_OPENSSL;TORRENT_USE_LIBCRYPTO;TORRENT_SSL_PEERS"
)

add_executable(juicy
  src/main.cpp
  src/app/MainWindow.cpp
  src/app/MainWindow.hpp
  src/player/MpvVideoWidget.cpp
  src/player/MpvVideoWidget.hpp
  src/torrent/TorrentSession.cpp
  src/torrent/TorrentSession.hpp
  src/torrent/TorrentStream.cpp
  src/torrent/TorrentStream.hpp
)

target_include_directories(juicy PRIVATE src)

target_compile_definitions(juicy PRIVATE
  JUICY_SOURCE_ANIME4K_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/anime4k"
)

target_link_libraries(juicy PRIVATE
  Libtorrent::Rasterbar
  PkgConfig::MPV
  Qt6::OpenGLWidgets
  Qt6::Widgets
)

if(JUICY_DEPS_ROOT)
  set_target_properties(juicy PROPERTIES
    BUILD_RPATH "${JUICY_DEPS_ROOT}/usr/lib"
  )
endif()

target_compile_options(juicy PRIVATE
  -Wall
  -Wextra
  -Wpedantic
  -Wconversion
  -Wsign-conversion
)

install(TARGETS juicy RUNTIME DESTINATION bin)
install(
  DIRECTORY assets/anime4k/
  DESTINATION share/juicy/anime4k
)

include(CTest)
if(BUILD_TESTING)
  add_executable(juicy-local-seeder tests/LocalTorrentSeeder.cpp)
  target_link_libraries(juicy-local-seeder PRIVATE Libtorrent::Rasterbar)
  target_compile_features(juicy-local-seeder PRIVATE cxx_std_20)
  target_compile_options(juicy-local-seeder PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
  )
  if(JUICY_DEPS_ROOT)
    set_target_properties(juicy-local-seeder PROPERTIES
      BUILD_RPATH "${JUICY_DEPS_ROOT}/usr/lib"
    )
  endif()
endif()
