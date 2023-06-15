// Definition of shell builtin commands.

#pragma once

#include "stdbool.h"

// Exits the shell.
void exit_shell();

// Runs the subroutine as the shell. Returns true if such a command is run
// returns false o.w.
bool exec_subroutine(char *argv[]);

// Spawns a new thread corresponding to the passed in argument.
void exec_command(char *argv[]);