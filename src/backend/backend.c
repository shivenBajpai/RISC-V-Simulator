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
#include "memory.h"

# define RUN_DELAY 200

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
    NOP         = 0, // Made up marker, Used to identify end of code.
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
    mul = 0b0110011+(0x0<<12)+(0x01<<25),
    mulh = 0b0110011+(0x1<<12)+(0x01<<25),
    mulsu = 0b0110011+(0x2<<12)+(0x01<<25),
    mulu = 0b0110011+(0x3<<12)+(0x01<<25),
    _div = 0b0110011+(0x4<<12)+(0x01<<25),
    divu = 0b0110011+(0x5<<12)+(0x01<<25),
    rem = 0b0110011+(0x6<<12)+(0x01<<25),
    remu = 0b0110011+(0x7<<12)+(0x01<<25),
}; 

static uint64_t registers[32] = {0};
static uint64_t pc = 0;
static vec *breakpoints = NULL;
static Memory* memory = NULL;
static stacktrace* stack = NULL;
static uint8_t* memory_data = NULL;
extern bool text_write_enabled;

// Utility functions used to link frontend to backend
uint64_t* get_register_pointer() {return &registers[0];}
uint64_t* get_pc_pointer() {return &pc;}
vec* get_breakpoints_pointer() {return breakpoints;}
Memory* get_memory_pointer() {return memory;}
CacheStats* get_cache_stats_pointer() {return &(memory->cache_stats);}
void set_stacktrace_pointer(stacktrace* stacktrace) {stack = stacktrace;}

// Resets memeory and registers. The hard parameters is true if this is a new file load and false if it is just a reset
void reset_backend(bool hard, CacheConfig cache_config) {
    if (hard) {
        if (breakpoints) free_managed_array(breakpoints);
        if (memory) free_vmem(memory);
        breakpoints = new_managed_array();
        memory = new_vmem(cache_config);
        memory_data = memory->data;
    } else {
        reset_cache(memory);
    }
    memset(registers, 0, sizeof(registers));
    memset(memory_data, 0, MEMORY_SIZE);
    pc = 0;
}

void destroy_backend() {
    if (memory) free_vmem(memory);
    if (breakpoints) free_managed_array(breakpoints);
}

