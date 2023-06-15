// Declaration of functions for the scheduler.

#pragma once

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <ucontext.h>
#include <unistd.h>
#include <valgrind/valgrind.h>

#include "../lib/linked_list.h"
#include "../lib/pcb.h"

// Registers the sleeping job with the sleep handler.
void set_sleep_alarm(unsigned int ticks, pcb *job);

// Initializes the sleep job linked list.
void init_sleep_queue();

// Returns a pointer to the sleep job linked list.
linked_list *get_sleep_queue();

// Decrements the clock ticks in all pcbs stored in sleep queue. return one sleep job that finished.
pcb *decrement_ticks_in_sleep_queue();

// Called from kernel in k_process_create(), added a newly created context in the sheduler queues.
void add_job_to_scheduler(pcb *job);

// Job is not running anymore, so it removes a context from the scheduler queues.
bool remove_job_from_scheduler(pcb *job);

// Returns a pointer to the active job.
pcb *get_active_job();

// Resets the active job to NULL.
void reset_active_job();

// Frees all of the scheduler's resources.
void free_scheduler_resources();

// Calls sigsuspend to stop the current context.
void suspend(void);

// Initializes the scheduler's data structures.
void init_scheduler();

// Initializes the shell's data structures.
void init_shell();

// Main function for scheduler context.
void scheduler(void);
