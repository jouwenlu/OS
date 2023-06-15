// Declaration of the logger functions.

#pragma once

#include "../lib/pcb.h"
#include "../kernel/scheduler.h"
#include "../lib/linked_list.h"
#include "../shell/shell.h"

typedef enum {
    SCHEDULE_EVT = 0,
    CREATE_EVT = 1,
    SIGNALED_EVT = 2,
    EXITED_EVT = 3,
    ZOMBIE_EVT = 4,
    ORPHAN_EVT = 5,
    WAITED_EVT = 6,
    BLOCKED_EVT = 7,
    UNBLOCKED_EVT = 8,
    STOPPED_EVT = 9,
    CONTINUED_EVT = 10
} LogEvent;

// Open log file and store fd.
void init_log(char *log_name);

// Returns the value of the clock ticks global counter;
unsigned int get_clock_ticks();

// Increments the global clock tick counter.
void increment_clock_ticks();

// Close log file.
void close_log();

// Log an event for a pcb, except nice.
void log_event(LogEvent evt, pcb *proc);

// Log nice.
void log_nice_event(pcb *proc, int old_nice);
