// Function declarations for file related user-level functions.

#pragma once

#include "../lib/pcb.h"

// Mounts a file system with the specificed name.
int f_mount(char *fs_name);

// Unmounts a file system.
void f_unmount();

// Adds the fd the OFT; returns the FD of said file. Returns `-1` if the same
// file is opened twice otherwise, 1 is returned.
int f_open(const char *fname, int mode);

// Given fd, n: the number of bytes of buf, and buf: the buffer to output the
// read text, return the number of bytes read, or `0` if EOF is reached. Returns
// `-1` upon error.
int f_read(int fd, int n, char *buf);

// Given fd, n: the number of bytes of str, and str: the string to write into
// the fd, increment the f_pos by the number of bytes written, return the number
// of bytes written, and return `-1` upon error.
int f_write(int fd, const char *str, int n);

// Removes the fd from the OFT and returns `1` upon success, otherwise returns
// `-1`.
int f_close(int fd);

// Yeets the file, as long as it is not in the OFT.
int f_unlink(const char *fname);

// Sets `f_pos` for `fd` to be the offset relative to whence.
// Whence can be:
//      F_SEEK_SET: beginning of the file
//      F_SEEK_CUR: the current position of the file pointer
//      F_SEEK_END: the end of file respectively
// Returns 1 upon success, and -1 in the case of an error.
int f_lseek(int fd, int offset, int whence);

// Lists the file filename in the directory.
// Lists all the files in the current directory if filename is NULL.
// This returns 1 if the filename is found, otherwise it returns 0.
int f_ls(char *filename);

// Checks if a file is open in the OFT by the fd, and renames the file to
// new_name. Returns 1 upon success, otherwise returns -1 upon error.
int f_rename(int fd, char *new_name);

// Changes the permission of a file given the fd.
// Returns 1 upon success, otherwise returns -1 upon error.
int f_change_perms(char *fname, char *op, int perm);

// Given a pcb, iterate through the fd and increment the
// reference count for each of the fd's.
void f_update_new_child_fd(pcb *child);

// Given a pcb, iterate through the fd and close each of
// the fd's in the array.
void f_close_child_fd(pcb *child);

// Returns -1 if file does not exist, otherwise returns size of the file.
int f_size(char *file);

// Returns true iff file exists and has the specified permissions.
bool f_has_permissions(char *file, int perm);