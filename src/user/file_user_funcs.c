// Function definitions for file related user-level functions.

#include "file_user_funcs.h"

#include "../fat/fat_util.h"
#include "../fat/file_kernel_funcs.h"
#include "../lib/fd.h"
#include "../lib/file_system.h"
#include "../lib/linked_list.h"
#include "../lib/signals.h"
#include "../user/process_user_funcs.h"
#include "../lib/errno.h"

linked_list OFT;
file_system *f_fs;
int ind;

int f_mount(char *fs_name) {
    init_linked_list(&OFT);

    // Preserving the 0 and 1 index for STDIN and STDOUT
    ind = 3;

    init_unmounted_fs();
    mount(fs_name);
    f_fs = get_mounted_fs();

    if (f_fs == NULL) {
        return -1;
    }

    return 1;
}

void f_unmount(char *fs_name) {
    clear(&OFT, free_file_descriptor);
    umount();
    f_fs = NULL;
}

int find_d_pos(char *name) {
    // Iterate through the directory entry linked list and keep track of how many elements have been iterated through.
    // Note also that the size of each directory entry will always be 64 bytes.
    linked_list_elem *curr = f_fs->dir.head;

    int de_offset = 0;
    int block_num = 1;
    bool first_iter = true;
    int max_de = f_fs->block_size / sizeof(directory_entry);

    while (curr != NULL) {
        if (de_offset == 0) {
            if (!first_iter) {
                block_num = f_fs->fat_region[block_num];
            }

            first_iter = false;
        }

        directory_entry *de = curr->val;

        if (de != NULL) {
            // Only throw an error if the current mode is F_WRITE as well.
            if (strcmp(de->name, name) == 0) {
                return get_offset_for_block_num(block_num) + de_offset * sizeof(directory_entry);
            }

            de_offset = (de_offset + 1) % max_de;
        }

        curr = curr->next;
    }

    // Returns -1 if nothing is found.
    return -1;
}

int f_open(const char *fname, int mode) {
    int ref = 0;

    if (mode < F_WRITE || mode > F_APPEND) {
        set_errno(INVALID_MODE);
        return -1;
    } else if (fname == NULL) {
        set_errno(INVALID_FILE_NAME);
        return -1;
    } else if (!is_posix(fname)) {
        set_errno(INVALID_FILE_NAME_POSIX);
        return -1;
    } else if (ind != 2) {
        // Checking if the file is already open in write mode.
        linked_list_elem *curr = OFT.head;

        while (curr != NULL) {
            file_descriptor *f = curr->val;

            if (f != NULL && f->de != NULL && strcmp(fname, f->de->name) == 0) {
                ref++;

                // only throw an error if the current mode is F_WRITE as well.
                if (mode == F_WRITE && f->mode == F_WRITE) {
                    set_errno(ATTEMPTED_DOUBLE_WRITE);
                    return -1;
                }
            }

            curr = curr->next;
        }
    }

    char *name = (char *) fname;
    linked_list_elem *fa = get_elem(&f_fs->dir, ll_find_file_by_name_predicate, name);
    if (fa == NULL) {
        fa = get_elem(&f_fs->dir, ll_find_removed_file, NULL);
    }

    directory_entry *d;

    if (fa == NULL) {
        if (mode == F_READ) {
            set_errno(READ_FILE_NOT_FOUND);
            return -1;
        }

        d = create_directory_entry();
        strcpy(d->name, name);
        push_back(&f_fs->dir, d);
    } else {
        d = fa->val;

        // Check permissions.
        if (mode == F_READ && (d->perm & READ_ONLY) == 0) {
            set_errno(PERMISSION_DENIED);
            return -1;
        }

        if ((mode == F_WRITE || mode == F_APPEND) && (d->perm & READ_WRITE) != READ_WRITE) {
            set_errno(PERMISSION_DENIED);
            return -1;
        }

        // If opened as write, then clear the file, and then append.
        if (mode == F_WRITE) {
            int block = d->firstBlock;

            while (block != EOF_IDX) {
                int next_block = f_fs->fat_region[block];
                f_fs->fat_region[block] = 0;
                block = next_block;
            }

            d->firstBlock = EOF_IDX;
            d->size = 0;
        }

        strcpy(d->name, name);
        d->mtime = time(NULL);
    }
    write_dell();

    file_descriptor *f = (file_descriptor *) malloc(sizeof(file_descriptor));
    HANDLE_SYS_CALL(f == NULL, "Unable to allocate FD\n");

    f->de = d;
    f->ind = ++ind;
    f->ref_index = 1;
    f->mode = mode;
    f->f_pos = -1;
    f->d_pos = find_d_pos(name);

    push_back(&OFT, f);

    // In the case that the filesize is non-zero, update the f->pos depending on the mode.
    if (d->size != 0 && f->mode == F_READ) {
        f_lseek(ind, 0, F_SEEK_SET);
    } else if (d->size != 0 && f->mode == F_APPEND) {
        f_lseek(ind, 0, F_SEEK_END);
    }

    // Need to add the fd to the pcb of the active process.
    pcb *active_job = get_active_job();
    for (int i = 0; i < 1024; i++) {
        if (active_job->fd[i] == -1) {
            active_job->fd[i] = ind;
            break;
        }
    }
    return ind;
}

