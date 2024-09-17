#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "globals.h"
#include "backend/backend.h"
#include "frontend/frontend.h"
#include "assembler/vec.h"
#include "assembler/assembler.h"
#include "backend/stacktrace.h"

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
			//core_dump(memory, text_file, data_file, stack_file); // TODO: Core dumps!
			core_dump(text_file);
		}

		fclose(text_file);
		fclose(data_file);
		fclose(stack_file);
	}
}

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
	set_frontend_memory_pointer(get_memory_pointer(), MEMORY_SIZE);
	set_breakpoints_pointer(get_breakpoints_pointer());

	vec* breakpoints = new_managed_array();
	stacktrace* stack = NULL;
	label_index* index = NULL;
	uint32_t* hexcode = NULL;
	Command command = NONE;
	uint8_t *memory_template = malloc(sizeof(uint8_t)* MEMORY_SIZE);
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

				if (cleaned_code) free(cleaned_code);
				cleaned_code = malloc(sizeof(char) * len+1);
				if (!cleaned_code) {
					show_error("Out Of Memory!");
					break;
				}
				
				if (index) free(index);
				index = new_label_index();
				memset(memory_template, 0, sizeof(memory_template));

				hexcode = assembler_main(fp, cleaned_code, index, memory_template); // Need to add Data segment capabiltiy, and also need to add Breakpoint parsing
				fclose(fp);
				if (!hexcode) {
					break;
				}

				if (get_section_label(index, 0) == -1) add_label(index, "main", 0);
				index_dedup(index);
				if (stack) free(stack);
				stack = new_stacktrace(index);
				st_push(stack, 0);

				// debug_print_label_index(index);

				reset_backend();
				//reset_frontend();
				memcpy(get_memory_pointer(), &hexcode[1], hexcode[0]*4); // hexcode[0] is implicitly the length in words. actual hexcode starts from hexcode[1]
				memcpy(get_memory_pointer()+DATA_BASE, memory_template+DATA_BASE, MEMORY_SIZE-DATA_BASE);

				update_code(cleaned_code, hexcode[0]); //  Need to update breakpoints as per ebreak instructions
				set_stack_pointer(stack);
				set_stacktrace_pointer(stack);
				set_breakpoints_pointer(get_breakpoints_pointer());
				set_labels_pointer(index);
				set_hexcode_pointer((uint32_t*) &hexcode[1]);
				break;

			case RUN:
				int result = run(&frontend_update);
				release_run_lock();
				if (result == 0) show_error("Execution stopped at breakpoint!");
				else if (result == 1) {
					show_error("Reached End of Program");
					st_clear(stack);
				}
				break;

			case RESET:
				if (stack) free(stack);
				stack = new_stacktrace(index);
				st_push(stack, 0);

				reset_backend();

				set_stack_pointer(stack);
				set_stacktrace_pointer(stack);
				set_breakpoints_pointer(get_breakpoints_pointer());
				memcpy(get_memory_pointer(), &hexcode[1], hexcode[0]*4);
				memcpy(get_memory_pointer()+DATA_BASE, memory_template+DATA_BASE, MEMORY_SIZE-DATA_BASE);
				set_hexcode_pointer((uint32_t*) &hexcode[1]);
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
	if (hexcode) free(hexcode);
	if (stack) free(stack);
	if (index) free(index);
	if (cleaned_code) free(cleaned_code);
	free(memory_template);
	destroy_frontend();
	// TODO: FREE BACKEND MEMORY
	free_managed_array(breakpoints);
	return 0;
}