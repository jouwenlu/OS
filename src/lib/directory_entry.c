// Definition of the directory entry.

#include "directory_entry.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "macros.h"

#include "../fat/fat_util.h"

directory_entry *create_directory_entry() {
    directory_entry *d = (directory_entry *) malloc(sizeof(directory_entry));
    HANDLE_SYS_CALL(d == NULL, "Error mallocing directory entry");

    d->firstBlock = EOF_IDX;
    d->size = 0;
    d->type = (uint8_t) REGULAR;
    d->perm = (uint8_t) READ_WRITE;
    d->mtime = time(NULL);

    for (int i = 0; i < 32; i++) {
        d->name[i] = 0;
    }

    for (int i = 0; i < 16; i++) {
        d->reserved[i] = 0;
    }

    return d;
}

void free_directory_entry(void *dir_entry) {
    free(dir_entry);
    dir_entry = NULL;
}

bool ll_find_removed_file(void *file_name, void *ll_val) {
    return ((directory_entry *) ll_val)->name[0] == (char) DELETED;
}

bool ll_find_file_by_name_predicate(void *file_name, void *ll_val) {
    return strcmp((char *) file_name, ((directory_entry *) ll_val)->name) == 0;
}

void print_de(directory_entry *de) {
    if (de == NULL) {
        printf("NULL\n");
        return;
    }
    printf("~~~~~~~~~~~~~~~~~~~~~\n");
    printf("DE Name: %s\n", de->name);
    printf("DE size: %d\n", de->size);
    printf("DE firstBlock: %d\n", de->firstBlock);
    printf("DE type: %d\n", de->type);
    printf("DE perm: %d\n", de->perm);
    printf("DE mtime: %ld\n", de->mtime);
    printf("~~~~~~~~~~~~~~~~~~~~~\n");
}