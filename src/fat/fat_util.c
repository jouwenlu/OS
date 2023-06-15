// Implementation of FAT API for manipulating the file system.

#include "fat_util.h"

#include "../lib/fd.h"
#include "../lib/file_system.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

#include "../lib/macros.h"

file_system fs;

void init_unmounted_fs() {
    fs.is_mounted = false;
    init_linked_list(&fs.dir);
}

file_system *get_mounted_fs() {
    return fs.is_mounted == false ? NULL : &fs;
}

bool exec_fat_command(char *command[]) {
    char **c = command;
    char *cmd_name = c[0];
    int num_args = 0; // Includes the command: 'mkfs small 3 2' has num_args = 4

    for (; command[num_args] != NULL; num_args++);

    if (strcmp(cmd_name, "mkfs") == 0) {
        // Usage: mkfs FS_NAME BLOCKS_IN_FAT BLOCK_SIZE_CONFIG
        // BLOCKS_IN_FAT in [1, 32]
        // BLOCK_SIZE_CONFIG in [0, 4]

        HANDLE_INVALID_INPUT(num_args != 4, "Incorrect number of arguments.\n");

        char *fs_name = c[1];
        int blocks_in_fat = atoi(c[2]);
        int block_size_config = atoi(c[3]);

        HANDLE_INVALID_INPUT(blocks_in_fat < 1 || blocks_in_fat > 32, "blocks_in_fat not in range [1, 32].\n");
        HANDLE_INVALID_INPUT(block_size_config < 0 || block_size_config > 4,
                             "block_size_config not in range [0, 4].\n");

        mkfs(fs_name, blocks_in_fat, block_size_config);
    } else if (strcmp(cmd_name, "mount") == 0) {
        HANDLE_INVALID_INPUT(num_args != 2, "Incorrect number of arguments.\n");

        char *fs_name = c[1];
        mount(fs_name);
    } else if (strcmp(cmd_name, "umount") == 0) {
        HANDLE_INVALID_INPUT(num_args != 1, "Incorrect number of arguments.\n");
        umount();
    } else if (strcmp(cmd_name, "touch") == 0) {
        HANDLE_INVALID_INPUT(!fs.is_mounted, "No FS mounted.\n");
        HANDLE_INVALID_INPUT(num_args < 2, "Incorrect number of arguments.\n");

        for (int i = 1; i < num_args; i++) {
            touch(c[i]);
        }
    } else if (strcmp(cmd_name, "mv") == 0) {
        HANDLE_INVALID_INPUT(!fs.is_mounted, "No FS mounted.\n");
        HANDLE_INVALID_INPUT(num_args != 3, "Incorrect number of arguments.\n");
        mv(c[1], c[2]);
    } else if (strcmp(cmd_name, "rm") == 0) {
        HANDLE_INVALID_INPUT(!fs.is_mounted, "No FS mounted.\n");
        HANDLE_INVALID_INPUT(num_args < 2, "Incorrect number of arguments.\n");

        for (int i = 1; i < num_args; i++) {
            rm(c[i]);
        }
    } else if (strcmp(cmd_name, "cat") == 0) {   // 4 variations
        HANDLE_INVALID_INPUT(!fs.is_mounted, "No FS mounted.\n");
        HANDLE_INVALID_INPUT(num_args < 2, "Incorrect number of arguments.\n");
        cat(c, num_args);
    } else if (strcmp(cmd_name, "cp") == 0) {
        HANDLE_INVALID_INPUT(!fs.is_mounted, "No FS mounted.\n");
        HANDLE_INVALID_INPUT(num_args < 3 || num_args > 4, "Incorrect number of arguments.\n");

        if (num_args == 3) {
            cpFATtoFAT(c[1], c[2]);
        } else if (strcmp(c[1], "-h") == 0) {
            cpHosttoFAT(c[2], c[3]);
        } else if (strcmp(c[2], "-h") == 0) {
            cpFATtoHost(c[1], c[3]);
        } else {
            HANDLE_INVALID_INPUT(true, "Incorrect usage of cp\n");
        }
    } else if (strcmp(cmd_name, "ls") == 0) {
        HANDLE_INVALID_INPUT(!fs.is_mounted, "No FS mounted.\n");
        HANDLE_INVALID_INPUT(num_args != 1, "Incorrect number of arguments.\n");
        ls();
    } else if (strcmp(cmd_name, "chmod") == 0) {
        HANDLE_INVALID_INPUT(num_args < 3, "Incorrect number of arguments.\n");

        for (int i = 2; i < num_args; i++) {
            chmod(c[1], c[i]);
        }
    } else {
        // Unknown/Un-implemented method
        return false;
    }

    return true;
}

