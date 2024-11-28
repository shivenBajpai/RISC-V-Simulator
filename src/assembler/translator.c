#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "translator.h"
// #include "../frontend/frontend.h"
#include "../supervisor/supervisor.h"

typedef struct alias {
    const char* name;
    int value;
} alias;

static const instruction_info instructions[] = {
    "add",      0b0110011,                      R_TYPE,
    "sub",      0b0110011+(0x20<<25),           R_TYPE,
    "xor",      0b0110011+(0x4<<12),            R_TYPE,
    "or",       0b0110011+(0x6<<12),            R_TYPE,
    "and",      0b0110011+(0x7<<12),            R_TYPE,
    "sll",      0b0110011+(0x1<<12),            R_TYPE,
    "srl",      0b0110011+(0x5<<12),            R_TYPE,
    "sra",      0b0110011+(0x5<<12)+(0x20<<25), R_TYPE,
    "slt",      0b0110011+(0x2<<12),            R_TYPE,
    "sltu",     0b0110011+(0x3<<12),            R_TYPE,

    "addi",     0b0010011+(0x0<<12),            I1_TYPE,
    "xori",     0b0010011+(0x4<<12),            I1_TYPE,
    "ori",      0b0010011+(0x6<<12),            I1_TYPE,
    "andi",     0b0010011+(0x7<<12),            I1_TYPE,
    "slli",     0b0010011+(0x1<<12)+(0x00<<26), I1B_TYPE,
    "srli",     0b0010011+(0x5<<12)+(0x00<<26), I1B_TYPE,
    "srai",     0b0010011+(0x5<<12)+(0x10<<26), I1B_TYPE,
    "slti",     0b0010011+(0x2<<12),            I1_TYPE,
    "sltiu",    0b0010011+(0x3<<12),            I1_TYPE,

    "lb",       0b0000011+(0x0<<12),            I2_TYPE,
    "lh",       0b0000011+(0x1<<12),            I2_TYPE,
    "lw",       0b0000011+(0x2<<12),            I2_TYPE,
    "ld",       0b0000011+(0x3<<12),            I2_TYPE,
    "lbu",      0b0000011+(0x4<<12),            I2_TYPE,
    "lhu",      0b0000011+(0x5<<12),            I2_TYPE,
    "lwu",      0b0000011+(0x6<<12),            I2_TYPE,

    "sb",       0b0100011+(0x0<<12),            S_TYPE,
    "sh",       0b0100011+(0x1<<12),            S_TYPE,
    "sw",       0b0100011+(0x2<<12),            S_TYPE,
    "sd",       0b0100011+(0x3<<12),            S_TYPE,

    "beq",      0b1100011+(0x0<<12),            B_TYPE,      
    "bne",      0b1100011+(0x1<<12),            B_TYPE,      
    "blt",      0b1100011+(0x4<<12),            B_TYPE,      
    "bge",      0b1100011+(0x5<<12),            B_TYPE,      
    "bltu",     0b1100011+(0x6<<12),            B_TYPE,         
    "bgeu",     0b1100011+(0x7<<12),            B_TYPE,     

    "jal",      0b1101111,                      J_TYPE,
    "jalr",     0b1100111,                      I3_TYPE,
    "lui",      0b0110111,                      U_TYPE,
    "auipc",    0b0010111,                      U_TYPE,   
    "ecall",    0b1110011,                      I4_TYPE,   
    "ebreak",   0b1110011+(0X1<<20),            I4_TYPE,   

    "mul",      0b0110011+(0x0<<12)+(0x01<<25), R_TYPE,
    "mulh",     0b0110011+(0x1<<12)+(0x01<<25), R_TYPE,
    "mulsu",    0b0110011+(0x2<<12)+(0x01<<25), R_TYPE,
    "mulu",     0b0110011+(0x3<<12)+(0x01<<25), R_TYPE,
    "div",      0b0110011+(0x4<<12)+(0x01<<25), R_TYPE,
    "divu",     0b0110011+(0x5<<12)+(0x01<<25), R_TYPE,
    "rem",      0b0110011+(0x6<<12)+(0x01<<25), R_TYPE,
    "remu",     0b0110011+(0x7<<12)+(0x01<<25), R_TYPE,

    // "li",       0b1111111,                      P_TYPE, 
    "nop",      0b0110011,                      I4_TYPE, // Converts to add x0 x0 x0
}; 

