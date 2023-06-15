
#include "scheduler.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <ucontext.h>
#include <unistd.h>

#include "pcb_table.h"
#include "threads.h"

#include "../lib/errno.h"
#include "../lib/linked_list.h"
#include "../lib/log.h"
#include "../lib/pcb.h"
#include "../shell/shell.h"

#define THREAD_COUNT 9 + 6 + 4

static pcb *shell_job;
static pcb *active_job;

int counter = 0;
char *random_str = NULL;

static const int centisecond = 10000; // 10 milliseconds

static linked_list high;
static linked_list mid;
static linked_list low;

static linked_list sleep_jobs;

bool after_suspend = false;

// Generates a random ordering of high, mid, and low priority jobs.
static void random_string() {
    if (random_str != NULL) {
        free(random_str);
    }

    random_str = (char *) malloc((THREAD_COUNT + 1) * sizeof(char));

    char *start = "HHHHHHHHHMMMMMMLLLL";
    strcpy(random_str, start);

    if (THREAD_COUNT > 1) {
        for (int i = 0; i < THREAD_COUNT; i++) {
            int j = rand() % THREAD_COUNT;
            const char temp = random_str[j];
            random_str[j] = random_str[i];
            random_str[i] = temp;
        }
    }
}

// Add a new running job to scheduler queues
void add_job_to_scheduler(pcb *job) {
    extract_elem(&high, pid_equal_predicate, &job->pid);
    extract_elem(&mid, pid_equal_predicate, &job->pid);
    extract_elem(&low, pid_equal_predicate, &job->pid);

    if (job->priority == -1) {
        push_back(&high, job);
    } else if (job->priority == 0) {
        push_back(&mid, job);
    } else {
        push_back(&low, job);
    }
}

// When job status changes and is not running, remove it from scheduler queues
bool remove_job_from_scheduler(pcb *proc) {
    linked_list_elem *h = extract_elem(&high, pid_equal_predicate, &(proc->pid));
    linked_list_elem *m = extract_elem(&mid, pid_equal_predicate, &(proc->pid));
    linked_list_elem *l = extract_elem(&low, pid_equal_predicate, &(proc->pid));

    bool success = h != NULL || m != NULL || l != NULL;

    // if (!success) {
    //     set_errno(NOTFOUNDINSCHEDULER);
    // }

    return success;
}

void set_sleep_alarm(unsigned int ticks, pcb *job) {
    job->remaining_clock_ticks = ticks;
    push_back(&sleep_jobs, job);
}

linked_list *get_sleep_queue() {
    if (is_empty(&sleep_jobs)) {
        return NULL;
    } else {
        return &sleep_jobs;
    }
}

pcb *decrement_ticks_in_sleep_queue() {
    linked_list *job_queue = get_sleep_queue();

    if (job_queue == NULL) {
        return NULL;
    }

    pcb *finished;
    bool found_one_finished_job = false;

    // Check if any sleep jobs are force terminated.
    linked_list alrd_terminated_jobs;
    init_linked_list(&alrd_terminated_jobs);

    if (!is_empty(job_queue)) {
        linked_list_elem *curr = job_queue->head;

        while (curr != NULL) {
            pcb *curr_pcb = curr->val;

            // Only update counters if sleep job is running.
            if (curr_pcb->status == RUNNING || curr_pcb->status == STOPPED) {
                curr_pcb->remaining_clock_ticks = curr_pcb->remaining_clock_ticks - 1;

                if (curr_pcb->remaining_clock_ticks <= 0 && !found_one_finished_job && curr_pcb->status == RUNNING) {
                    finished = curr_pcb;
                    found_one_finished_job = true;
                }
            } else if (curr_pcb->status == TERMINATED || curr_pcb->status == ORPHANED) {
                push_back(&alrd_terminated_jobs, &(curr_pcb->pid));
            }

            curr = curr->next;
        }
    }

    // Remove the forced terminated sleep jobs
    for (int i = 0; i < alrd_terminated_jobs.size; i++) {
        pid_t *pid_ptr = (pop_head(&alrd_terminated_jobs));
        extract_elem(job_queue, pid_equal_predicate, pid_ptr);
    }

    // Remove the completed sleep job from sleep queue 
    if (found_one_finished_job) {
        extract_elem(job_queue, pid_equal_predicate, &(finished->pid));
        return finished;
    } else {
        return NULL;
    }
}

