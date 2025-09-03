cmake_minimum_required(VERSION 3.10)
project(crystal VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Localization
set(LOCALEDIR "${CMAKE_INSTALL_FULL_DATADIR}/locale")

# Build options
option(ENABLE_SCRIPTING "Enable Lua scripting support" ON)

# Define executable sources
set(SOURCES
  crystal.cc Socket.cc io.cc grid.cc telnet.cc wcwidth.c
  scripting.cc commands.cc common.cc commandeditor.cc keybindings.cc display.cc main.cc
)

# Define headers for IDEs
set(HEADERS
  Socket.h common.h crystal.h grid.h io.h telnet.h
  scripting.h commands.h commandeditor.h url.h
)

add_executable(crystal ${SOURCES} ${HEADERS})

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

target_link_libraries(crystal PRIVATE -lzstd)

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

# Optional: Lua scripting
if(ENABLE_SCRIPTING)
  find_path(LUA_INCLUDE_DIR lua.h
    PATH_SUFFIXES lua5.4 lua5.3 lua5.2 lua5.1 lua50 lua
  )

  find_library(LUA_LIBRARY NAMES lua lua5.4 lua5.3 lua5.2 lua5.1 lua50)
  find_library(LUALIB_LIBRARY NAMES lualib lualib50)

  if(LUA_INCLUDE_DIR AND LUA_LIBRARY AND LUALIB_LIBRARY)
    target_include_directories(crystal PRIVATE ${LUA_INCLUDE_DIR})
    target_link_libraries(crystal PRIVATE ${LUA_LIBRARY} ${LUALIB_LIBRARY})
    target_compile_definitions(crystal PRIVATE HAVE_LUA)
  else()
    message(WARNING "Lua not found - scripting will not be built.")
  endif()
endif()

target_include_directories(crystal PRIVATE json/include)

# Manpage installation
install(FILES crystal.6 DESTINATION share/man/man6)

# Install the executable
install(TARGETS crystal DESTINATION bin)

# Install extra files (optional for Debian)
install(FILES crystal.lua DESTINATION share/${PROJECT_NAME})
