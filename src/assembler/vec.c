#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Vec is an array-like struct that internally handles allocating more memory as the array expands.
typedef struct vec {
	size_t len;
	size_t capacity;
	uint64_t* values;
} vec;

vec* new_managed_array() {
	vec* array = malloc(sizeof(vec)); 
	
	if (!array) {
		return NULL;
	}

	array->len = 0;
	array->capacity = 4;
	array->values = malloc(4*sizeof(uint64_t));

	if (!array->values) {
		free(array);
		return NULL;
	}

	return array;
}

int append(vec* array, uint64_t value) {

	array->values[array->len] = value;
	array->len++;

	//printf("allocating %zu to %p, first value %lu\n", array->capacity * sizeof(uint64_t), array->values, array->values[1]);

	// If space is insufficient, we double the capacity
	if (array->len == array->capacity) {
		array->capacity *= 2;
		uint64_t* values_new = realloc(array->values, array->capacity * sizeof(uint64_t));
		if (!values_new) {
			printf("Failed to allocate memory for label index\n");
			return 1;
		}
		array->values = values_new;
	}

	return 0;
}

void vec_remove(vec* array, size_t index) {
	if (index != array->len-1) {
		memcpy(array->values+index, array->values+index+1, array->len-index-1);
	}

	array->len -= 1;
}

void free_managed_array(vec* array) {
	free(array->values);
	free(array);
}

// For debugging purposes
void print_array(vec* index) {

	printf("[");
	for(int i=0; i<index->len; i++) {
		printf("%lu,", index->values[i]);
	}
	printf("]\n");
	
}