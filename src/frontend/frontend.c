#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include "../assembler/index.h"
#include "../assembler/vec.h"
#include "frontend.h"
#include "../globals.h"
#include "../backend/stacktrace.h"

#define C_NORMAL 0
#define C_OFF_NORMAL 1
#define C_ERROR 2
#define C_RUNNING 3
#define C_TERMINAL 4
#define C_NORMAL_HIGHLIGHT 5
#define C_OFF_NORMAL_HIGHLIGHT 6

#define COLOR_GRAY COLOR_CYAN

static int rows=0, columns=0;
static int cursor=0;
static int input=ERR;
static int code_scroll=0;
static int aux_scroll=0;
static int lines_of_code=0;
static char* last_command = NULL;;
static char* input_buffer = NULL;;
static size_t input_buffer_len;
static bool initialized = false;
static bool showing_error = false;
static bool showing_mem = false;
static bool color_mode = false;
static bool run_lock = false;
static bool showing_run_lock = false;
static MEVENT mouse;

static uint64_t last_reg_write = -2;
static uint64_t memory_size = 0;
static uint64_t* regs = NULL;
static uint64_t* pc = NULL;
static uint8_t* memory = NULL;
static int* code_v_offsets = NULL;
static char** code = NULL;
static uint32_t* hexcode = NULL;
static vec* breakpoints = NULL;
static label_index* labels = NULL;
static stacktrace* stack = NULL;

void set_frontend_register_pointer(uint64_t* regs_pointer) {regs = regs_pointer;}
void set_frontend_pc_pointer(uint64_t* pc_pointer) {pc = pc_pointer;}
void set_frontend_memory_pointer(uint8_t* memory_pointer, uint64_t size_of_memory) {memory = memory_pointer; memory_size = size_of_memory;}
void set_breakpoints_pointer(vec* breakpoints_pointer) {breakpoints = breakpoints_pointer;}
void set_stack_pointer(stacktrace* stacktrace) {stack = stacktrace;}
void set_hexcode_pointer(uint32_t* hexcode_pointer) {hexcode = hexcode_pointer;}
void set_run_lock() {run_lock = true; showing_run_lock = true;}
void set_reg_write(uint64_t reg) {last_reg_write = reg;}

void reset_frontend(bool hard) {
    if (hard) code_scroll = 0;
    last_reg_write = -2;
    if (!showing_mem) aux_scroll = 0;
    if (hard) vec_clear(breakpoints);
}

void release_run_lock() {
    run_lock = false;
    if (showing_run_lock) {    
        showing_error = false;
        curs_set(1);
        input_buffer[0] = '$';
        input_buffer[1] = '\0';
        input_buffer_len = 1;
    }
}

// Must always be called after setting code
void set_labels_pointer(label_index* index) {
    labels = index;
    int offset = 0;
    int last = 0;

    if (code_v_offsets) free(code_v_offsets);
    code_v_offsets = malloc(sizeof(int)*lines_of_code);

    for (int i=0; i<index->len; i++) {

        // if (index->positions[i] == last) continue;

        for(int j=last; j<index->positions[i]; j++) {
            code_v_offsets[j] = offset+j;
        }

        last = index->positions[i];
        offset += 2;
    }

    for(int j=last; j<lines_of_code; j++) {
        code_v_offsets[j] = offset+j;
    }
}

void update_code(char* code_pointer, uint64_t n) {
    if (code) free(code);
    
    code = malloc(sizeof(char*)*(n+1));
    code[0] = code_pointer;
    int i=1;
    char c;

    lines_of_code = n;

    while((c=*(code_pointer++)) != '\0') {
        if (c=='\n') {
            code[i] = code_pointer;
            *(code_pointer-1) = '\0';
            i++;
        }
    }
}

void draw_outline_rect(int x, int y, int h, int w) {
    mvaddch(y,x,'+');
    for (int i=2; i<w; i++) addch('-');
    addch('+');

    mvaddch(y+h-1,x,'+');

    for (int i=2; i<w; i++) addch('-');
    addch('+');

    for (int i=2; i<h; i++) {
        mvaddch(y+i-1,x,'|');
        mvaddch(y+i-1,x+w-1,'|');
    }
}

int write_centered(int x, int y, int w, char* string) {
    int len = strlen(string);
    if(len>w) return 1;

    int offset = (w-len)/2;

    mvaddstr(y, x+offset, string);
    return 0;
}

