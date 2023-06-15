# Penn OS

*Note: This is a copy of the original repository we worked on, which has a different repository owner. Thus, there are no additional branches and commits.*

Written by Jio Jeong (`jiojeong`), Reifon Chiu (`rechiu`), Rowena Lu (`jouwenlu`), and Sumukh Govindaraju (`sumig`).

### Usage Instructions

To build, run:

```
$ make
```

This builds all the executables to [`bin/`](bin/). You can run with `./bin/pennfat` or `./bin/pennos FS [log]`

To run an executable with valgrind, run:

```
valgrind --leak-check=full --track-origins=yes --verbose ./executable
```

### Source Files
```
bin/
	pennfat
	pennos 
log/
	scheduler.log
doc/
	CompanionDoc.pdf
src/
	fat/
		fat_util.c
		fat_util.h
		fat.c
		file_kernel_funcs.c
		file_kernel_funcs.h
	kernel/
		os.c
		pcb_table.c
		pcb_table.h
		process_kernel_funcs.c
		process_kernel_funcs.h
		scheduler.c
		scheduler.h
		threads.c
		threads.h
	lib/
		directory_entry.c
		directory_entry.h
		errno.c
		errno.h
		fd.c
		fd.h
		file_system.h
		linked_list.c
		linked_list.h
		log.c
		log.h
		macros.h
		parser.h
		pcb.c
		pcb.h
		signals.h
		status.c
		status.h
	shell/
		commands.c
		commands.h
		job.c
		job.h
		redirects.c
		redirects.h
		shell.c
		shell.h
	user/
		file_user_funcs.c
		file_user_funcs.h
		process_user_funcs.c
		process_user_funcs.h
		scheduler_user_funcs.c
		scheduler_user_funcs.h
		stress.c
		stress.h
.gitignore
Makefile
parser-aarch64.o
parser-x86_64.o
README.md
```

### Extra Credit
- Memory-leak free

### Overview of Work Done
*Standalone FAT:*
Created a user interface that is able to create a file system of configurable size and manipulate files within it and the host OS.

*File Interface:*
Implemented a sequence of functions to allow users to access the FAT and read/write/seek within various files in the FAT. This handles opening and closing file descriptors for individual processes, as well as redirects.

*Scheduler:*
Created a kernel that is able to spawn process threads with different priority levels, and schedule them to run in a round-robin fashion. It also handles signals for the various processes and interactions between parent and child processes.

*Shell:*
Integrated a shell to allow the user to run processes in PennOS. The shell conducts job handling and we implemented a variety of user programs that can be found by running `man` in the shell.

### Code Layout
The source code is divided into several directories.

The `fat/` folder contains code for the Standalone FAT and FAT utility functions/file kernel-level functions.

The `kernel/` folder contains all of the process management code including the scheduler, OS main function, and process kernel-level functions.

The `lib/` folder contains all of the useful data structures and functions that are utilized across the various parts including useful macros, a generic linked list type, etc.

The `shell/` folder contains all of the code related to running the shell, managing background processes, and running shell user programs/built-ins.

The `user/` folder contains all of the user-level functions for process/file management.
