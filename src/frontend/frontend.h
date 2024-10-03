#ifndef FRONTEND_H
#define FRONTEND_H
#include <stdint.h>
#include "../assembler/index.h"
#include "../assembler/vec.h"
#include <stdbool.h>
#include "../backend/stacktrace.h"

// Messages exchanged between frontend and other sections of the application
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

int init_frontend();
void destroy_frontend();

void set_frontend_register_pointer(uint64_t* regs_pointer);
void set_frontend_pc_pointer(uint64_t* pc_pointer);
void set_frontend_memory_pointer(uint8_t* memory_pointer, uint64_t size_of_memory);
void set_breakpoints_pointer(vec* breakpoints_pointer);
void set_stack_pointer(stacktrace* stacktrace);
void set_reg_write(uint64_t reg);
void set_run_lock();
void set_hexcode_pointer(uint32_t* hexcode_pointer);

void set_labels_pointer(label_index* index);
void update_code(char* code_pointer, uint64_t n);
void show_error(char* format, ...);
void release_run_lock();
void reset_frontend(bool hard);
Command frontend_update();

#endif