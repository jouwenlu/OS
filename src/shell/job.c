// Implementation of the Job data structure.

#include "job.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include "shell.h"
#include "../kernel/pcb_table.h"

job *create_job(pid_t pid) {
    job *jb = (job *) malloc(sizeof(job));
    if (jb == NULL) {
        return NULL;
        perror("malloc");
    }
    jb->job_pid = pid;
    jb->job_id = min_available_id();
    return jb;
}

void free_job(void *j) {
    free(j);
}

bool job_equal_predicate(void *target_pid, void *curr_job) {
    job *jb = (job *) curr_job;
    pid_t *pid = target_pid;
    return (jb->job_pid == *pid);
}

bool job_id_equal_predicate(void *target_job_id, void *curr_job) {
    job *jb = (job *) curr_job;
    int *job_id = target_job_id;
    return (jb->job_id == *job_id);
}


void print_job(job *j) {
    pcb *proc = find_pcb_in_table(j->job_pid);

    char *status_msg = NULL;

    if (proc->status == STOPPED || proc->status == RUNNING) {
        if (proc->status == STOPPED) {
            status_msg = "stopped";
        } else if (proc->status == RUNNING) {
            status_msg = "running";
        }

        fprintf(stderr, "[%d] %d: %s (%s)\n", j->job_id, proc->pid, proc->cmd, status_msg);
    }
}
