#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "globals.h"
#include "emulate.h"
#include "windowmenu.h"

extern RV32I_Core_t cpu;
extern WindowMenu_t WindowMenus;
extern EmuCmd_t Cmd;

extern _Success_(return != 0)
INT ReadBinary
(
    _Out_writes_bytes_(dest_size) PCHAR restrict dest,
    _In_ SIZE_T dest_size
);

static uint32_t fetch_instruction() 
{
    return cpu.memory[cpu.pc] |
        cpu.memory[cpu.pc + 1] << 8 |
        cpu.memory[cpu.pc + 2] << 16 |
        cpu.memory[cpu.pc + 3] << 24;
}

inline static void set_pc_imm(uint16_t value)
{
    cpu.pc = value & 0xFFFFFFFC;
}

inline static void set_pc(uint16_t value)
{
    cpu.pc = (value + cpu.pc) - 4 & 0xFFFFFFFC;
}

static void add_inst(uint8_t rd, uint32_t rs1, uint32_t s2)
{
    cpu.regs[rd] = rs1 + s2;
}

static void sub_inst(uint8_t rd, uint32_t rs1, uint32_t s2)
{
    cpu.regs[rd] = rs1 - s2;
}

static void sll_inst(uint8_t rd, uint32_t rs1, uint32_t shamt)
{
    cpu.regs[rd] = rs1 << shamt;
}

static void slt_inst(uint8_t rd, int32_t rs1, int32_t s2)
{
    cpu.regs[rd] = rs1 < s2 ? 1 : 0;
}

static void sltu_inst(uint8_t rd, uint32_t rs1, uint32_t s2)
{
    cpu.regs[rd] = rs1 < s2 ? 1 : 0;
}

static void xor_inst(uint8_t rd, uint32_t rs1, uint32_t s2)
{
    cpu.regs[rd] = rs1 ^ s2;
}

static void srl_inst(uint8_t rd, uint32_t rs1, uint32_t shamt)
{
    cpu.regs[rd] = rs1 >> shamt;
}

static void sra_inst(uint8_t rd, int32_t rs1, uint32_t shamt)
{
    cpu.regs[rd] = rs1 >> shamt | (rs1 < 0 ? (~0U << (32 - shamt)) : 0);
}

static void or_inst(uint8_t rd, uint32_t rs1, uint32_t s2)
{
    cpu.regs[rd] = rs1 | s2;
}

static void and_inst(uint8_t rd, uint32_t rs1, uint32_t s2)
{
    cpu.regs[rd] = rs1 & s2;
}

static void arith_inst(uint8_t funct3, 
                       uint8_t funct7, uint8_t rd, 
                       uint8_t rs1, uint32_t s2, bool is_arith)
{
    switch (funct3)
    {
        case 0:                                                 // ADDITION/SUBTRACTION
            if (is_arith && !(funct7 & 0x20) || !is_arith)
                add_inst(rd, cpu.regs[rs1], s2);
            else
                sub_inst(rd, cpu.regs[rs1], s2);
            break;
        case 1:                                                 // SLL (SHIFT LEFT LOGICAL)
            sll_inst(rd, cpu.regs[rs1], s2); break;
        case 2:                                                 // SLT (SET LESS THEN)
            slt_inst(rd, cpu.regs[rs1], s2); break;
        case 3:                                                 // SLTU (SET LESS THEN UNSIGNED)
            sltu_inst(rd, cpu.regs[rs1], s2); break;
        case 4:                                                 // XOR
            xor_inst(rd, cpu.regs[rs1], s2); break;
        case 5:                                                 // SLR (SHIFT RIGHT LOGICAL)/SRA (SHIFT RIGHT ARITHMETIC)
            if (is_arith && !(funct7 & 0x20) || !is_arith)
                srl_inst(rd, cpu.regs[rs1], cpu.regs[rs1]);
            else
                sra_inst(rd, cpu.regs[rs1], s2);
            break;
        case 6:                                                 // OR
            or_inst(rd, cpu.regs[rs1], s2); break;
        case 7:                                                 // AND
            and_inst(rd, cpu.regs[rs1], s2); break;
    }
}

