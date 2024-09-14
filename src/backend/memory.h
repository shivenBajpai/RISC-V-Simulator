#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdio.h>

typedef struct Memory Memory;

Memory* new_memory(uint64_t data_addr, uint64_t size);

void memory_write_text(Memory* memory, uint8_t *bytes, uint64_t len);
void memory_write_data(Memory* memory, uint8_t *bytes, uint64_t len);

void memory_put(Memory* memory, uint8_t byte, uint64_t addr);
void memory_put_many(Memory* memory, uint8_t *bytes, uint64_t len, uint64_t addr);

uint8_t memory_get(Memory* memory, uint64_t addr);

int memory_core_dump(Memory* memory, FILE* text, FILE* data, FILE* stack);
#endif