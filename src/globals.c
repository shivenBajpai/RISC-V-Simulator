#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include "backend/backend.h"
#include "globals.h"

bool segfault_flag = false;         // For Crash handler
bool text_write_enabled = false;    // Allow writing to text segment 
bool tle_flag = false;
char input_file[256] = "";            // Name of input file
char active_file[256] = "cache";           // Name of active code file (may be the same as input file)

int cli_output_type = SIGNED;
char* cli_input_file = NULL;
char* cli_test_file = NULL;
bool cli_regs = false;
uint64_t cli_mem_max = 0;
uint64_t cli_mem_min = 0;
uint64_t cli_n_cases = -1;
int64_t cli_cycles = -1;