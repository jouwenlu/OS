// Declaration of file_system data types.

#pragma once

#include <stdint.h>

#include "linked_list.h"

#define EOF_IDX 0xFFFF

typedef struct file_system_st {
    // Null-terminated name of the file system. Should be dynamically allocated.
    char *fs_name;

    // File Descriptor referencing the FS file. Must be opened on mounting for reading and writing and closed on unmounting.
    int fd;

    // Pointer to the memory mapped FAT table region of the file system. Should initially be set to NULL.
    uint16_t *fat_region;

    // Number of bytes in the FAT region.
    uint32_t fat_size;

    // Number of entries in the FAT region. This is equal to fat_size / 2;
    // This is equivalent to the number of blocks in the entire fs.
    uint32_t num_fat_entries;

    // Number of bytes in the data region.
    uint32_t data_region_size;

    // Number of bytes in a block.
    int block_size;

    // Represents whether or not it has been mounted or not
    bool is_mounted;

    // Linked list of directory entries.
    linked_list dir;
} file_system;