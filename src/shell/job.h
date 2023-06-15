// Header file for the Job data structure.

#pragma once

#include <stdbool.h>
#include <sys/types.h>

// Placeholder value for the PID of a process that has already finished.
#define FINISHED_PID_VAL -1

typedef struct job_st {
    // pid of corresponding process pcb 
    pid_t job_pid;

    // Job index to print in jobs built in command.
    int job_id;
} job;

// Creates a job object from the specified details. Uses malloc
// to dynamically allocate space for PIDs array and command string.
job *create_job(pid_t pid);

// Frees the dynamically allocated pointers in the specified job.
void free_job(void *j);

// Used to find a job in a linked list with a given pid.
bool job_equal_predicate(void *target_pid, void *curr_job);

// Used to find a job in a linked list with a given job id.
bool job_id_equal_predicate(void *target_job_id, void *curr_job);

// Prints a job to console for debugging.
void print_job(job *j);
