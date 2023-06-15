// Function definitions for file related kernel-level functions.

#include "file_kernel_funcs.h"

#include "fat_util.h"
#include "../lib/fd.h"
#include "../lib/errno.h"

int next_free_block_standalone(file_system *fs) {
    for (int i = 2; i < fs->num_fat_entries; i++) {
        if (fs->fat_region[i] == 0 && i != EOF_IDX) {
            return i;
        }
    }

    set_errno(NO_MORE_SPACE);
    return -1;
}

// Returns the file position for l_seek() given the current block (to start on) and the offset.
int find_curr_block_in_fat(int curr_block, int offset, file_system *f_fs) {
    while (offset > f_fs->block_size) {
        // iterating by block_size chunks thru the FAT.
        offset -= f_fs->block_size;
        // iterating to the next block.
        if (offset > 0) {
            // if a new block is necessary, add it
            // in this case, we need to pad the hole with '\0', and then set the offset.
            if (f_fs->fat_region[curr_block] == EOF_IDX) {
                f_fs->fat_region[curr_block] = next_free_block_standalone(f_fs);
                if ((int) f_fs->fat_region[curr_block] == -1) {
                    set_errno(NO_MORE_SPACE);
                    return -1;
                }
                curr_block = f_fs->fat_region[curr_block];
                f_fs->fat_region[curr_block] = EOF_IDX;
            } else {
                curr_block = f_fs->fat_region[curr_block];
            }
        }
    }

    return get_offset_for_block_num(curr_block) + offset;
}

int k_lseek(int fd, int offset, int whence, file_system *f_fs, linked_list *OFT) {
    // lseek() allows the file offset to be set beyond the end of the
    // file (but this does not change the size of the file).  If data is
    // later written at this point, subsequent reads of the data in the
    // gap (a "hole") return null bytes ('\0') until data is actually
    // written into the gap.
    if (whence < 0 || whence > 2) {
        set_errno(INVALID_WHENCE);
        return -1;
    } else if (offset < 0) {
        set_errno(INVALID_OFFSET);
        return -1;
    }

    linked_list_elem *f = get_elem(OFT, OFT_find_fd_by_fd_predicate, &fd);

    if (f == NULL) {
        set_errno(FILE_NOT_FOUND);
        return -1;
    }

    file_descriptor *file = f->val;
    int curr_block = file->de->firstBlock;
    if (file->de->firstBlock == EOF_IDX) {
        set_errno(UNALLOCATED_BLOCK);
        return -1;
    }

    // We are at the beginning of the file.
    if (whence == F_SEEK_SET) {
        // Iterate through the FAT to find the offset.
        file->f_pos = find_curr_block_in_fat(curr_block, offset, f_fs);
        if (file->f_pos == -1) {
            set_errno(NO_MORE_SPACE);
            return -1;
        }
        return 1;
    } else if (whence == F_SEEK_CUR) {
        if (file->f_pos + offset < file->de->size) {
            // Basically adding the stuff in the current block to the offset since we'll be doing things by block sized chunks.
            offset += (file->f_pos - f_fs->block_size) % f_fs->block_size;
            // Note that the block num is always offset by the directory block.
            // Hence, we subtract by the block size and then we divide by the block_size.
            int temp = next_free_block_standalone(f_fs);
            if (temp == -1) {
                set_errno(NO_MORE_SPACE);
                return -1;
            }
            curr_block =
                    curr_block == EOF_IDX ? temp : (file->f_pos - f_fs->block_size) / f_fs->block_size;
            file->f_pos = find_curr_block_in_fat(curr_block, offset, f_fs);
            if (file->f_pos == -1){
                set_errno(NO_MORE_SPACE);
                return -1;
            }
        } else {
            // Get the last block of the file in the FAT.
            file->f_pos = find_curr_block_in_fat(curr_block, file->de->size, f_fs);
            if (file->f_pos == -1){
                set_errno(NO_MORE_SPACE);
                return -1;
            }

            // Now write a bunch of 0's.
            if (offset > 0) {
                char hole[offset];
                memset(hole, '\0', offset);
                k_write(fd, offset, hole, f_fs, OFT);
            }
        }

        return 1;
    } else {
        // Get the last block of the file in the FAT.
        file->f_pos = find_curr_block_in_fat(curr_block, file->de->size, f_fs);
        if (file->f_pos == -1){
            set_errno(NO_MORE_SPACE);
            return -1;
        }

        // now write a bunch of 0's
        if (offset > 0) {
            char hole[offset];
            memset(hole, '\0', offset);
            k_write(fd, offset, hole, f_fs, OFT);
        }

        return 1;
    }
}

