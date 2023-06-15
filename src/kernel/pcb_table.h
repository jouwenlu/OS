// Declaration of PCB Table functions.

#pragma once

#include "scheduler.h"
#include "../lib/pcb.h"
#include "../shell/shell.h"

// Initialize an PCB Table.
void init_pcb_table();

// Add PCB to PCB Table.
void add_pcb_to_table(pcb *to_add);

// Find PCB in the linked list.
pcb *find_pcb_in_table(int pid);

// Delte PCB in the linked list. Does not free.
void delete_pcb_in_table(int pid);

// Returns a pointer to the PCB table.
linked_list *get_process_table();

// Clears and frees all the entries in the entire PCB table.
void free_process_table();
