cmake_minimum_required(VERSION 3.24)

project(
  juicy
  VERSION 0.1.0
  DESCRIPTION "A minimal Linux torrent video player with Anime4K"
  LANGUAGES CXX
)

include(GNUInstallDirs)

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
  JUICY_SOURCE_ICON="${CMAKE_CURRENT_SOURCE_DIR}/data/icons/juicy.svg"
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

install(TARGETS juicy RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
install(
  DIRECTORY assets/anime4k/
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/juicy/anime4k"
)
# A graphical session does not inherit the shell's PATH, so a bare "Exec=juicy"
# never resolves under a user prefix like ~/.local/bin: the launcher hangs on
# "launching" and no process starts. Rewrite it at install time, when the real
# prefix is known (--install --prefix overrides the configured one).
install(CODE "
  file(READ \"${CMAKE_CURRENT_SOURCE_DIR}/data/juicy.desktop\" desktop_entry)
  string(REPLACE
    \"Exec=juicy\"
    \"Exec=\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/juicy\"
    desktop_entry \"\${desktop_entry}\"
  )
  file(WRITE
    \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/applications/juicy.desktop\"
    \"\${desktop_entry}\"
  )
  message(STATUS \"Installing: \${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATAROOTDIR}/applications/juicy.desktop\")
")
install(
  FILES data/icons/juicy.svg
  DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps"
)

include(CTest)
if(BUILD_TESTING)
  find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

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

  add_executable(juicy-torrent-session-test
    tests/TorrentSessionTest.cpp
    src/torrent/TorrentSession.cpp
    src/torrent/TorrentSession.hpp
    src/torrent/TorrentStream.cpp
    src/torrent/TorrentStream.hpp
  )
  target_include_directories(juicy-torrent-session-test PRIVATE src)
  target_link_libraries(juicy-torrent-session-test PRIVATE
    Libtorrent::Rasterbar
    Qt6::Test
  )
  target_compile_options(juicy-torrent-session-test PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
  )
  if(JUICY_DEPS_ROOT)
    set_target_properties(juicy-torrent-session-test PROPERTIES
      BUILD_RPATH "${JUICY_DEPS_ROOT}/usr/lib"
    )
  endif()
  add_test(NAME torrent-session COMMAND juicy-torrent-session-test)
endif()
