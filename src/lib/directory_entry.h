// Declaration of the directory entry.

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

typedef enum {
    UNKNOWN = 0,
    REGULAR = 1,
    DIRECTORY = 2,
    SYM_LINK = 4
} FileType;

typedef enum {
    NONE = 0,
    EXEC_ONLY = 1,
    WRITE_ONLY = 2,
    READ_ONLY = 4,
    READ_EXEC = 5,
    READ_WRITE = 6,
    READ_WRITE_EXEC = 7
} FilePermission;

typedef enum {
    END_OF_DIRECTORY = 0,
    DELETED = 1,
    DELETED_BUT_IN_USE = 2,
    FILE_EXISTS = 3
} DirectoryEntrySpecialType;

typedef struct directory_entry_st {
    // Null-terminated file name.
    // name[0] = 0 means end of directory, name[0] = 1 means deleted entry and the file is also deleted,
    // name[0] = 2 means delete entry and file is still being used.
    char name[32];

    // Number of bytes in the file.
    uint32_t size;

    // The first block number of the file (undefined if size is zero).
    uint16_t firstBlock;

    // Should be cast from the FileType enum.
    uint8_t type;

    // Should be cast from the FilePermission enum.
    uint8_t perm;

    // Creation/modification time of the file.
    time_t mtime;

    // Extra 16 bytes reserved so struct is 64 bytes.
    char reserved[16];
} directory_entry;

// Dynamically allocates a directory entry.
directory_entry *create_directory_entry();

// Frees entries in a dynamically allocated directory_entry.
void free_directory_entry(void *dir_entry);

// Predicate to match a directory entry in directory entry linked list to an entry where the file was removed.
// ll_val is not used.
bool ll_find_removed_file(void *file_name, void *ll_val);

// Predicate to match a directory entry in directory entry linked list by its file name.
bool ll_find_file_by_name_predicate(void *file_name, void *ll_val);

// Debugging function to print out the parameters of the given directory entry.
void print_de(directory_entry *de);