// Copyright (c) Howard Kapustein
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

// Win32 / Shell
#include <windows.h>
#include <unknwn.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <prsht.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <strsafe.h>
#include <AppxPackaging.h>

// Windows Runtime (ABI projection)
#include <roapi.h>
#include <winstring.h>
#include <windows.management.deployment.h>

// Windows Implementation Library (WIL)
#include <wil/com.h>
#include <wil/registry.h>
#include <wil/resource.h>
#include <wil/result.h>
#include <wil/token_helpers.h>
#include <wil/win32_helpers.h>
#include <wil/winrt.h>

#include <MSIXDeployment.h>
#include <MSIXPackaging.h>
#include <MSIXSigning.h>
#include <msixcli.h>
#include <wil_extension.h>

#include "MSIXPackage.h"
