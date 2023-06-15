// Implementation of the PCB data structure.

#include "pcb.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "../kernel/scheduler.h"
#include "../kernel/pcb_table.h"
#include "../kernel/threads.h"
#include "../lib/log.h"

bool pid_equal_predicate(void *pid, void *target_pcb) {
    pid_t *target_pid = pid;
    pcb *p = (pcb *) target_pcb;
    return (p->pid == *target_pid);
}

void free_process_pcb(void *proc) {
    if (proc == NULL) {
        return;
    }

    pcb *process = (pcb *) proc;

    if (get_active_job() == process) {
        reset_active_job();
    }

    // remove process from process table
    delete_pcb_in_table(process->pid);

    // Free the process pcb values
    if (process->cmd != NULL) {
        free(process->cmd);
    }

    if (process->argv != NULL) {
        for (int i = 0; process->argv[i] != NULL; i++) {
            free(process->argv[i]);
        }

        free(process->argv);
    }

    if (process->childLL != NULL) {
        clear(process->childLL, free_process_pcb);
        free(process->childLL);
    }

    if (process->zombieLL != NULL) {
        clear(process->zombieLL, free_process_pcb);
        free(process->zombieLL);
    }

    if (process->context != NULL) {
        free_context(process->context);
    }

    free(process);
}

bool process_complete(pcb *j) {
    if (j->pid != FINISHED_PID_VAL) {
        return false;
    } else {
        return true;
    }
}

pcb *get_job_from_pid(linked_list *linked_list, int pid) {
    linked_list_elem *cur_elem;

    if (!is_empty(linked_list)) {
        // Iterating from the tail to the head
        cur_elem = linked_list->head;

        while (cur_elem != NULL) {
            pcb *val = (pcb *) cur_elem->val;

            if (val->pid == pid) {
                return val;
            }
            cur_elem = cur_elem->next;
        }
    }
    return NULL;
}

void update_job_status(linked_list *linked_list, int pid, int status) {
    linked_list_elem *cur_elem;

    if (!is_empty(linked_list)) {
        cur_elem = linked_list->head;
        while (cur_elem != NULL) {
            pcb *val = (pcb *) cur_elem->val;

            if (val->pid == pid) {
                val->status = status;
                return;
            }
            cur_elem = cur_elem->next;
        }
    }
}
