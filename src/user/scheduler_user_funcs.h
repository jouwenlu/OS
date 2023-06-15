// Function declarations for scheduling related user-level functions.

#pragma once

#include <sys/types.h>

// Sets the priority of the thread pid to priority.
int p_nice(pid_t pid, int priority);

// Sets the calling process to blocked until ticks of the system clock elapse,
// and then sets the thread to running. Importantly, p_sleep should not return
// until the thread resumes running; however, it can be interrupted by a
// S_SIGTERM signal. Like sleep(3) in Linux, the clock keeps ticking even when
// p_sleep is interrupted.
void p_sleep(unsigned int ticks);