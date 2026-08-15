// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

// Win32 / Shell
#include <Windows.h>
#include <unknwn.h>
#include <commctrl.h>
#include <shellapi.h>
#include <strsafe.h>
#include <AppxPackaging.h>

// Windows Runtime (ABI projection)
#include <roapi.h>
#include <winstring.h>
#include <windows.management.deployment.h>

#include <wil/resource.h>
#include <wil/result.h>
#include <wil/winrt.h>

#include <wil_extension.h>
