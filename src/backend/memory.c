#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "../globals.h"

// An abstraction over the memory that imitates a large array externally but optimizes memory utilization internally
// Makes the assumptions that text segment starts at 0x0 and also that layout is always <text> <data> <heap> <stack>

typedef struct Memory {
                        // vbase pointers are adjusted so that vbase_pointer[addr] points to the value of memory at addr
    uint8_t *text;      // Pointer to text segment base
    uint8_t *data;      // Pointer to data segment base
    uint8_t *data_vbase;
    uint8_t *stack;     // Pointer to stack base
    uint8_t *stack_vbase;
    
    uint64_t text_end;  // Internal numbers signifying upto which virtual memory address, memory is actually allocated for every segment
    uint64_t data_end;
    uint64_t stack_end;

    uint64_t data_addr; // Base address of data segment
    uint64_t size;      // Size in bytes, equivalent to highest valid address + 1
} Memory;

Memory* new_memory(uint64_t data_addr, uint64_t size) {
    
    if (data_addr > size || data_addr == 0)  return NULL;
    
    Memory *memory = malloc(sizeof(Memory));
    memory->data_addr = data_addr;
    memory->size = size;

    memory->text_end = 0;
    memory->data_end = data_addr;
    memory->stack_end = size-1;

    memory->text = malloc(sizeof(uint8_t));
    memory->data = malloc(sizeof(uint8_t));
    memory->stack = malloc(sizeof(uint8_t));

    // text_vbase is the same as text
    memory->data_vbase = memory->data - memory-> data_addr;
    memory->stack_vbase = memory->stack - memory->size;

    return memory;
}

void memory_write_text(Memory* memory, uint8_t *bytes, uint64_t len) {

    if (len > memory->text_end + 1) {
        if (len > memory->data_addr) {
            exit(1); // Caller should make sure this never happens
        }

        uint8_t *tmp = realloc(memory->text, sizeof(uint8_t)*len);
        if (!tmp) {
            printf("Out of memory!");
            exit(1);
        }

        memory->text = tmp;
        memory->text_end = len-1;
    }

    memcpy(memory->text, bytes, len);
}

// Caveat: Need to shrink stack
void memory_write_data(Memory* memory, uint8_t *bytes, uint64_t len) {

    if (len > memory->data_end - memory->data_addr + 1) {
        if (len > memory->size - memory->data_addr) {
            exit(1); // Caller should make sure this never happens
        }

        uint8_t *tmp = realloc(memory->data, sizeof(uint8_t)*len);
        if (!tmp) {
            printf("Out of memory!");
            exit(1);
        }

        memory->data = tmp;
        memory->data_end = memory->data_addr + len-1;
    }

    memcpy(memory->data, bytes, len);
}

void memory_put(Memory* memory, uint8_t byte, uint64_t addr) {
    
    if (addr > memory->size) {
        segfault_flag  = true;
        exit(1); // Segfault
    }
    
    else if (addr >= memory->stack_end) memory->stack_vbase[addr] = byte;
    else if (addr >= memory->data_addr){
        if (addr > memory->data_end) {
            // Find which choice requires minimum memory allocation
            uint64_t d_stack = memory->stack_end - addr;
            uint64_t d_data = addr - memory->data_end;

            if (d_data > d_stack) {

                uint64_t new_len = memory->size - addr;

                uint8_t *tmp = realloc(memory->data, sizeof(uint8_t)*new_len);
                if (!tmp) {
                    printf("Out of memory!");
                    exit(1);
                }

                memory->stack = tmp;
                memory->stack_end = memory->size - new_len;
                memory->stack_vbase = memory->stack - memory->size;
                
                memory->stack_vbase[addr] = byte;

            } else {

                uint64_t new_len = addr - memory->data_addr;

                uint8_t *tmp = realloc(memory->data, sizeof(uint8_t)*new_len);
                if (!tmp) {
                    printf("Out of memory!");
                    exit(1);
                }

                memory->data = tmp;
                memory->data_end = memory->data_addr + new_len-1;
                memory->data_vbase = memory->data - memory->data_addr;

                memory->data_vbase[addr] = byte;
            }
        }
        else memory->data_vbase[addr] = byte;
    }

    else if (text_write_enabled) {
        if (addr > memory->text_end) {

            uint8_t *tmp = realloc(memory->text, sizeof(uint8_t)*(addr + 1));
            if (!tmp) {
                printf("Out of memory!");
                exit(1);
            }

            memory->text = tmp;
            memory->text_end = addr;
        }

        memory->text[addr] = byte;
    };

}

void memory_put_many(Memory* memory, uint8_t *bytes, uint64_t len, uint64_t addr) {

    for(int i=0 ; i<len; i++) {
        memory_put(memory, bytes[i], addr++);
    }

}

uint8_t memory_get(Memory* memory, uint64_t addr) {

    if (addr > memory->size) {
        segfault_flag  = true;
        exit(1); // Segfault
    }
    else if (addr > memory->stack_end) return *(memory->stack_vbase + addr);
    else if (addr > memory->data_addr) return (addr > memory->data_end)?0:*(memory->data_vbase + addr);
    else return (addr > memory->text_end)?0:*(memory->text + addr);

    return 0;
}

int memory_core_dump(Memory* memory, FILE* text, FILE* data, FILE* stack) {
    
    if (fwrite(memory->text, sizeof(uint8_t), memory->text_end + 1, text) != memory->text_end+1) printf("Textfile dump failed\n");
    if (fwrite(memory->data, sizeof(uint8_t), memory->data_end - memory->data_addr+1, text) != memory->data_end-memory->data_addr+1) printf("data file dump failed\n");
    if (fwrite(memory->stack, sizeof(uint8_t), memory->size - memory->stack_end, text) != memory->size - memory->stack_end) printf("stack file dump failed\n");
    
    return 0;
}