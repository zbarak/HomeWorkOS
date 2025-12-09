#include "../header/func.h"

int main(int argc, char *argv[])
{
    // ============================
    // 1. Validate arguments
    // ============================
    if (argc != 5) {
        fprintf(stderr, "hw2: invalid number of arguments\n");
        fprintf(stderr, "Usage: hw2 <cmdfile> <num_threads> <num_counters> <log_enabled>\n");
        return 1;
    }

    char *cmd_filename = argv[1];
    int num_threads    = atoi(argv[2]);
    int num_counters   = atoi(argv[3]);
    int log_enabled    = atoi(argv[4]);

    // ============================
    // 2. Open command file
    // ============================
    FILE *cmdfile = fopen(cmd_filename, "r");
    if (!cmdfile) {
        report_syscall_error("fopen");
        return 1;
    }

    // ============================
    // 3. If logging, open dispatcher log file
    // ============================
    FILE *dispatcher_log = NULL;
    if (log_enabled) {
        dispatcher_log = fopen("dispatcher.txt", "w");
        if (!dispatcher_log) {
            report_syscall_error("fopen");
            fclose(cmdfile);
            return 1;
        }
    }

    // ============================
    // 4. Initialize system (threads, counters, mutexes)
    // ============================
    if (init_system(num_threads, num_counters, log_enabled) != 0) {
        fprintf(stderr, "hw2: init_system failed\n");
        if (dispatcher_log) fclose(dispatcher_log);
        fclose(cmdfile);
        return 1;
    }

    // ============================
    // 5. Main dispatcher loop
    // ============================
    char line[MAX_LINE];
    while (fgets(line, sizeof(line), cmdfile)) {

        // Remove trailing newline
        line[strcspn(line, "\n")] = '\0';

        // Skip empty lines
        if (strlen(line) == 0)
            continue;

        long long read_time = since_start_ms();

        // Write dispatcher log
        if (dispatcher_log) {
            fprintf(dispatcher_log,"TIME %lld: read cmd line: %s\n",read_time, line);
            fflush(dispatcher_log);
        }

        // Parse dispatcher commands
        if (strncmp(line, "dispatcher", 10) == 0) {

            // Format: "dispatcher msleep X"
            if (strstr(line, "msleep")) {
                int ms = atoi(line + strlen("dispatcher msleep "));
                msleep_ms(ms);
            }

            // Format: "dispatcher wait"
            else if (strstr(line, "wait")) {
                dispatcher_wait_for_all_jobs();
            }

            // Anything else is invalid
            else {
                fprintf(stderr, "hw2: invalid dispatcher command\n");
            }
        }

        // Parse worker job lines
        else if (strncmp(line, "worker", 6) == 0) {
            // Offload to workers
            if (enqueue_job(line, read_time) != 0) {
                fprintf(stderr, "hw2: enqueue_job failed\n");
            }
        }

        else {
            fprintf(stderr, "hw2: invalid command line\n");
        }
    }

    fclose(cmdfile);

    // ============================
    // 6. Signal dispatcher done
    // ============================
    g_dispatcher_done = 1;

    // wake workers so those sleeping on empty queue exit correctly
    pthread_cond_broadcast(&g_job_queue.has_jobs);

    // ============================
    // 7. Wait for remaining jobs
    // ============================
    dispatcher_wait_for_all_jobs();

    // ============================
    // 8. Write stats.txt
    // ============================
    write_stats_file("stats.txt");

    // ============================
    // 9. Cleanup everything
    // ============================
    shutdown_system();

    if (dispatcher_log)
        fclose(dispatcher_log);

    return 0;
}
