// Declaration of functions for creating and managing ucontexts.

#pragma once

#include <ucontext.h>

void init_threads();

void start_scheduler_thread();

// Makes the context given a u_context pointer, calles getcontext(), sets stack, link, etc.
void make_context(ucontext_t *ucp, void (*func)(), char **func_and_arg);

// Frees ucontext.
void free_context(ucontext_t *ucp);

// Returns a pointer to the main OS context.
ucontext_t *get_os_context();

// Returns a pointer to the scheduler context.
ucontext_t *get_scheduler_context();

// Returns a pointer to the idle context.
ucontext_t *get_idle_context();

// Returns a pointer to the idle context.
ucontext_t *get_shell_context();

// Sets the UC link for the scheduler context.
void set_scheduler_uc_link(ucontext_t *ucp);

// Free the initialized contexts.
void free_init_contexts();
