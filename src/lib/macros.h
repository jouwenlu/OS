// Defines useful macros to use.

#pragma once

#include <stdio.h>
#include <stdlib.h>

#define FILE_OPEN_MODE 0666

#define MAX_OPEN_FDS 1024

#define EMPTY_FD -1

#define HANDLE_INVALID_INPUT(valEvaluatesToTrueIfInvalid, error_msg) \
        if (valEvaluatesToTrueIfInvalid) { \
            write(STDERR_FILENO, error_msg, sizeof(error_msg)); \
            return false; \
        }

#define HANDLE_INVALID_INPUT_RET_VAL(valEvaluatesToTrueIfInvalid, error_msg, ret_val) \
        if (valEvaluatesToTrueIfInvalid) { \
            write(STDERR_FILENO, error_msg, sizeof(error_msg)); \
            return ret_val; \
        }

#define HANDLE_INVALID_INPUT_VOID(valEvaluatesToTrueIfInvalid, error_msg) \
        if (valEvaluatesToTrueIfInvalid) { \
            write(STDERR_FILENO, error_msg, sizeof(error_msg)); \
            return; \
        }

#define HANDLE_INVALID_INPUT_VOID_FMT(valEvaluatesToTrueIfInvalid, formatted_error_msg, arg1) \
        if (valEvaluatesToTrueIfInvalid) { \
            fprintf(stderr, formatted_error_msg, arg1); \
            return; \
        }

#define HANDLE_SYS_CALL(valEvaluatesToTrueIfSysCallError, error_msg) \
        if (valEvaluatesToTrueIfSysCallError) { \
            perror(error_msg); \
            exit(EXIT_FAILURE); \
        }

#define TICKS_PER_SECOND 10

#define SECONDS_TO_TICKS(sec) sec * TICKS_PER_SECOND

#define MIN(a, b) (a < b ? a : b)

#define MAX(a, b) (a > b ? a : b)