// Main file for Penn OS.
#include "pcb_table.h"

#include "scheduler.h"
#include "threads.h"

#include "../fat/fat_util.h"
#include "../lib/log.h"
#include "../lib/macros.h"
#include "../lib/pcb.h"
#include "../shell/shell.h"
#include "../user/file_user_funcs.h"

#define DEFAULT_LOG_NAME "log/scheduler.log"

int main(int argc, char **argv) {
    HANDLE_INVALID_INPUT(argc < 2 || argc > 3, "Usage: ./pennos fatfs [schedLog]\n");

    // Starting up the filesystem.
    if (f_mount(argv[1]) < 0) {
        return 0;
    } 

    if (argc == 3) {
        init_log(argv[2]);
    } else {
        init_log(DEFAULT_LOG_NAME);
    }

    init_threads();

    init_pcb_table();

    init_sleep_queue();

    init_scheduler();

    init_shell();

    start_scheduler_thread();

    // Re-entry point once the shell exits.

    // Free everything
    free_init_contexts();
    f_unmount();
    free_scheduler_resources();
    free_process_table();
    free_bg_queue();
    close_log();

    return 0;
}
