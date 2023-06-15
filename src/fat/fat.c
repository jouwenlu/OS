// Main file for standalone FAT.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fat_util.h"
#include "../lib/parser.h"

#define PENNFAT_PROMPT "pennfat# "

int main(int argc, char **argv) {
    int num_bytes;

    init_unmounted_fs();

    while (1) {
        char cmd[MAX_LINE_LENGTH] = {'\0'};

        write(STDERR_FILENO, PENNFAT_PROMPT, sizeof(PENNFAT_PROMPT));
        num_bytes = read(STDIN_FILENO, cmd, MAX_LINE_LENGTH);

        // ctrl+D with nothing, so exit.
        if (num_bytes == 0) {
            write(STDERR_FILENO, "\n", 1);
            exit(EXIT_SUCCESS);
        } else if (num_bytes == -1) {
            perror("Error while reading input.");
            exit(EXIT_FAILURE);
        }

        if (num_bytes > 0) {
            if (num_bytes == 1 && cmd[num_bytes - 1] == '\n') {
                // If user only presses enter, then continue.
                continue;
            } else if (cmd[num_bytes - 1] == '\n') {
                // If user enters something, then we null terminate it.
                cmd[num_bytes - 1] = '\0';
            } else {
                // If ctrl+d after entering something, just write a new line it.
                write(STDERR_FILENO, "\n", 1);
                continue;
            }
        }

        struct parsed_command *command;
        int err = parse_command(cmd, &command);
        if (err == -1) {
            perror("Parser system call error.");
            exit(EXIT_FAILURE);
        } else if (err > 0) {
            write(STDERR_FILENO, ERROR_PARSING_COMMAND_MSG, sizeof(ERROR_PARSING_COMMAND_MSG));
            continue;
        }

        // If no args, i.e. nothing entered, just continue on.
        if (command->num_commands == 0) {
            continue;
        }

        exec_fat_command(command->commands[0]);
        free(command);
    }

    return 0;
}