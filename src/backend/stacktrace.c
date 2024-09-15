#include "stacktrace.h"
#include "stdio.h"

void st_push(stacktrace* st, int line) {

    // printf("Attempted push %d\n", line);
    int label = get_section_label(st->index, line);
    // printf("Got %d\n", label);

    append(st->stack, line);
    append(st->label_indices, label);
    
    st->len++;
}

void st_pop(stacktrace* st) {

    st->len--;
    vec_remove(st->stack, st->len);
    vec_remove(st->label_indices, st->len);

}

void st_update(stacktrace* st, int line) {
    st->stack->values[st->len-1] = line;
}

stacktrace* new_stacktrace(label_index* index) {
    stacktrace* st = malloc(sizeof(stacktrace));

    st->index = index;
    st->label_indices = new_managed_array();
    st->stack = new_managed_array();
    st->len = 0;

    return st;
}