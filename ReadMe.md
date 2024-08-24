wxExplorerBrowser
=========

Introduction
---------

wxExplorerBrowser is a wxWidgets control hosting [IExplorerBrowser](https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-iexplorerbrowser). 

![wxExplorerBrowserDemo Screenshot](screenshots/wxExplorerBrowserDemo.png?raw=true)

wxExplorerBrowser does not support all the features of IExplorerBrowser and may have some issues, see the comment for wxExplorerBrowserImplHelper::_SetFilter() in the source code. 
It probably is not ready for production use but perhaps it could serve as an inspiration.

Requirements
---------

IExplorerBrowser interface is available only on Windows Vista and newer. 

To build wxExplorerBrowser, wxWidgets v3.2 or newer and a compiler supporting C++11 with Windows Vista API.

Tested with MSVC 2017, 2019, 2022; and GCC (MSYS2 package mingw-w64-ucrt-x86_64).

Using
---------

See wxExplorerBrowser.h for documentation and demo.cpp showing some of the features of wxExplorerBrowser in action.

Licence
---------

[wxWidgets licence](https://github.com/wxWidgets/wxWidgets/blob/master/docs/licence.txt) 