static const alias registers[] = {
    "x0", 0,
    "x1", 1,
    "x2", 2,
    "x3", 3,
    "x4", 4,
    "x5", 5,
    "x6", 6,
    "x7", 7,
    "x8", 8,
    "x9", 9,
    "x10", 10,
    "x11", 11,
    "x12", 12,
    "x13", 13,
    "x14", 14,
    "x15", 15,
    "x16", 16,
    "x17", 17,
    "x18", 18,
    "x19", 19,
    "x20", 20,
    "x21", 21,
    "x22", 22,
    "x23", 23,
    "x24", 24,
    "x25", 25,
    "x26", 26,
    "x27", 27,
    "x28", 28,
    "x29", 29,
    "x30", 30,
    "x31", 31,
    "zero", 0,
    "ra", 1,
    "sp", 2,
    "gp", 3,
    "tp", 4,
    "t0", 5,
    "t1", 6,
    "t2", 7,
    "s0", 8,
    "fp", 8,
    "s1", 9,
    "a0", 10,
    "a1", 11,
    "a2", 12,
    "a3", 13,
    "a4", 14,
    "a5", 15,
    "a6", 16,
    "a7", 17,
    "s2", 18,
    "s3", 19,
    "s4", 20,
    "s5", 21,
    "s6", 22,
    "s7", 23,
    "s8", 24,
    "s9", 25,
    "s10", 26,
    "s11", 27,
    "t3", 28,
    "t4", 29,
    "t5", 30,
    "t6", 31,
};

// Names for error reporting purposes
const char* argument_type_names[] = {
    "Immediate Value",
    "Offset/Flag",
    "Register"
}; 

// Converts register name into register number 
int parse_alias(char* name) {
    for (int i = 0; i<65; i++) {
        if (strcmp(name, registers[i].name) == 0) {
            return registers[i].value;
        }
    }

    return -1;
}

// Convert instruction name into instruction_info* by looking it up in the list of instructions
const instruction_info* parse_instruction(char* name) {
    for (int i = 0; i<50; i++) {
        if (strcmp(name, instructions[i].name) == 0) {
            return &instructions[i];
        }
    }

    return NULL;
}

// Generalized Function that parses instruction arguments from the file pointer directly. 
// What type of arguments to expect is specified in the function's arguments itself
int* parse_args(char** fpp, label_index* labels, int n_args, argument_type* types, uint64_t* line_number, int instruction_number) {
    int i_args = 0;
    int current_arg = 0;
    char c;

    // Instruction Arguments are parsed into a single string that is divided into 128 char segments for each argument
    char* args = malloc(n_args*128*sizeof(char)); 
    if (!args) {
        printf("Out of memory!");
        return NULL;
    }

    // Seperate the instruction arguments into aforementioned 128 char segments
    while ((c = (**(fpp))) != '\0') {
        *fpp += 1;
        switch (c){
            case ' ':
            case '\t':
                break;

            case ',':
            case'(':
                if (current_arg==(n_args-1)) {
                    show_error("Error on line %lu, Expected 3 operands, Found more than 3\n", *line_number);
                    free(args);
                    return NULL;
                }
                args[current_arg*128 + i_args] = '\0';
                current_arg++;
                i_args = 0;
                break;

            case ')':
                break;
            case '\n':
                goto exit;

            default:
                if (i_args==127) {
                    args[current_arg*128 + i_args] = '\0';
                    show_error("Error on line %lu, Illegal operand: %s\n", *line_number, args + current_arg*128);
                    free(args);
                    return NULL;
                }
                args[current_arg*128 + i_args++] = c;
        }
    }

    exit:
    if (current_arg<(n_args-1)) {
        show_error("Error on line %lu, Expected 3 operands, Less operands than expected\n", *line_number);
        free(args);
        return NULL;
    }
    args[current_arg*128 + i_args] = '\0';
    int* converted_args = malloc(sizeof(unsigned long)*n_args);
    if (!converted_args) {
        printf("Out of memory!");
        free(args);
        return NULL;
    }

    // Attempt to parse each argument based on expected type
    for (current_arg=0; current_arg<n_args; current_arg++) {
        char* arg = args + current_arg*128;

        switch (types[current_arg]) {
            case IMMEDIATE:

                char *endptr;
                if (arg[0] == '0') {
                    if (arg[1] == 'x') {
                        converted_args[current_arg] = strtol(arg+2, &endptr, 16);
                    } else if (arg[1] == 'b') {
                        converted_args[current_arg] = strtol(arg+2, &endptr, 2);
                    } else {
                        converted_args[current_arg] = strtol(arg+1, &endptr, 8);
                    }
                } else {
                    converted_args[current_arg] = strtol(arg, &endptr, 10);
                }

                if (endptr == arg || *endptr != '\0') {
                    show_error("Argument %d on line %lu is invalid for type %s: %s\n", current_arg+1, *line_number, argument_type_names[types[current_arg]], arg);
                    free(args);
                    return NULL;
                }

                break;

            case OFFSET:

                if (!isalpha(arg[0])) {
                    char *endptr;
                    converted_args[current_arg] = strtol(arg, &endptr, 10);

                    if (endptr == arg || *endptr != '\0') {
                        show_error("Failed to interpret argument %d on line %lu as numeric offset: %s\n", current_arg+1, *line_number, arg);
                        free(args);
                        free(converted_args);
                        return NULL;
                    }

                } else {
                    int position = label_to_position(labels, arg);

                    if (position==-1) {
                        show_error("Unseen label on line %lu: %s\n", *line_number, arg);
                        free(args);
                        free(converted_args);
                        return NULL;
                    }

                    converted_args[current_arg] = (position - instruction_number)*4;
                }

                break;

            case REGISTER:
                converted_args[current_arg] = parse_alias(arg);

                if (converted_args[current_arg]==-1) {
                    show_error("Unknown register on line %lu: %s\n", *line_number, arg);
                    free(args);
                    free(converted_args);
                    return NULL;
                }
                
                break;

            default:
                show_error("Error on line %lu\nUnknown argument type %d for %s, did you forget to write a case?\n", *line_number, types[current_arg], args + current_arg*128);
				return NULL;
        }
    }

    free(args);
    return converted_args;
}

