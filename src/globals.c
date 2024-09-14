#include <stdbool.h>
#include <stdlib.h>

bool segfault_flag = false;         // For Crash handler
bool text_write_enabled = false;    // Allow writing to text segment 
char* cause = NULL;
char* input_file = NULL;