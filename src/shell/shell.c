// Main file for Penn Shell.

#include "shell.h"

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "commands.h"
#include "job.h"
#include "../lib/linked_list.h"
#include "../lib/parser.h"
#include "../lib/fd.h"
#include "../lib/pcb.h"
#include "../lib/log.h"
#include "../lib/signals.h"
#include "../lib/errno.h"
#include "../kernel/scheduler.h"
#include "../kernel/pcb_table.h"
#include "../user/process_user_funcs.h"
#include "../user/file_user_funcs.h"

// Represents if there is a current command running.
pid_t foreground_pid = NO_ACTIVE_PROCESS;
bool interactive_mode = false;

// Queue of jobs.
linked_list background_jobs;

// Return true if the child terminated normally, that is, by a call to p_exit
// or by returning.
void update_fg_pid(pid_t pid) {
    foreground_pid = pid;
}

void sig_handler(int signo) {
    if (signo == SIGINT || signo == SIGTSTP) {
        // Pass terminal control back to Penn Shell.
        f_write(STDERR_FILENO, "\n", 1);

        if (foreground_pid == NO_ACTIVE_PROCESS) {
            f_write(STDERR_FILENO, PROMPT, sizeof(PROMPT));
        }
    }

    if (foreground_pid != NO_ACTIVE_PROCESS) {
        if (signo == SIGINT) {
            int result = p_kill(foreground_pid, S_SIGTERM);
            if (result == -1) {
                p_perror("p_kill did not work, could not terminate the process\n");
            }

            update_fg_pid(NO_ACTIVE_PROCESS);
            suspend();
        } else if (signo == SIGTSTP) {
            // Ctrl+Z (stop process).

            int result = p_kill(foreground_pid, S_SIGSTOP);
            if (result == -1) {
                p_perror("p_kill did not work, could not stop the process\n");
            }

            // If fg process is stopped, move it to bg
            pcb *proc = find_pcb_in_table(foreground_pid);
            proc->is_bg = true;

            // Only add job to queue if it's not already there
            if (!elem_exists(&background_jobs, job_equal_predicate, &foreground_pid)) {
                job *jb = create_job(proc->pid);
                push_back(&background_jobs, jb);
            }

            update_fg_pid(NO_ACTIVE_PROCESS);
            suspend();
        }
    }
}

void register_sig_handlers() {
    // Register SIGINT handler.
    if (signal(SIGINT, sig_handler) == SIG_ERR) {
        perror("Error: Unable to catch SIGINT.");
        exit_shell();
    }

    // Register SIGTSTP handler.
    if (signal(SIGTSTP, sig_handler) == SIG_ERR) {
        perror("Error: Unable to catch SIGTSTP.");
        exit_shell();
    }

    // Ignore SIGTTOU.
    if (signal(SIGTTOU, SIG_IGN) == SIG_ERR) {
        perror("Error: Unable to ignore SIGTTOU.");
        exit_shell();
    }
}

void remove_bg_job(pid_t job_pid) {
    extract_elem(&background_jobs, job_equal_predicate, &job_pid);
}

void refresh_bg_jobs() {
    if (!is_empty(&background_jobs)) {
        int status;
        linked_list_elem *cur = background_jobs.head;

        while (cur != NULL) {
            linked_list_elem *next_node = cur->next;
            job *jb = (job *) cur->val;
            pid_t ret_pid = p_waitpid(jb->job_pid, &status, true); // nohang = true;

            // Only cleanup if process was exited or terminated.
            if (W_WIFEXITED(status) || W_WIFSIGNALED(status)) {
                // Free job struct if waitpid cleans up the pcb.
                remove_bg_job(ret_pid);
                free(jb);
            }

            cur = next_node;
        }
    }
}