int get_block_size_from_config(int block_size_config) {
    return block_size_config <= 0 ? 256 : 512 << (block_size_config - 1);
}

void mkfs(char *fs_name, int blocks_in_fat, int block_size_config) {
    int block_size = get_block_size_from_config(block_size_config);
    int num_fat_entries = (block_size * blocks_in_fat) / 2;
    int data_region_size = block_size * (num_fat_entries - 1);

    // Edge case, because you can't have a link to 0xFFFF (EOF indicator).
    if (num_fat_entries == 0x10000) {
        data_region_size -= block_size;
    }

    int fd = open(fs_name, O_WRONLY | O_CREAT | O_TRUNC, FILE_OPEN_MODE);
    HANDLE_SYS_CALL(fd < 0, "Error opening file in mkfs");

    // MSB = blocks_in_fat, LSB = block_size_config.
    uint16_t fat0 = (((blocks_in_fat & 0xFF) << 8) | (block_size_config & 0xFF));

    // Directory block takes only 1 block initially.
    uint16_t fat1 = EOF_IDX;

    HANDLE_SYS_CALL(write(fd, &fat0, sizeof(uint16_t)) < 0, "Error writing fat[0]");
    HANDLE_SYS_CALL(write(fd, &fat1, sizeof(uint16_t)) < 0, "Error writing fat[1]");

    uint16_t empty = 0;

    for (int i = 2; i < num_fat_entries; i++) {
        HANDLE_SYS_CALL(write(fd, &empty, sizeof(uint16_t)) < 0, "Error writing FAT region");
    }

    uint8_t data_block[block_size];
    for (int i = 0; i < block_size; i++) {
        data_block[i] = 0;
    }

    for (int i = 0; i < data_region_size; i += block_size) {
        HANDLE_SYS_CALL(write(fd, &data_block, sizeof(uint8_t) * block_size) < 0, "Error writing data region");
    }

    HANDLE_SYS_CALL(close(fd) < 0, "Error closing file");
}

off_t get_offset_for_block_num(uint16_t block_num) {
    return block_num == EOF_IDX ? -1 : fs.fat_size + (block_num - 1) * fs.block_size;
}

int get_block_num_from_offset(int offset) {
    return offset < fs.fat_size ? EOF_IDX : (offset - fs.fat_size) / fs.block_size + 1;
}

int get_offset_within_block(int offset) {
    return offset < fs.fat_size ? offset % fs.fat_size : (offset - fs.fat_size) % fs.block_size;
}

int next_free_block() {
    for (int i = 2; i < fs.num_fat_entries; i++) {
        if (fs.fat_region[i] == 0 && i != EOF_IDX) {
            return i;
        }
    }

    fprintf(stderr, "FAT has no free blocks.\n");
    exit(EXIT_FAILURE);
}

void read_directory_entries() {
    HANDLE_SYS_CALL(lseek(fs.fd, get_offset_for_block_num(1), SEEK_SET) < 0, "Error lseeking to read directory.");
    clear(&fs.dir, free_directory_entry);

    size_t entry_size = sizeof(directory_entry);

    int next_block = fs.fat_region[1];
    int entries_per_block = fs.block_size / entry_size;

    while (true) {
        for (int i = 0; i < entries_per_block; i++) {
            directory_entry *d = create_directory_entry();
            HANDLE_SYS_CALL(read(fs.fd, d, entry_size) < 0, "Error reading directory entry");

            // Empty entry.
            if (d->name[0] <= DELETED_BUT_IN_USE) {
                free(d);
            } else {
                push_back(&fs.dir, d);
            }
        }

        // End of directory block.
        if (next_block == EOF_IDX) {
            return;
        }

        HANDLE_SYS_CALL(lseek(fs.fd, get_offset_for_block_num(next_block), SEEK_SET) < 0,
                        "Error lseeking to read directory.");
        next_block = fs.fat_region[next_block];
    }
}

