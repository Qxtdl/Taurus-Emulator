#include <Windows.h>
#include <CommCtrl.h>
#include <stdio.h>
#include <crtdbg.h>

#include "emulate.h"
#include "windowmenu.h"
#include "resource.h"

RV32I_Core_t cpu = {0};

MSG msg = {0};
WindowMenu_t WindowMenus = {0};
EmuCmd_t Cmd = {0};

_Success_(return != 0)
INT ReadBinary
(
    _Out_writes_bytes_(dest_size) PCHAR restrict dest, 
    _In_ SIZE_T dest_size
)
{
    _ASSERTE(dest != NULL);
    _ASSERTE(dest_size > 0);

    CHAR filename[MAX_PATH] = "";

    OPENFILENAMEA ofn = {0};
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.lpstrFile = filename;
    ofn.nMaxFile = sizeof filename;
    ofn.lpstrFilter = "All Files\0*.*\0Binary Files\0*.bin*\0\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    
    if (!GetOpenFileNameA(&ofn))
        return 0;

    FILE *bin = fopen(filename, "rb");
    if (!bin) return 0;

    fseek(bin, 0, SEEK_END);
    LONG size = ftell(bin);

    rewind(bin);

    PCHAR buffer = (PCHAR)HeapAlloc(GetProcessHeap(), 0, size);
    if (!buffer) 
    {
        fclose(bin);
        return 0;
    }

    SIZE_T read_bytes = fread(dest, 1, dest_size, bin);
    fclose(bin);
    if (read_bytes != size) 
    {
        LOG("You are trying to read too big of a file. Limit is %zu bytes, you tried %zu bytes.", sizeof cpu.memory, size);
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }

    return 1;
}

WNDPROC g_pfnOldWndProc;

LRESULT CALLBACK OutputWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_KEYDOWN:
            if (wParam == VK_RETURN)
            {
                GetWindowTextA(WindowMenus.CommandBar, Cmd.cmd, sizeof Cmd.cmd);
                Cmd.commandPending = TRUE;
            }
            break;
        default:
            return CallWindowProc(g_pfnOldWndProc, hWnd, uMsg, wParam, lParam);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
        {
            WindowMenus.Output = CreateWindowEx(
                WS_EX_CLIENTEDGE,
                TEXT("EDIT"),
                TEXT(""),
                WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
                10, 10, 800, 500,
                hWnd,
                (HMENU)OUTPUT_MENU_ID,
                GetModuleHandle(NULL),
                NULL
            );
            if (WindowMenus.Output == NULL)
                LOG_ERROR("Failed to create Output window");
            if (WindowMenus.Output != NULL)
                SendMessage(WindowMenus.Output, EM_LIMITTEXT, (WPARAM)MAXLONG, 0);
            else
                LOG_ERROR("WindowMenus.Output is NULL, cannot set EM_LIMITTEXT");

            WindowMenus.DebugContinue = CreateWindowEx(
                WS_EX_CLIENTEDGE,
                TEXT("BUTTON"),
                TEXT("DEBUG CONTINUE"),
                WS_CHILD | WS_VISIBLE,
                10, 525, 200, 50,
                hWnd,
                (HMENU)DEBUG_CONTINUE_BUTTON_ID,
                GetModuleHandle(NULL),
                NULL
            );
            if (WindowMenus.DebugContinue == NULL)
                LOG_ERROR("Failed to create debug continue button");

            WindowMenus.CommandBar = CreateWindowEx(
                WS_EX_CLIENTEDGE,
                TEXT("EDIT"),
                TEXT("COMMAND BAR (try me)"),
                WS_CHILD | WS_VISIBLE | WS_OVERLAPPED | WS_TABSTOP,
                40 + 200, 525, 200, 50,
                hWnd,
                (HMENU)COMMAND_BAR_ID,
                GetModuleHandle(NULL),
                NULL
            );
            if (WindowMenus.CommandBar == NULL)
                LOG_ERROR("Failed to create command bar");

            if (WindowMenus.CommandBar != NULL)
                g_pfnOldWndProc = (WNDPROC)SetWindowLongPtr(WindowMenus.CommandBar, GWLP_WNDPROC, (LONG_PTR)OutputWndProc);
            else
                LOG_ERROR("WindowMenus.CommandBar is NULL");
            break;
        }
        case WM_COMMAND:
            HWND childHwnd = (HWND)lParam;
            MenuID_t buttonNo = LOWORD(wParam);
            INT notification = HIWORD(wParam);

            if (buttonNo == DEBUG_CONTINUE_BUTTON_ID && notification == BN_CLICKED)
                WindowMenus.DebugContinuePressed = TRUE;
            break;
        case WM_LBUTTONDOWN:
        {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(hWnd, &pt);

            if (ChildWindowFromPoint(hWnd, pt) == hWnd);
                SetFocus(hWnd);
            break;
        }
        case WM_CLOSE:
            DestroyWindow(hWnd);
            exit(0);
            break;
        case WM_DESTROY:
            PostQuitMessage(0); break;
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
}

volatile bool g_fWndProcWorkerSetupDone = FALSE;

DWORD WINAPI WndProcWorker(_In_ HINSTANCE *hInst)
{
    const WCHAR name[] = L"RISC-V Emulator 9000";

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = *hInst;
    wc.hbrBackground = CreateSolidBrush(RGB(120, 120, 100));
    wc.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));
    wc.lpszClassName = name;

    if (!RegisterClass(&wc))
        LOG("Failed to register Video Graphics Window Class");

    HWND hWnd = CreateWindow(
        wc.lpszClassName,
        name,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU,
        0, 0,
        1024, 1024,
        0, 0,
        *hInst,
        NULL
    );

    g_fWndProcWorkerSetupDone = TRUE;

    while (1) 
        if (GetMessage(&msg, NULL, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
}

INT APIENTRY WinMain(
    _In_ HINSTANCE hInst,
    _In_opt_ HINSTANCE hInstPrev,
    _In_ PSTR cmdline,
    _In_ INT cmd_show
)
{
    HANDLE hThread = CreateThread(
        NULL,
        0,
        WndProcWorker,
        &hInst,
        0,
        NULL
    );
    if (!hThread)
        LOG_ERROR("Failed to create thread for WndProcWorker");

    while (!ReadBinary(cpu.memory, sizeof cpu.memory))
        LOG_ERROR("Failed to read RAM snapshot. Retry? Windows error code = 0x%X", GetLastError());

    LOG("Emulation Started");
	Emulate();
}

void Emulate()
{
	while (1)
	{
		emulate_core_cycle();
	}
}
