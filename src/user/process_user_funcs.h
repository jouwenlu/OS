// Function declarations for process related user-level functions.

#pragma once

#include <stdbool.h>
#include <sys/types.h>

// Forks a new thread that retains most of the attributes of the parent thread.
pid_t p_spawn(void (*func)(), char *argv[], int fd0, int fd1, int prior);

// Sets the calling thread as blocked (if nohang is false) until a child of the calling thread changes state.
// p_waitpid returns the pid of the child which has changed state on success, or -1 on error.
pid_t p_waitpid(pid_t pid, int *wstatus, bool nohang);

// Sends the signal sig to the thread referenced by pid. It returns 0 on success, or -1 on error.
int p_kill(pid_t pid, int sig);

// Exits the current thread unconditionally.
void p_exit(void);