// Definition of status functions.

#include "status.h"

bool W_WIFEXITED(int status) {
    return status == EXITED;
}

bool W_WIFSTOPPED(int status) {
    return status == STOPPED;
}

bool W_WIFSIGNALED(int status) {
    return status == TERMINATED;
}

char get_status_code(process_status status) {
    switch (status) {
        case STOPPED:
            return 'S';
        case RUNNING:
            return 'R';
        case ZOMBIED:
        case TERMINATED:
        case EXITED:
            return 'Z';
        case ORPHANED:
        default:
            return 'O';
    }
}