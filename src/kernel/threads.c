// Definitions of functions for creating and managing ucontexts.
// Contains special ucontext pointers.

#include "threads.h"

#include <stddef.h>
#include <signal.h>
#include <valgrind/valgrind.h>

#include "scheduler.h"
#include "../shell/shell.h"

// Main context for OS.
static ucontext_t os_context;

// Context containing the scheduler function.
static ucontext_t *scheduler_context;

// Context for when all 3 priority queues are empty.
static ucontext_t *idle_context;

// Shell when all 3 priority queues are empty.
static ucontext_t *shell_context;

void set_stack(stack_t *stack) {
    int stacksize = 131072;
    void *sp = malloc(stacksize);
    VALGRIND_STACK_REGISTER(sp, sp + stacksize);

    *stack = (stack_t) {.ss_sp = sp, .ss_size = stacksize};
}

void configure_scheduler_context() {
    getcontext(scheduler_context);
    sigemptyset(&scheduler_context->uc_sigmask);

    // Block the alarm signal in the scheduler.
    sigaddset(&scheduler_context->uc_sigmask, SIGALRM);

    set_stack(&scheduler_context->uc_stack);
    scheduler_context->uc_link = NULL;
    makecontext(scheduler_context, scheduler, 0);
}

void init_threads() {
    scheduler_context = (ucontext_t *) malloc(sizeof(ucontext_t));
    configure_scheduler_context();

    idle_context = (ucontext_t *) malloc(sizeof(ucontext_t));
    make_context(idle_context, suspend, NULL);

    shell_context = (ucontext_t *) malloc(sizeof(ucontext_t));
    make_context(shell_context, main_shell, NULL);
}

void start_scheduler_thread() {
    swapcontext(&os_context, scheduler_context);
}

void make_context(ucontext_t *ucp, void (*func)(), char **func_and_arg) {
    getcontext(ucp);
    sigemptyset(&ucp->uc_sigmask);

    set_stack(&ucp->uc_stack);
    ucp->uc_link = scheduler_context;

    if (func_and_arg == NULL) {
        makecontext(ucp, func, 0);
    } else {
        makecontext(ucp, func, 1, func_and_arg);
    }
}

void free_context(ucontext_t *ucp) {
    free(ucp->uc_stack.ss_sp);
    free(ucp);
}

ucontext_t *get_os_context() {
    return &os_context;
}

ucontext_t *get_scheduler_context() {
    return scheduler_context;
}

ucontext_t *get_idle_context() {
    return idle_context;
}

ucontext_t *get_shell_context() {
    return shell_context;
}

void set_scheduler_uc_link(ucontext_t *ucp) {
    scheduler_context->uc_link = ucp;
}

void free_init_contexts() {
    free_context(scheduler_context);
    free_context(idle_context);

    scheduler_context = NULL;
    idle_context = NULL;
    shell_context = NULL;
}