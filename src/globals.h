#ifndef GLOBALS_H
#define GLOBALS_H
#include <stdbool.h>

extern bool segfault_flag; // For Crash handler
extern bool text_write_enabled;    // Allow writing to text segment 
extern char input_file[256];
extern char active_file[256];

extern char* cli_input_file;
extern char* cli_test_file;
extern bool cli_regs;
extern uint64_t cli_n_cases;
extern uint64_t cli_mem_max;
extern int64_t cli_cycles;

#endif