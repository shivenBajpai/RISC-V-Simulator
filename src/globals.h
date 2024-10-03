#ifndef GLOBALS_H
#define GLOBALS_H
#include <stdbool.h>

extern bool segfault_flag; // For Crash handler
extern bool text_write_enabled;    // Allow writing to text segment 
extern char* input_file;

#endif