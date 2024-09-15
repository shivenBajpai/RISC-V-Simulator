#ifndef BACKEND_H
#define BACKEND_H
#include "stdio.h"
#include "../assembler/vec.h"
#include "stacktrace.h"

#define DATA_BASE 0x10000
#define MEMORY_SIZE 0x50000 + 1 // Also used as end from which stack grows downward

//int core_dump(Memory* memory, FILE* text, FILE* data, FILE* stack);
int core_dump(FILE* text);

int load(uint8_t* bytes, size_t n);
int step();
int run();

void reset_backend();
void set_stacktrace_pointer(stacktrace* stacktrace);
uint64_t* get_register_pointer();
uint64_t* get_pc_pointer();
vec* get_breakpoints_pointer();
uint8_t* get_memory_pointer();

#endif