void write_regs(int x, int y, int h, int w) {
    if(w<26) return;

    int padding = (w-26)/3;
    int name_x = x + 2 + padding;
    int value_x = x + 2 + 3 + 1 + 2*padding;
    bool color_toggle = false;

    for(int i=0; i<((h>36)?32:(h-4)); i++) {

        if (color_toggle) {
            if (i-1==last_reg_write) attroff(COLOR_PAIR(C_NORMAL_HIGHLIGHT));
            else attroff(COLOR_PAIR(C_NORMAL));

            if (i==last_reg_write) attron(COLOR_PAIR(C_OFF_NORMAL_HIGHLIGHT));
            else attron(COLOR_PAIR(C_OFF_NORMAL));
        } else {
            if (i-1==last_reg_write) attroff(COLOR_PAIR(C_OFF_NORMAL_HIGHLIGHT));
            else attroff(COLOR_PAIR(C_OFF_NORMAL));

            if (i==last_reg_write) attron(COLOR_PAIR(C_NORMAL_HIGHLIGHT));
            else attron(COLOR_PAIR(C_NORMAL));
        }

        color_toggle = !color_toggle;

        mvprintw(y+2+i, name_x, "x%02d %*s 0x%016lX", i, padding, "", regs[i]);
        //mvprintw(y+2+i, value_x, "0x%016lX", regs[i]);
    }

    attroff(COLOR_PAIR(C_OFF_NORMAL));
}

void write_code(int x, int y, int h, int w) {
    
    if (!code) return;
    
    int inst_len = w-32;
    int n_lines = lines_of_code-code_scroll<h-4?lines_of_code-code_scroll:h-4;
    unsigned i=0;

    if (inst_len<1) return;
    char* label = NULL;

    while (i<n_lines && y+2+code_v_offsets[i+code_scroll]-code_scroll < w-1) {
        mvprintw(y+2+code_v_offsets[i+code_scroll]-code_scroll, x+5, "% 5d %04x: %.*s ", (i+code_scroll), (i+code_scroll)*4, inst_len, code[i+code_scroll]);
        mvprintw(y+2+code_v_offsets[i+code_scroll]-code_scroll, x+w-2-12, "%08X %s ", hexcode[i+code_scroll], "  ");
        i++;
    }

    i = *pc/4;
    if (i>=code_scroll && i<n_lines+code_scroll) {
        size_t size = sizeof(char) * (w+1);
        char* line = malloc(size);
        snprintf(line, w+1, "% 5d %04x: %.*s", i, (i)*4, inst_len, code[i]);
        int space_count = w-strlen(line)-19;

        attron(COLOR_PAIR(C_RUNNING));
        mvprintw(y+2+code_v_offsets[i+code_scroll]-code_scroll, x+5, "%s%*s%08X %s ", line, space_count, "", hexcode[i], "EX");
        attroff(COLOR_PAIR(C_RUNNING));
        if (line) free(line);
    }

    int pos;
    for (int i=0; i<breakpoints->len; i++) {
        pos = breakpoints->values[i]-code_scroll;
        if (pos>=0 && pos<n_lines) mvaddch(y+2+pos, x+3, '>'); // ADd scrolling
    }

    for (int i=0; i<labels->len; i++) {
        int label_y = y+2+code_v_offsets[code_v_offsets[labels->positions[i]]]-code_scroll-1;
        printf("%d\n", label_y);
        if (label_y>=code_scroll && label_y<n_lines+code_scroll) mvaddstr(label_y, x+2, labels->labels[i]);;
    }
}

void new_write_code(int x, int y, int h, int w) {

    if (!code) return;
    
    int inst_len = w-32;
    if (inst_len<1) return;

    int num_lines = code_v_offsets[lines_of_code-1]-code_scroll+1;
    num_lines = num_lines<h-4?num_lines:h-4;

    int print_y = 0;
    int pos;

    for (int i=0; i<lines_of_code; i++) {
        print_y = code_v_offsets[i]-code_scroll;
        if (print_y >= 0 && print_y < num_lines) {
            mvprintw(y+2+print_y, x+5, "% 5d %04x: %.*s ", (i), (i)*4, inst_len, code[i]);
            mvprintw(y+2+print_y, x+w-2-12, "%08X %s ", hexcode[i], "  ");
        }
    }

    pos = *pc/4;
    print_y = pos<lines_of_code?code_v_offsets[pos]-code_scroll:-1;
    if (print_y>=0 && print_y<num_lines) {
        size_t size = sizeof(char) * (w+1);
        char* line = malloc(size);
        snprintf(line, w+1, "% 5d %04x: %.*s", pos, (pos)*4, inst_len, code[pos]);
        int space_count = w-strlen(line)-19;

        attron(COLOR_PAIR(C_RUNNING));
        mvprintw(y+2+print_y, x+5, "%s%*s%08X %s ", line, space_count, "", hexcode[pos], "EX");
        attroff(COLOR_PAIR(C_RUNNING));
        if (line) free(line);
    }

    for (int i=0; i<breakpoints->len; i++) {
        pos = breakpoints->values[i];
        print_y = code_v_offsets[pos]-code_scroll;
        if (print_y>=0 && pos<num_lines) mvaddch(y+2+print_y, x+3, '>');
    }

    for (int i=0; i<labels->len; i++) {
        pos = labels->positions[i];
        print_y = code_v_offsets[pos]-code_scroll-1;
        if (print_y>=0 && print_y<num_lines) mvprintw(y+2+print_y, x+7, "<%s>:", labels->labels[i]);
    }
}

