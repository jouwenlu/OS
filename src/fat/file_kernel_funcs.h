// Function declarations for file related kernel-level functions.

#pragma once

#include "../lib/file_system.h"
#include "../lib/linked_list.h"

// Kernel level function for seeking within the FAT.
int k_lseek(int fd, int offset, int whence, file_system *f_fs, linked_list *OFT);

// Kernel level function for reading from the FAT.
int k_read(int fd, int n, char *buf, file_system *f_fs, linked_list *OFT);

// Kernel level function for writing to the FAT.
int k_write(int fd, int n, char *buf, file_system *f_fs, linked_list *OFT);