// Below are a series of helper functions
// Each of them simply call the above parses with the right information, and then perform bounds checks and bit manipulation
// before returning the encoded arguemnts as a long int
// 
// The purpose of these functions being defined like this, is to declutter the code for the second_pass function in main.c

// long P_type_parser(char** args_raw, label_index* labels, uint64_t* line_number, int instruction_number, bool* fail_flag, vec* constants) {
//     argument_type types[] = {REGISTER, IMMEDIATE};
//     int* args = parse_args(args_raw, labels, 2, types, line_number, instruction_number);

//     if (!args) {
//         *fail_flag = true;
//         return -1;
//     }

//     append(constants, args[1]);
//     int result = (args[0] << 7) + ((constants->len-1) << 12);

//     free(args);
//     return result;
// }

long R_type_parser(char** args_raw, label_index* labels, uint64_t* line_number, int instruction_number, bool* fail_flag) {
    argument_type types[] = {REGISTER, REGISTER, REGISTER};
    int* args = parse_args(args_raw, labels, 3, types, line_number, instruction_number);

    if (!args) {
        *fail_flag = true;
        return -1;
    }

    int result = (args[0] << 7) + (args[1] << 15) + (args[2] << 20);

    free(args);
    return result;
}

long I1_type_parser(char** args_raw, label_index* labels, uint64_t* line_number, int instruction_number, bool* fail_flag) {
    argument_type types[] = {REGISTER, REGISTER, IMMEDIATE};
    int* args = parse_args(args_raw, labels, 3, types, line_number, instruction_number);

    if (!args) {
        *fail_flag = true;
        return -1;
    }

    if (args[2] > 2047 || args[2] < -2048) {
        show_error("Error on line %li: Immediate value too large. Stopping...\n", *line_number);
        *fail_flag = true;
        return -1;
    }

    int result = (args[0] << 7) + (args[1] << 15) + (args[2] << 20);
    free(args);
    return result;
}

long I1B_type_parser(char** args_raw, label_index* labels, uint64_t* line_number, int instruction_number, bool* fail_flag) {
    argument_type types[] = {REGISTER, REGISTER, IMMEDIATE};
    int* args = parse_args(args_raw, labels, 3, types, line_number, instruction_number);

    if (!args) {
        *fail_flag = true;
        return -1;
    }

    if (args[2] > 63) {
        show_error("Error on line %li: Shift amount cannot exceed 63 bits. Stopping...\n", *line_number);
        *fail_flag = true;
        return -1;
    }

    int result = (args[0] << 7) + (args[1] << 15) + (args[2] << 20);
    free(args);
    return result;
}

