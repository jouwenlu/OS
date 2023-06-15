#include "errno.h"
#include "../user/file_user_funcs.h"

errno_st ERRNO;

void set_errno(errno_st input) {
    ERRNO = input;
}

void p_perror(char *shell_pass_in) {
    char result[1000] = "";

    if (ERRNO == NOCHILDCREATED) {
        strcat(result, "p_perror: k_process_create failed to create a child.\n");
    } else if (ERRNO == ACTIVEJOBNULL) {
        strcat(result, "p_perror: Active job is null (idle).\n");
    } else if (ERRNO == NOTINPCBTABLE) {
        strcat(result, "p_perror: The pid could not be found in the pcb table.\n");
    } else if (ERRNO == KILLZOMBIE) {
        strcat(result, "p_perror: Attempted to send signal to a zombie process or nonexistant process.\n");
    } else if (ERRNO == NOSUCHCHILD) {
        strcat(result, "p_perror: The parent does not have a child with desired pid.\n");
    } else if (ERRNO == NOTFOUNDINSCHEDULER) {
        strcat(result, "p_perror: Job could not be found in the scheduler queue of right priority.\n");
    } else if (ERRNO == INVALIDPRIORITY) {
        strcat(result, "p_perror: Priorities can only be -1, 0, 1.\n");
    } else if (ERRNO == INVALIDSIGNAL) {
        strcat(result, "p_perror: Signals can only be term, stop, cont.\n");
    } else if (ERRNO == INVALID_WHENCE) {
        strcat(result, "p_perror: Make sure `whence` is correct.\n");
    } else if (ERRNO == INVALID_OFFSET) {
        strcat(result, "p_perror: Offset cannot be negative.\n");
    } else if (ERRNO == FILE_NOT_FOUND) {
        strcat(result, "p_perror: File not found with given fd in k_lseek.\n");
    } else if (ERRNO == UNALLOCATED_BLOCK) {
        strcat(result, "p_perror: The first block is 0xFFFF, please allocate a block for it.\n");
    } else if (ERRNO == PERMISSION_DENIED) {
        strcat(result, "p_perror: Insufficient file permissions.\n");
    } else if (ERRNO == INVALID_MODE) {
        strcat(result, "p_perror: f_open Mode set to an invalid value.\n");
    } else if (ERRNO == INVALID_FILE_NAME) {
        strcat(result, "p_perror: f_open File name is NULL.\n");
    } else if (ERRNO == INVALID_FILE_NAME_POSIX) {
        strcat(result, "p_perror: f_open File name does not match POSIX standard.\n");
    } else if (ERRNO == ATTEMPTED_DOUBLE_WRITE) {
        strcat(result, "p_perror: f_open File is already open in write mode.\n");
    } else if (ERRNO == READ_FILE_NOT_FOUND) {
        strcat(result, "p_perror: f_open File not found and was opened in F_READ mode.\n");
    } else if (ERRNO == CLOSE_UNOPEN_FILE) {
        strcat(result, "p_perror: f_close File is not open.\n");
    } else if (ERRNO == DOUBLE_DELETION) {
        strcat(result, "p_perror: File has already been deleted.\n");
    } else if (ERRNO == FILE_NOT_FOUND_OFT) {
        strcat(result, "p_perror: fd not found in OFT\n");
    } else if (ERRNO == NO_MORE_SPACE) {
        strcat(result, "p_perror: no more space in the fs\n");
    } else {
        return;
    }

    if (shell_pass_in != NULL) {
        strcat(result, shell_pass_in);
    }

    f_write(STDOUT_FILENO, result, strlen(result));
    set_errno(NOERROR);
}