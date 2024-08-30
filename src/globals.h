#ifndef GLOBALS_H
#define GLOBALS_H
#include <stdbool.h>

extern bool log_warnings;       // Logging related flags
extern bool log_debug;
extern bool segfault_flag; // For Crash handler
extern bool text_write_enabled;    // Allow writing to text segment 

#endif