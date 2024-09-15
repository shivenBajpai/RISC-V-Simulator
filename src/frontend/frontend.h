#ifndef FRONTEND_H
#define FRONTEND_H
#include <stdint.h>
#include "../assembler/index.h"
#include "../assembler/vec.h"
#include <stdbool.h>
#include "../backend/stacktrace.h"

typedef enum {
    LOAD,
    RUN,
    EXIT,
    SHOW_STACK,
    STEP,
    STOP,
    BREAKLINE,
    RESET,
    NONE
} Command;

typedef struct DisplayData {
    uint64_t* regs; 
    uint64_t* pc; 
    uint8_t* memory; 
    size_t n_lines; // parse
    char** text_code; // todo
    char** hex_code; // todo
    label_index* label_data; 
    int* active_line; // from pc
    int* breakpoints;
    bool* running_flag;
} DisplayData;

int init_frontend();
void destroy_frontend();
void set_frontend_register_pointer(uint64_t* regs_pointer);
void set_frontend_pc_pointer(uint64_t* pc_pointer);
void set_frontend_memory_pointer(uint8_t* memory_pointer, uint64_t size_of_memory);
void set_breakpoints_pointer(vec* breakpoints_pointer);
void set_labels_pointer(label_index* index);
void set_stack_pointer(stacktrace* stacktrace);
void update_code(char* code_pointer, uint64_t n);
void set_hexcode_pointer(uint32_t* hexcode_pointer);
void show_error(char* format, ...);
void set_run_lock();
void release_run_lock();
Command frontend_update();

#endif