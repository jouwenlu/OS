// Implementation of the OFT for the file system.

#include "fd.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "macros.h"
#include "../fat/fat_util.h"
#include "../lib/pcb.h"
#include "../shell/shell.h"
#include "../kernel/scheduler.h"
#include "../user/process_user_funcs.h"

bool OFT_find_fd_by_name_predicate(void *fd_name, void *ll_val) {
    if (ll_val == NULL) {
        return false;
    }
    file_descriptor *fd = ll_val;
    directory_entry *de = fd->de;
    char *de_name = de->name;
    return strcmp((char *) fd_name, de_name) == 0;
}

bool OFT_find_fd_by_fd_predicate(void *fd_ind, void *ll_val) {
    if (ll_val == NULL) {
        return false;
    }
    file_descriptor *fd = ll_val;
    int *fd_int = (int *) fd_ind;
    return fd->ind == *fd_int;
}

bool exists_other_fd_by_d_pos(linked_list *OFT, int d_pos, int ind) {
    linked_list_elem *curr_elem = OFT->head;
    while (curr_elem != NULL) {
        file_descriptor *fd = curr_elem->val;
        if (fd != NULL) {
            if (fd->ind != ind && fd->d_pos == d_pos) {
                return true;
            }
        }
        curr_elem = curr_elem->next;
    }
    return false;
}

void free_file_descriptor(void *file_descriptor) {
    free(file_descriptor);
    file_descriptor = NULL;
}

bool is_posix(const char *fname) {
    if (fname == NULL) {
        return false;
    }

    for (int i = 0; i < strlen(fname); i++) {
        char c = fname[i];
        if (!((c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') ||
              c == '.' || c == '_' || c == '-')) {
            return false;
        }
    }
    return true;
}