int init_frontend() {

    initscr();
    raw();
    keypad(stdscr, TRUE);
    halfdelay(1);
    noecho();
    mousemask(ALL_MOUSE_EVENTS, NULL);
    mouse.x = 0;

    if (has_colors()) {
        color_mode = true;
        start_color();

        init_color(COLOR_CYAN, 250, 250, 250);
        init_pair(C_OFF_NORMAL, COLOR_WHITE, COLOR_GRAY);
        init_pair(C_ERROR, COLOR_WHITE, COLOR_RED);
        init_pair(C_RUNNING, COLOR_WHITE, COLOR_RED);
        init_pair(C_TERMINAL, COLOR_GREEN, COLOR_BLACK);
        init_pair(C_NORMAL_HIGHLIGHT, COLOR_WHITE, COLOR_YELLOW);
        init_pair(C_OFF_NORMAL_HIGHLIGHT, COLOR_WHITE, COLOR_YELLOW);
    }

    input_buffer = malloc((getmaxx(stdscr)-1)*sizeof(char));
    last_command = malloc((getmaxx(stdscr)-1)*sizeof(char));

    if(!input_buffer || !last_command) return 1;

    input_buffer[0] = '$';
    input_buffer[1] = '\0';
    input_buffer_len = 1;

    initialized = true;
    return 0;
}

void write_memory(int x, int y, int w, int h) {
    int padding = w-6-3-11;

    if (padding<2) return;

    int last_line = aux_scroll+(h-5);
    int offset=0;
    bool color_toggle = false;
    if (last_line > memory_size-1) last_line = memory_size-1;

    for (int i=aux_scroll; i<=last_line; i++) {

        if (color_toggle) {
            attron(COLOR_PAIR(C_OFF_NORMAL));
        } else {
            attroff(COLOR_PAIR(C_OFF_NORMAL));
        }

        color_toggle = !color_toggle;

        mvprintw(y+2+offset, x+2+padding/4, "0x%08X %*s 0x%02x", i, padding/2, "", memory[i]);
        offset++;
    }

    attroff(COLOR_PAIR(C_OFF_NORMAL));
}

void write_stack(int x, int y, int w, int h) {
    
    if (!stack) return;

    if (stack->len == 0) {
        write_centered(x, y+2, w, "Empty Call Stack: Execution complete");
        return;
    }

    char* line = malloc(sizeof(char)*w);

    int last_line = aux_scroll+(h-5);
    int offset = 0;
    if (last_line > stack->len) last_line = stack->len;

    for (int i=aux_scroll; i<last_line; i++) {
        if (stack->label_indices->values[i] < 0) continue;
        snprintf(line, w-4, "(%s:%ld)", labels->labels[stack->label_indices->values[i]], stack->stack->values[i]);
        write_centered(x, y+2+offset, w, line);
        offset++;
    }

    free(line);

}

