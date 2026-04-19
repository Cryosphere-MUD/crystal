cmake_minimum_required(VERSION 3.10)
project(crystal VERSION 0.4.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Localization
set(LOCALEDIR "${CMAKE_INSTALL_FULL_DATADIR}/locale")

# Build options
option(ENABLE_SCRIPTING "Enable Lua scripting support" ON)

# Define executable sources
set(SOURCES
  src/main.cc src/keybindings.cc src/Runtime.cc
  src/grid.cc src/commandeditor.cc src/display.cc
  src/telnet.cc src/io.cc src/commands.cc src/common.cc
  src/scripting.cc src/url.cc src/ssh.cc
)

# Define headers for IDEs
set(HEADERS
  src/common.h src/Runtime.h src/grid.h src/io.h src/telnet.h
  src/scripting.h src/commands.h src/commandeditor.h src/url.h src/ssh.h
)

add_executable(crystal ${SOURCES} ${HEADERS})

# ASIO (standalone, header-only)
find_path(ASIO_INCLUDE_DIR asio.hpp
  HINTS /opt/homebrew/include /usr/local/include /usr/include
)
if(ASIO_INCLUDE_DIR)
  target_include_directories(crystal PRIVATE ${ASIO_INCLUDE_DIR})
  target_compile_definitions(crystal PRIVATE ASIO_STANDALONE)
else()
  message(FATAL_ERROR "asio.hpp not found — install standalone ASIO (e.g. brew install asio)")
endif()

# Compiler definitions
target_compile_definitions(crystal PRIVATE
  -DLOCALEDIR="${LOCALEDIR}"
  -DPACKAGE="crystal"
  -DVERSION=\"${PROJECT_VERSION}\"
)

# Platform headers
include(CheckIncludeFile)
check_include_file("fcntl.h" HAVE_FCNTL_H)
check_include_file("sys/ioctl.h" HAVE_SYS_IOCTL_H)
check_include_file("sys/time.h" HAVE_SYS_TIME_H)
check_include_file("unistd.h" HAVE_UNISTD_H)

# Libraries
find_library(CURSES_LIB curses)
if(NOT CURSES_LIB)
  find_library(CURSES_LIB ncurses REQUIRED)
endif()
target_link_libraries(crystal PRIVATE ${CURSES_LIB})

find_package(ZLIB)
if(ZLIB_FOUND)
  target_link_libraries(crystal PRIVATE ZLIB::ZLIB)
  target_compile_definitions(crystal PRIVATE HAVE_ZLIB)
else()
  message(WARNING "zlib not found - MCCP support will not be built")
endif()

set(Zstd_FOUND FALSE)
find_path(ZSTD_INCLUDE_DIR zstd.h)

if (ZSTD_INCLUDE_DIR)
  find_library(ZSTD_LIBRARY zstd)
  if (ZSTD_LIBRARY)
    set(Zstd_FOUND TRUE)
  endif()
endif()

if(Zstd_FOUND)
  target_link_libraries(crystal PRIVATE ${ZSTD_LIBRARY})
  target_compile_definitions(crystal PRIVATE HAVE_ZSTD)
else()
  message(WARNING "zstd not found - MCCP4 support will not be built")
endif()

find_library(ICONV_LIB iconv)
if(ICONV_LIB)
  target_link_libraries(crystal PRIVATE ${ICONV_LIB})
endif()

find_package(OpenSSL)
if(OPENSSL_FOUND)
  target_link_libraries(crystal PRIVATE OpenSSL::SSL)
  target_compile_definitions(crystal PRIVATE HAVE_SSL)
else()
  message(WARNING "OpenSSL not found - SSL support will not be built")
endif()

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(LIBSSH2 libssh2)
endif()
if(NOT LIBSSH2_FOUND)
  find_path(LIBSSH2_INCLUDE_DIR libssh2.h)
  find_library(LIBSSH2_LIBRARY NAMES ssh2 libssh2)
  if(LIBSSH2_INCLUDE_DIR AND LIBSSH2_LIBRARY)
    set(LIBSSH2_FOUND TRUE)
    set(LIBSSH2_INCLUDE_DIRS ${LIBSSH2_INCLUDE_DIR})
    set(LIBSSH2_LIBRARIES ${LIBSSH2_LIBRARY})
  endif()
endif()
if(LIBSSH2_FOUND)
  target_include_directories(crystal PRIVATE ${LIBSSH2_INCLUDE_DIRS})
  target_link_directories(crystal PRIVATE ${LIBSSH2_LIBRARY_DIRS})
  target_link_libraries(crystal PRIVATE ${LIBSSH2_LIBRARIES})
  target_compile_definitions(crystal PRIVATE HAVE_LIBSSH2)
  message(STATUS "libssh2 found - SSH support enabled")
else()
  message(WARNING "libssh2 not found - SSH support will not be built")
endif()

# Optional: Lua scripting
if(ENABLE_SCRIPTING)
  find_path(LUA_INCLUDE_DIR lua.h
    PATH_SUFFIXES lua5.4
  )

  find_library(LUA_LIBRARY NAMES lua5.4)

  if(LUA_INCLUDE_DIR AND LUA_LIBRARY)
    target_include_directories(crystal PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(crystal PRIVATE ${LUA_LIBRARY})
    target_compile_definitions(crystal PRIVATE HAVE_LUA)
  else()
    message(WARNING "Lua not found - scripting will not be built.")
  endif()
endif()

# Use fmtlib3
add_subdirectory(vendor/fmt)
target_link_libraries(crystal PRIVATE fmt::fmt)

target_include_directories(crystal PRIVATE vendor/json/include vendor)

# Manpage installation
install(FILES crystal.6 DESTINATION share/man/man6)

# Install the executable
install(TARGETS crystal DESTINATION bin)

# Install extra files (optional for Debian)
install(FILES crystal.lua DESTINATION share/${PROJECT_NAME})
