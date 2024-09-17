#ifndef VEC_H
#define VEC_H
#include <stdlib.h>
#include <stdint.h>

typedef struct vec {
	size_t len;
	size_t capacity;
	uint64_t* values;
} vec; 

int append(vec* array, uint64_t value);

void vec_remove(vec* array, size_t index);

void vec_clear(vec* array);

vec* new_managed_array();

void free_managed_array(vec* array);

void print_array(vec* index);

#endif