int k_read(int fd, int n, char *buf, file_system *f_fs, linked_list *OFT) {
    linked_list_elem *f = get_elem(OFT, OFT_find_fd_by_fd_predicate, &fd);

    if (f == NULL) {
        return -1;
    }

    file_descriptor *file = f->val;

    // Not reading anything.
    if (n == 0) {
        return 0;
    }

    directory_entry *de = file->de;

    // Empty file, so not reading anything.
    if (de->size == 0) {
        return 0;
    }

    if (file->f_pos == -1) {
        k_lseek(fd, 0, F_SEEK_SET, f_fs, OFT);
    }

    int current_block = get_block_num_from_offset(file->f_pos);
    int offset = get_offset_within_block(file->f_pos);

    int total_bytes_read = 0;

    int bytes_before_loc = 0;
    int block = de->firstBlock;

    while (block != current_block) {
        block = f_fs->fat_region[block];
        bytes_before_loc += f_fs->block_size;
    }

    bytes_before_loc += offset;

    int bytes_remaining_in_file = de->size - bytes_before_loc;

    // END OF FILE.
    if (bytes_remaining_in_file <= 0) {
        return 0;
    }

    while (n > 0 && bytes_remaining_in_file > 0 && current_block != EOF_IDX) {
        // Read whichever is smaller, n bytes, remainder of file, or the rest of the block.
        int bytes_to_read = MIN(n, MIN(bytes_remaining_in_file, f_fs->block_size - offset));

        HANDLE_SYS_CALL(lseek(f_fs->fd, get_offset_for_block_num(current_block) + offset, SEEK_SET) < 0,
                        "lseek: Issue k_read from fs\n");
        HANDLE_SYS_CALL(read(f_fs->fd, buf + total_bytes_read, bytes_to_read) < 0, "read: Issue k_read from fs\n");

        total_bytes_read += bytes_to_read;
        n -= bytes_to_read;
        bytes_remaining_in_file -= bytes_to_read;

        // Reset offset after first block.
        offset = 0;

        if (bytes_remaining_in_file > 0 && n > 0) {
            current_block = f_fs->fat_region[current_block];
        }
    }

    file->f_pos = get_offset_for_block_num(current_block) + ((bytes_before_loc + total_bytes_read) % f_fs->block_size);

    return total_bytes_read;
}

int k_write(int fd, int n, char *buf, file_system *f_fs, linked_list *OFT) {
    linked_list_elem *f = get_elem(OFT, OFT_find_fd_by_fd_predicate, &fd);

    if (f == NULL) {
        return -1;
    }

    file_descriptor *file = f->val;

    // Insufficient permissions.
    if (file->mode != F_WRITE && file->mode != F_APPEND) {
        set_errno(PERMISSION_DENIED);
        return -1;
    }

    // Not writing anything.
    if (n == 0) {
        return 0;
    }

    directory_entry *de = file->de;

    // New file.
    if (file->f_pos == -1) {
        de->firstBlock = next_free_block_standalone(f_fs);
        if ((int) de->firstBlock == -1) {
            set_errno(NO_MORE_SPACE);
            return -1;
        }
        f_fs->fat_region[de->firstBlock] = EOF_IDX;
        k_lseek(fd, 0, F_SEEK_SET, f_fs, OFT);
    }

    int current_block = get_block_num_from_offset(file->f_pos);
    int offset = get_offset_within_block(file->f_pos);

    int total_bytes_written = 0;

    int block = de->firstBlock;
    int bytes_before_loc = 0;

    while (block != current_block) {
        block = f_fs->fat_region[block];
        bytes_before_loc += f_fs->block_size;
    }

    bytes_before_loc += offset;

    while (n > 0) {
        // Write whichever is smaller: n bytes or the rest of the block.
        int bytes_to_write = MIN(n, f_fs->block_size - offset);

        HANDLE_SYS_CALL(lseek(f_fs->fd, get_offset_for_block_num(current_block) + offset, SEEK_SET) < 0,
                        "lseek: Issue k_write from fs\n");
        HANDLE_SYS_CALL(write(f_fs->fd, buf + total_bytes_written, bytes_to_write) < 0,
                        "write: Issue k_write from fs\n");

        total_bytes_written += bytes_to_write;
        n -= bytes_to_write;

        // Reset offset after first block.
        offset = 0;

        if (n > 0) {
            // Allocate new block if necessary.
            if (f_fs->fat_region[current_block] == EOF_IDX) {
                f_fs->fat_region[current_block] = next_free_block_standalone(f_fs);
                if ((int) f_fs->fat_region[current_block] == -1) {
                    set_errno(NO_MORE_SPACE);
                    return -1;
                }
            }

            current_block = f_fs->fat_region[current_block];

            if (f_fs->fat_region[current_block] == 0) {
                f_fs->fat_region[current_block] = EOF_IDX;
            }
        }
    }

    de->mtime = time(NULL);
    de->size = MAX(bytes_before_loc + total_bytes_written, de->size);
    file->f_pos = get_offset_for_block_num(current_block) + (de->size % f_fs->block_size);
    write_dell();

    return total_bytes_written;
}