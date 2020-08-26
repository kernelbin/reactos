/*
 * PROJECT:     ReactOS Applications Manager
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     Functions to load / save settings from reg.
 * COPYRIGHT:   Copyright 2020 He Yang           (1160386205@qq.com)
 */

#include "rapps.h"
#include "settings.h"


VOID FillDefaultSettings(PSETTINGS_INFO pSettingsInfo)
{
    ATL::CStringW szDownloadDir;
    ZeroMemory(pSettingsInfo, sizeof(SETTINGS_INFO));

    pSettingsInfo->bSaveWndPos = TRUE;
    pSettingsInfo->bUpdateAtStart = FALSE;
    pSettingsInfo->bLogEnabled = TRUE;
    pSettingsInfo->bUseSource = FALSE;

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, szDownloadDir.GetBuffer(MAX_PATH))))
    {
        szDownloadDir.ReleaseBuffer();
        if (!szDownloadDir.GetEnvironmentVariableW(L"SystemDrive"))
        {
            szDownloadDir = L"C:";
        }
    }
    else
    {
        szDownloadDir.ReleaseBuffer();
    }

    PathAppendW(szDownloadDir.GetBuffer(MAX_PATH), L"\\RAPPS Downloads");
    szDownloadDir.ReleaseBuffer();

    ATL::CStringW::CopyChars(pSettingsInfo->szDownloadDir,
        _countof(pSettingsInfo->szDownloadDir),
        szDownloadDir.GetString(),
        szDownloadDir.GetLength() + 1);

    pSettingsInfo->bDelInstaller = FALSE;
    pSettingsInfo->Maximized = FALSE;
    pSettingsInfo->Left = CW_USEDEFAULT;
    pSettingsInfo->Top = CW_USEDEFAULT;
    pSettingsInfo->Width = 680;
    pSettingsInfo->Height = 450;
}

BOOL LoadSettings()
{
    ATL::CRegKey RegKey;
    DWORD dwSize;
    BOOL bResult = FALSE;
    if (RegKey.Open(HKEY_CURRENT_USER, L"Software\\ReactOS\\rapps", KEY_READ) == ERROR_SUCCESS)
    {
        dwSize = sizeof(SettingsInfo);
        bResult = (RegKey.QueryBinaryValue(L"Settings", (PVOID)&SettingsInfo, &dwSize) == ERROR_SUCCESS);

        RegKey.Close();
    }

    return bResult;
}

VOID SaveSettings(HWND hwnd)
{
    WINDOWPLACEMENT wp;
    ATL::CRegKey RegKey;

    if (SettingsInfo.bSaveWndPos)
    {
        wp.length = sizeof(wp);
        GetWindowPlacement(hwnd, &wp);

        SettingsInfo.Left = wp.rcNormalPosition.left;
        SettingsInfo.Top = wp.rcNormalPosition.top;
        SettingsInfo.Width = wp.rcNormalPosition.right - wp.rcNormalPosition.left;
        SettingsInfo.Height = wp.rcNormalPosition.bottom - wp.rcNormalPosition.top;
        SettingsInfo.Maximized = (wp.showCmd == SW_MAXIMIZE
            || (wp.showCmd == SW_SHOWMINIMIZED
                && (wp.flags & WPF_RESTORETOMAXIMIZED)));
    }

    if (RegKey.Create(HKEY_CURRENT_USER, L"Software\\ReactOS\\rapps", NULL,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, NULL) == ERROR_SUCCESS)
    {
        RegKey.SetBinaryValue(L"Settings", (const PVOID)&SettingsInfo, sizeof(SettingsInfo));
        RegKey.Close();
    }
}