// Signal handler for SIGALRM.
static void alarm_handler(int signum) {
    increment_clock_ticks();
    pcb *to_handle = decrement_ticks_in_sleep_queue();

    if (to_handle != NULL) {
        to_handle->blocked = false;
        add_job_to_scheduler(to_handle);
        log_event(UNBLOCKED_EVT, to_handle);
    }

    // Make sure you're not switching to NULL scheduler.
    if (get_scheduler_context() != NULL) {
        if (active_job != NULL) {
            swapcontext(active_job->context, get_scheduler_context());
        } else {
            ucontext_t dummycontext;
            swapcontext(&dummycontext, get_scheduler_context());
        }
    }
}

// Registers the ALRM handler.
static void set_alarm_handler(void) {
    struct sigaction act;
    act.sa_handler = alarm_handler;
    act.sa_flags = SA_RESTART;
    sigfillset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);
}

// Sets the timer to send a SIGALRM on every tick.
static void set_timer(void) {
    struct itimerval it;

    it.it_interval = (struct timeval) {.tv_usec = centisecond * 10};
    it.it_value = it.it_interval;

    setitimer(ITIMER_REAL, &it, NULL);
}

void add_shell_pcb_to_table() {
    shell_job = (pcb *) malloc(sizeof(pcb));
    shell_job->pid = 1;
    shell_job->status = RUNNING;
    shell_job->status_changed = false;
    shell_job->blocked = false;

    shell_job->context = get_shell_context();

    shell_job->parent = NULL;
    shell_job->cmd = (char *) malloc(sizeof("shell"));
    strcpy(shell_job->cmd, "shell");

    shell_job->argv = NULL;

    linked_list *zombie_list = (linked_list *) malloc(sizeof(linked_list));
    init_linked_list(zombie_list);
    shell_job->zombieLL = zombie_list;

    linked_list *child_list = (linked_list *) malloc(sizeof(linked_list));
    init_linked_list(child_list);
    shell_job->childLL = child_list;

    shell_job->priority = -1;

    shell_job->fd[0] = STDIN_FILENO;
    shell_job->fd[1] = STDOUT_FILENO;

    shell_job->is_bg = false;

    for (int i = 2; i < MAX_OPEN_FDS; i++) {
        shell_job->fd[i] = EMPTY_FD;
    }

    add_pcb_to_table(shell_job);

    return;
}

void init_sleep_queue() {
    init_linked_list(&sleep_jobs);
}

void init_shell() {
    add_shell_pcb_to_table();
    active_job = shell_job;

    add_job_to_scheduler(active_job);
}

void init_scheduler(void) {
    init_linked_list(&high);
    init_linked_list(&mid);
    init_linked_list(&low);
    active_job = NULL;

    set_alarm_handler();
    set_timer();

    return;
}

pcb *get_active_job(void) {
    return active_job;
}

void reset_active_job() {
    active_job = NULL;
}

void free_no_op(void *val) {}

void free_scheduler_resources() {
    // Don't free the values, because they'll be freed by PCB Table.
    clear(&high, free_no_op);
    clear(&mid, free_no_op);
    clear(&low, free_no_op);
    clear(&sleep_jobs, free_no_op);
    free(random_str);

    shell_job = NULL;
    active_job = NULL;
    random_str = NULL;
}

// This is used for the idle process.
void suspend(void) {
    sigset_t empty_set;
    sigemptyset(&empty_set);
    sigsuspend(&empty_set);
}

// This schedules the next context
void scheduler() {
    if (active_job != NULL) {
        // If active job is still running, add it back to queue.
        if (active_job->status == RUNNING && active_job->blocked == 0) {
            add_job_to_scheduler(active_job);
        }
    }

    // Reset active_job to NULL.
    active_job = NULL;

    // If all three queues are empty, switch to the idle context.
    if (is_empty(&high) && is_empty(&mid) && is_empty(&low)) {
        set_scheduler_uc_link(get_idle_context());
        setcontext(get_idle_context());
        return;
    }

    // At this point, there is at least one ucontext in the queues.
    bool checked_high = false;
    bool checked_mid = false;
    bool checked_low = false;

    random_string();

    while (!(checked_high && checked_mid && checked_low)) {
        if (counter == THREAD_COUNT) {
            random_string();
            counter = 0;
        }

        char whichQueue = random_str[counter];
        if (whichQueue == 'H') {
            checked_high = true;

            if (!is_empty(&high)) {
                active_job = pop_head(&high);
                break;
            }
        } else if (whichQueue == 'M') {
            checked_mid = true;

            if (!is_empty(&mid)) {
                active_job = pop_head(&mid);
                break;
            }
        } else {
            checked_low = true;

            if (!is_empty(&low)) {
                active_job = pop_head(&low);
                break;
            }
        }

        counter = counter + 1;
    }

    // Logging.
    log_event(SCHEDULE_EVT, active_job);

    // Set context of newly scheduled job
    set_scheduler_uc_link(active_job->context);
    setcontext(active_job->context);
}