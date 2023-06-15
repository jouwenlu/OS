// Definition of PCB Table functions.

#include "pcb_table.h"

linked_list process_table;

void init_pcb_table() {
    init_linked_list(&process_table);
}

void add_pcb_to_table(pcb *to_add) {
    push_back(&process_table, to_add);
}

pcb *find_pcb_in_table(int pid) {
    linked_list_elem *elem = get_elem(&process_table, pid_equal_predicate, &pid);
    return elem == NULL ? NULL : elem->val;
}

void delete_pcb_in_table(int pid) {
    extract_elem(&process_table, pid_equal_predicate, &pid);
}

linked_list *get_process_table() {
    return &process_table;
}

void free_process_table() {
    clear(&process_table, free_process_pcb);
}