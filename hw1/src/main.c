// ============================================================================
// main.c  — Dispatcher side (reads file, posts jobs, waits for workers)
// ============================================================================

#include "../header/func.h"

int main(int argc, char *argv[])
{
    // -----------------------------
    // 1. Check command line arguments
    // -----------------------------
    if (argc != 5) {
        fprintf(stderr, "hw2: invalid number of arguments\n");
        fprintf(stderr, "Usage: hw2 <cmdfile> <num_threads> <num_counters> <log_enabled>\n");
        return 1;
    }

    char *cmd_filename = argv[1];
    int num_threads    = atoi(argv[2]);
    int num_counters   = atoi(argv[3]);
    int log_enabled    = atoi(argv[4]);

    // Simple sanity checks (range checks are also done in init_system)
    if (num_threads <= 0 || num_threads > MAX_THREADS ||
        num_counters <= 0 || num_counters > MAX_COUNTERS ||
        (log_enabled != 0 && log_enabled != 1)) {
        fprintf(stderr, "hw2: invalid arguments\n");
        return 1;
    }

    // -----------------------------
    // 2. Open command file
    // -----------------------------
    FILE *cmdfile = fopen(cmd_filename, "r");
    if (!cmdfile) {
        report_syscall_error("fopen");
        return 1;
    }

    // -----------------------------
    // 3. Open dispatcher log (if enabled)
    // -----------------------------
    FILE *dispatcher_log = NULL;
    if (log_enabled) {
        dispatcher_log = fopen("dispatcher.txt", "w");
        if (!dispatcher_log) {
            report_syscall_error("fopen");
            fclose(cmdfile);
            return 1;
        }
    }

    // -----------------------------
    // 4. Initialize system (threads, counters, mutexes, etc.)
    // -----------------------------
    if (init_system(num_threads, num_counters, log_enabled) != 0) {
        fprintf(stderr, "hw2: init_system failed\n");
        if (dispatcher_log) fclose(dispatcher_log);
        fclose(cmdfile);
        return 1;
    }

    // -----------------------------
    // 5. Main dispatcher loop: read lines and act
    // -----------------------------
    char line[MAX_LINE];

    while (fgets(line, sizeof(line), cmdfile)) {

        // Remove trailing newline
        line[strcspn(line, "\n")] = '\0';

        // Skip empty lines
        if (line[0] == '\0')
            continue;

        long long read_time = since_start_ms();

        // Log that we read this line
        if (dispatcher_log) {
            fprintf(dispatcher_log,
                    "TIME %lld: read cmd line: %s\n",
                    read_time, line);
            fflush(dispatcher_log);
        }

        // -------------------------------------------------
        // Dispatcher commands: "dispatcher msleep X" / "dispatcher wait"
        // -------------------------------------------------
        if (strncmp(line, "dispatcher", 10) == 0) {

            // Make a copy to safely tokenize by spaces/tabs
            char tmp[MAX_LINE];
            strncpy(tmp, line, MAX_LINE - 1);
            tmp[MAX_LINE - 1] = '\0';

            char *saveptr = NULL;
            char *tok0 = strtok_r(tmp, " \t", &saveptr); // "dispatcher"
            char *tok1 = strtok_r(NULL, " \t", &saveptr); // "msleep" or "wait"
            char *tok2 = strtok_r(NULL, " \t", &saveptr); // X for msleep

            if (!tok1) {
                fprintf(stderr, "hw2: invalid dispatcher command\n");
            }
            else if (strcmp(tok1, "msleep") == 0) {
                if (!tok2) {
                    fprintf(stderr, "hw2: invalid dispatcher msleep command\n");
                } else {
                    int ms = atoi(tok2);
                    msleep_ms(ms);
                }
            }
            else if (strcmp(tok1, "wait") == 0) {
                dispatcher_wait_for_all_jobs();
            }
            else {
                fprintf(stderr, "hw2: invalid dispatcher command\n");
            }
        }

        // -------------------------------------------------
        // Worker job line: starts with "worker"
        // -------------------------------------------------
        else if (strncmp(line, "worker", 6) == 0) {

            if (enqueue_job(line, read_time) != 0) {
                fprintf(stderr, "hw2: enqueue_job failed\n");
            }
        }

        // -------------------------------------------------
        // Anything else is invalid
        // -------------------------------------------------
        else {
            fprintf(stderr, "hw2: invalid command line\n");
        }
    }

    fclose(cmdfile);

    // -----------------------------
    // 6. Wait until all jobs finish
    // -----------------------------
    dispatcher_wait_for_all_jobs();

    // -----------------------------
    // 7. Tell workers no more jobs are coming and wake them
    // -----------------------------
    g_dispatcher_done = 1;
    pthread_cond_broadcast(&g_job_queue.has_jobs);

    // -----------------------------
    // 8. Write stats.txt
    // -----------------------------
    write_stats_file("stats.txt");

    // -----------------------------
    // 9. Cleanup and exit
    // -----------------------------
    shutdown_system();

    if (dispatcher_log)
        fclose(dispatcher_log);

    return 0;
}