void draw() {
    getmaxyx(stdscr, rows, columns);

    int input_root_x = 0, input_root_y = rows-4, input_h=4, input_w=columns;
    int register_root_x = 0.75*columns, register_root_y = 0, register_w=columns-register_root_x, register_h=input_root_y+1;
    int aux_root_x = 0.5*columns, aux_root_y = 0, aux_w=register_root_x-aux_root_x+1, aux_h=input_root_y+1;
    int code_root_x = 0, code_root_y = 0, code_w=aux_root_x+1, code_h=input_root_y+1;
    
    erase();
    draw_outline_rect(0,0,rows,columns);
    draw_outline_rect(input_root_x, input_root_y, input_h, input_w);
    draw_outline_rect(register_root_x, register_root_y, register_h, register_w);
    draw_outline_rect(aux_root_x, aux_root_y, aux_h, aux_w);
    draw_outline_rect(code_root_x, code_root_y, code_h, code_w);

    write_centered(register_root_x, register_root_y, register_w, "REGISTERS");
    write_centered(aux_root_x, aux_root_y, aux_w, showing_mem?"MEMORY":"STACK");
    write_centered(code_root_x, code_root_y, code_w, "CODE");

    mvprintw(0,0,"PC: %08lX", *pc);
    write_regs(register_root_x, register_root_y, register_h, register_w);
    if (showing_mem) write_memory(aux_root_x, aux_root_y, aux_w, aux_h);
    else write_stack(aux_root_x, aux_root_y, aux_w, aux_h);
    new_write_code(code_root_x, code_root_y, code_h, code_w);

    //mvprintw(input_root_y+2, input_root_x+input_w-15, "PC=0x%08lX", *pc);
    
    if (showing_error) attron(COLOR_PAIR(C_ERROR));
    else attron(COLOR_PAIR(C_TERMINAL));

    mvaddstr(input_root_y+1, input_root_x+1+cursor, input_buffer);
    
    if (showing_error) attroff(COLOR_PAIR(C_ERROR));
    else attroff(COLOR_PAIR(C_TERMINAL));

    refresh();
}

void destroy_frontend() {
    initialized = false;
    endwin();
}

void show_error(char* format, ...) {
    va_list args;
    va_start(args, format);
    showing_run_lock = false;

    if (initialized) {
        vsprintf(input_buffer, format, args);
        curs_set(0);
        showing_error = true;
    }
    else vprintf(format, args);

    va_end(args);
}