void write_dell() {
    if (!is_empty(&fs.dir)) {
        linked_list_elem *curr = fs.dir.head;

        // Maximum number of directory entries in one block.
        int max_de = fs.block_size / sizeof(directory_entry);

        // Keeping track of the first write, to prevent incorrect offsets.
        bool init_write = true;
        int count = 0;
        int curr_block = 1;
        uint8_t empty_block[fs.block_size];
        memset(empty_block, 0, fs.block_size);

        // Clear out directory entries in FAT.
        int dir_block = fs.fat_region[curr_block];
        fs.fat_region[curr_block] = EOF_IDX;
        while (dir_block != EOF_IDX) {
            int temp = fs.fat_region[dir_block];
            fs.fat_region[dir_block] = 0;
            dir_block = temp;
        }

        while (curr != NULL) {
            if (curr->val == NULL) {
                curr = curr->next;
                continue;
            }

            // If this is the initial write, or requires a new block, set
            // the pointer to the correct location and if necessary, point
            // the old block to the new one and set the new one to 0xFFFF.
            if (count % max_de == 0) {
                int block_num = 1;

                if (!init_write) {
                    if (fs.fat_region[curr_block] == EOF_IDX) {
                        fs.fat_region[curr_block] = block_num = next_free_block();
                        fs.fat_region[block_num] = EOF_IDX;
                    } else {
                        block_num = fs.fat_region[curr_block];
                    }
                }

                curr_block = block_num;
                HANDLE_SYS_CALL(lseek(fs.fd, get_offset_for_block_num(curr_block), SEEK_SET) < 0, "Error during lseek");

                // Clear out block and go back to write.
                HANDLE_SYS_CALL(write(fs.fd, empty_block, fs.block_size) < 0, "Error during write");
                HANDLE_SYS_CALL(lseek(fs.fd, get_offset_for_block_num(curr_block), SEEK_SET) < 0, "Error during lseek");
            }

            HANDLE_SYS_CALL(write(fs.fd, curr->val, sizeof(directory_entry)) < 0, "Error during write");
            curr = curr->next;
            count++;
            init_write = false;
        }
    }
}

bool mount(char *fs_name) {
    if (fs.is_mounted) {
        umount();
    }

    fs.fd = open(fs_name, O_RDWR, FILE_OPEN_MODE);
    HANDLE_INVALID_INPUT(fs.fd < 0, "Error opening fs to mount. File probably doesn't exist.\n");

    fs.fs_name = (char *) malloc(strlen(fs_name) * sizeof(char) + 1);
    if (fs.fs_name == NULL) {
        perror("Error mallocing fs_name in mount");
        exit(EXIT_FAILURE);
    }

    strcpy(fs.fs_name, fs_name);

    uint16_t fat0;
    HANDLE_SYS_CALL(read(fs.fd, &fat0, sizeof(uint16_t)) < 0, "Error reading fat[0]");

    int block_size_config = fat0 & 0xFF;
    int blocks_in_fat = (fat0 & 0xFF00) >> 8;

    fs.block_size = get_block_size_from_config(block_size_config);
    fs.fat_size = fs.block_size * blocks_in_fat;
    fs.num_fat_entries = fs.fat_size / 2;
    fs.data_region_size = fs.block_size * (fs.num_fat_entries - 1);

    // Edge case, because you can't have a link to 0xFFFF (EOF indicator).
    if (fs.num_fat_entries == 0x10000) {
        fs.data_region_size -= fs.block_size;
    }

    fs.fat_region = mmap(NULL, fs.fat_size, PROT_READ | PROT_WRITE, MAP_SHARED, fs.fd, 0);

    if (fs.fat_region == MAP_FAILED) {
        perror("Error calling mmap on the FAT region");
        exit(EXIT_FAILURE);
    }

    read_directory_entries();
    fs.is_mounted = true;

    return true;
}

