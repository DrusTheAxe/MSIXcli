// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

// C/C++ Library
#include <fcntl.h>
#include <io.h>
#include <tuple>

// Win32 / Shell
#include <windows.h>
#include <unknwn.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <prsht.h>
#include <intshcut.h>
#include <propsys.h>
#include <propkey.h>
#include <propvarutil.h>
#include <shidfact.h>
#include <commctrl.h>
#include <strsafe.h>
#include <AppxPackaging.h>

// Windows Runtime (ABI projection)
#include <roapi.h>
#include <winstring.h>
#include <windows.management.deployment.h>

// Windows Implementation Library (WIL)
#include <wil/com.h>
#include <wil/filesystem.h>
#include <wil/registry.h>
#include <wil/resource.h>
#include <wil/result.h>
#include <wil/winrt.h>

#include <MSIXDeployment.h>
#include <MSIXPackaging.h>
#include <MSIXSigning.h>
#include <wil_array.h>
#include <wil_extension.h>
#include <wil_winrt_vector.h>