Command frontend_update() {	
    draw();
    input = getch();

    if (input == ERR) {
        return NONE;
    }

    if (input == KEY_MOUSE) {
        getmouse(&mouse);

        if (mouse.bstate == BUTTON4_PRESSED) {
            if (mouse.x<columns/2) code_scroll = code_scroll==0?code_scroll:code_scroll-1;
            else if (mouse.x<3*columns/4) {
                if (showing_mem) aux_scroll = aux_scroll==0?aux_scroll:aux_scroll-1;
                else aux_scroll = aux_scroll==0?aux_scroll:aux_scroll-1;
            }
        }

        if (mouse.bstate == BUTTON5_PRESSED) {
            if (mouse.x<columns/2) code_scroll = code_scroll<=code_v_offsets[lines_of_code-1]-5?code_scroll+1:code_scroll;
            else if (mouse.x<3*columns/4) {
                if (showing_mem) aux_scroll = aux_scroll<=(memory_size-5)?aux_scroll+1:aux_scroll;
                else aux_scroll = aux_scroll<=(stack->len-5)?aux_scroll+1:aux_scroll;
            }
        }

        return NONE;
    }

    if (input == KEY_F(1)) { 
        if (run_lock) {
            show_error("Stopping execution, press F1 again to exit");
            return STOP;
        }
        return EXIT;
    }

    if (input == KEY_F(6)) {
        if (run_lock) {
            showing_error = false;
            return STOP;
        }
        show_error("Nothing is running!");
        return NONE;
    }

    if (input == KEY_UP) {
        if (mouse.x<columns/2) code_scroll = code_scroll==0?code_scroll:code_scroll-1;
        else if (mouse.x<3*columns/4) {
            if (showing_mem) aux_scroll = aux_scroll==0?aux_scroll:aux_scroll-1;
            else aux_scroll = aux_scroll==0?aux_scroll:aux_scroll-1;
        }
    }

    if (input == KEY_DOWN) {
        if (mouse.x<columns/2) code_scroll = code_scroll<=code_v_offsets[lines_of_code-1]-5?code_scroll+1:code_scroll;
        else if (mouse.x<3*columns/4) {
            if (showing_mem) aux_scroll = aux_scroll<=(memory_size-5)?aux_scroll+1:aux_scroll;
            else aux_scroll = aux_scroll<=(stack->len-5)?aux_scroll+1:aux_scroll;
        }
    }

    if (input == KEY_F(8)) {
        if (run_lock) {
            show_error("Command invalid while running!");
            return NONE;
        }
        return STEP;
    }

    if (input == KEY_F(5)) {
        if (run_lock) {
            show_error("Already running, Press F6 or type \"stop\" to stop!");
            return NONE;
        }

        if (*pc/4 >= lines_of_code) {
            show_error("Nothing to run! use reset command to reset");
            return NONE;
        }

        set_run_lock();
        show_error("Running! Press F6 or type \"stop\" to stop execution");
        return RUN;
    }

    if (showing_error) {
        showing_run_lock = false;
        showing_error = false;
        curs_set(1);
        input_buffer[0] = '$';
        input_buffer[1] = '\0';
        input_buffer_len = 1;

        if (input>31 && input<127 && input_buffer_len<columns-3) {

            input_buffer[input_buffer_len] = input;
            input_buffer[input_buffer_len+1] = '\0';
            input_buffer_len += 1;
        }

        return NONE;
    }

    if ((input == KEY_BACKSPACE) && input_buffer_len>1) {
        input_buffer_len -= 1;
        input_buffer[input_buffer_len] = '\0';
    }

    if (input == KEY_ENTER || input == 10) {
        strcpy(last_command, input_buffer);

        input_buffer[0] = '$';
        input_buffer[1] = '\0';
        int last_command_len = input_buffer_len;
        input_buffer_len = 1;

        if (!strncmp("$load ", last_command, 6)) {
            if (run_lock) {
                show_error("Command invalid while running!");
                return NONE;
            }
            strcpy(input_file, last_command+6);
            return LOAD;

        } else if (!strncmp("$break ", last_command, 7)) {

            if (run_lock) {
                show_error("Command invalid while running!");
                return NONE;
            }

            char* end_ptr = NULL;
            unsigned long break_line = strtol(last_command+7, &end_ptr, 10);

            if (end_ptr-last_command != strlen(last_command)) {
                show_error("Failed to parse line number");
                return NONE;
            }

            if (break_line < 0 | break_line > lines_of_code-1) {
                show_error("Invalid Line number");
                return NONE;
            }

            for (int i=0; i<breakpoints->len; i++) {
                if (break_line==breakpoints->values[i]) {
                    vec_remove(breakpoints, i);
                    return NONE;
                }
            }

            append(breakpoints, break_line);

        } else if (!strncmp("$mem", last_command, 4)) {

            if (strlen(last_command) == 4) {
                showing_mem = true;
                return NONE;
            }

            char* end_ptr = NULL;
            uint64_t new_addr, count;

            if (last_command[5] == '0' && last_command[6] == 'b') {
                new_addr = strtoul(last_command+7, &end_ptr, 2);
            }
            else {
                new_addr = strtoul(last_command+5, &end_ptr, 0);
            }

            if (*end_ptr != ' ' && *end_ptr != '\0') {
                show_error("Invalid Memory Address!");
                return NONE;
            }

            if (new_addr >= memory_size) {
                show_error("Memory Address out of bounds!");
                return NONE;
            }
            
            aux_scroll = new_addr;
            showing_mem = true;

        } else if (last_command_len == 4 && !strcmp("$run", last_command)) {

            if (run_lock) {
                show_error("Already running, Press F6 or type \"stop\" to stop!");
                return NONE;
            }

            if (*pc/4 >= lines_of_code) {
                show_error("Nothing to run! use reset command to reset");
                return NONE;
            }

            set_run_lock();
            show_error("Running! Press F6 or type \"stop\" to stop execution");
            return RUN;

        } else if (last_command_len == 5 && !strcmp("$step", last_command)) {
            if (run_lock) {
                show_error("Command invalid while running!");
                return NONE;
            }
            return STEP;

        } else if (last_command_len == 6 && !strcmp("$reset", last_command)) {
            if (run_lock) {
                show_error("Command invalid while running!");
                return NONE;
            }
            return RESET;

        } else if (last_command_len == 11 && !strcmp("$show-stack", last_command)) {
            if (!showing_mem) show_error("Stack Trace is already shown on the right!");
            else aux_scroll = 0;
            showing_mem = false;

        } else if (last_command_len == 5 && !strcmp("$regs", last_command)) {
            show_error("Registers are already shown on the right pane!");

        } else if (last_command_len == 5 && !strcmp("$exit", last_command)) {
            if (run_lock) {
                show_error("Stopping execution, use exit again to exit");
                return STOP;
            }
            return EXIT;

        } else if (last_command_len == 5 && !strcmp("$stop", last_command)) {
            return STOP;

        } else {
            show_error("Invalid command");
        }

    } else if (input>31 && input<127 && input_buffer_len<columns-16) {

        input_buffer[input_buffer_len] = input;
        input_buffer[input_buffer_len+1] = '\0';
        input_buffer_len += 1;
    }

    return NONE;
}