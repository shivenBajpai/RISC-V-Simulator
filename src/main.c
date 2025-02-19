#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif

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
#include "time.h"

static stacktrace* stack = NULL;
static label_index* index_of_labels = NULL;
static uint32_t* hexcode = NULL;
static uint8_t *memory_template = NULL;
static char* cleaned_code = NULL;
static CacheConfig cache_config;


// Ensures memory is freed and ncurses mode is exited properly, regardless of exit cause`
void exit_handler() {

	if (hexcode) free(hexcode);
	if (stack) st_free(stack);
	if (index_of_labels) free_label_index(index_of_labels);
	if (cleaned_code) free(cleaned_code);
	if (memory_template) free(memory_template);
	destroy_frontend();
	destroy_backend();
}

int main(int* argc, char** argv) {
	
	Command command = NONE;
	bool file_loaded = false;

	cache_config.has_cache = 0;
	
	atexit(*(exit_handler));
	// input_file = malloc(sizeof(char) * 256); // Allocate memory for the input file_name // TODO: make this be fixed size and put it in d-segment

	// CLI Switches
    while(*(++argv) != NULL) {
            
		if (strcmp(*argv,"--smc")==0 || strcmp(*argv,"--self-modifying-code")==0) {
			text_write_enabled = true;
		}
    }

	srand(time(NULL));

	// Initialization
	reset_backend(true, cache_config);

	init_frontend();
	set_frontend_register_pointer(get_register_pointer());
	set_frontend_pc_pointer(get_pc_pointer());
	set_frontend_memory_pointer(get_memory_pointer(), MEMORY_SIZE);
	set_breakpoints_pointer(get_breakpoints_pointer());

	memory_template = malloc(sizeof(uint8_t)* MEMORY_SIZE);

	FILE* fp = NULL;

	// Main loop
	// Polls for updates from the frontend, and processes them
	while (1) {
		switch (frontend_update()) {
			case LOAD:
				
				fp = fopen(input_file, "r");

				if (!fp) {
					show_error("Failed to read %s!", input_file);
					break;
				}

				fseek(fp, 0L, SEEK_END);
				long len = ftell(fp);
				fseek(fp, 0L, SEEK_SET);

				char* new_cleaned_code = malloc(sizeof(char) * len+1);
				if (!new_cleaned_code) {
					show_error("Out Of Memory!");
					break;
				}
				
				label_index* new_index_of_labels = new_label_index();

				uint8_t* new_memory_template = malloc(sizeof(uint8_t)* MEMORY_SIZE);
				memset(new_memory_template, 0, sizeof(uint8_t)*MEMORY_SIZE);

				hexcode = assembler_main(fp, new_cleaned_code, new_index_of_labels, new_memory_template);
				fclose(fp);

				// If assembler failed, free temporary memory and abort
				if (!hexcode) {
					free(new_cleaned_code);
					free(new_index_of_labels);
					free(new_memory_template);
					break;
				}
				
				// Else, update state
				strcpy(active_file, input_file);
				active_file[strlen(active_file)-2] = '\0';
				snprintf(cache_config.trace_file_name, 300, "%s.output", active_file);

				if (index_of_labels) free_label_index(index_of_labels);
				index_of_labels = new_index_of_labels;

				if (cleaned_code) free(cleaned_code);
				cleaned_code = new_cleaned_code;

				if (memory_template) free(memory_template);
				memory_template = new_memory_template;

				if (get_section_label(index_of_labels, 0) == -1) prepend_label(index_of_labels, "main", 0); // Adding main to stack if there is no label at the start
				index_dedup(index_of_labels);
				if (stack) st_free(stack);
				stack = new_stacktrace(index_of_labels);
				st_push(stack, 0);

				reset_backend(true, cache_config);
				reset_frontend(true);

				// Write data segment and instructions into memory
				memcpy(get_memory_pointer()->data, &hexcode[1], hexcode[0]*4); // hexcode[0] is implicitly the length in words. actual hexcode starts from hexcode[1]
				memcpy(get_memory_pointer()->data+DATA_BASE, memory_template+DATA_BASE, MEMORY_SIZE-DATA_BASE);

				// Give frontend new pointers to data in backend
				update_code(cleaned_code, hexcode[0]);
				set_stack_pointer(stack);
				set_stacktrace_pointer(stack);
				set_breakpoints_pointer(get_breakpoints_pointer());
				set_frontend_memory_pointer(get_memory_pointer(), MEMORY_SIZE);
				set_labels_pointer(index_of_labels);
				set_hexcode_pointer((uint32_t*) &hexcode[1]);
				
				file_loaded = true;
				break;

			case RUN:
				int result = run(&frontend_update);
				release_run_lock();
				if (result == 2) show_error("Execution stopped at breakpoint!");
				else if (result == 1) {
					show_error("Reached End of Program");
				}
				break;

			case RESET:
				// Reset stack
				if (stack) st_free(stack);
				stack = new_stacktrace(index_of_labels);
				st_push(stack, 0);

				reset_backend(false, cache_config);
				reset_frontend(false);

				// Give frontend new pointers to data in backend
				set_stack_pointer(stack);
				set_stacktrace_pointer(stack);
				set_breakpoints_pointer(get_breakpoints_pointer());
				
				// Reset data segment and instructions in memory
				memcpy(get_memory_pointer()->data, &hexcode[1], hexcode[0]*4);
				memcpy(get_memory_pointer()->data+DATA_BASE, memory_template+DATA_BASE, MEMORY_SIZE-DATA_BASE);
				set_hexcode_pointer((uint32_t*) &hexcode[1]);
				break;

			case EXIT:
				goto exit;

			case SHOW_STACK:
				break;

			case STEP:
				if (step() == 1) show_error("Nothing to step");
				break;

			case CACHE_DISABLE:
				if (!cache_config.has_cache) {
					show_error("Cache is already disabled!");
					break;
				}

				cache_config.has_cache = false;

				reset_backend(true, cache_config);
				reset_frontend(false);

				// Give frontend new pointers to data in backend
				set_breakpoints_pointer(get_breakpoints_pointer());
				set_frontend_memory_pointer(get_memory_pointer(), MEMORY_SIZE);
				
				if (file_loaded) {
					// Reset stack
					if (stack) st_free(stack);
					stack = new_stacktrace(index_of_labels);
					st_push(stack, 0);
					
					set_stack_pointer(stack);
					set_stacktrace_pointer(stack);

					// Reset data segment and instructions in memory
					memcpy(get_memory_pointer()->data, &hexcode[1], hexcode[0]*4);
					memcpy(get_memory_pointer()->data+DATA_BASE, memory_template+DATA_BASE, MEMORY_SIZE-DATA_BASE);
					set_hexcode_pointer((uint32_t*) &hexcode[1]);
				}	

				break;

			case CACHE_ENABLE:
				fp = fopen(input_file, "r");

				if (!fp) {
					show_error("Failed to open %s!", input_file);
					break;
				}

				CacheConfig new_config = read_cache_config(fp);

				if (!new_config.has_cache) break;
				cache_config = new_config;				

				reset_backend(true, cache_config);
				reset_frontend(false);

				// Give frontend new pointers to data in backend
				set_breakpoints_pointer(get_breakpoints_pointer());
				set_frontend_memory_pointer(get_memory_pointer(), MEMORY_SIZE);
				
				if (file_loaded) {
					// Reset stack
					if (stack) st_free(stack);
					stack = new_stacktrace(index_of_labels);
					st_push(stack, 0);

					set_stack_pointer(stack);
					set_stacktrace_pointer(stack);

					// Reset data segment and instructions in memory
					memcpy(get_memory_pointer()->data, &hexcode[1], hexcode[0]*4);
					memcpy(get_memory_pointer()->data+DATA_BASE, memory_template+DATA_BASE, MEMORY_SIZE-DATA_BASE);
					set_hexcode_pointer((uint32_t*) &hexcode[1]);
				}

				fclose(fp);
				break;

			case CACHE_DUMP:
				if (!cache_config.has_cache) {
					show_error("Cache is disabled!");
					break;
				}

				fp = fopen(input_file, "w");

				if (!fp) {
					show_error("Failed to open %s!", input_file);
					break;
				}

				dump_cache(get_memory_pointer(), fp);

				fclose(fp);
				show_error("Cache dump successful!");
				break;

			case CACHE_INVALIDATE:
				if (!cache_config.has_cache) {
					show_error("Cache is disabled!");
					break;
				}

				invalidate_cache(get_memory_pointer());
				break;
		}
	}

	exit:
	return 0;
}