#include <Windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <crtdbg.h>

#include "util.h"
#include "windowmenu.h"

extern WindowMenu_t WindowMenus;
extern volatile bool g_fWndProcWorkerSetupDone;

VOID LOG(_In_z_ PCSTR restrict text, ...)
{
    _ASSERTE(text != NULL);

    va_list args;
    va_start(args, text);
    vprintf(text, args);

    CHAR buf[1024] = {0};
    vsprintf_s(buf, sizeof buf, text, args);
    va_end(args);
    
    while (!g_fWndProcWorkerSetupDone);
    SendMessageA(WindowMenus.Output, EM_REPLACESEL, FALSE, (LPARAM)"[LOG]: ");
    SendMessageA(WindowMenus.Output, EM_REPLACESEL, FALSE, (LPARAM)buf);
    SendMessageA(WindowMenus.Output, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
}

VOID LOG2(_In_z_ PCSTR restrict text, ...)
{
    _ASSERTE(text != NULL);

    va_list args;
    va_start(args, text);
    vprintf(text, args);

    CHAR buf[1024] = {0};
    vsprintf_s(buf, sizeof buf, text, args);
    va_end(args);

    while (!g_fWndProcWorkerSetupDone);
    SendMessageA(WindowMenus.Output, EM_REPLACESEL, FALSE, (LPARAM)buf);
}

VOID LOG_ERROR(_In_z_ PCSTR restrict text,  ...)
{
    _ASSERTE(text != NULL);

    va_list args;
    va_start(args, text);
    vprintf(text, args);

    CHAR buf[1024] = {0};
    vsprintf_s(buf, sizeof buf, text, args);
    va_end(args);

    while (!g_fWndProcWorkerSetupDone);
    SendMessageA(WindowMenus.Output, EM_REPLACESEL, FALSE, (LPARAM)"[ERROR]: ");
    SendMessageA(WindowMenus.Output, EM_REPLACESEL, FALSE, (LPARAM)buf);
    SendMessageA(WindowMenus.Output, EM_REPLACESEL, FALSE, (LPARAM)"\r\n");
}
