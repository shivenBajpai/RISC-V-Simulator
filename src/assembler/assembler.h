#ifndef ASSEMBLER_H
#define ASSEMBLER_H
#include <stdio.h>
#include "index.h"
#include "vec.h"

int* assembler_main(FILE *in_fp, char *clean_fp, label_index* index, uint8_t* memory, vec* constants);
int data_only_run(FILE* in_fp, uint8_t* memory);

#endif