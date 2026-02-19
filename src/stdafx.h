#pragma once

#define _ALLOW_RTCc_IN_STL
#define _SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING
#define _WINSOCKAPI_
#define NOMINMAX

#include <memory>

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <objbase.h>
#include <unknwn.h>
#include <shellapi.h>
#include <commctrl.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <gdiplus.h>
#include <atlbase.h>
#include <atlwin.h>
#include <shlwapi.h>
#include <ole2.h>
#include <winhttp.h>
#include <commdlg.h>
#include <shlobj.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shlwapi.lib")

using namespace Gdiplus;

#include "columns_ui/foobar2000/SDK/foobar2000.h"
#include "columns_ui/foobar2000/SDK/coreDarkMode.h"

#include "resource.h"
