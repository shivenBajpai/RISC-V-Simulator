#ifndef ASSEMBLER_H
#define ASSEMBLER_H
#include <stdio.h>
#include "index.h"

int* assembler_main(FILE *in_fp, char *clean_fp, label_index* index);

#endif