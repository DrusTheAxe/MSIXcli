// Copyright (C) Howard Kapustein. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#define IDC_STATIC              -1

#define IDI_MSIXMONITOR         1

#define IDD_ABOUT               100
#define IDD_ACTIVITY            101

#define IDR_TRAY_MENU           200

#define IDC_ABOUT_ICON          1000
#define IDC_ABOUT_NAME          1001
#define IDC_ABOUT_DESCRIPTION   1002
#define IDC_ABOUT_COPYRIGHT     1003
#define IDC_HOME                1004
#if defined(RC_INVOKED)
#define IDC_HOME_LABEL  "<a href=""https://github.com/drustheaxe/MSIXcli"">MSIXcli home</a>"
#endif

#define IDC_ACTIVITY_LIST       2000

#define IDM_TRAY_ABOUT          40001
#define IDM_TRAY_ACTIVITY       40002
#define IDM_TRAY_ENABLE_MONITOR 40003
#define IDM_TRAY_EXIT           40004
