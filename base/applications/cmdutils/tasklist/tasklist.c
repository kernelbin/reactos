/*
 * PROJECT:     ReactOS Tasklist Command
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     Displays a list of currently running processes on the computer.
 * COPYRIGHT:   Copyright 2020 He Yang (1160386205@qq.com)
 */

#include <stdio.h>
#include <stdlib.h>

#define WIN32_NO_STATUS
#include <windows.h>
#include <ndk/psfuncs.h>

#include <conutils.h>

#include "resource.h"


#define NT_SYSTEM_QUERY_MAX_RETRY 5


#define COLUMNWIDTH_IMAGENAME     25
#define COLUMNWIDTH_PID           8
#define COLUMNWIDTH_SESSION       11
#define COLUMNWIDTH_MEMUSAGE      12


#define HEADER_STR_MAXLEN 64


static WCHAR opHelp[] = L"?";
static WCHAR opVerbose[] = L"v";
static PWCHAR opList[] = {opHelp, opVerbose};


#define OP_PARAM_INVALID -1

#define OP_PARAM_HELP 0
#define OP_PARAM_VERBOSE 1


typedef NTSTATUS (NTAPI *NT_QUERY_SYSTEM_INFORMATION) (
    SYSTEM_INFORMATION_CLASS SystemInformationClass,
    PVOID SystemInformation,
    ULONG InformationLength,
    PULONG ResultLength);

VOID PrintSplitLine(int Length)
{
    for (int i = 0; i < Length; i++)
    {
        ConPrintf(StdOut, L"=");
    }
}

VOID PrintHeader()
{
    WCHAR lpstrImageName[HEADER_STR_MAXLEN];
    WCHAR lpstrPID[HEADER_STR_MAXLEN];
    WCHAR lpstrSession[HEADER_STR_MAXLEN];
    WCHAR lpstrMemUsage[HEADER_STR_MAXLEN];

    LoadStringW(GetModuleHandle(NULL), IDS_HEADER_IMAGENAME, lpstrImageName, HEADER_STR_MAXLEN);
    LoadStringW(GetModuleHandle(NULL), IDS_HEADER_PID, lpstrPID, HEADER_STR_MAXLEN);
    LoadStringW(GetModuleHandle(NULL), IDS_HEADER_SESSION, lpstrSession, HEADER_STR_MAXLEN);
    LoadStringW(GetModuleHandle(NULL), IDS_HEADER_MEMUSAGE, lpstrMemUsage, HEADER_STR_MAXLEN);

    ConPrintf(
        StdOut, L"%-*.*ls %*.*ls %*.*ls %*.*ls",
        COLUMNWIDTH_IMAGENAME, COLUMNWIDTH_IMAGENAME, lpstrImageName,
        COLUMNWIDTH_PID, COLUMNWIDTH_PID, lpstrPID,
        COLUMNWIDTH_SESSION, COLUMNWIDTH_SESSION, lpstrSession,
        COLUMNWIDTH_MEMUSAGE, COLUMNWIDTH_MEMUSAGE, lpstrMemUsage);

    ConPrintf(StdOut, L"\n");

    PrintSplitLine(COLUMNWIDTH_IMAGENAME);
    ConPrintf(StdOut, L" ");
    PrintSplitLine(COLUMNWIDTH_PID);
    ConPrintf(StdOut, L" ");
    PrintSplitLine(COLUMNWIDTH_SESSION);
    ConPrintf(StdOut, L" ");
    PrintSplitLine(COLUMNWIDTH_MEMUSAGE);

    ConPrintf(StdOut, L"\n");
}

