// Main file for Penn Shell.

#pragma once

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../kernel/scheduler.h"
#include "../lib/linked_list.h"
#include "../lib/parser.h"
#include "../lib/pcb.h"
#include "job.h"

#define NO_ACTIVE_PROCESS -1

// Used to find a PCB with a specified PID.
bool pid_equal_predicate(void *pid, void *target_pcb);

// Signal handler for Shell signals.
void sig_handler(int signo);

// Register the signal handlers.
void register_sig_handlers();

// Waits on background jobs.
void refresh_background();

// Handles a user input with a specified priority to spawn the jobs with.
void handle_input(char *input, int priority);

// Main shell function.
void main_shell(void);

// Find a job with the specified job id as a string. If arg is NULL, then this
// is the last stopped job or the last backgrounded job.
job *find_bg_job(char *arg);

// Finds the minimum available job id to assign to the next job.
int min_available_id();

// Removes a background job with the specified pid.
void remove_bg_job(pid_t job_pid);

// Updates the foreground PID of the shell.
void update_fg_pid(pid_t pid);

// Returns the background queue.
linked_list *get_bg_queue();

// Clears and frees all the jobs in the background queue.
void free_bg_queue();
