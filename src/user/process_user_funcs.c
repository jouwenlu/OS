// Function definitions for process related user-level functions.

#include "process_user_funcs.h"

#include "file_user_funcs.h"

#include "../kernel/process_kernel_funcs.h"
#include "../kernel/pcb_table.h"
#include "../kernel/threads.h"
#include "../lib/log.h"
#include "../lib/signals.h"
#include "../lib/errno.h"

// Dynamically allocate command name.
char *allocate_process_name(char *argv[]) {
    int total_str_len = 1;
    for (int i = 0; argv[i] != NULL; i++) {
        if (i != 0) {
            total_str_len++;
        }

        total_str_len += strlen(argv[i]);
    }

    char *cmd = (char *) malloc(total_str_len * sizeof(char));
    if (cmd == NULL) {
        return NULL;
    }

    cmd[0] = '\0';

    for (int i = 0; argv[i] != NULL; i++) {
        if (i != 0) {
            strcat(cmd, " ");
        }

        strcat(cmd, argv[i]);
    }

    return cmd;
}

// Dynamically allocates and copies from argv.
// Last element is a NULL pointer.
char **allocate_arguments(char *argv[]) {
    if (argv == NULL) {
        return NULL;
    }

    int num_args = 0;
    for (; argv[num_args] != NULL; num_args++);

    char **ret_val = (char **) malloc((num_args + 1) * sizeof(char *));
    if (ret_val == NULL) {
        return NULL;
    }

    ret_val[num_args] = NULL;

    for (int i = 0; i < num_args; i++) {
        ret_val[i] = (char *) malloc((strlen(argv[i]) + 1) * sizeof(char));
        strcpy(ret_val[i], argv[i]);
    }

    return ret_val;
}

pid_t p_spawn(void (*func)(), char *argv[], int fd0, int fd1, int priority) {
    pcb *parent_job = get_active_job();

    if (parent_job == NULL) {
        set_errno(ACTIVEJOBNULL);
        return -1;
    }

    if (!((priority == 0) | (priority == -1) | (priority == 1))) {
        set_errno(INVALIDPRIORITY);
        return -1;
    }

    pcb *child_process = k_process_create(parent_job);

    if (child_process == NULL) {
        // NOCHILDCREATED
        set_errno(NOCHILDCREATED);
        return -1;
    }

    child_process->cmd = allocate_process_name(argv);
    if (child_process->cmd == NULL) {
        perror("malloc");
        return -1;
    }

    child_process->argv = allocate_arguments(argv);

    child_process->priority = priority;

    for (int i = 2; i < MAX_OPEN_FDS; i++) {
        if (child_process->fd[i] == fd0 || child_process->fd[i] == fd1) {
            child_process->fd[i] = EMPTY_FD;
        }
    }

    child_process->fd[0] = fd0;
    child_process->fd[1] = fd1;

    f_update_new_child_fd(child_process);

    make_context(child_process->context, func, child_process->argv);
    add_job_to_scheduler(child_process);
    add_pcb_to_table(child_process);

    // Logging.
    log_event(CREATE_EVT, child_process);

    return child_process->pid;
}

int p_kill(pid_t pid, int sig) {
    if (sig == INVALID) {
        set_errno(INVALIDSIGNAL);
        return -1;
    }

    pcb *target_job = find_pcb_in_table(pid);

    if (target_job != NULL) {
        remove_job_from_scheduler(target_job);
        k_process_kill(target_job, sig);
        return 0;
    } else {
        set_errno(NOTINPCBTABLE);
        return -1;
    }
}

pcb *find_changed_child(pcb *parent_pcb, pid_t child_pid) {
    // Look through child queues to see if any child changed status.
    linked_list *child_list = parent_pcb->childLL;

    // Iterate through the 2 child queues.
    for (int i = 0; i < 2; i++) {
        if (!is_empty(child_list)) {
            linked_list_elem *cur_elem = child_list->head;

            while (cur_elem != NULL) {
                pcb *cur_pcb = cur_elem->val;

                if (cur_pcb->status_changed) {
                    return cur_pcb;
                }

                cur_elem = cur_elem->next;
            }
        }

        child_list = parent_pcb->zombieLL;
    }

    return NULL;
}

pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang) {
    pcb *parent_pcb = get_active_job();
    pcb *child_pcb = NULL;

    bool wait_any_child = false;
    if (pid == -1) {
        wait_any_child = true;
    }

    if (wait_any_child) {
        // If the child queues are empty, should return -1.
        if (is_empty(parent_pcb->childLL) && is_empty(parent_pcb->zombieLL)) {
            return -1;
        }

        // Check if there's any child with a changed status.
        child_pcb = find_changed_child(parent_pcb, pid);
        if (child_pcb == NULL && nohang) {
            // No child have changed status & no hang.
            return 0;
        }
    } else {
        // Look for the target child with given pid
        linked_list_elem *child1 =
                get_elem(parent_pcb->zombieLL, pid_equal_predicate, &pid);
        linked_list_elem *child2 =
                get_elem(parent_pcb->childLL, pid_equal_predicate, &pid);

        // Return -1 on error (no such child exists).
        if (child1 == NULL && child2 == NULL) {
            // NOSUCHCHILD
            set_errno(NOSUCHCHILD);
            return -1;
        }

        if (child1 != NULL) {
            child_pcb = child1->val;
        } else {
            child_pcb = child2->val;
        }
    }

    if (child_pcb != NULL) {
        pid = child_pcb->pid;
        if (wstatus != NULL) {
            *wstatus = (int) child_pcb->status;
        }
    }

    // Won't block.
    if (nohang) {
        if (child_pcb->status_changed) {
            // Only want cleanup if process terminated.
            if (child_pcb->status == TERMINATED ||
                child_pcb->status == EXITED || child_pcb->status == ORPHANED) {
                // Normal completion for background job (job status reporting)
                if (child_pcb->status == EXITED && child_pcb->is_bg) {
                    fprintf(stderr, "Finished: %s\n", child_pcb->cmd);
                }

                // Logging.
                log_event(WAITED_EVT, child_pcb);

                k_process_cleanup(child_pcb);
            } else {
                // Rest flag if status was changed but not terminated.
                child_pcb->status_changed = 0;
            }
            return pid;
        } else {
            // No change in status.
            return 0;
        }
    } else {
        // Block current process until child changes state.
        // Only block if there's no status change yet.
        if (child_pcb != NULL) {
            if (child_pcb->status_changed == false) {
                // Don't need to remove job from scheduler b/c already removed when running.
                parent_pcb->blocked = true;

                // Logging.
                log_event(BLOCKED_EVT, parent_pcb);
            }
        }

        while (true) {
            if (wait_any_child) {
                // If no child to wait and still blocking, should return -1.
                if (is_empty(parent_pcb->childLL) && is_empty(parent_pcb->zombieLL)) {
                    return -1;
                }
                child_pcb = find_changed_child(parent_pcb, pid);
            }

            if (child_pcb != NULL) {
                if (child_pcb->status_changed) {
                    pid = child_pcb->pid;
                    if (wstatus != NULL) {
                        *wstatus = (int) child_pcb->status;
                    }

                    if (child_pcb->status == TERMINATED || child_pcb->status == EXITED ||
                        child_pcb->status == ORPHANED) {

                        // Logging.
                        log_event(WAITED_EVT, child_pcb);
                        k_process_cleanup(child_pcb);
                    } else {
                        child_pcb->status_changed = 0;
                    }

                    return pid;
                }
            }

            suspend();
        }
    }
}

void p_exit(void) {
    pcb *active_job = get_active_job();

    if (active_job == NULL) {
        set_errno(ACTIVEJOBNULL);
        return;
    }

    active_job->status = EXITED;

    // Logging.
    log_event(EXITED_EVT, active_job);
    k_process_kill(active_job, S_SIGTERM);

    suspend();
}
