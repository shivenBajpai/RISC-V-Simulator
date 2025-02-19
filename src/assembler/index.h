#ifndef INDEX_H
#define INDEX_H
#include <stdlib.h>

typedef struct label_index {
	size_t len;
	size_t capacity;
	char** labels;
	int* positions;
} label_index;


int add_label(label_index* index, char* label, int position);

int prepend_label(label_index* index, char* label, int position);

int label_to_position(label_index* index, char* label);

char* position_to_label(label_index* index, int position);

void index_dedup(label_index* index);

int get_section_label(label_index* index, int line);

label_index* new_label_index();

void free_label_index(label_index* index);

void debug_print_label_index(label_index* index);

#endif
