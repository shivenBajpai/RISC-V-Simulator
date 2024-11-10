#include <stdbool.h>
#include <stdlib.h>

bool segfault_flag = false;         // For Crash handler
bool text_write_enabled = false;    // Allow writing to text segment 
char input_file[256] = "";            // Name of input file
char active_file[256] = "cache";           // Name of active code file (may be the same as input file)