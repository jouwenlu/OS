// Header file for the PCB data structure. Adopted from the Job data structure
#pragma once

#include <stdbool.h>
#include <sys/types.h>
#include <ucontext.h>

#include "../lib/linked_list.h"
#include "../lib/macros.h"
#include "../lib/status.h"

// Placeholder value for the PID of a process that has already finished.
#define FINISHED_PID_VAL -1

typedef struct pcb_st {
    // Current status of the job.
    process_status status;

    // Whether the status has changed or not since last calling waitpid() on
    // this process
    bool status_changed;

    bool blocked;

    // context of the process
    ucontext_t *context;

    // pid for the process
    pid_t pid;

    // parent process
    struct pcb_st *parent;

    // Dynamically allocated string representing the pipeline name.
    char *cmd;

    // should be a nonzero starting value if cmd is sleep
    int remaining_clock_ticks;

    // zombie child processes
    linked_list *zombieLL;

    // nonzombie child processes
    linked_list *childLL;

    // priority level
    int priority;

    // True if this is a background process
    bool is_bg;

    // Array of open file descriptors
    // Index 0 is stdin fd, Index 1 is stdout fd
    int fd[MAX_OPEN_FDS];

    // Array of string arguments passed to the process. Should be dynamically
    // allocated and null terminated (i.e. last element is NULL).
    char **argv;
} pcb;

// Predicate for determining if a PCB in a linked list has the specified pid.
bool pid_equal_predicate(void *pid, void *target_pcb);

// Frees the fields in the pcb proc and proc itself.
void free_process_pcb(void *proc);

// Checks if all of the processes in the job are complete, based on
// their PID being equal to FINISHED_PID_VAL.
bool process_complete(pcb *j);

// Returns a job given a pid.
pcb *get_job_from_pid(linked_list *linked_list, int pid);

// Flips the status of a job, given the pid.
void update_job_status(linked_list *linked_list, int pid, int status);
