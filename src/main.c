#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "memory.h"

#define DATA_BASE 0x10000
#define MEMORY_SIZE 0x50000 + 1 // Also used as end from which stack grows downward

uint64_t registers[32];
uint64_t pc;
char* input_file_name = "input.hex";
Memory *memory;

void exit_handler() {
	
}

int main(int* argc, char** argv) {

	atexit(*(exit_handler));

    while((++argv) != NULL) {
            
		if (strcmp(*argv,"-i")==0 || strcmp(*argv,"--input")==0) {
			argv++;
			if (!*argv) {
				printf("Missing input file name for option input\n");
				return 1;
			}

			input_file_name = *argv;
		}
    }

    pc = 0;
	for(int i=0; i<32; i++) registers[i] = 0;
	if (!(memory = new_memory(DATA_BASE, MEMORY_SIZE))) {
		printf("Failed to create virtual memory, Exiting...");
		return 1;
	}

    // Todo: Compilation step


    return EXIT_SUCCESS;
}