BOOL EnumProcessAndPrint(BOOL bVerbose)
{
    // Load ntdll.dll in order to use NtQuerySystemInformation
    HMODULE hNtDLL = LoadLibraryW(L"Ntdll.dll");
    if (!hNtDLL)
    {
        ConResMsgPrintf(StdOut, 0, IDS_ENUM_FAILED);
        return FALSE;
    }

    NT_QUERY_SYSTEM_INFORMATION PtrNtQuerySystemInformation =
        (NT_QUERY_SYSTEM_INFORMATION)GetProcAddress(hNtDLL, "NtQuerySystemInformation");

    if (!PtrNtQuerySystemInformation)
    {
        ConResMsgPrintf(StdOut, 0, IDS_ENUM_FAILED);
        FreeLibrary(hNtDLL);
        return FALSE;
    }

    // call NtQuerySystemInformation for the process information
    ULONG ProcessInfoBufferLength = 0;
    ULONG ResultLength = 0;
    PBYTE ProcessInfoBuffer = 0;

    // Get the buffer size we need
    NTSTATUS Status = PtrNtQuerySystemInformation(SystemProcessInformation, NULL, 0, &ResultLength);

    for (int Retry = 0; Retry < NT_SYSTEM_QUERY_MAX_RETRY; Retry++)
    {
        // (Re)allocate buffer
        ProcessInfoBufferLength = ResultLength;
        ResultLength = 0;
        if (ProcessInfoBuffer)
        {
            PBYTE NewProcessInfoBuffer = HeapReAlloc(GetProcessHeap(), 0, ProcessInfoBuffer, ProcessInfoBufferLength);
            if (NewProcessInfoBuffer)
            {
                ProcessInfoBuffer = NewProcessInfoBuffer;
            }
            else
            {
                // out of memory ?
                ConResMsgPrintf(StdOut, 0, IDS_OUT_OF_MEMORY);
                HeapFree(GetProcessHeap(), 0, ProcessInfoBuffer);
                FreeLibrary(hNtDLL);
                return FALSE;
            }
        }
        else
        {
            ProcessInfoBuffer = HeapAlloc(GetProcessHeap(), 0, ProcessInfoBufferLength);
            if (!ProcessInfoBuffer)
            {
                // out of memory ?
                ConResMsgPrintf(StdOut, 0, IDS_OUT_OF_MEMORY);
                FreeLibrary(hNtDLL);
                return FALSE;
            }
        }

        // Query information
        Status = PtrNtQuerySystemInformation(
            SystemProcessInformation, ProcessInfoBuffer, ProcessInfoBufferLength, &ResultLength);

        if (Status != STATUS_INFO_LENGTH_MISMATCH)
        {
            break;
        }
    }

    if (!NT_SUCCESS(Status))
    {
        // tried NT_SYSTEM_QUERY_MAX_RETRY times, or failed with some other reason
        ConResMsgPrintf(StdOut, 0, IDS_ENUM_FAILED);
        HeapFree(GetProcessHeap(), 0, ProcessInfoBuffer);
        FreeLibrary(hNtDLL);
        return FALSE;
    }

    // print header
    PrintHeader();

    PSYSTEM_PROCESS_INFORMATION pSPI;
    pSPI = (PSYSTEM_PROCESS_INFORMATION)ProcessInfoBuffer;
    while (pSPI)
    {
        // TODO: refactor code printing information
        ConPrintf(
            StdOut, L"%-*.*ls %*d %*d %*d K\n",
            COLUMNWIDTH_IMAGENAME, COLUMNWIDTH_IMAGENAME, pSPI->ImageName.Buffer,
            COLUMNWIDTH_PID, pSPI->UniqueProcessId,
            COLUMNWIDTH_SESSION, pSPI->SessionId,
            COLUMNWIDTH_MEMUSAGE - 2, pSPI->WorkingSetSize / 1024);
        if (pSPI->NextEntryOffset == 0)
            break;
        pSPI = (PSYSTEM_PROCESS_INFORMATION)((LPBYTE)pSPI + pSPI->NextEntryOffset);
    }

    HeapFree(GetProcessHeap(), 0, ProcessInfoBuffer);
    return TRUE;
}

int GetArgumentType(WCHAR* argument)
{
    int i;

    if (argument[0] != L'/' && argument[0] != L'-')
    {
        return OP_PARAM_INVALID;
    }
    argument++;

    for (i = 0; i < _countof(opList); i++)
    {
        if (!wcsicmp(opList[i], argument))
        {
            return i;
        }
    }
    return OP_PARAM_INVALID;
}

BOOL ProcessArguments(int argc, WCHAR *argv[])
{
    int i;
    BOOL bHasHelp = FALSE, bHasVerbose = FALSE;
    for (i = 1; i < argc; i++)
    {
        int Argument = GetArgumentType(argv[i]);

        switch (Argument)
        {
        case OP_PARAM_HELP:
        {
            if (bHasHelp)
            {
                // -? already specified
                ConResMsgPrintf(StdOut, 0, IDS_PARAM_TOO_MUCH, argv[i], 1);
                ConResMsgPrintf(StdOut, 0, IDS_USAGE);
                return FALSE;
            }
            bHasHelp = TRUE;
            break;
        }
        case OP_PARAM_VERBOSE:
        {
            if (bHasVerbose)
            {
                // -V already specified
                ConResMsgPrintf(StdOut, 0, IDS_PARAM_TOO_MUCH, argv[i], 1);
                ConResMsgPrintf(StdOut, 0, IDS_USAGE);
                return FALSE;
            }
            bHasVerbose = TRUE;
            break;
        }
        case OP_PARAM_INVALID:
        default:
        {
            ConResMsgPrintf(StdOut, 0, IDS_INVALID_OPTION);
            ConResMsgPrintf(StdOut, 0, IDS_USAGE);
            return FALSE;
        }
        }
    }

    if (bHasHelp)
    {
        if (argc > 2) // any parameters other than -? is specified
        {
            ConResMsgPrintf(StdOut, 0, IDS_INVALID_SYNTAX);
            ConResMsgPrintf(StdOut, 0, IDS_USAGE);
            return FALSE;
        }
        else
        {
            ConResMsgPrintf(StdOut, 0, IDS_USAGE);
            ConResMsgPrintf(StdOut, 0, IDS_DESCRIPTION);
            exit(0);
        }
    }
    else
    {
        EnumProcessAndPrint(bHasVerbose);
    }
    return TRUE;
}

int wmain(int argc, WCHAR *argv[])
{
    /* Initialize the Console Standard Streams */
    ConInitStdStreams();

    if(!ProcessArguments(argc, argv))
    {
        return 1;
    }
    return 0;
}
