// Declaration of error handling data structures and functions.

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef enum {
    // Process/kernel errors
    NOERROR,
    NOCHILDCREATED,
    ACTIVEJOBNULL,
    NOTINPCBTABLE,
    KILLZOMBIE,
    NOSUCHCHILD,
    NOTFOUNDINSCHEDULER,
    INVALIDPRIORITY,
    INVALIDSIGNAL,

    // File kernel errors
    INVALID_WHENCE,
    INVALID_OFFSET,
    FILE_NOT_FOUND,
    UNALLOCATED_BLOCK,
    PERMISSION_DENIED,

    // File user errors
    INVALID_MODE,
    INVALID_FILE_NAME,
    INVALID_FILE_NAME_POSIX,
    ATTEMPTED_DOUBLE_WRITE,
    READ_FILE_NOT_FOUND,
    CLOSE_UNOPEN_FILE,
    DOUBLE_DELETION,
    FILE_NOT_FOUND_OFT,

    // Critical Errors
    NO_MORE_SPACE
} errno_st;

// Sets the global error number.
void set_errno(errno_st i);

// Prints the corresponding error message for the error message.
void p_perror(char *shell_pass_in);