int redirect(int fd) {
    pcb *calling_proc = get_active_job();

    if (fd == STDIN_FILENO) {
        return calling_proc->fd[0];
    }

    if (fd == STDOUT_FILENO) {
        return calling_proc->fd[1];
    }

    return fd;
}

int f_read(int fd, int n, char *buf) {
    fd = redirect(fd);

    // Terminal control.
    pcb *calling_proc = get_active_job();
    if (calling_proc->is_bg && fd == STDIN_FILENO) {
        p_kill(calling_proc->pid, S_SIGSTOP);
        suspend();
    }

    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        return read(fd, buf, n);
    }

    int temp = k_read(fd, n, buf, f_fs, &OFT);
    if (temp == -1) {
        set_errno(NO_MORE_SPACE);
        return -1;
    }
    return temp;
}

int f_write(int fd, const char *str, int n) {
    fd = redirect(fd);

    if (fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        return write(fd, str, n);
    }

    int temp = k_write(fd, n, (char *) str, f_fs, &OFT);
    if (temp == -1) {
        set_errno(NO_MORE_SPACE);
        return -1;
    }
    return temp;
}

int f_close(int fd) {
    // Remove this from the OFT.
    // Also remove the fd from the active_job (see f_open()).
    linked_list_elem *elem = get_elem(&OFT, OFT_find_fd_by_fd_predicate, &fd);

    if (elem == NULL) {
        set_errno(CLOSE_UNOPEN_FILE);
        return -1;
    }

    file_descriptor *file = elem->val;

    // If the number of references to this fd is greater than 1, then decrement the number of references and reset the value in the fd array in the active job.
    if (file->ref_index >= 1) {
        file->ref_index--;
        pcb *active_job = get_active_job();

        for (int i = 0; i < 1024; i++) {
            if (active_job->fd[i] == file->ind) {
                active_job->fd[i] = -1;
                break;
            }
        }
    }

    if (file->ref_index != 0) {
        return 1;
    }

    directory_entry *de = (directory_entry *) file->de;

    // If the file was not unlinked yet, then do not delete the file, but free it from the OFT.
    if (!exists_other_fd_by_d_pos(&OFT, file->d_pos, file->ind)) {
        // In this case, this is the last instance of the file, so we delete it if it starts with a '2'.
        if (de->name[0] == (char) DELETED_BUT_IN_USE) {
            // mark the filename with a 1
            de->name[0] = (char) DELETED;
            de->size = 0;
            int next_block = de->firstBlock;

            while (next_block != EOF_IDX) {
                int temp_block = f_fs->fat_region[next_block];
                f_fs->fat_region[next_block] = 0;
                next_block = temp_block;
            }

            de->firstBlock = EOF_IDX;
            write_dell();
        }
    }

    remove_elem(&OFT, OFT_find_fd_by_fd_predicate, &fd, free_file_descriptor);
    return 1;
}

int f_unlink(const char *fname) {
    char *name = (char *) fname;
    linked_list_elem *elem = get_elem(&f_fs->dir, ll_find_file_by_name_predicate, name);

    if (elem == NULL) {
        set_errno(FILE_NOT_FOUND);
        return -1;
    }

    directory_entry *de = (directory_entry *) elem->val;
    if (de->name[0] < FILE_EXISTS) {
        set_errno(DOUBLE_DELETION);
        return -1;
    }

    linked_list_elem *elem_OFT = get_elem(&OFT, OFT_find_fd_by_name_predicate, name);
    if (elem_OFT == NULL) {
        // In this case, the file is not open. Therefore, we delete the file and free the FAT.
        // mark the filename with a 1.
        de->name[0] = (char) DELETED;
        de->size = 0;
        int next_block = de->firstBlock;

        while (next_block != EOF_IDX) {
            int temp_block = f_fs->fat_region[next_block];
            f_fs->fat_region[next_block] = 0;
            next_block = temp_block;
        }

        de->firstBlock = EOF_IDX;
    } else {
        // In this case, the file is open. Therefore, we mark it as deleted but in use.
        // mark the filename with a 2.
        de->name[0] = DELETED_BUT_IN_USE;
    }

    write_dell();
    return 1;
}

