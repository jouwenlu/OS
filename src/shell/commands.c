// Implementation of shell builtin commands.

#include "commands.h"

#include <string.h>
#include "shell.h"
#include "job.h"
#include "../fat/fat_util.h"
#include "../kernel/pcb_table.h"
#include "../kernel/threads.h"
#include "../lib/fd.h"
#include "../lib/macros.h"
#include "../lib/signals.h"
#include "../lib/status.h"
#include "../lib/errno.h"
#include "../user/file_user_funcs.h"
#include "../user/process_user_funcs.h"
#include "../user/scheduler_user_funcs.h"
#include "../user/stress.h"

void exit_shell() {
    swapcontext(get_active_job()->context, get_os_context());
}

void zombie_child() {
    // MMMMM Brains...!
    p_exit();
}

void zombify() {
    char *args[2] = {"zombie_child", NULL};
    p_spawn(zombie_child, args, STDIN_FILENO, STDOUT_FILENO, 0);
    while (1);
    return;
}

void orphan_child() {
    // Please sir,
    // I want some more
    while (1);
}

void orphanify() {
    char *args[2] = {"orphan_child", NULL};
    p_spawn(orphan_child, args, STDIN_FILENO, STDOUT_FILENO, 0);
    return;
}

bool exec_subroutine(char *argv[]) {
    char *cmd = argv[0];

    // Includes the command: 'mkfs small 3 2' has num_args = 4.
    int num_args = 0;
    for (; argv[num_args] != NULL; num_args++);

    if (strcmp(cmd, "man") == 0) {
        char *my_str = "These are all the available commands:\n"
                       "nice priority command [arg] : set the priority of the command to priority and execute the command. \n"
                       "nice_pid priority pid : adjust the nice level of process pid to priority priority. \n"
                       "man : list all available commands. \n"
                       "bg [job_id] : continue the last stopped job, or the job specified by job_id. \n"
                       "fg [job_id] bring the last stopped or backgrounded job to the foreground, or the job specified by job_id. \n"
                       "jobs : list all jobs. \n"
                       "logout : exit the shell and shutdown PennOS. \n"
                       "cat : the usual cat from bash.\n"
                       "sleep x : sleep for x seconds.\n"
                       "busy : waits indefinitely\n"
                       "echo : similar to echo(1) in the VM.\n"
                       "ls : list all files in the working directory (similar to ls -il in bash), same formatting as ls in the standalone PennFAT.\n"
                       "touch file ... : create an empty file if it does not exist, or update its timestamp otherwise.\n"
                       "mv src dest : rename src to dest.\n"
                       "cp src dest : copy src to dest.\n"
                       "rm file ... : remove files.\n"
                       "chmod : similar to chmod(1) in the VM.\n"
                       "ps : list all processes on PennOS. Display pid, ppid, and priority.\n"
                       "kill [ -SIGNAL_NAME ] pid ... : send the specified signal to the specified processes, where -SIGNAL_NAME is either term (the default), stop, or cont, corresponding to S_SIGTERM, S_SIGSTOP, and S_SIGCONT, respectively. Similar to /bin/kill in the VM.\n"
                       "zombify : creates a zombie process.\n"
                       "orphanify : creates an orphan process.\n";
        fprintf(stderr, "%s", my_str);
    } else if (strcmp(cmd, "jobs") == 0) {
        linked_list *bg_queue = get_bg_queue();
        if (!is_empty(bg_queue)) {
            linked_list_elem *curr = bg_queue->head;
            while (curr != NULL) {
                job *jb = curr->val;
                print_job(jb);
                curr = curr->next;
            }
        }
        return true;
    } else if (strcmp(cmd, "nice_pid") == 0) {
        int new_priority = atoi(argv[1]);
        int target_pid = atoi(argv[2]);
        int result = p_nice(new_priority, target_pid);
        if (result == -1) {
            p_perror("p_nice did not work\n");
        }
        return true;
    } else if (strcmp(cmd, "bg") == 0) {
        // Continue the last stopped job, or the job specified by job_id.
        job *jb = find_bg_job(argv[1]);
        HANDLE_INVALID_INPUT_RET_VAL(jb == NULL, "bg: specified job does not exist\n", true);

        // Change the job status to running.
        if (p_kill(jb->job_pid, S_SIGCONT) < 0) {
            p_perror("process doesn't exist");
            return true;
        }
    } else if (strcmp(cmd, "fg") == 0) {
        // Bring the last stopped or backgrounded job to the foreground, or the job specified by job_id.
        job *jb = find_bg_job(argv[1]);
        HANDLE_INVALID_INPUT_RET_VAL(jb == NULL, "fg: specified job does not exist\n", true);

        pcb *proc_pcb = find_pcb_in_table(jb->job_pid);
        proc_pcb->is_bg = false;

        // Was running in bg.
        if (proc_pcb->status == RUNNING) {
            // Job status reporting.
            fprintf(stderr, "%s\n", proc_pcb->cmd);
        } else {
            // Was stopped in bg.
            if (p_kill(jb->job_pid, S_SIGCONT) < 0) {
                p_perror("Error resuming process in bg");
                return true;
            }
        }

        // Execute process in fg & wait with blocking
        update_fg_pid(proc_pcb->pid);
        int status;

        pid_t ret_pid = p_waitpid(jb->job_pid, &status, false);
        if (W_WIFEXITED(status) || W_WIFSIGNALED(status)) {
            // remove job from background queue if job terminated/exited
            remove_bg_job(ret_pid);
            free_job(jb);
        }

        return true;
    } else if (strcmp(cmd, "kill") == 0) {
        //  kill [ -SIGNAL_NAME ] pid ... (S*) send the specified signal to the specified processes
        HANDLE_INVALID_INPUT_RET_VAL(num_args < 2, "kill: not enough arguments\n", true);
        int sig = -1;

        if (strcmp(argv[1], "-stop") == 0) {
            sig = S_SIGSTOP;
        } else if (strcmp(argv[1], "-cont") == 0) {
            sig = S_SIGCONT;
        } else if (strcmp(argv[1], "-term") == 0) {
            sig = S_SIGTERM;
        }

        int first_pid_idx = 2;

        // No signal was specified, so assume sigterm.
        if (sig == -1) {
            sig = S_SIGTERM;
            first_pid_idx = 1;
        }

        for (int i = first_pid_idx; i < num_args; i++) {
            pid_t proc_pid = atoi(argv[i]);
            int result = p_kill(proc_pid, sig);

            if (result == -1) {
                p_perror("p_kill failed\n");
            }
        }

        return true;
    } else {
        return false;
    }
    return true;
}