static void load_inst(uint8_t funct3, uint8_t rd, uint8_t rs1, uint32_t offset)
{
    switch (funct3) 
    {
        case 0:                                                                     // lb (load byte)
            cpu.regs[rd] = (int32_t)(int16_t)cpu.memory[cpu.regs[rs1] + offset]; break;
        case 1:                                                                     // lh (load half)
            if ((cpu.regs[rs1] + offset) & 0x1)
                LOG("Misaligned load half %X at %X", cpu.regs[rs1] + offset, cpu.pc);
            cpu.regs[rd] = (int32_t)(cpu.memory[cpu.regs[rs1] + offset] |
                cpu.memory[cpu.regs[rs1] + offset + 1] << 8);
            break;
        case 2:                                                                     // lw (load word)
            if ((cpu.regs[rs1] + offset) & 0x3)
                LOG("Misaligned load word %X at %X", cpu.regs[rs1] + offset, cpu.pc);
            cpu.regs[rd] = cpu.memory[cpu.regs[rs1] + offset] |
                cpu.memory[cpu.regs[rs1] + offset + 1] << 8 |
                cpu.memory[cpu.regs[rs1] + offset + 2] << 16 |
                cpu.memory[cpu.regs[rs1] + offset + 3] << 24;
            break;
        case 4:                                                                     // lbu (load byte unsigned)
            cpu.regs[rd] = cpu.memory[cpu.regs[rs1] + offset]; break;
        case 5:                                                                     // lhu (load half unsigned)
            if ((cpu.regs[rs1] + offset) & 0x1)
                LOG("Misaligned load unsigned half %X at %X", cpu.regs[rs1] + offset, cpu.pc);
            cpu.regs[rd] = cpu.memory[cpu.regs[rs1] + offset] |
                cpu.memory[cpu.regs[rs1] + offset + 1] << 8;
            break;
        default:
            LOG("Unrecognized load op %X from address %X at %X", funct3, cpu.regs[rs1] + offset, cpu.pc);
            break;
    }
}

static void store_inst(uint8_t funct3, uint8_t rs1, uint8_t rs2, uint32_t offset)
{
    //if (cpu.regs[rs1] + offset <
    switch (funct3)
    {
        case 0:                                                                         // sb (store byte)
            cpu.memory[cpu.regs[rs1] + offset] = cpu.regs[rs2]; break;
        case 1:                                                                         // sh (store half)
            if ((cpu.regs[rs1] + offset) & 0x1)
                LOG("Misaligned store half %X at %X" "\n", cpu.regs[rs1] + offset, cpu.pc);
            cpu.memory[cpu.regs[rs1] + offset] = cpu.regs[rs2];
            cpu.memory[cpu.regs[rs1] + offset + 1] = cpu.regs[rs2] >> 8;
            break;
        case 2:                                                                         // sw (store word)
            if ((cpu.regs[rs1] + offset) & 0x3)
                LOG("Misaligned store word %X at %X", cpu.regs[rs1] + offset, cpu.pc);
            cpu.memory[cpu.regs[rs1] + offset] = cpu.regs[rs2];
            cpu.memory[cpu.regs[rs1] + offset + 1] = cpu.regs[rs2] >> 8;
            cpu.memory[cpu.regs[rs1] + offset + 2] = cpu.regs[rs2] >> 16;
            cpu.memory[cpu.regs[rs1] + offset + 3] = cpu.regs[rs2] >> 24;
            break;
        default:
            LOG("Unrecognized store op %X to address %X at %X", funct3, cpu.regs[rs1] + offset, cpu.pc);
    }
}

static void jal_inst(uint8_t rd, uint32_t imm)
{
    if (rd) cpu.regs[rd] = cpu.pc + 4;
    set_pc(imm);
}

static void jalr_inst(uint8_t rd, uint8_t rs1, uint32_t imm)
{
    if (rd) cpu.regs[rd] = cpu.pc + 4;
    set_pc(cpu.regs[rs1] + imm);
}

static void lui_inst(uint8_t rd, uint32_t imm)
{
    cpu.regs[rd] = imm;
}

static void auipc_inst(uint8_t rd, uint32_t imm)
{
    cpu.regs[rd] = cpu.pc + imm;
}

static void branch_inst(uint8_t funct3, uint8_t rs1, uint8_t rs2, uint32_t imm)
{
    switch (funct3)
    {
        case 0:                                                     // beq
            if (cpu.regs[rs1] == cpu.regs[rs2])
                set_pc(imm); 
            break;
        case 1:                                                     // bne
            if (cpu.regs[rs1] != cpu.regs[rs2])
                set_pc(imm); 
            break;
        case 4:                                                     // blt
            if ((int32_t)cpu.regs[rs1] < (int32_t)cpu.regs[rs2])
                set_pc(imm); 
            break;
        case 5:                                                     // bge
            if ((int32_t)cpu.regs[rs1] > (int32_t)cpu.regs[rs2])
                set_pc(imm); 
            break;
        case 6:                                                     // bltu
            if (cpu.regs[rs1] < cpu.regs[rs2])
                set_pc(imm); 
            break;
        case 7:                                                     // bgeu
            if (cpu.regs[rs1] > cpu.regs[rs2])
                set_pc(imm); 
            break;
    }
}

static int32_t I_immgen(uint32_t instruction)
{
    int32_t imm = instruction >> 20 & 0xFFF;
    if (imm & 0x800)
        imm |= 0xFFFFF000;
    return imm;
}


static uint32_t S_immgen(uint32_t instruction, uint32_t helper0)
{
    uint32_t imm = helper0 |
        instruction >> 20 & 0xFFFFF000;
    if (instruction & 0x80000000)
        imm |= 0xFFFFF000;
    return imm;
}

static uint32_t J_immgen(uint32_t instruction)
{   
    uint32_t imm2 = instruction >> 20 & 0x7FF;
    uint32_t imm = instruction >> 20 & 0x7FF | instruction & 0xFF700;
    if (instruction & 0x80000000)
        imm |= 0xFFF00000;
    if (instruction & 0x00100000)
        imm |= 0x800;
    return imm;
}

