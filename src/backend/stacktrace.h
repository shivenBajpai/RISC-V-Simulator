#ifndef STACKTRACE_H
#define STACKTRACE_H
#include "../assembler/vec.h"
#include "../assembler/index.h"

typedef struct stacktrace
{
    vec* stack; // List of line numbers
    vec* label_indices; // Index of label in label_index
    label_index* index; // Pointer to the label index in which the label_indices are
    int len; // Length of stack trace
} stacktrace;

void st_push(stacktrace* st, int line);

void st_pop(stacktrace* st);

void st_update(stacktrace* st, int line);

void st_clear(stacktrace* st);

stacktrace* new_stacktrace(label_index* index);

void st_free(stacktrace* st);

#endif