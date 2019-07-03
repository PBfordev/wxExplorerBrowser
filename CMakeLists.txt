######################################################################
# Author:      PB
# Purpose:     CMake for wxExplorerBrowser demo
# Copyright:   (c) 2018 PB <pbfordev@gmail.com>
# Licence:     wxWindows licence
######################################################################

cmake_minimum_required(VERSION 3.1 FATAL_ERROR)
project(wxExplorerBrowserDemo)

if ( NOT WIN32 )
  message(FATAL_ERROR "wxExplorerBrowser is available only for Microsoft Windows")
endif()

# wxExplorerBrowser requires wxWidgets 3.1 and higher
find_package(wxWidgets 3.1.0 COMPONENTS core base REQUIRED)

# wxExplorerBrowser demo can be built in Debug and/or Release configurations
if ( CMAKE_CONFIGURATION_TYPES )
  set(CMAKE_CONFIGURATION_TYPES Debug Release)
  set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING
       "Reset the configurations to what we need" FORCE)
elseif (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Choose the type of build, options are: Debug Release" FORCE)
endif()


add_definitions(-D__WXMSW__)
add_definitions(-D_WIN32_WINNT=0x0600)
include( ${wxWidgets_USE_FILE} )

list(APPEND src_files "${CMAKE_CURRENT_SOURCE_DIR}/demo.cpp")
list(APPEND src_files "${CMAKE_CURRENT_SOURCE_DIR}/wxExplorerBrowser.cpp")
list(APPEND src_files "${CMAKE_CURRENT_SOURCE_DIR}/demo.rc")
add_executable(demo WIN32 ${src_files} )
target_link_libraries(demo ${wxWidgets_LIBRARIES} version shlwapi oleacc uxtheme)

if ( MSVC )  
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
  set(VS_STARTUP_PROJECT demo)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE=1)
  add_definitions(-D_CRT_NON_CONFORMING_SWPRINTFS=1)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS=1)
endif() 