static uint32_t U_immgen(uint32_t instruction)
{
    return instruction & 0xFFFFF000;
}

static uint32_t B_immgen(uint32_t instruction)
{
    uint32_t imm = (instruction >> 31 & 0x1) << 12 |
        (instruction >> 25 & 0x3F) << 5 |
        (instruction >> 8 & 0xF) << 1 |
        (instruction >> 7 & 0x1) << 11;

    if (imm & 0x1000)
        imm |= 0xFFFFE000;

    return imm;
}

bool canRun = true;

void ProcessPendingCmd()
{
    if (!Cmd.commandPending) return;
    
    if (strcmp(Cmd.cmd, "help") == 0)
        LOG("Available Commands: \r\nhelp Show this message\r\nhold Toggle running the hart\r\n"
            "setpc <value> Set the PC to value\r\nldram Load Ram, as-well as reset CPU struct to 0\r\n"
            "dump Dump all regs\r\nmemdump <addr0> <addr1> Mem dump from addr0 to addr1\r\ncls Clear Screen"
        );
    if (strncmp(Cmd.cmd, "setpc ", 6) == 0)
    {
        set_pc_imm(strtol(Cmd.cmd + 6, NULL, 16));
        LOG("CPU PC = %X", cpu.pc);
    }
    if (strncmp(Cmd.cmd, "memdump ", 8) == 0)
    {
        uint16_t addr = strtol(Cmd.cmd + 8, NULL, 16);
        uint16_t addr2 = strtol(Cmd.cmd + 8 + 4 + 1, NULL, 16);
        for (int i = 0;;i++)
        {
            LOG("%X : %X", addr, cpu.memory[addr]);
            if (addr++ == addr2)
                break;
        }
    }
    if (strcmp(Cmd.cmd, "hold") == 0) 
    {
        canRun = !canRun;
        LOG("canRun = %s", canRun ? "true" : "false");
    }
    if (strcmp(Cmd.cmd, "ldram") == 0)
    {
        ZeroMemory(&cpu, sizeof cpu);
        while (!ReadBinary(cpu.memory, sizeof cpu.memory))
            LOG_ERROR("Failed to read RAM snapshot. Retry? Windows error code = 0x%X", GetLastError());
    }
    if (strcmp(Cmd.cmd, "dump") == 0)
    {
        LOG("Dumping CPU Registers");
        for (int i = 0; i < _countof(cpu.regs); i++)
            LOG2("x%d : %X\r\n", i, cpu.regs[i]);
        LOG2("pc : %X\r\n", cpu.pc);
        LOG2("----------------\r\n");
    }
    if (strcmp(Cmd.cmd, "cls") == 0)
        SendMessageA(WindowMenus.Output, WM_SETTEXT, 0, (LPARAM)"");

    Cmd.commandPending = false;
}

void emulate_core_cycle()
{
    ProcessPendingCmd();

    if (!canRun) return;

    uint32_t instruction = fetch_instruction();
    uint8_t opcode = instruction & 0x7F;
    uint8_t funct3 = (instruction >> 12) & 0x7;
    uint8_t funct7 = (instruction >> 25) & 0x3F;
    uint8_t rd = (instruction >> 7) & 0x1F;
    uint8_t rs1 = (instruction >> 15) & 0x1F;
    uint8_t rs2 = (instruction >> 20) & 0x1F;

    switch (opcode)
    {
        case 0b0110011: // ARITH TYPE
            if (rd == 0) break;
            arith_inst(funct3, funct7, rd, rs1, cpu.regs[rs2], true); break;
        case 0b0010011: // ARITH-I TYPE
            if (rd == 0) break;
            arith_inst(funct3, funct7, rd, rs1, I_immgen(instruction), false); break;
        case 0b0000011: // LOAD
            if (rd == 0) break;
            load_inst(funct3, rd, rs1, I_immgen(instruction)); break;
        case 0b0100011: // STORE
            store_inst(funct3, rs1, rs2, S_immgen(instruction, rd)); break;
        case 0b1100011: // BRANCH
            branch_inst(funct3, rs1, rs2, B_immgen(instruction)); break;
        case 0b1101111: // JAL
            jal_inst(rd, J_immgen(instruction)); break;
        case 0b1100111: // JALR
            jalr_inst(rd, rs1, I_immgen(instruction)); break;
        case 0b0110111: // LUI
            lui_inst(rd, U_immgen(instruction)); break;
        case 0b0010111: // AUIPC
            auipc_inst(rd, U_immgen(instruction));
            break;
        case 0b1110011: // CUSTOM EBREAK
            LOG("EBREAK hit at %X\r\n----------------", cpu.pc);
            for (int i = 0; i < _countof(cpu.regs); i++)
                LOG2("x%d : %X\r\n", i, cpu.regs[i]);
            LOG2("----------------\r\n");

            while (!WindowMenus.DebugContinuePressed);
            WindowMenus.DebugContinuePressed = false;
            break;
        default:        // NOP
            LOG("Unknown opcode %X at %X", opcode, cpu.pc); break;
    }

    cpu.pc+= 4;
}