int f_lseek(int fd, int offset, int whence) {
    int temp = k_lseek(fd, offset, whence, f_fs, &OFT);
    if (temp == -1) {
        set_errno(NO_MORE_SPACE);
        return -1;
    }
    return temp;
}

int f_ls(char *filename) {
    // Since the file wasn't found, print out all files.
    // The columns above are first block number, permissions, size, month, day, time, and name.
    char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    for (linked_list_elem *l = f_fs->dir.head; l != NULL; l = l->next) {
        directory_entry *de = l->val;

        if (filename != NULL && strcmp(de->name, filename) != 0) {
            continue;
        }

        if (de->name[0] < FILE_EXISTS) {
            continue;
        }

        int block_num = de->firstBlock;
        char x = de->perm & EXEC_ONLY ? 'x' : '-';
        char r = de->perm & READ_ONLY ? 'r' : '-';
        char w = de->perm & WRITE_ONLY ? 'w' : '-';
        int size = de->size;
        char *name = de->name;

        struct tm t = *localtime(&de->mtime);

        char *month = months[t.tm_mon];
        int day = t.tm_mday;
        int hour = t.tm_hour;
        int minute = t.tm_min;

        char str[MAX_LINE_LENGTH] = {'\0'}; // Way more than necessary.
        sprintf(str, "%5d %c%c%c %5d %s %-2d %02d:%02d %s\n", block_num, x, r, w, size, month, day, hour, minute, name);
        f_write(STDOUT_FILENO, str, strlen(str));
    }

    return 1;
}

int f_rename(int fd, char *new_name) {
    linked_list_elem *elem = get_elem(&OFT, OFT_find_fd_by_fd_predicate, &fd);
    if (elem == NULL) {
        set_errno(FILE_NOT_FOUND);
        return -1;
    }

    file_descriptor *f = (file_descriptor *) elem->val;
    strcpy(f->de->name, new_name);
    f->de->mtime = time(NULL);
    write_dell();
    return 1;
}

int f_change_perms(char *fname, char *op, int perm) {
    linked_list_elem *elem = get_elem(&f_fs->dir, ll_find_file_by_name_predicate, fname);
    if (elem == NULL) {
        set_errno(FILE_NOT_FOUND);
        return -1;
    }

    directory_entry *de = (directory_entry *) elem->val;
    if (op[0] == '+') {
        de->perm |= perm;
    } else {
        de->perm &= ~perm;
    }

    write_dell();
    return 1;
}

void f_update_new_child_fd(pcb *child) {
    for (int i = 0; i < MAX_OPEN_FDS; i++) {
        if (child->fd[i] > 2) {
            linked_list_elem *elem = get_elem(&OFT, OFT_find_fd_by_fd_predicate, &child->fd[i]);

            if (elem != NULL) {
                file_descriptor *f = elem->val;
                f->ref_index++;
            } else {
                set_errno(FILE_NOT_FOUND_OFT);
                return;
            }
        }
    }
}

void f_close_child_fd(pcb *child) {
    for (int i = 0; i < MAX_OPEN_FDS; i++) {
        if (child->fd[i] > 2) {
            linked_list_elem *elem = get_elem(&OFT, OFT_find_fd_by_fd_predicate, &child->fd[i]);

            if (elem != NULL) {
                f_close(child->fd[i]);
            } else {
                set_errno(FILE_NOT_FOUND_OFT);
                return;
            }
        }
    }
}

// Returns -1 if file does not exist, otherwise returns size of the file.
int f_size(char *file) {
    linked_list_elem *elem = get_elem(&f_fs->dir, ll_find_file_by_name_predicate, file);

    if (elem == NULL) {
        return -1;
    }

    directory_entry *de = elem->val;
    return de->size;
}

bool f_has_permissions(char *file, int perm) {
    linked_list_elem *elem = get_elem(&f_fs->dir, ll_find_file_by_name_predicate, file);

    if (elem == NULL) {
        return false;
    }

    directory_entry *de = elem->val;
    return (de->perm & perm) != 0;
}
