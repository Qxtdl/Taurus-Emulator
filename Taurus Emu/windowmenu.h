#pragma once

#include <Windows.h>
#include <stdbool.h>

#include "windowmenu.h"

typedef enum MenuID {
	OUTPUT_MENU_ID,
	DEBUG_CONTINUE_BUTTON_ID,
	COMMAND_BAR_ID
} MenuID_t;

typedef struct WindowMenu {
	HWND Output;
	HWND DebugContinue;
	HWND CommandBar;

	//bool CommandIssued;
	volatile bool DebugContinuePressed;
} WindowMenu_t;

