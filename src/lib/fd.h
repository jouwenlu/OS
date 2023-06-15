// Declaration of the OFT for the file system.

#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "../kernel/scheduler.h"
#include "../lib/pcb.h"
#include "directory_entry.h"
#include "linked_list.h"
#include "parser.h"

#define EOF_IDX 0xFFFF

typedef enum {
    F_WRITE = 1,
    F_READ = 2,
    F_APPEND = 3
} mode;

typedef enum {
    F_SEEK_SET = 0,
    F_SEEK_CUR = 1,
    F_SEEK_END = 2
} whence;

typedef struct fd_st {
    // The corresponding Directory Entry.
    directory_entry *de;

    // The index assigned to the file descriptor.
    int ind;

    // The number of times this file is currently open/being referenced.
    int ref_index;

    // The mode in which this file was opened.
    int mode;

    // The offset in the fs of the data region.
    int f_pos;

    // The offset in the fs of the directory entry.
    int d_pos;
} file_descriptor;

// The predicate used to find an element in a linked list via name.
bool OFT_find_fd_by_name_predicate(void *fd_name, void *ll_val);

// The predicate used to find an element in a linked list via fd number.
bool OFT_find_fd_by_fd_predicate(void *fd_ind, void *ll_val);

// Returns true if there is an element in the OFT that has the same directory entry but a different fd.
bool exists_other_fd_by_d_pos(linked_list *OFT, int d_pos, int ind);

// Frees the given fd pointer.
void free_file_descriptor(void *file_descriptor);

// Returns whether the given string fname meets the POSIX standard.
bool is_posix(const char *fname);