void handle_input(char *input, int priority) {
    struct parsed_command *command;
    int err = parse_command(input, &command);
    if (err == -1) {
        perror("Parser system call error.");
        exit(EXIT_FAILURE);
    } else if (err > 0) {
        write(STDERR_FILENO, ERROR_PARSING_COMMAND_MSG, sizeof(ERROR_PARSING_COMMAND_MSG));
        return;
    }

    // If no args, i.e. nothing entered, just continue on.
    if (command->num_commands == 0) {
        return;
    }

    if (strcmp(command->commands[0][0], "logout") == 0) {
        free(command);
        exit_shell();
        return;
    } else if (strcmp(command->commands[0][0], "nice") == 0) {
        int num_args = 0;
        for (; command->commands[0][num_args] != NULL; num_args++);

        if (num_args < 3) {
            fprintf(stderr, "nice: incorrect number of arguments\n");
            free(command);
            return;
        }

        int priority = atoi(command->commands[0][1]);

        // Reconstruct rest of command as a single user input and handle as such.
        char buf[MAX_LINE_LENGTH] = {'\0'};
        for (int i = 2; i < num_args; i++) {
            if (i != 2) {
                strcat(buf, " ");
            }

            strcat(buf, command->commands[0][i]);
        }

        if (command->stdin_file != NULL) {
            strcat(buf, " < ");
            strcat(buf, command->stdin_file);
        }

        if (command->stdout_file != NULL) {
            strcat(buf, command->is_file_append ? " >> " : " > ");
            strcat(buf, command->stdout_file);
        }

        if (command->is_background) {
            strcat(buf, " &");
        }

        handle_input(buf, priority);
        free(command);
        return;
    }

    if (!exec_subroutine(command->commands[0])) {
        pcb *active_job = get_active_job();
        int fd0 = active_job->fd[0];
        if (command->stdin_file != NULL) {
            fd0 = f_open(command->stdin_file, F_READ);

            if (fd0 < 0) {
                p_perror("Opening fd0 redirect\n");
                free(command);
                return;
            }
        }

        int fd1 = active_job->fd[1];
        if (command->stdout_file) {
            fd1 = f_open(command->stdout_file, (command->is_file_append ? F_APPEND : F_WRITE));

            if (fd1 < 0) {
                p_perror("Opening fd1 redirect\n");
                free(command);
                return;
            }
        }

        pid_t child = p_spawn(&exec_command, command->commands[0], fd0, fd1, priority);
        if (child == -1) {
            p_perror("p_spawn did not work\n");
        }

        foreground_pid = child;

        if (fd0 > STDERR_FILENO && fd0 != active_job->fd[0]) {
            f_close(fd0);
        }

        if (fd1 > STDERR_FILENO && fd1 != active_job->fd[1]) {
            f_close(fd1);
        }

        // Check if process is in the background
        if (command->is_background) {
            pcb *child_process = find_pcb_in_table(child);
            child_process->is_bg = true;
            job *child_job = create_job(child_process->pid);
            push_back(&background_jobs, child_job);
            // Job status report
            fprintf(stderr, "Running: %s\n", child_process->cmd);
        } else {
            // Process is in foreground.

            int status;
            bool nohang = false;

            pid_t result = p_waitpid(child, &status, nohang);
            if (result == -1) {
                p_perror("p_waitpid failed\n");
            }
        }
    }

    foreground_pid = NO_ACTIVE_PROCESS;
    free(command);
}

void main_shell(void) {
    init_linked_list(&background_jobs);

    int num_bytes;

    while (1) {
        register_sig_handlers();

        refresh_bg_jobs();

        char cmd[MAX_LINE_LENGTH] = {'\0'};

        f_write(STDERR_FILENO, PROMPT, sizeof(PROMPT));
        num_bytes = f_read(STDIN_FILENO, MAX_LINE_LENGTH, cmd);

        // ctrl+D with nothing, so exit.
        if (num_bytes == 0) {
            f_write(STDERR_FILENO, "\n", 1);
            break;
        } else if (num_bytes == -1 && !interactive_mode) {
            break;
        } else if (num_bytes == -1) {
            perror("Error while reading input.");
            break;
        }

        if (num_bytes > 0) {
            if (num_bytes == 1 && cmd[num_bytes - 1] == '\n') {
                // If user only presses enter, then continue.
                continue;
            } else if (cmd[num_bytes - 1] == '\n') {
                // If user enters something, then we null terminate it.
                cmd[num_bytes - 1] = '\0';
            } else {
                // If ctrl+d after entering something, just write a new line it.
                f_write(STDERR_FILENO, "\n", 1);
                continue;
            }
        }

        handle_input(cmd, 0);
    }

    exit_shell();
}

// This is used for the bg builtin.
// Returns last stopped bg job or the last bg job if nothing is stopped.
job *get_current_job(linked_list *linked_list) {
    linked_list_elem *cur_elem;

    if (!is_empty(linked_list)) {
        // Iterating from the tail to the head.
        cur_elem = linked_list->tail;

        while (cur_elem != NULL) {
            job *cur_job = (job *) cur_elem->val;
            pcb *cur_pcb = find_pcb_in_table(cur_job->job_pid);

            if (cur_pcb->status == STOPPED) {
                return cur_job;
            }

            cur_elem = cur_elem->prev;
        }

        // If there is no stopped process then return the most recent background process.
        return linked_list->tail->val;
    }

    return NULL;
}

job *find_bg_job(char *arg) {
    if (arg == NULL) {
        return get_current_job(&background_jobs);
    } else {
        int int_arg = atoi(arg);
        linked_list_elem *job_elem = get_elem(&background_jobs, job_id_equal_predicate, &int_arg);
        return job_elem == NULL ? NULL : job_elem->val;
    }
}

int min_available_id() {
    if (background_jobs.size == 0) {
        return 1;
    }

    int min = background_jobs.size + 1;
    int occupied_ids[background_jobs.size + 1];

    for (int i = 0; i < background_jobs.size + 1; i++) {
        occupied_ids[i] = 0;
    }

    linked_list_elem *cur = background_jobs.head;
    while (cur != NULL) {
        job *job_ptr = cur->val;
        occupied_ids[job_ptr->job_id] = 1;
        cur = cur->next;
    }

    for (int i = 1; i < background_jobs.size; i++) {
        if (occupied_ids[i] != 1) {
            min = i;
            break;
        }
    }

    return min;
}

linked_list *get_bg_queue() {
    return &background_jobs;
}

void free_bg_queue() {
    clear(&background_jobs, free_job);
}
