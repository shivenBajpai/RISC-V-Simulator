#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ncurses.h>
#include "globals.h"
#include "backend/backend.h"
#include "frontend/frontend.h"
#include "assembler/vec.h"
#include "assembler/assembler.h"

void exit_handler() {

	destroy_frontend();

	if (cause != NULL) {
		printf("%s\n", cause);
	}

	if (segfault_flag) {
		FILE *text_file = fopen("text.hex", "w");
		FILE *data_file = fopen("data.hex", "w");
		FILE *stack_file = fopen("stack.hex", "w");

		if (!text_file || !data_file || !stack_file) {
			printf("Failed to Core dump, couldn't write to files!\n");
		} else {
			//core_dump(memory, text_file, data_file, stack_file);
			core_dump(text_file);
		}

		fclose(text_file);
		fclose(data_file);
		fclose(stack_file);
	}
}

// char** split_command(char* command) {
// 	char* args[8] = {NULL};
// 	int i=0;
// 	int j=0;

// 	do {
// 		if (command == " ") {
// 			args[i][j] = '\0';
// 			i++;
// 			j=0;

// 			if (i == 8) {
// 				//ERROR: TOO MANY
// 			}

// 			args[i] = malloc(sizeof(char) * 128);
// 		}

// 		args[i][j++] = *command;
// 		if (j==127) {
// 			// ERROR: Too long
// 		}
// 	} while (command++ != '\0');

// 	return args;	
// }

int main(int* argc, char** argv) {
	
	atexit(*(exit_handler));
	input_file = malloc(sizeof(char) * 256);

    while(*(++argv) != NULL) {
            
		if (strcmp(*argv,"--smc")==0 || strcmp(*argv,"--self-modifying-code")==0) {
			text_write_enabled = true;
		}
    }

	reset_backend();

	init_frontend();
	set_frontend_register_pointer(get_register_pointer());
	set_frontend_pc_pointer(get_pc_pointer());
	set_frontend_memory_pointer(get_memory_pointer());
	set_breakpoints_pointer(get_breakpoints_pointer());

	vec* breakpoints = new_managed_array();
	Command command = NONE;
	char* cleaned_code = NULL;

	while (1) {
		switch (frontend_update()) {
			case LOAD:
	
				FILE* fp = fopen(input_file, "r");

				if (!fp) {
					show_error("Failed to read %s!", input_file);
					break;
				}

				fseek(fp, 0L, SEEK_END);
				long len = ftell(fp);
				fseek(fp, 0L, SEEK_SET);
				//printf("TryBuf size: %zu\n", sizeof(char) * len);

				if (cleaned_code) free(cleaned_code);
				cleaned_code = malloc(sizeof(char) * len);
				if (!cleaned_code) {
					show_error("Out Of Memory!");
					break;
				} else {
					//show_error("Buf size: %zu", sizeof(char) * len);
				}
				label_index* index;

				uint32_t* hexcode = assembler_main(fp, cleaned_code, index); // Need to add Data segment capabiltiy, and also need to add Breakpoint parsing
				if (!hexcode) {
					//show_error("Invalid State!");
					break;
				}
				fclose(fp);

				reset_backend();
				memcpy(get_memory_pointer(), &hexcode[1], hexcode[0]*4); // hexcode[0] is implicitly the length in words. actual hexcode starts from hexcode[1]
				
				set_labels_pointer(index);
				update_code(cleaned_code, hexcode[0]); //  Need to update breakpoints as per ebreak instructions
				set_hexcode_pointer((uint32_t*) &hexcode[1]);
				break;

			case RUN:
				int result = run(&frontend_update);
				release_run_lock();
				break;

			case EXIT:
				goto exit;

			case SHOW_STACK:
				break;

			case STEP:
				if (step()) show_error("Nothing to step");
				break;
		}
	}

	exit:
	destroy_frontend();
	free_managed_array(breakpoints);
	return 0;
}