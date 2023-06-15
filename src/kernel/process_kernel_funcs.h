// Function declarations for process related kernel-level functions.

#pragma once

#include "../lib/pcb.h"

// Creates a new child thread and associated PCB.
pcb *k_process_create(pcb *parent);

// Kills the process referenced by process with the signal signal.
void k_process_kill(pcb *process, int signal);

// Called when a processes' resources need to be cleaned up/freed.
void k_process_cleanup(pcb *process);