// Declaration of status type and user-level functions.

#pragma once

#include <stdbool.h>

typedef enum {
    STOPPED,
    RUNNING,
    ZOMBIED,
    TERMINATED,
    EXITED,
    ORPHANED,
} process_status;

// Returns true if the child terminated normally, that is, by a call to p_exit
// or by returning. User-level function.
bool W_WIFEXITED(int status);

// Returns true if the child was stopped by a signal. User-level function.
bool W_WIFSTOPPED(int status);

// Returns true if the child was terminated by a signal, that is, by a call to
// p_kill with the S_SIGTERM signal. User-level function.
bool W_WIFSIGNALED(int status);

// Returns a 1 character code which is a human readable version representation
// to print out.
char get_status_code(process_status status);