// Implementation of the STEP command
int step() {
    if (pc+3 >= DATA_BASE) {
        show_error("Segmentation Fault! PC ran into data segment");
        return 3;
    }

    // Deconstruct instruction
    uint32_t instruction = (memory_data[pc+3] << 24) | (memory_data[pc+2] << 16) | (memory_data[pc+1] << 8) |  memory_data[pc];
    uint32_t funct_op = 0;
    uint64_t imm = 0;
    uint64_t *rd = registers + ((0x00000F80 & instruction) >> 7);
    uint64_t *rs1 = registers + ((0x000F8000 & instruction) >> 15);
    uint64_t *rs2 = registers + ((0x01F00000 & instruction) >> 20);

    uint64_t data;

    // Match instruction type, extract immediate and funct bits appropriately
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
    
    // Update the line number on the stack
    st_update(stack, (pc/4)+1);

    // Execute the instruction. All registers are unsigned by default. Only signed comparisons and offsets have to be type casted  
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
            *rd = ((int64_t) *rs1 < (int64_t) *rs2)?1:0;
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
            if (imm & 0x20) *rd = (*rs1 >> imm) + ~((~0) >> imm); //srai
            else  *rd = *rs1 >> imm; //srli
            break;

        case slti:
            *rd = ((int64_t) *rs1 < (int64_t) imm)?1:0;
            break;

        case sltiu:
            *rd = (*rs1 < imm)?1:0;
            break;

        case lb:
            if (*rs1 + imm >= MEMORY_SIZE) {
                show_error("Invalid Memory Access! line %d attempted to read byte at 0x%08lX", pc/4, (*rs1 + imm));
                return 3;
            }
            // data = *(memory_data + *rs1 + imm);
            data = read_data_byte(memory, *rs1 + imm);
            if (data&0x00000080) data |= 0xFFFFFFFFFFFFFF00;
            *rd = data;
            break;

        case lh:
            if (*rs1 + imm + 1 >= MEMORY_SIZE) {
                show_error("Invalid Memory Access! line %d attempted to read hword at 0x%08lX", pc/4, (*rs1 + imm));
                return 3;
            }
            // data = *(uint16_t*)(memory_data + *rs1 + imm);
            data = read_data_halfword(memory, *rs1 + imm);
            if (data&0x00008000) data |= 0xFFFFFFFFFFFF0000;
            *rd = data;
            break;

        case lw:
            if (*rs1 + imm + 3 >= MEMORY_SIZE) {
                show_error("Invalid Memory Access! line %d attempted to read word at 0x%08lX", pc/4, (*rs1 + imm));
                return 3;
            }
            // data = *(uint32_t*)(memory_data + *rs1 + imm);
            data = read_data_word(memory, *rs1 + imm);
            if (data&0x80000000) data |= 0xFFFFFFFF00000000;
            *rd = data;
            break;

        case ld:
            if (*rs1 + imm + 7 >= MEMORY_SIZE) {
                show_error("Invalid Memory Access! line %d attempted to read dword at 0x%08lX", pc/4, (*rs1 + imm));
                return 3;
            }
            // data = *(uint64_t*)(memory_data + *rs1 + imm);
            data = read_data_doubleword(memory, *rs1 + imm);
            *rd = data;
            break;

        case lbu:
            if (*rs1 + imm >= MEMORY_SIZE) {
                show_error("Invalid Memory Access! line %d attempted to read byte at 0x%08lX", pc/4, (*rs1 + imm));
                return 3;
            }
            // *rs1 = *(memory_data + *rs1 + imm);
            *rd = read_data_byte(memory, *rs1 + imm);
            break;

        case lhu:
            if (*rs1 + imm + 1 >= MEMORY_SIZE) {
                show_error("Invalid Memory Access! line %d attempted to read hword at 0x%08lX", pc/4, (*rs1 + imm));
                return 3;
            }
            // *rs1 = *(uint16_t*)(memory_data + *rs1 + imm);
            *rd = read_data_halfword(memory, *rs1 + imm);
            break;

        case lwu:
            if (*rs1 + imm + 3 >= MEMORY_SIZE) {
                show_error("Invalid Memory Access! line %d attempted to read word at 0x%08lX", pc/4, (*rs1 + imm));
                return 3;
            }
            // *rs1 = *(uint32_t*)(memory_data + *rs1 + imm);
            *rd = read_data_word(memory, *rs1 + imm);
            break;

        case sb:
            if (*rs1 + imm >= MEMORY_SIZE) {
                show_error("Invalid Memory Access! line %d attempted to write byte at 0x%08lX", pc/4, (*rs1 + imm));
                return 3;
            }

            if (!text_write_enabled && *rs1 + imm < DATA_BASE) {
                show_error("Invalid Memory Access! line %d attempted to write byte at 0x%08lX, smc is not enabled.", pc/4, (*rs1 + imm));
                return 3;
            }
            write_data_byte(memory, *rs1 + imm, *rs2);
            // memcpy(memory_data + *rs1 + imm, rs2, 1);
            break;

        case sh:
            if (*rs1 + imm + 1 >= MEMORY_SIZE) {
                show_error("Invalid Memory Access! line %d attempted to write hword at 0x%08lX", pc/4, (*rs1 + imm));
                return 3;
            }

            if (!text_write_enabled && *rs1 + imm< DATA_BASE) {
                show_error("Invalid Memory Access! line %d attempted to write hword at 0x%08lX, smc is not enabled.", pc/4, (*rs1 + imm));
                return 3;
            }
            write_data_halfword(memory, *rs1 + imm, *rs2);
            // memcpy(memory_data + *rs1 + imm, rs2, 2);
            break;
        
        case sw:
            if (*rs1 + imm + 3 >= MEMORY_SIZE) {
                show_error("Invalid Memory Access! line %d attempted to write word at 0x%08lX", pc/4, (*rs1 + imm));
                return 3;
            }

            if (!text_write_enabled && *rs1 + imm< DATA_BASE) {
                show_error("Invalid Memory Access! line %d attempted to write word at 0x%08lX, smc is not enabled.", pc/4, (*rs1 + imm));
                return 3;
            }
            write_data_word(memory, *rs1 + imm, *rs2);
            // memcpy(memory_data + *rs1 + imm, rs2, 4);
            break;
        
        case sd:
            if (*rs1 + imm + 7 >= MEMORY_SIZE) {
                show_error("Invalid Memory Access! line %d attempted to write dword at 0x%08lX", pc/4, (*rs1 + imm));
                return 3;
            }

            if (!text_write_enabled && *rs1 + imm< DATA_BASE) {
                show_error("Invalid Memory Access! line %d attempted to write dword at 0x%08lX, smc is not enabled.", pc/4, (*rs1 + imm));
                return 3;
            }
            write_data_doubleword(memory, *rs1 + imm, *rs2);
            // memcpy(memory_data + *rs1 + imm, rs2, 8);
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
            *rd = pc + 4;
            pc += imm - 4;
            st_push(stack, 1 + pc/4);
            st_update(stack, -1);
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

        case mul:
            *rd = (*rs1 * *rs2);
            break;

        case mulh:
            *rd = ((int64_t) *rs1 * (__int128_t) *rs2)<<64;
            break;

        case mulsu:
            *rd = ((int64_t) *rs1 * (__uint128_t) *rs2)<<64;
            break;

        case mulu:
            *rd = ((uint64_t) *rs1 * (__uint128_t) *rs2)<<64;
            break;

        case _div:
            if (*rs2 != 0) *rd = (int64_t) *rs1 / (int64_t) *rs2;
            else *rd = 0;
            break;

        case divu:
            if (*rs2 != 0) *rd = *rs1 / *rs2;
            else *rd = 0;
            break;

        case rem:
            if (*rs2 != 0) *rd = (int64_t) *rs1 % (int64_t) *rs2;
            else *rd = 0;
            break;

        case remu:
            if (*rs2 != 0) *rd = (int64_t) *rs1 % (int64_t) *rs2;
            else *rd = 0;
            break;
    }

    pc += 4; // Increment the PC
    registers[0] = 0; // Make sure x0 doesn't change

    if (memory_data[pc] == ebreak) { // stop if next instruction is a breakpoint
        return 2;
    }

    if (memory_data[pc] == NOP) { // assume end of code if NOP is encountered.
        st_clear(stack);
    }

    for (int i=0; i<breakpoints->len; i++) { // stop if next instruction is a breakpoint
        if (pc/4==breakpoints->values[i]) {
            return 2;
        }
    }

    return 0;
}

// Runs till ebreak or end of program
// Returns 0 if user requested termination
// Returns 1 if end of program is reached
// Returns 2 if breakpoint is reached
int run(Command (*callback)(void)) {
    static time_t next_tick = 0;
    static struct timeb time;
    int result;

    while (1) {
        // Keep updating frontend while we wait out the delay between instructions
        do{
            if ((*callback)() == STOP) return 0;
            ftime(&time);
        } while (time.time*1000 + time.millitm < next_tick);
        next_tick = time.time*1000 + time.millitm + RUN_DELAY;

        // Call step here
        if ((result = step())) return result;
        // if ((*callback)() == STOP) return 0;
    }
}

int run_to_end(Command (*callback)(void)) {
    // static time_t next_tick = 0;
    // static struct timeb time;
    int result;

    while (1) {
        // Keep updating frontend while we wait out the delay between instructions
        if ((*callback)() == STOP) return 0;
        // do{
        //     ftime(&time);
        // } while (time.time*1000 + time.millitm < next_tick);
        // next_tick = time.time*1000 + time.millitm + 0;

        // Call step here
        if ((result = step())) return result;
        // if ((*callback)() == STOP) return 0;
    }
}