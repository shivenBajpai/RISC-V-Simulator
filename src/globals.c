#include <stdbool.h>

bool log_warnings = true;           // Logging related flags
bool log_debug = true;
bool segfault_flag = false;         // For Crash handler
bool text_write_enabled = false;    // Allow writing to text segment 