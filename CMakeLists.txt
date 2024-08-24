######################################################################
# Author:      PB
# Purpose:     CMake for wxExplorerBrowser demo
# Copyright:   (c) 2018 PB <pbfordev@gmail.com>
# Licence:     wxWindows licence
######################################################################

cmake_minimum_required(VERSION 3.24 FATAL_ERROR)
project(wxExplorerBrowserDemo)

if(NOT WIN32)
  message(FATAL_ERROR "wxExplorerBrowser is available only for Microsoft Windows.")
endif()

find_package(wxWidgets 3.2 COMPONENTS core base REQUIRED)
if(wxWidgets_USE_FILE)
  include(${wxWidgets_USE_FILE})
endif()

set(SOURCES
  wxExplorerBrowser.h
  wxExplorerBrowser.cpp
  demo.cpp
  "${wxWidgets_ROOT_DIR}/include/wx/msw/wx.rc"  
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE wxUSE_RC_MANIFEST wxUSE_DPI_AWARE_MANIFEST=2)
if(MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE /W4)
  add_compile_definitions(_CRT_SECURE_NO_DEPRECATE _CRT_NON_CONFORMING_SWPRINTFS _SCL_SECURE_NO_WARNINGS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
else() # GCC or clang
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${wxWidgets_LIBRARIES})
if(MINGW) # work around the breaking change in wxWidgets 3.3
  target_link_libraries(${PROJECT_NAME} PRIVATE gdiplus msimg32)
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE YES)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property (DIRECTORY PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})