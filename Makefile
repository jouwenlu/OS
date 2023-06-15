CC = clang
# Replace -O1 with -g for a debug version during development
CFLAGS = -Wall -Werror -O1

LIB_DIR = ./src/lib
LIB_SRCS = $(wildcard ./src/lib/*.c)
LIB_OBJS = $(LIB_SRCS:.c=.o)

OUT = ./bin

SHELL_DIR = ./src/shell
SHELL_SRCS = $(wildcard $(SHELL_DIR)/*.c)
SHELL_OBJS = $(SHELL_SRCS:.c=.o)

OS_DIR = ./src/kernel
OS_SRCS = $(wildcard $(OS_DIR)/*.c)
OS_NO_MAIN_SRCS := $(filter-out $(OS_DIR)/os.c, $(OS_SRCS))
OS_NO_MAIN_OBJS = $(OS_NO_MAIN_SRCS:.c=.o)

USER_DIR = ./src/user
USER_SRCS = $(wildcard $(USER_DIR)/*.c)
USER_OBJS = $(USER_SRCS:.c=.o)

FAT_DIR = ./src/fat
FAT_SRCS = $(wildcard $(FAT_DIR)/*.c)
FAT_NO_MAIN_SRCS := $(filter-out $(FAT_DIR)/fat.c, $(FAT_SRCS))
FAT_NO_MAIN_OBJS = $(FAT_NO_MAIN_SRCS:.c=.o)

FAT_OBJS = $(FAT_SRCS:.c=.o)
FAT_EXEC_NAME = pennfat
$(FAT_EXEC_NAME) : $(FAT_OBJS) $(OS_NO_MAIN_OBJS) $(USER_OBJS) $(SHELL_OBJS) $(LIB_OBJS)
	$(CC) -o $(OUT)/$(FAT_EXEC_NAME) $^ parser-$(shell uname -p).o

OS_OBJS = $(OS_SRCS:.c=.o)
OS_EXEC_NAME = pennos
$(OS_EXEC_NAME) : $(OS_OBJS) $(FAT_NO_MAIN_OBJS) $(USER_OBJS) $(SHELL_OBJS) $(LIB_OBJS)
	$(CC) -o $(OUT)/$(OS_EXEC_NAME) $^ parser-$(shell uname -p).o

.PHONY: all clean submit top

all : $(FAT_EXEC_NAME) $(OS_EXEC_NAME)

clean :
	$(RM) $(SHELL_DIR)/*.o
	$(RM) $(FAT_DIR)/*.o
	$(RM) $(LIB_DIR)/*.o
	$(RM) $(OS_DIR)/*.o
	$(RM) $(USER_DIR)/*.o
	$(RM) bin/*

top :
	top -p `pgrep -d "," pennos`

GROUP_NAME = project-2-group-11
submit :
	tar --exclude-vcs -cvaf $(GROUP_NAME).tar.gz ../22fa-$(GROUP_NAME)

.DEFAULT_GOAL := all