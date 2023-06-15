// Function definitions for scheduling related user-level functions.

#include "scheduler_user_funcs.h"

#include "../kernel/pcb_table.h"

#include "../lib/log.h"
#include "../lib/pcb.h"
#include "../lib/errno.h"

// sets the priority of the thread pid to priority.
int p_nice(int priority, pid_t pid) {
    if (!(priority == -1 || priority == 0 || priority == 1)) {
        set_errno(INVALIDPRIORITY);
        return -1;
    }

    pcb *job = find_pcb_in_table(pid);

    // NOTINPCBTABLE
    if (job == NULL) {
        set_errno(NOTINPCBTABLE);
        return -1;
    }

    int old_nice = job->priority;
    bool result = remove_job_from_scheduler(job);
    if (!result) {
        return -1;
    }

    job->priority = (pid_t) priority;

    add_job_to_scheduler(job);

    // Logging.
    log_nice_event(job, old_nice);
    return 0;
}

// Sets the calling process to blocked until ticks of the system clock elapse,
// and then sets the thread to running. Importantly, p_sleep should not
// return until the thread resumes running; however, it can be interrupted by a
// S_SIGTERM signal. Like sleep(3) in Linux, the clock keeps ticking even when
// p_sleep is interrupted.
void p_sleep(unsigned int ticks) {
    pcb *calling_job = get_active_job();

    // ACTIVEJOBNULL
    if (calling_job == NULL) {
        set_errno(ACTIVEJOBNULL);
        return;
    }

    calling_job->blocked = true;

    log_event(BLOCKED_EVT, calling_job);
    set_sleep_alarm(ticks, calling_job);

    suspend();
}
