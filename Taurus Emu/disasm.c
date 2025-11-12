#include <stdlib.h>
#include <stdint.h>

#include "disasm.h"

int currentInst;

static uint32_t fetch_instruction(const char *mem, int currentInst)
{
    return mem[currentInst] |
        mem[currentInst + 1] << 8 |
        mem[currentInst + 2] << 16 |
        mem[currentInst + 3] << 24;
}

char *disasm(const void *mem, size_t mem_size)
{
    uint32_t instruction = fetch_instruction(mem, currentInst);
    uint8_t opcode = instruction & 0x7F;
    uint8_t funct3 = (instruction >> 12) & 0x7;
    uint8_t funct7 = (instruction >> 25) & 0x3F;
    uint8_t rd = (instruction >> 7) & 0x1F;
    uint8_t rs1 = (instruction >> 15) & 0x1F;
    uint8_t rs2 = (instruction >> 20) & 0x1F;

	switch (opcode)
	{

	}
}
