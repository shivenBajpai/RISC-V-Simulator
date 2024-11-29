#ifndef GLOBALS_H
#define GLOBALS_H
#include <stdbool.h>

extern bool segfault_flag; // For Crash handler
extern bool text_write_enabled;    // Allow writing to text segment 
extern bool tle_flag; 
extern char input_file[256];
extern char active_file[256];

extern int cli_output_type;
extern char* cli_input_file;
extern char* cli_test_file;
extern bool cli_regs;
extern uint64_t cli_n_cases;
extern uint64_t cli_mem_max;
extern uint64_t cli_mem_min;
extern int64_t cli_cycles;

typedef enum OUTPUT_TYPE {
    SIGNED,
    UNSIGNED,
    HEX
} OUTPUT_TYPE;

#endif