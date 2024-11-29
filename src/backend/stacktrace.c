#include "stacktrace.h"
#include "stdio.h"

void st_push(stacktrace* st, int line) {
    return;
    int label = get_section_label(st->index, line);

    append(st->stack, line);
    append(st->label_indices, label);
    
    st->len++;
}

void st_pop(stacktrace* st) {
    return;
    if (st->len == 0) return;
    st->len--;
    vec_remove(st->stack, st->len);
    vec_remove(st->label_indices, st->len);

}

void st_clear(stacktrace* st) {
    return;
    st->len = 0;
    vec_clear(st->stack);
    vec_clear(st->label_indices);
}

void st_update(stacktrace* st, int line) {
    return;
    st->stack->values[st->len-1] = line;
}

void st_free(stacktrace* st) {
    free_managed_array(st->stack);
    free_managed_array(st->label_indices);
    free(st);
}

stacktrace* new_stacktrace(label_index* index) {
    stacktrace* st = malloc(sizeof(stacktrace));

    st->index = index;
    st->label_indices = new_managed_array();
    st->stack = new_managed_array();
    st->len = 0;

    return st;
}