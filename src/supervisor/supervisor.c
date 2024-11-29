#include <stdarg.h>
#include "supervisor.h"
#include "../globals.h"
#include <string.h>

static bool initialized = false;
static int case_count = -1;
static int substep = 0;
static FILE* test_file = NULL;
static bool run_lock = false;

static Memory* memory = NULL;
static uint64_t* regs = NULL;

int init_frontend() {return 0;}

void destroy_frontend() {return;}

void set_frontend_register_pointer(uint64_t* regs_pointer) {regs = regs_pointer;}
void set_frontend_pc_pointer(uint64_t* pc_pointer) {return;}
void set_frontend_memory_pointer(Memory* memory_pointer, uint64_t size_of_memory) {memory = memory_pointer;}
void set_breakpoints_pointer(vec* breakpoints_pointer) {return;}
void set_stack_pointer(stacktrace* stacktrace) {return;}
void set_reg_write(uint64_t reg) {return;}
void set_run_lock() {return;}
void set_hexcode_pointer(uint32_t* hexcode_pointer) {return;}

void set_labels_pointer(label_index* index) {return;}
void update_code(char* code_pointer, uint64_t n) {return;}
void show_error(char* format, ...) {
    if (tle_flag) {
        printf("Supervisor: Cycle Limit Exceeded while running testcase %d (step %d)!\n", case_count, substep);
        return;
    }
    
    va_list args;
    va_start(args, format);
    printf("Supervisor: While running testcase %d (step %d) An Error Occured:\n", case_count, substep);
    vprintf(format, args);
    printf("\n");
    va_end(args);
    exit(EXIT_FAILURE);
}
void release_run_lock() {run_lock = false;}
void reset_frontend(bool hard) {return;}

Command frontend_update() {
    if (run_lock) return NONE;
    if (case_count == -1) {
        strcpy(input_file, cli_input_file);
        case_count++;
        return LOAD;
    }

    substep++;
    switch (substep) {
        case 1: 
            return PATCH_DATA;

        case 2: 
            run_lock = true;
            return RUN_END;

        case 3:
            // Dump all things as applicable
            if (cli_regs) {
                for (int i=0; i<32; i++) {
                    switch (cli_output_type) {
                        case SIGNED: printf("%ld, ", regs[i]); break;
                        case UNSIGNED: printf("%lu, ", regs[i]); break;
                        case HEX: printf("0x%lx, ", regs[i]); break; 
                    }
                    // printf("%ld, ", regs[i]);
                }
                printf("\n");
            }

            // printf("%ld %ld\n", cli_mem_max, cli_mem_min);
            if (cli_mem_max) {
                for (int i=cli_mem_min; i<cli_mem_max; i+=8) {
                    switch (cli_output_type) {
                        case SIGNED: printf("%ld, ", *(int64_t*) (memory->data+i)); break;
                        case UNSIGNED: printf("%lu, ", *(uint64_t*) (memory->data+i)); break;
                        case HEX: printf("0x%lx, ", *(uint64_t*) (memory->data+i)); break; 
                    }
                }
                printf("\n");
            }

            // TODO: cache flushing and stats
            
            substep = 0; 
            if (cli_n_cases == ++case_count) return EXIT;
            else return NONE;
    }
};
