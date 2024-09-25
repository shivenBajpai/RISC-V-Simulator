#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/timeb.h>
#include <bits/types/struct_timeb.h>
#include <string.h>
#include "../globals.h"
#include "../assembler/vec.h"
#include "../frontend/frontend.h"
#include "backend.h"

# define RUN_DELAY 125

enum Opcode {
    R_Type      = 0b0110011,
    I_Type      = 0b0010011,
    Load_Type   = 0b0000011,
    S_Type      = 0b0100011,
    B_Type      = 0b1100011,
    JAL         = 0b1101111,
    JALR        = 0b1100111,
    LUI         = 0b0110111,
    AUIPC       = 0b0010111,
    EBREAK      = 0b1110011,
    NOP         = 0, // Made up marker
};

enum Instruction_Constants{
    add = 0b0110011,
    sub = 0b0110011+(0x20<<25),
    xor = 0b0110011+(0x4<<12),
    or = 0b0110011+(0x6<<12),
    and = 0b0110011+(0x7<<12),
    sll = 0b0110011+(0x1<<12),
    srl = 0b0110011+(0x5<<12),
    sra = 0b0110011+(0x5<<12)+(0x20<<25),
    slt = 0b0110011+(0x2<<12),
    sltu = 0b0110011+(0x3<<12),
    addi = 0b0010011+(0x0<<12),
    xori = 0b0010011+(0x4<<12),
    ori = 0b0010011+(0x6<<12),
    andi = 0b0010011+(0x7<<12),
    slli = 0b0010011+(0x1<<12)+(0x00<<26),
    srli = 0b0010011+(0x5<<12)+(0x00<<26),
    srai = 0b0010011+(0x5<<12)+(0x10<<26),
    slti = 0b0010011+(0x2<<12),
    sltiu = 0b0010011+(0x3<<12),
    lb = 0b0000011+(0x0<<12),
    lh = 0b0000011+(0x1<<12),
    lw = 0b0000011+(0x2<<12),
    ld = 0b0000011+(0x3<<12),
    lbu = 0b0000011+(0x4<<12),
    lhu = 0b0000011+(0x5<<12),
    lwu = 0b0000011+(0x6<<12),
    sb = 0b0100011+(0x0<<12),
    sh = 0b0100011+(0x1<<12),
    sw = 0b0100011+(0x2<<12),
    sd = 0b0100011+(0x3<<12),
    beq = 0b1100011+(0x0<<12),
    bne = 0b1100011+(0x1<<12),
    blt = 0b1100011+(0x4<<12),
    bge = 0b1100011+(0x5<<12),
    bltu = 0b1100011+(0x6<<12),
    bgeu = 0b1100011+(0x7<<12),
    jal = 0b1101111,
    jalr = 0b1100111,
    lui = 0b0110111,
    auipc = 0b0010111,
    ecall = 0b1110011,
    ebreak = 0b1110011+(0X1<<20),
}; 

static uint64_t registers[32] = {0};
static uint64_t pc = 0;
static vec *breakpoints = NULL;
static stacktrace *stack = NULL;
static uint8_t memory[MEMORY_SIZE] = {0};
//Memory *memory;

int core_dump(FILE* text) { 
    //return memory_core_dump(memory, text, data, stack);
    if (fwrite(memory, sizeof(uint8_t), MEMORY_SIZE, text) != MEMORY_SIZE) printf("Core Dump Failed");
    return 0;
}

int load(uint8_t* bytes, size_t n) {
    if (n > MEMORY_SIZE) {
        cause = "Initialization data exceeds memory size";
        exit(1);
    }

    memcpy(memory, bytes, n);
}

uint64_t* get_register_pointer() {return &registers[0];}
uint64_t* get_pc_pointer() {return &pc;}
vec* get_breakpoints_pointer() {return breakpoints;}
uint8_t* get_memory_pointer() {return &memory[0];}
void set_stacktrace_pointer(stacktrace* stacktrace) {stack = stacktrace;}

