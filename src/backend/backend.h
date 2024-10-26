#ifndef BACKEND_H
#define BACKEND_H
#include "stdio.h"
#include "../assembler/vec.h"
#include "stacktrace.h"
#include "memory.h"

#define DATA_BASE 0x10000
#define MEMORY_SIZE 0x50000 + 1 // Also used as end from which stack grows downward

int step();
int run();

void reset_backend(bool hard, CacheConfig cache_config);
void set_stacktrace_pointer(stacktrace* stacktrace);
void destroy_backend();
uint64_t* get_register_pointer();
uint64_t* get_pc_pointer();
vec* get_breakpoints_pointer();
Memory* get_memory_pointer();
CacheStats* get_cache_stats_pointer();

#endif