#pragma once

#include <Windows.h>

#include "util.h"

VOID LOG(_In_z_ PCSTR restrict text, ...);
VOID LOG2(_In_z_ PCSTR restrict text, ...);
VOID LOG_ERROR(_In_z_ PCSTR restrict text, ...);
