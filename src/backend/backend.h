#ifndef BACKEND_H
#define BACKEND_H
#include "stdio.h"
#include "../assembler/vec.h"

//int core_dump(Memory* memory, FILE* text, FILE* data, FILE* stack);
int core_dump(FILE* text);

int load(uint8_t* bytes, size_t n);
int step();
int run();

void reset_backend();
uint64_t* get_register_pointer();
uint64_t* get_pc_pointer();
vec* get_breakpoints_pointer();
uint8_t* get_memory_pointer();

#endif