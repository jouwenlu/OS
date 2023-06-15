// Declaration of FAT API for manipulating the file system.

#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "../lib/directory_entry.h"
#include "../lib/file_system.h"
#include "../lib/linked_list.h"
#include "../lib/parser.h"

// Sets the default values for the necessary fields in the global file_system struct
// before it's used or mounted.
void init_unmounted_fs();

// Returns the mounted FS. NULL if it's unmounted. Must be called after init_unmounted_fs.
file_system *get_mounted_fs();

// Executes a FAT command (as specified in the writeup). Returns
// true iff the user command corresponds to a legal usage of a FAT
// command. (i.e. mkfs without all the args will return false).
// Requires a file_system* because some of the commands deal with a file system
// that has been mounted.
// print_err_msg is true if this should print user error messages to the console.
bool exec_fat_command(char *command[]);

// Writes the elements in the directory entry linked list to the filesystem.
void write_dell();

// Return the next free block in the FAT
int next_free_block();

// Returns the offset in the FS for a given block number.
off_t get_offset_for_block_num(uint16_t block_num);

// Returns the block number pointed to by the offset in the fs.
// The offset is the byte within the whole fs.
// Returns EOF_IDX if it's in the FAT region.
int get_block_num_from_offset(int offset);

// Returns the offset within the block of an offset.
// The offset is the byte within the whole fs.
int get_offset_within_block(int offset);

// Writes a file system of the desired size to a file with name fs_name. 
// Pre-Condition: blocks_in_fat in [1, 32], block_size_config in [0, 4].
void mkfs(char *fs_name, int blocks_in_fat, int block_size_config);

// Mounts a file system located at fs_name and populates the fs struct with
// the pertinent information. Returns true if the file system was successfully
// mounted. Would return false if fs_name does not correspond to a readable
// file.
bool mount(char *fs_name);

// Unmounts the file system specified by fs.
void umount();

// Touch creates a new file if none exists, otherwise updates the mmtime of the file.
// Also returns a pointer to the directory_entry that was created/modified.
directory_entry *touch(char *file);

// List all files in the directory.
void ls();

// Renames the src file in dst file.
void mv(char *src, char *dst);

// Removes the file from the file system.
void rm(char *file);

// Our own implemented `cat` command. Takes in the File System and the string array of commands.
void cat(char **c, int num_args);

// Changes the permissions on file.
void chmod(char *op, char *file);

// Copies a file src to a file dst where both live in the specified file system.
void cpFATtoFAT(char *src, char *dst);

// Copies a file src to a FAT file dst where src is on the host machine.
void cpHosttoFAT(char *src, char *dst);

// Copies a FAT file src to a file dst where dst is on the host machine.
void cpFATtoHost(char *src, char *dst);

// Debugging command to print out all DE's in the dell.
void print_dell();

// Returns whether the file with the given filename is in the dell.
bool is_in_dell(char *fname);
