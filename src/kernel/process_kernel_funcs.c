// Function definitions for process related kernel-level functions.

#include "process_kernel_funcs.h"

#include "../lib/linked_list.h"
#include "../lib/log.h"
#include "../lib/signals.h"
#include "../user/file_user_funcs.h"

static int univCounter = 2;

pcb *k_process_create(pcb *parent) {
    pcb *child = (pcb *) malloc(sizeof(pcb));

    if (child == NULL) {
        perror("malloc");
        return NULL;
    }

    child->status = RUNNING;
    child->status_changed = false;
    child->context = (ucontext_t *) malloc(sizeof(ucontext_t));
    child->pid = univCounter;
    child->is_bg = false;
    child->blocked = 0;
    univCounter++;

    child->parent = parent;

    child->argv = NULL;

    linked_list *zombie_list = (linked_list *) malloc(sizeof(linked_list));
    init_linked_list(zombie_list);
    child->zombieLL = zombie_list;

    linked_list *child_list = (linked_list *) malloc(sizeof(linked_list));
    init_linked_list(child_list);
    child->childLL = child_list;

    child->priority = parent->priority;

    for (int i = 2; i < MAX_OPEN_FDS; i++) {
        child->fd[i] = parent->fd[i];
    }

    push_back(parent->childLL, child);

    return child;
}

void k_process_kill(pcb *process, int signal) {
    if (process->status != EXITED) {
        log_event(SIGNALED_EVT, process);
    }

    if (signal == S_SIGSTOP && process->status == RUNNING) {
        process->status = STOPPED;
        process->status_changed = 1;

        // Logging.
        log_event(STOPPED_EVT, process);

        // If parent is blocked because of child process, resume parent.
        if (process->parent != NULL) {
            if (process->parent->blocked == 1) {
                process->parent->blocked = 0;

                // Logging.
                log_event(UNBLOCKED_EVT, process->parent);
                add_job_to_scheduler(process->parent);
            }
        }
    } else if (signal == S_SIGCONT && process->status == STOPPED) {
        process->status = RUNNING;

        // Logging.
        log_event(CONTINUED_EVT, process);

        if (process->blocked == false) {
            add_job_to_scheduler(process);
        }
    } else if (signal == S_SIGTERM && process->status != TERMINATED &&
               process->status != ORPHANED) {
        // Terminate if process is not already terminated or orphaned.
        process->status_changed = 1;

        // Remove from sleep queue on SIGTERM, if applicable.
        if (get_sleep_queue() != NULL) {
            extract_elem(get_sleep_queue(), pid_equal_predicate, &process->pid);
        }

        // Close child fd's on termination.
        f_close_child_fd(process);

        // Logging.
        log_event(ZOMBIE_EVT, process);
        if (process->status != EXITED) {
            // Status was changed earlier to EXITED when p_exit() was called.
            process->status = TERMINATED;
        }

        // If process is terminated, set all its children to orphaned.
        int s1 = process->zombieLL->size;

        for (int i = 0; i < s1; i++) {
            pcb *child1 = pop_head(process->zombieLL);
            child1->status = ORPHANED;
            child1->status_changed = 1;
            push_back(process->zombieLL, child1);
            remove_job_from_scheduler(child1);

            // Logging.
            log_event(ORPHAN_EVT, child1);
        }

        int s2 = process->childLL->size;

        for (int i = 0; i < s2; i++) {
            pcb *child2 = pop_head(process->childLL);
            child2->status = ORPHANED;
            child2->status_changed = 1;
            push_back(process->childLL, child2);
            remove_job_from_scheduler(child2);

            // Logging.
            log_event(ORPHAN_EVT, child2);
        }

        if (process->parent != NULL) {
            // Move this process to zombieLL of its parent.
            extract_elem(process->parent->childLL, pid_equal_predicate,
                         &process->pid);
            push_back(process->parent->zombieLL, process);

            if (process->parent->blocked == 1) {
                process->parent->blocked = 0;

                // Logging.
                log_event(UNBLOCKED_EVT, process->parent);
                add_job_to_scheduler(process->parent);
            }
        } else {
            // THIS IS THE SHELL CASE.
            process->context->uc_link = 0;
        }
    }
}

void k_process_cleanup(pcb *process) {
    if (process != NULL) {
        if (process->status == TERMINATED || process->status == EXITED ||
            process->status == ORPHANED) {

            // Remove children from queues and free them.
            int s1 = process->zombieLL->size;
            for (int i = 0; i < s1; i++) {
                free_process_pcb(pop_head(process->zombieLL));
            }

            int s2 = process->childLL->size;
            for (int i = 0; i < s2; i++) {
                free_process_pcb(pop_head(process->childLL));
            }

            // Remove current process from its parent queue.
            extract_elem(process->parent->childLL, pid_equal_predicate,
                         &process->pid);
            extract_elem(process->parent->zombieLL, pid_equal_predicate,
                         &process->pid);
            free_process_pcb(process);
        }
    }
}