long I2_type_parser(char** args_raw, label_index* labels, uint64_t* line_number, int instruction_number, bool* fail_flag) {
    argument_type types[] = {REGISTER, IMMEDIATE, REGISTER};
    int* args = parse_args(args_raw, labels, 3, types, line_number, instruction_number);

    if (!args) {
        *fail_flag = true;
        return -1;
    }

    if (args[1] > 2047 || args[1] < -2048) {
        show_error("Error on line %li: Immediate value too large. Stopping...\n", *line_number);
        *fail_flag = true;
        return -1;
    }

    int result = (args[0] << 7) + (args[2] << 15) + (args[1] << 20);
    free(args);
    return result;
}

long S_type_parser(char** args_raw, label_index* labels, uint64_t* line_number, int instruction_number, bool* fail_flag) {
    argument_type types[] = {REGISTER, IMMEDIATE, REGISTER};
    int* args = parse_args(args_raw, labels, 3, types, line_number, instruction_number);

    if (!args) {
        *fail_flag = true;
        return -1;
    }

    if (args[1] > 2047 || args[1] < -2048) {
        show_error("Error on line %li: Immediate value too large. Stopping...\n", *line_number);
        *fail_flag = true;
        return -1;
    }

    int rearranged_immediate = ((args[1] & 0x0000001F) << 7) + ((args[1] & 0x00000FE0) << 20);
    int result = (args[2] << 15) + rearranged_immediate + (args[0] << 20);
    free(args);
    return result;
}

long B_type_parser(char** args_raw, label_index* labels, uint64_t* line_number, int instruction_number, bool* fail_flag) {
    argument_type types[] = {REGISTER, REGISTER, OFFSET};
    int* args = parse_args(args_raw, labels, 3, types, line_number, instruction_number);

    if (!args) {
        *fail_flag = true;
        return -1;
    }

    if (args[2] > 4094*4 || args[2] < -4096*4) {
        show_error("Error on line %li: Branch offset too large. Stopping...\n", *line_number);
        *fail_flag = true;
        return -1;
    }

    int rearranged_offset = ((args[2] & 0x0000001E) << 7) + ((args[2] & 0x00000800) >> 4) + ((args[2] & 0x00001000) << 19) + ((args[2] & 0x000007E0) << 20);
    int result = (args[0] << 15) + (args[1] << 20) + rearranged_offset;

    free(args);
    return result;
}

long U_type_parser(char** args_raw, label_index* labels, uint64_t* line_number, int instruction_number, bool* fail_flag) {
    argument_type types[] = {REGISTER, IMMEDIATE};
    int* args = parse_args(args_raw, labels, 2, types, line_number, instruction_number);

    if (!args) {
        *fail_flag = true;
        return -1;
    }

    if (args[1] < 0 || args[1] > 0xFFFFF) {
        show_error("Error on line %li: Offset does not in range 0x0...0xFFFFF Stopping...\n", *line_number);
        *fail_flag = true;
        return -1;
    }

    int result = (args[0] << 7) + (args[1] << 12);

    free(args);
    return result;
}

long J_type_parser(char** args_raw, label_index* labels, uint64_t* line_number, int instruction_number, bool* fail_flag) {
    argument_type types[] = {REGISTER, OFFSET};
    int* args = parse_args(args_raw, labels, 2, types, line_number, instruction_number);

    if (!args) {
        *fail_flag = true;
        return -1;
    }

    int rearranged_offset = (args[1] & 0x000FF000) + ((args[1] & 0x000007FE) << 20) + ((args[1] & 0x00000800) << 9) + ((args[1] & 0x00100000) << 11);
    int result = (args[0] << 7) + rearranged_offset;

    free(args);
    return result;
}

long I3_type_parser(char** args_raw, label_index* labels, uint64_t* line_number, int instruction_number, bool* fail_flag) {
    argument_type types[] = {REGISTER, REGISTER, IMMEDIATE};
    int* args = parse_args(args_raw, labels, 3, types, line_number, instruction_number);

    if (!args) {
        *fail_flag = true;
        return -1;
    }

    int rearranged_offset = (args[2] & 0x00000FFF) << 20;
    int result = (args[0] << 7) + (args[1] << 15) + rearranged_offset;

    free(args);
    return result;
}