void reset_backend() {
    memset(registers, 0, sizeof(registers));
    memset(memory, 0, sizeof(memory));
    if (breakpoints) free_managed_array(breakpoints);
    breakpoints = new_managed_array();
    pc = 0;
}

int step() {
    // Run code 
    if (pc+3 >= DATA_BASE) {
        segfault_flag = true;
        cause = "Program counter ran into data segment";
        exit(1);
    }

    uint32_t instruction = (memory[pc+3] << 24) | (memory[pc+2] << 16) | (memory[pc+1] << 8) |  memory[pc];
    uint32_t funct_op = 0;
    uint64_t imm = 0;
    uint64_t *rd = registers + ((0x00000F80 & instruction) >> 7);
    uint64_t *rs1 = registers + ((0x000F8000 & instruction) >> 15);
    uint64_t *rs2 = registers + ((0x01F00000 & instruction) >> 20);

    uint64_t data;

    switch (instruction & 0x7F) {
        case R_Type:
            funct_op = instruction & 0xFE00707F;
            set_reg_write(rd - registers);
            break;

        case I_Type:
        case JALR:
        case Load_Type:
            imm = (instruction & 0xFFF00000) >> 20;
            imm |= (imm & 0x800)?0xFFFFFFFFFFFFF000:0; // Sign Bit extension
            funct_op = instruction & 0x0000707F;
            set_reg_write(rd - registers);
            break;

        case S_Type:
            imm = ((instruction & 0xFE000000) >> 20) + ((instruction & 0x00000F80) >> 7);
            imm |= (imm & 0x800)?0xFFFFFFFFFFFFF000:0; // Sign Bit extension
            funct_op = instruction & 0x0000707F;
            break;

        case B_Type:
            imm = (((instruction & 0x80000000) >> 19) + ((instruction & 0x7E000000) >> 20) + ((instruction & 0x00000F00) >> 7) + ((instruction & 0x00000080) << 4));
            imm |= (imm & 0x1000)?0xFFFFFFFFFFFFF000:0; // Sign Bit extension
            funct_op = instruction & 0x0000707F;
            break;

        case EBREAK:
            funct_op = instruction & 0x0010007F;
            pc += 4;
            return 0;

        case JAL:
            imm = (((instruction & 0x000FF000)) + ((instruction & 0x00100000) >> 9) + ((instruction & 0x80000000) >> 11) + ((instruction & 0x7FE00000) >> 20));
            imm |= (imm & 0x100000)?0xFFFFFFFFFFF00000:0; // Sign Bit extension
            funct_op = instruction & 0x0000007F;
            set_reg_write(rd - registers);
            break;

        case LUI:
        case AUIPC:
            imm = (instruction & 0xFFFFF000) >> 12;
            funct_op = instruction & 0x0000007F;
            set_reg_write(rd - registers);
            break;

        case NOP:
            return 1;

        default:
            funct_op = instruction & 0x0000007F;
    }
    
    switch (funct_op) {
        case add:
            *rd = *rs1 + *rs2;
            break;

        case sub:
            *rd = *rs1 - *rs2;
            break;

        case xor:
            *rd = *rs1 ^ *rs2;
            break;

        case or:
            *rd = *rs1 | *rs2;
            break;

        case and:
            *rd = *rs1 & *rs2;
            break;

        case sll:
            *rd = *rs1 << *rs2;
            break;

        case srl:
            *rd = *rs1 >> *rs2;
            break;

        case sra:
            *rd = (*rs1 >> *rs2) + ~((~0) >> *rs2);
            break;

        case slt:
            *rd = (*rs1 < *rs2)?1:0;
            break;

        case sltu:
            *rd = (*rs1 < *rs2)?1:0;
            break;

        case addi:
            *rd = *rs1 + imm;
            break;

        case xori:
            *rd = *rs1 ^ imm;
            break;

        case ori:
            *rd = *rs1 | imm;
            break;

        case andi:
            *rd = *rs1 & imm;
            break;

        case slli:
            *rd = *rs1 << imm;
            break;

        case srli:
            if (imm & 0x20) *rd = (*rs1 >> imm) + ~((~0) >> imm);
            else  *rd = *rs1 >> imm;
            break;

        case slti:
            *rd = ((int64_t) *rs1 < (int64_t) imm)?1:0;
            break;

        case sltiu:
            *rd = (*rs1 < imm)?1:0;
            break;

        case lb:
            data = *(memory + *rs1 + imm);
            if (data&0x00000008) data |= 0xFFFFFF0;
            *rd = data;
            break;

        case lh:
            data = *(uint16_t*)(memory + *rs1 + imm); // TODO: Memory safety
            if (data&0x00000080) data |= 0xFFFFF00;
            *rd = data;
            break;

        case lw:
            data = *(uint32_t*)(memory + *rs1 + imm);
            if (data&0x00008000) data |= 0xFFFFFF0;
            *rd = data;
            break;

        case ld:
            data = *(uint64_t*)(memory + *rs1 + imm);
            if (data&0x80000000) data |= 0xFFFFFF0;
            *rd = data;
            break;

        case lbu:
            *rs1 = *(memory + *rs1 + imm);
            break;

        case lhu:
            *rs1 = *(uint16_t*)(memory + *rs1 + imm);
            break;

        case lwu:
            *rs1 = *(uint32_t*)(memory + *rs1 + imm);
            break;

        case sb:
            memcpy(memory + *rs1 + imm, rs2, 1);
            break;

        case sh:
            memcpy(memory + *rs1 + imm, rs2, 2);
            break;
        
        case sw:
            memcpy(memory + *rs1 + imm, rs2, 4);
            break;
        
        case sd:
            memcpy(memory + *rs1 + imm, rs2, 8);
            break;

        case beq:
            if (*rs1 == *rs2) pc += imm-4;
            break;

        case bne:
            if (*rs1 != *rs2) pc += imm-4;
            break;
            
        case blt:
            if ((int64_t) *rs1 < (int64_t) *rs2) pc += imm-4;
            break;
            
        case bge:
            if ((int64_t) *rs1 >= (int64_t) *rs2) pc += imm-4;
            break;
            
        case bltu:
            if (*rs1 < *rs2) pc += imm-4;
            break;
            
        case bgeu:
            if (*rs1 >= *rs2) pc += imm-4;
            break;

        case jal:
            st_push(stack, 1 + pc/4);
            *rd = pc + 4;
            pc += imm - 4;
            break;

        case jalr:
            *rd = pc + 4;
            pc = *rs1 + imm - 4;
            st_pop(stack);
            break;

        case lui:
            *rd = imm << 12;
            break;

        case auipc:
            *rd = pc + (imm << 12);
            break;

        case ecall:
        case ebreak:
            break;
    }

    pc += 4;
    registers[0] = 0;

    if (memory[pc] == ebreak) {
        return 1;
    }

    for (int i=0; i<breakpoints->len; i++) {
        if (pc/4==breakpoints->values[i]) {
            return 1;
        }
    }

    return 0;
}

// Runs till ebreak or end of program
// Returns 2 if user requested termination
// Returns 1 if end of program is reached
// Returns 0 if breakpoint is reached
int run(Command (*callback)(void)) {
    static time_t next_tick = 0;
    static struct timeb time;

    while (memory[pc] != ebreak && pc < DATA_BASE) {
        do{
            if ((*callback)() == STOP) return 2;
            ftime(&time);
        } while (time.time*1000 + time.millitm < next_tick);
        next_tick = time.time*1000 + time.millitm + RUN_DELAY;

        if (step()) return (pc >= DATA_BASE || memory[pc] == NOP);
        if ((*callback)() == STOP) return 2;
    }

    return (pc >= DATA_BASE);
}