void umount() {
    if (!fs.is_mounted) {
        return;
    }

    // Just in case.
    write_dell();

    fs.is_mounted = false;

    free(fs.fs_name);
    fs.fs_name = NULL;

    if (munmap(fs.fat_region, fs.fat_size) != 0) {
        perror("Error calling munmap on the FAT region");
        exit(EXIT_FAILURE);
    }

    HANDLE_SYS_CALL(close(fs.fd) < 0, "Error closing fs to unmount");
    fs.fd = -1;

    fs.fat_region = NULL;
    clear(&fs.dir, free_directory_entry);
}

directory_entry *touch(char *file) {
    linked_list_elem *f = get_elem(&fs.dir, ll_find_file_by_name_predicate, file);

    if (f == NULL) {
        f = get_elem(&fs.dir, ll_find_removed_file, NULL);
    }

    directory_entry *d;

    if (f == NULL) {
        d = create_directory_entry();
        strcpy(d->name, file);
        push_back(&fs.dir, d);
    } else {
        d = f->val;
        strcpy(d->name, file);
        d->mtime = time(NULL);
    }

    write_dell();
    return d;
}

void ls() {
    // The columns above are first block number, permissions, size, month, day, time, and name.
    char months[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

    for (linked_list_elem *l = fs.dir.head; l != NULL; l = l->next) {
        directory_entry *de = l->val;

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

        fprintf(stderr, "%5d %c%c%c %5d %s %-2d %02d:%02d %s\n", block_num, x, r, w, size, month, day, hour, minute,
                name);
    }
}

void mv(char *src, char *dst) {
    linked_list_elem *src_elem = get_elem(&fs.dir, ll_find_file_by_name_predicate, src);
    HANDLE_INVALID_INPUT_VOID_FMT(src_elem == NULL, "mv: Source File %s does not exist.\n", src);

    linked_list_elem *dst_elem = get_elem(&fs.dir, ll_find_file_by_name_predicate, dst);
    if (dst_elem != NULL) {
        directory_entry *dst_de = dst_elem->val;
        rm(dst_de->name);
    }

    directory_entry *src_de = src_elem->val;
    src_de->mtime = time(NULL);
    strcpy(src_de->name, dst);
    write_dell();
}

void rm(char *file) {
    linked_list_elem *elem = get_elem(&fs.dir, ll_find_file_by_name_predicate, file);

    HANDLE_INVALID_INPUT_VOID_FMT(elem == NULL, "rm: File %s does not exist.\n", file);

    directory_entry *de = (directory_entry *) elem->val;

    HANDLE_INVALID_INPUT_VOID_FMT(de->name[0] < FILE_EXISTS, "rm: File %s has already been deleted.\n", file);

    de->name[0] = (char) DELETED;
    de->size = 0;

    int next_block = de->firstBlock;

    while (next_block != EOF_IDX) {
        int temp_block = fs.fat_region[next_block];
        fs.fat_region[next_block] = 0;
        next_block = temp_block;
    }

    de->firstBlock = EOF_IDX;
    write_dell();
}

void cat(char **c, int num_args) {
    // cat command options:
    // cat [FILE]
    // cat [-wa] [FILE]
    // cat [FILE] [-wa] [FILE]

    bool overwrite = strcmp(c[num_args - 2], "-w") == 0;
    bool append = strcmp(c[num_args - 2], "-a") == 0;

    if (!(overwrite || append)) {
        // cat [FILE ...]
        // Output file contents for each file.
        for (int i = 1; i < num_args; i++) {
            // New line between files.
            if (i != 1) {
                HANDLE_SYS_CALL(write(STDOUT_FILENO, "\n", 1) < 0, "Error writing block.");
            }

            // Traverse through the dell to find the directory entry with the given name.
            linked_list_elem *f = get_elem(&fs.dir, ll_find_file_by_name_predicate, c[i]);

            // If the file doesn't exist, then make it print nothing.
            if (f == NULL) {
                fprintf(stderr, "cat: %s: No such file or directory\n", c[i]);
                continue;
            }

            directory_entry *de = f->val;

            if ((de->perm & READ_ONLY) == 0) {
                fprintf(stderr, "cat: %s: Permission denied\n", c[i]);
                continue;
            }

            int curr_block = de->firstBlock;
            char buf[fs.block_size];
            int bytes_left = de->size;

            // Now we print out the contents of the file. We read in block sized chunks.
            while (curr_block != EOF_IDX && bytes_left > 0) {
                memset(buf, 0, sizeof(buf));

                int bytes_to_read = MIN(fs.block_size, bytes_left);

                HANDLE_SYS_CALL(lseek(fs.fd, get_offset_for_block_num(curr_block), SEEK_SET) < 0,
                                "Error lseeking to read directory.");
                int bytes_read = read(fs.fd, buf, bytes_to_read);
                HANDLE_SYS_CALL(bytes_read < 0, "Error reading block.");

                HANDLE_SYS_CALL(write(STDOUT_FILENO, buf, bytes_read) < 0, "Error writing block.");

                // Update the block index to the next block index.
                curr_block = fs.fat_region[curr_block];
                bytes_left -= bytes_to_read;
            }
        }
    } else if (num_args == 3) {
        // cat -w OUTPUT_FILE or
        // cat -a OUTPUT_FILE

        touch(c[2]);
        linked_list_elem *f = get_elem(&fs.dir, ll_find_file_by_name_predicate, c[2]);
        HANDLE_INVALID_INPUT_VOID_FMT(f == NULL, "cat: %s: No such file or directory", c[2]);

        directory_entry *de = f->val;

        // If overwrite, then just clear the file, and then append.
        if (overwrite) {
            int block = de->firstBlock;

            while (block != EOF_IDX) {
                int next_block = fs.fat_region[block];
                fs.fat_region[block] = 0;
                block = next_block;
            }

            de->firstBlock = EOF_IDX;
            de->size = 0;
        }

        int bytes_read;

        bool first_write = de->size == 0;

        // Find last block used in file.
        int dst_block = de->firstBlock;
        while (dst_block != EOF_IDX) {
            if (fs.fat_region[dst_block] == EOF_IDX) {
                break;
            }

            dst_block = fs.fat_region[dst_block];
        }

        while (true) {
            uint8_t buf[MAX_LINE_LENGTH] = {'\0'};
            bytes_read = read(STDIN_FILENO, buf, MAX_LINE_LENGTH);
            HANDLE_SYS_CALL(bytes_read < 0, "cat: read error from stdin\n");

            // Done.
            if (bytes_read == 0) {
                break;
            }

            int buf_offset = 0;

            while (bytes_read > 0) {
                // Need a new block.
                if (de->size % fs.block_size == 0) {
                    int next_block = next_free_block();

                    if (first_write) {
                        de->firstBlock = next_block;
                        first_write = false;
                    } else {
                        fs.fat_region[dst_block] = next_block;
                    }

                    dst_block = next_block;
                    fs.fat_region[dst_block] = EOF_IDX;
                }

                int offset_within_block = de->size % fs.block_size;
                int block_remainder = fs.block_size - offset_within_block;
                int bytes_to_write_in_block = block_remainder < bytes_read ? block_remainder : bytes_read;

                HANDLE_SYS_CALL(lseek(fs.fd, get_offset_for_block_num(dst_block) + offset_within_block, SEEK_SET) < 0,
                                "cat: Error lseeking to output file.");
                HANDLE_SYS_CALL(write(fs.fd, buf + buf_offset, sizeof(uint8_t) * bytes_to_write_in_block) < 0,
                                "cat: Error writing to output file.");

                buf_offset += bytes_to_write_in_block;
                de->size += bytes_to_write_in_block;
                bytes_read -= bytes_to_write_in_block;
            }
        }
    } else {
        // cat FILE ... -w OUTPUT_FILE or
        // cat FILE ... -a OUTPUT_FILE

        char *dst = c[num_args - 1];
        touch(dst);
        linked_list_elem *f = get_elem(&fs.dir, ll_find_file_by_name_predicate, dst);
        HANDLE_INVALID_INPUT_VOID_FMT(f == NULL, "cat: %s: No such file or directory", dst);

        directory_entry *de = f->val;

        // If overwrite, then just clear the file, and then append.
        if (overwrite) {
            int block = de->firstBlock;

            while (block != EOF_IDX) {
                int next_block = fs.fat_region[block];
                fs.fat_region[block] = 0;
                block = next_block;
            }

            de->firstBlock = EOF_IDX;
            de->size = 0;
        }

        // Find last block used in file.
        int dst_block = de->firstBlock;
        while (dst_block != EOF_IDX) {
            if (fs.fat_region[dst_block] == EOF_IDX) {
                break;
            }

            dst_block = fs.fat_region[dst_block];
        }

        bool first_write = de->size == 0;

        for (int i = 1; i < num_args - 2; i++) {
            linked_list_elem *src = get_elem(&fs.dir, ll_find_file_by_name_predicate, c[i]);
            HANDLE_INVALID_INPUT_VOID_FMT(src == NULL, "cat: %s: No such file or directory", c[i]);

            directory_entry *src_de = src->val;
            int src_block = src_de->firstBlock;

            int total_bytes_remaining = src_de->size;
            uint8_t buf[fs.block_size];

            while (src_block != EOF_IDX) {
                memset(buf, 0, sizeof(uint8_t) * fs.block_size);

                int bytes_read = MIN(total_bytes_remaining, fs.block_size);

                HANDLE_SYS_CALL(lseek(fs.fd, get_offset_for_block_num(src_block), SEEK_SET) < 0,
                                "cat: Error lseeking to src file.");
                HANDLE_SYS_CALL(read(fs.fd, buf, sizeof(uint8_t) * bytes_read) < 0,
                                "cat: Error writing to output file.");

                total_bytes_remaining -= bytes_read;
                int buf_offset = 0;

                while (bytes_read > 0) {
                    // Need a new block.
                    if (de->size % fs.block_size == 0) {
                        int next_block = next_free_block();

                        if (first_write) {
                            de->firstBlock = next_block;
                            first_write = false;
                        } else {
                            fs.fat_region[dst_block] = next_block;
                        }

                        dst_block = next_block;
                        fs.fat_region[dst_block] = EOF_IDX;
                    }

                    int offset_within_block = de->size % fs.block_size;
                    int block_remainder = fs.block_size - offset_within_block;
                    int bytes_to_write_in_block = block_remainder < bytes_read ? block_remainder : bytes_read;

                    HANDLE_SYS_CALL(
                            lseek(fs.fd, get_offset_for_block_num(dst_block) + offset_within_block, SEEK_SET) < 0,
                            "cat: Error lseeking to output file.");
                    HANDLE_SYS_CALL(write(fs.fd, buf + buf_offset, sizeof(uint8_t) * bytes_to_write_in_block) < 0,
                                    "cat: Error writing to output file.");

                    buf_offset += bytes_to_write_in_block;
                    de->size += bytes_to_write_in_block;
                    bytes_read -= bytes_to_write_in_block;
                }

                src_block = fs.fat_region[src_block];
            }
        }
    }

    write_dell();
}

void chmod(char *op, char *file) {
    HANDLE_INVALID_INPUT_VOID(
            strlen(op) != 2 || (op[0] != '+' && op[0] != '-') || (op[1] != 'r' && op[1] != 'w' && op[1] != 'x'),
            "Usage: chmod [+/-][r/w/x] file1 ...\n");

    int perm = op[1] == 'r' ? READ_ONLY : (op[1] == 'w' ? WRITE_ONLY : EXEC_ONLY);

    linked_list_elem *file_elem = get_elem(&fs.dir, ll_find_file_by_name_predicate, file);

    HANDLE_INVALID_INPUT_VOID_FMT(file_elem == NULL, "chmod: file %s does not exist.\n", file);

    directory_entry *de = (directory_entry *) file_elem->val;

    if (op[0] == '+') {
        de->perm |= perm;
    } else {
        de->perm &= ~perm;
    }

    write_dell();
}

void cpFATtoFAT(char *src, char *dst) {
    linked_list_elem *src_elem = get_elem(&fs.dir, ll_find_file_by_name_predicate, src);
    HANDLE_INVALID_INPUT_VOID_FMT(src_elem == NULL, "cp: Source File %s does not exist.\n", src);

    directory_entry *src_de = src_elem->val;
    HANDLE_INVALID_INPUT_VOID_FMT((src_de->perm & READ_ONLY) == 0, "cp: %s: Permission denied\n", src);

    // Create file if not there.
    touch(dst);

    linked_list_elem *dst_elem = get_elem(&fs.dir, ll_find_file_by_name_predicate, dst);
    HANDLE_INVALID_INPUT_VOID_FMT(dst_elem == NULL, "cp: Destination File %s does not exist.\n", dst);

    directory_entry *dst_de = dst_elem->val;
    HANDLE_INVALID_INPUT_VOID_FMT((dst_de->perm & WRITE_ONLY) == 0, "cp: %s: Permission denied\n", dst);

    dst_de->size = src_de->size;
    dst_de->mtime = time(NULL);

    // Remove all dst blocks.
    int dst_block = dst_de->firstBlock;
    while (dst_block != EOF_IDX) {
        int next = fs.fat_region[dst_block];
        fs.fat_region[dst_block] = 0;
        dst_block = next;
    }

    // If empty file, set firstBlock = 0xFFFF and return.
    if (dst_de->size == 0) {
        dst_de->firstBlock = EOF_IDX;
        write_dell();
        return;
    }

    // Allocate first block for dst.
    dst_de->firstBlock = next_free_block();
    fs.fat_region[dst_de->firstBlock] = EOF_IDX;

    int current_src_block = src_de->firstBlock;
    int current_dst_block = dst_de->firstBlock;
    int bytes_left = src_de->size;

    uint8_t src_block_data[fs.block_size];

    while (current_src_block != EOF_IDX) {
        // Read from source.
        HANDLE_SYS_CALL(lseek(fs.fd, get_offset_for_block_num(current_src_block), SEEK_SET) < 0,
                        "Error lseeking to src file.");
        HANDLE_SYS_CALL(read(fs.fd, src_block_data, sizeof(uint8_t) * fs.block_size) < 0, "Error reading src file.");

        // Write to destination.
        HANDLE_SYS_CALL(lseek(fs.fd, get_offset_for_block_num(current_dst_block), SEEK_SET) < 0,
                        "Error lseeking to dst file.");
        HANDLE_SYS_CALL(write(fs.fd, src_block_data, sizeof(uint8_t) * fs.block_size) < 0, "Error writing dst file.");

        // Update values.
        bytes_left -= fs.block_size;
        current_src_block = fs.fat_region[current_src_block];

        if (bytes_left > 0) {
            fs.fat_region[current_dst_block] = next_free_block();
            current_dst_block = fs.fat_region[current_dst_block];
            fs.fat_region[current_dst_block] = EOF_IDX;
        }
    }

    write_dell();
}

void cpHosttoFAT(char *src, char *dst) {
    // Open file to read.
    int src_fd = open(src, O_RDWR, FILE_OPEN_MODE);
    HANDLE_INVALID_INPUT_VOID(src_fd < 0, "cp: Error opening src file to copy from. File probably doesn't exist.\n");

    // Create file if not there.
    touch(dst);

    linked_list_elem *dst_elem = get_elem(&fs.dir, ll_find_file_by_name_predicate, dst);
    HANDLE_INVALID_INPUT_VOID_FMT(dst_elem == NULL, "cp: Destination File %s does not exist.\n", dst);

    directory_entry *dst_de = dst_elem->val;
    HANDLE_INVALID_INPUT_VOID_FMT((dst_de->perm & WRITE_ONLY) == 0, "cp: %s: Permission denied\n", dst);

    dst_de->size = 0;
    dst_de->mtime = time(NULL);

    // Remove all dst blocks.
    int dst_block = dst_de->firstBlock;
    while (dst_block != EOF_IDX) {
        int next = fs.fat_region[dst_block];
        fs.fat_region[dst_block] = 0;
        dst_block = next;
    }

    // Allocate first block for dst.
    dst_de->firstBlock = next_free_block();
    fs.fat_region[dst_de->firstBlock] = EOF_IDX;

    int current_dst_block = dst_de->firstBlock;
    bool first_block = true;

    uint8_t src_block_data[fs.block_size];

    while (true) {
        // Read from source.
        int bytes_read = read(src_fd, src_block_data, sizeof(uint8_t) * fs.block_size);
        HANDLE_SYS_CALL(bytes_read < 0, "Error reading src file.");

        // EOF with nothing to write.
        if (bytes_read == 0) {
            break;
        }

        // Write to destination.
        // Allocate block to write to, if it's not the first block.
        if (!first_block) {
            fs.fat_region[current_dst_block] = next_free_block();
            current_dst_block = fs.fat_region[current_dst_block];
            fs.fat_region[current_dst_block] = EOF_IDX;
        }

        HANDLE_SYS_CALL(lseek(fs.fd, get_offset_for_block_num(current_dst_block), SEEK_SET) < 0,
                        "Error lseeking to dst file.");
        HANDLE_SYS_CALL(write(fs.fd, src_block_data, sizeof(uint8_t) * bytes_read) < 0, "Error writing dst file.");

        // Update size.
        dst_de->size += bytes_read;
        first_block = false;
    }

    // If empty file, set free firstBlock, firstBlock = 0xFFFF and return.
    if (dst_de->size == 0) {
        fs.fat_region[dst_de->firstBlock] = 0;
        dst_de->firstBlock = EOF_IDX;
    }

    write_dell();
    close(src_fd);
}

void cpFATtoHost(char *src, char *dst) {
    linked_list_elem *src_elem = get_elem(&fs.dir, ll_find_file_by_name_predicate, src);
    HANDLE_INVALID_INPUT_VOID_FMT(src_elem == NULL, "Source File %s does not exist.\n", src);

    directory_entry *src_de = src_elem->val;
    HANDLE_INVALID_INPUT_VOID_FMT((src_de->perm & READ_ONLY) == 0, "cp: %s: Permission denied\n", src);

    int dst_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, FILE_OPEN_MODE);
    HANDLE_SYS_CALL(dst_fd < 0, "Error opening host file to copy to\n.");

    int current_src_block = src_de->firstBlock;
    uint8_t src_block_data[fs.block_size];
    int bytes_remaining = src_de->size;

    while (current_src_block != EOF_IDX) {
        // Read from source.
        HANDLE_SYS_CALL(lseek(fs.fd, get_offset_for_block_num(current_src_block), SEEK_SET) < 0,
                        "Error lseeking to src file.");
        HANDLE_SYS_CALL(read(fs.fd, src_block_data, sizeof(uint8_t) * fs.block_size) < 0, "Error reading src file.");

        int bytes_to_write = bytes_remaining >= fs.block_size ? fs.block_size : bytes_remaining;

        // Write to destination.
        HANDLE_SYS_CALL(write(dst_fd, src_block_data, sizeof(uint8_t) * bytes_to_write) < 0, "Error writing dst file.");

        // Go to next block.
        current_src_block = fs.fat_region[current_src_block];
        bytes_remaining -= bytes_to_write;
    }

    close(dst_fd);
}

void print_dell() {
    linked_list_elem *curr = fs.dir.head;
    while (curr != NULL) {
        if (curr->val != NULL) {
            print_de(curr->val);
        }
        curr = curr->next;
    }
}

bool is_in_dell(char *fname) {
    return get_elem(&fs.dir, ll_find_file_by_name_predicate, fname) != NULL;
}
