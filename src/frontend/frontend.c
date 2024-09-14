#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../assembler/index.h"
#include "../assembler/vec.h"
#include "frontend.h"
#include "../globals.h"
#include <ncurses.h>

#define C_NORMAL 0
#define C_OFF_NORMAL 1
#define C_ERROR 2
#define C_RUNNING 3
#define C_TERMINAL 4

#define COLOR_GRAY COLOR_CYAN

int rows, columns;
int cursor=0;
int input;
int code_scroll;
int lines_of_code=0;
char* last_command;
char* input_buffer;
size_t input_buffer_len;
bool initialized = false;
bool showing_error = false;
bool color_mode = false;
bool run_lock = false;

uint64_t* regs;
uint64_t* pc;
uint8_t* memory;
char** code=NULL;
uint32_t* hexcode=NULL;
vec* breakpoints;
label_index* labels;

void set_frontend_register_pointer(uint64_t* regs_pointer) {regs = regs_pointer;}
void set_frontend_pc_pointer(uint64_t* pc_pointer) {pc = pc_pointer;}
void set_frontend_memory_pointer(uint8_t* memory_pointer) {memory = memory_pointer;}
void set_breakpoints_pointer(vec* breakpoints_pointer) {breakpoints = breakpoints_pointer;}
void set_labels_pointer(label_index* index) {free(labels); labels = index;}
void set_hexcode_pointer(uint32_t* hexcode_pointer) {hexcode = hexcode_pointer;}
void set_run_lock() {run_lock = true;}
void release_run_lock() {
    run_lock = false;
    showing_error = false;
    curs_set(1);
    input_buffer[0] = '$';
    input_buffer[1] = '\0';
    input_buffer_len = 1;
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
            attron(COLOR_PAIR(C_OFF_NORMAL));
        } else {
            attroff(COLOR_PAIR(C_OFF_NORMAL));
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

    
    if (inst_len<1) return;

    for (int i=0; i<(n_lines); i++) {
        mvprintw(y+2+i, x+5, "% 5d %04x: %.*s ", (i+code_scroll), (i+code_scroll)*4, inst_len, code[i+code_scroll]);
        mvprintw(y+2+i, x+w-2-12, "%08X %s ", hexcode[i+code_scroll], "  ");
    }

    unsigned i = *pc/4;
    if (i>=code_scroll && i<n_lines+code_scroll) {
        size_t size = sizeof(char) * (w+1);
        char* line = malloc(size);
        snprintf(line, w+1, "% 5d %04x: %.*s", i, (i)*4, inst_len, code[i]);
        int space_count = w-strlen(line)-19;

        attron(COLOR_PAIR(C_RUNNING));
        mvprintw(y+2+i-code_scroll, x+5, "%s%*s%08X %s ", line, space_count, "", hexcode[i], "EX");
        attroff(COLOR_PAIR(C_RUNNING));
        if (line) free(line);
    }

        for (int i=0; i<breakpoints->len; i++) {
            mvaddch(y+2+breakpoints->values[i], x+3, '>'); // ADd scrolling
        }
}

int init_frontend() {

    initscr();
    raw();
    keypad(stdscr, TRUE);
    halfdelay(2);
    noecho();

    if (has_colors()) {
        color_mode = true;
        start_color();

        init_color(COLOR_CYAN, 250, 250, 250);
        init_pair(C_OFF_NORMAL, COLOR_WHITE, COLOR_GRAY);
        init_pair(C_ERROR, COLOR_WHITE, COLOR_RED);
        init_pair(C_RUNNING, COLOR_WHITE, COLOR_RED);
        init_pair(C_TERMINAL, COLOR_GREEN, COLOR_BLACK);
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

void draw() {
    getmaxyx(stdscr, rows, columns);

    int input_root_x = 0, input_root_y = rows-4, input_h=4, input_w=columns;
    int register_root_x = 0.75*columns, register_root_y = 0, register_w=columns-register_root_x, register_h=input_root_y+1;
    int stack_root_x = 0.5*columns, stack_root_y = 0, stack_w=register_root_x-stack_root_x+1, stack_h=input_root_y+1;
    int code_root_x = 0, code_root_y = 0, code_w=stack_root_x+1, code_h=input_root_y+1;
    
    erase();
    draw_outline_rect(0,0,rows,columns);
    draw_outline_rect(input_root_x, input_root_y, input_h, input_w);
    draw_outline_rect(register_root_x, register_root_y, register_h, register_w);
    draw_outline_rect(stack_root_x, stack_root_y, stack_h, stack_w);
    draw_outline_rect(code_root_x, code_root_y, code_h, code_w);

    write_centered(register_root_x, register_root_y, register_w, "REGISTERS");
    write_centered(stack_root_x, stack_root_y, stack_w, "STACK");
    write_centered(code_root_x, code_root_y, code_w, "CODE");

    mvprintw(0,0,"PC is: %lu %d %d", *pc, code_scroll, lines_of_code);
    write_regs(register_root_x, register_root_y, register_h, register_w);
    write_code(code_root_x, code_root_y, code_h, code_w);

    if (showing_error) attron(COLOR_PAIR(C_ERROR));
    else attron(COLOR_PAIR(C_TERMINAL));

    mvaddstr(input_root_y+1, input_root_x+1+cursor, input_buffer);
    
    if (showing_error) attroff(COLOR_PAIR(C_ERROR));
    else attroff(COLOR_PAIR(C_TERMINAL));

    refresh();
}

void parse_intruction() {
    input_buffer[0] = '$';
    input_buffer[1] = '\0';
    input_buffer_len = 1;
}

void destroy_frontend() {
    initialized = false;
    endwin();
}

void show_error(char* format, ...) {
    va_list args;
    va_start(args, format);

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

    if (input == KEY_F(1)) { 
        return EXIT;
    }

    if (input == KEY_F(6)) {
        return STOP;
    }

    if (input == KEY_UP) {
        code_scroll = code_scroll==0?code_scroll:code_scroll-1;
    }

    if (input == KEY_DOWN) {
        code_scroll = code_scroll<=(lines_of_code-10)?code_scroll+1:code_scroll;
    }

    if (run_lock) return input=='q'?STOP:NONE;

    if (input == KEY_F(8)) {
        return STEP;
    }

    if (input == KEY_F(5)) {
        return RUN;
    }

    if (showing_error) {
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
            strcpy(input_file, last_command+6);
            return LOAD;
        } else if (!strncmp("$break ", last_command, 7)) {
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

            return NONE;

        } else if (last_command_len == 4 && !strcmp("$run", last_command)) {
            set_run_lock();
            show_error("Running! Use F6 or q to stop execution");
            return RUN;

        } else if (last_command_len == 5 && !strcmp("$step", last_command)) {
            return STEP;

        } else if (last_command_len == 5 && !strcmp("$regs", last_command)) {
            show_error("Registers are already shown on the right pane!");

        } else if (last_command_len == 5 && !strcmp("$exit", last_command)) {
            return EXIT;

        } else {
            show_error("Invalid command");
        }

    } else if (input>31 && input<127 && input_buffer_len<columns-3) {

        input_buffer[input_buffer_len] = input;
        input_buffer[input_buffer_len+1] = '\0';
        input_buffer_len += 1;
    }

    return NONE;
}