void exec_command(char *argv[]) {
    char *cmd = argv[0];

    // Includes the command: 'mkfs small 3 2' has num_args = 4.
    int num_args = 0;
    for (; argv[num_args] != NULL; num_args++);

    if (strcmp(cmd, "rm") == 0) {
        HANDLE_INVALID_INPUT_VOID(num_args < 2, "rm: Incorrect number of arguments");

        for (int i = 1; i < num_args; i++) {
            if (f_unlink(argv[i]) == -1) {
                p_perror(NULL);
            }
        }
    } else if (strcmp(cmd, "chmod") == 0) {
        HANDLE_INVALID_INPUT_VOID(num_args < 3, "chmod: Incorrect number of arguments");

        char *op = argv[1];
        char *file = argv[2];
        HANDLE_INVALID_INPUT_VOID(
                strlen(op) != 2 || (op[0] != '+' && op[0] != '-') || (op[1] != 'r' && op[1] != 'w' && op[1] != 'x'),
                "Usage: chmod [+/-][r/w/x] file1 ...\n");
        int perm = op[1] == 'r' ? READ_ONLY : (op[1] == 'w' ? WRITE_ONLY : EXEC_ONLY);

        if (f_change_perms(file, op, perm) == -1) {
            p_perror(NULL);
        }
    } else if (strcmp(cmd, "mv") == 0) {
        HANDLE_INVALID_INPUT_VOID(num_args != 3, "mv: Incorrect number of arguments");

        char *src = argv[1];
        char *dest = argv[2];

        int fd_src;
        if (is_in_dell(dest)) {
            if (f_unlink(dest) == -1) {
                p_perror(NULL);
            }
        }

        fd_src = f_open(src, F_READ);
        if (fd_src == -1) {
            p_perror(NULL);
        }

        if (f_rename(fd_src, dest) == -1) {
            p_perror(NULL);
        }

        if (f_close(fd_src) == -1) {
            p_perror(NULL);
        }
    } else if (strcmp(cmd, "ls") == 0) {
        if (num_args > 1) {
            f_ls(argv[1]);
        } else {
            f_ls(NULL);
        }
    } else if (strcmp(cmd, "touch") == 0) {
        HANDLE_INVALID_INPUT_VOID(num_args < 2, "touch: Incorrect number of arguments");

        for (int i = 1; i < num_args; i++) {
            int fd = f_open(argv[i], F_APPEND);
            if (fd == -1) {
                p_perror(NULL);
            }
            if (f_close(fd) == -1) {
                p_perror(NULL);
            }
        }
    } else if (strcmp(cmd, "cp") == 0) {
        HANDLE_INVALID_INPUT_VOID(num_args != 3, "cp: Incorrect number of arguments");

        char buf[MAX_LINE_LENGTH];
        int src = f_open(argv[1], F_READ);
        if (src == -1) {
            p_perror(NULL);
        }

        int dst = f_open(argv[2], F_WRITE);
        if (dst == -1) {
            p_perror(NULL);
        }

        while (true) {
            int bytes_read = f_read(src, MAX_LINE_LENGTH, buf);
            if (bytes_read == -1) {
                p_perror(NULL);
            }

            if (f_write(dst, buf, bytes_read) == -1) {
                p_perror(NULL);
            }

            if (bytes_read < MAX_LINE_LENGTH) {
                break;
            }
        }

        if (f_close(src) == -1) {
            p_perror(NULL);
        }

        if (f_close(dst) == -1) {
            p_perror(NULL);
        }
    } else if (strcmp(cmd, "cat") == 0) {
        if (num_args == 1) {
            char buf[MAX_LINE_LENGTH] = {'\0'};

            while (true) {
                int bytes_read = f_read(STDIN_FILENO, MAX_LINE_LENGTH, buf);
                if (bytes_read == -1) {
                    p_perror(NULL);
                }

                if (bytes_read <= 0) {
                    break;
                }

                if (f_write(STDOUT_FILENO, buf, bytes_read) == -1) {
                    p_perror(NULL);
                }
            }
        } else {
            for (int i = 1; i < num_args; i++) {
                int src = f_open(argv[i], F_READ);

                if (src == -1) {
                    p_perror(NULL);
                    continue;
                }

                while (true) {
                    char buf[MAX_LINE_LENGTH] = {'\0'};

                    int bytes_read = f_read(src, MAX_LINE_LENGTH, buf);
                    if (bytes_read == -1) {
                        p_perror(NULL);
                    }

                    if (f_write(STDOUT_FILENO, buf, bytes_read) == -1) {
                        p_perror(NULL);
                    }

                    if (bytes_read < MAX_LINE_LENGTH) {
                        break;
                    }
                }

                if (f_close(src) == -1) {
                    p_perror(NULL);
                }
            }
        }
    } else if (strcmp(cmd, "sleep") == 0) {
        if (num_args == 2) {
            p_sleep(SECONDS_TO_TICKS(atoi(argv[1])));
        } else {
            fprintf(stderr, "sleep: incorrect number of arguments\n");
        }
    } else if (strcmp(cmd, "busy") == 0) {
        while (true) {}
    } else if (strcmp(cmd, "echo") == 0) {
        for (int i = 1; i < num_args; i++) {
            if (i != 1) {
                if (f_write(STDOUT_FILENO, " ", 1) == -1) {
                    p_perror(NULL);
                }
            }

            if (f_write(STDOUT_FILENO, argv[i], strlen(argv[i])) == -1) {
                p_perror(NULL);
            }
        }

        if (f_write(STDOUT_FILENO, "\n", 1) == -1) {
            p_perror(NULL);
        }
    } else if (strcmp(cmd, "ps") == 0) {
        linked_list *pctb = get_process_table();

        if (pctb->size == 0) {
            fprintf(stderr, "no processes to display/n");
        } else {
            linked_list_elem *curr = pctb->head;
            char str[4096] = {'\0'}; // way more than necessary
            char *format = "PID PPID PRI STAT CMD\n";
            if (f_write(STDOUT_FILENO, format, strlen(format)) == -1) {
                p_perror(NULL);
            }

            while (curr != NULL) {
                pcb *curr_pcb = curr->val;
                char status =
                        curr_pcb->blocked && curr_pcb->status == RUNNING ? 'B' : get_status_code(curr_pcb->status);
                int ppid = curr_pcb->parent != NULL ? curr_pcb->parent->pid : 0;
                sprintf(str, "%3d %4d %3d  %c   %s\n", curr_pcb->pid, ppid, curr_pcb->priority, status, curr_pcb->cmd);

                if (f_write(STDOUT_FILENO, str, strlen(str)) == -1) {
                    p_perror(NULL);
                }

                curr = curr->next;
            }
        }
    } else if (strcmp(cmd, "zombify") == 0) {
        zombify();
    } else if (strcmp(cmd, "orphanify") == 0) {
        orphanify();
    } else if (strcmp(cmd, "hang") == 0) {
        hang();
    } else if (strcmp(cmd, "nohang") == 0) {
        nohang();
    } else if (strcmp(cmd, "recur") == 0) {
        recur();
    } else {
        int script_size = f_size(argv[0]);

        if (script_size == -1) {
            fprintf(stderr, "Unknown command\n");
        } else if (!f_has_permissions(argv[0], EXEC_ONLY)) {
            fprintf(stderr, "File %s is not executable\n", argv[0]);
        } else {
            int script_fd = f_open(argv[0], F_READ);
            if (script_fd == -1) {
                p_perror(NULL);
            }

            char buf[script_size + 1];
            buf[script_size] = '\0';

            if (f_read(script_fd, script_size, buf) == -1) {
                p_perror(NULL);
            }

            const char *delim = "\n";
            char *line = strtok(buf, delim);
            while (line != NULL) {
                handle_input(line, 0);
                line = strtok(NULL, delim);
            }

            if (f_close(script_fd) == -1) {
                p_perror(NULL);
            }
        }
    }

    p_exit();
}
