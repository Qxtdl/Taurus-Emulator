#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "globals.h"
#include "util.h"
#include "emulate.h"

typedef struct RV32I_Core {
	uint8_t memory[RAM_SIZE];
	uint32_t regs[32];
	uint16_t pc;

	// io
	uint32_t io[4];
} RV32I_Core_t;

typedef struct EmuCmd {
	char cmd[512];
	volatile bool commandPending;
} EmuCmd_t;

void Emulate();
void emulate_core_cycle();
