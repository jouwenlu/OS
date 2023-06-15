// Definition of the logger functions.

#include "log.h"

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "macros.h"

static char evt_names[11][10] = {
        "SCHEDULE",
        "CREATE",
        "SIGNALED",
        "EXITED",
        "ZOMBIE",
        "ORPHAN",
        "WAITED",
        "BLOCKED",
        "UNBLOCKED",
        "STOPPED",
        "CONTINUED"
};

static unsigned int clock_ticks = 0;

static int log_fd = -1;

void init_log(char *log_name) {
    int fd = open(log_name, O_WRONLY | O_CREAT | O_TRUNC, FILE_OPEN_MODE);
    HANDLE_SYS_CALL(fd < 0, "Error opening log file.");
    log_fd = fd;
}

void increment_clock_ticks() {
    clock_ticks = clock_ticks + 1;
}

unsigned int get_clock_ticks() {
    return clock_ticks;
}

void close_log() {
    HANDLE_SYS_CALL(close(log_fd) < 0, "Error closing log file.");
    log_fd = -1;
}

void log_event(LogEvent evt, pcb *proc) {
    dprintf(log_fd, "[%u] %s %d %d %s\n", clock_ticks, evt_names[evt], proc->pid, proc->priority, proc->cmd);

    char *status_str = NULL;

    if (evt == STOPPED_EVT) {
        status_str = "Stopped";
    } else if (evt == CONTINUED_EVT) {
        if (proc->is_bg) {
            status_str = "Running";
        } else {
            status_str = "Restarting";
        }
    }
    if (status_str != NULL) {
        fprintf(stderr, "%s: %s\n", status_str, proc->cmd);
    }
}

void log_nice_event(pcb *proc, int old_nice) {
    dprintf(log_fd, "[%u] NICE %d %d %d %s\n", clock_ticks, proc->pid, old_nice, proc->priority, proc->cmd);
}

