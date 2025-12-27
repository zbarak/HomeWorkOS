// ============================================================================
// func.c  (Implementation file for dispatcher/worker system)
// Written simply, step-by-step, with clear explanations.
// ============================================================================

#include <ctype.h>      // for isspace()
#include "../header/func.h"

// -------------------------
// Global variables
// -------------------------

JobQueue g_job_queue;
Stats    g_stats;
extern pthread_mutex_t g_counter_mutex[MAX_COUNTERS];


long long g_start_time_ms = 0;

int g_jobs_in_progress = 0;
pthread_mutex_t g_jobs_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  g_jobs_zero_cond = PTHREAD_COND_INITIALIZER;

int g_dispatcher_done = 0;
int g_log_enabled     = 0;
int g_num_counters    = 0;
int g_num_threads     = 0;

pthread_t *g_worker_threads = NULL; // allocated in init_system


// ============================================================================
// TIME HELPERS
// ============================================================================

// Return current time in ms since system boot (monotonic clock)
long long now_ms(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        report_syscall_error("clock_gettime");
        return 0;
    }
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// Return ms since program started
long long since_start_ms(void)
{
    return now_ms() - g_start_time_ms;
}

// Sleep without busy-waiting
void msleep_ms(int ms)
{
    if (ms > 0)
        usleep((useconds_t)ms * 1000);
}

// Print errors in a friendly way
void report_syscall_error(const char *name)
{
    fprintf(stderr, "hw2: %s failed, errno = %d\n", name, errno);
}



// ============================================================================
// EXECUTE A SINGLE BASIC COMMAND (worker side)
// This function handles: msleep, increment, decrement.
// The repeat logic is handled in worker_thread_main.
// ============================================================================

static void execute_single_command(char *cmd)
{
    // Remove spaces before the command
    while (*cmd && isspace((unsigned char)*cmd)) cmd++;

    // ---------------------
    // msleep X
    // ---------------------
    if (strncmp(cmd, "msleep", 6) == 0 &&
        (cmd[6] == '\0' || isspace((unsigned char)cmd[6])))
    {
        char *p = cmd + 6;
        while (*p && isspace((unsigned char)*p)) p++;
        msleep_ms(atoi(p));
        return;
    }

    // ---------------------
    // increment X
    // ---------------------
    if (strncmp(cmd, "increment", 9) == 0 &&
        (cmd[9] == '\0' || isspace((unsigned char)cmd[9])))
    {
        char *p = cmd + 9;
        while (*p && isspace((unsigned char)*p)) p++;
        int cid = atoi(p);

        if (cid >= 0 && cid < g_num_counters) {
            pthread_mutex_lock(&g_counter_mutex[cid]);

            char fname[32];
            sprintf(fname, "count%02d.txt", cid);

            FILE *f = fopen(fname, "r+");
            if (!f) {
                report_syscall_error("fopen");
            } else {
                long long val = 0;
                fscanf(f, "%lld", &val);
                val++;
                fseek(f, 0, SEEK_SET);
                fprintf(f, "%lld\n", val);
                fflush(f);
                fclose(f);
            }

            pthread_mutex_unlock(&g_counter_mutex[cid]);
        }
        return;
    }

    // ---------------------
    // decrement X
    // ---------------------
    if (strncmp(cmd, "decrement", 9) == 0 &&
        (cmd[9] == '\0' || isspace((unsigned char)cmd[9])))
    {
        char *p = cmd + 9;
        while (*p && isspace((unsigned char)*p)) p++;
        int cid = atoi(p);

        if (cid >= 0 && cid < g_num_counters) {
            pthread_mutex_lock(&g_counter_mutex[cid]);

            char fname[32];
            sprintf(fname, "count%02d.txt", cid);

            FILE *f = fopen(fname, "r+");
            if (!f) {
                report_syscall_error("fopen");
            } else {
                long long val = 0;
                fscanf(f, "%lld", &val);
                val--;
                fseek(f, 0, SEEK_SET);
                fprintf(f, "%lld\n", val);
                fflush(f);
                fclose(f);
            }

            pthread_mutex_unlock(&g_counter_mutex[cid]);
        }
        return;
    }

    // repeat is handled at a higher level, so ignore it here

    // Unknown command → print warning
    fprintf(stderr, "hw2: invalid worker command: %s\n", cmd);
}



// ============================================================================
// WORKER THREAD FUNCTION
// ============================================================================

static void *worker_thread_main(void *arg)
{
    int thread_id = (int)(long)arg;

    // Open log file if needed
    FILE *logf = NULL;
    if (g_log_enabled) {
        char fname[32];
        sprintf(fname, "thread%02d.txt", thread_id);
        logf = fopen(fname, "w");
        if (!logf) {
            report_syscall_error("fopen");
        }
    }

    while (1) {

        // -----------------------------
        // DEQUEUE A JOB
        // -----------------------------
        pthread_mutex_lock(&g_job_queue.mutex);

        // Wait if queue is empty and more jobs may come
        while (g_job_queue.head == NULL && g_dispatcher_done == 0) {
            pthread_cond_wait(&g_job_queue.has_jobs, &g_job_queue.mutex);
        }

        // No jobs AND dispatcher is done → exit thread
        if (g_job_queue.head == NULL && g_dispatcher_done == 1) {
            pthread_mutex_unlock(&g_job_queue.mutex);
            break;
        }

        // Remove job from queue
        Job *job = g_job_queue.head;
        g_job_queue.head = job->next;
        if (g_job_queue.head == NULL)
            g_job_queue.tail = NULL;

        pthread_mutex_unlock(&g_job_queue.mutex);


        // -----------------------------
        // Log job START
        // -----------------------------
        long long start = since_start_ms();
        if (logf) {
            fprintf(logf, "TIME %lld: START job %s\n", start, job->line);
            fflush(logf);
        }

        // -----------------------------
        // PARSE JOB LINE
        // -----------------------------
        char *copy = strdup(job->line);
        if (!copy) {
            report_syscall_error("strdup");
            goto FINISH_JOB;
        }

        // Skip the leading word "worker"
        char *p = copy;
        while (*p && isspace((unsigned char)*p)) p++;
        if (strncmp(p, "worker", 6) == 0) p += 6;
        while (*p && isspace((unsigned char)*p)) p++;

        // Split remaining text by ';'
        char *basic[128];
        int n = 0;

        char *tok = strtok(p, ";");
        while (tok && n < 128) {
            while (*tok && isspace((unsigned char)*tok)) tok++;
            basic[n++] = tok;
            tok = strtok(NULL, ";");
        }

        // -----------------------------
        // FIND repeat
        // -----------------------------
        int repeat_index = -1;
        int repeat_times = 1;

        for (int i = 0; i < n; i++) {
            char *c = basic[i];
            while (*c && isspace((unsigned char)*c)) c++;

            if (strncmp(c, "repeat", 6) == 0 &&
                (c[6] == '\0' || isspace((unsigned char)c[6])))
            {
                char *q = c + 6;
                while (*q && isspace((unsigned char)*q)) q++;
                repeat_times = atoi(q);
                repeat_index = i;
                break;
            }
        }

        // -----------------------------
        // EXECUTE COMMANDS
        // -----------------------------
        if (repeat_index == -1) {
            // No repeat → run all once
            for (int i = 0; i < n; i++)
                execute_single_command(basic[i]);
        }
        else {
            // Commands before repeat → once
            for (int i = 0; i < repeat_index; i++)
                execute_single_command(basic[i]);

            // Commands after repeat → repeat_times times
            for (int r = 0; r < repeat_times; r++) {
                for (int i = repeat_index + 1; i < n; i++)
                    execute_single_command(basic[i]);
            }
        }

        free(copy);

FINISH_JOB:

        // -----------------------------
        // Log END
        // -----------------------------
        long long end = since_start_ms();
        if (logf) {
            fprintf(logf, "TIME %lld: END job %s\n", end, job->line);
            fflush(logf);
        }

        // -----------------------------
        // Update statistics
        // -----------------------------
        long long turnaround = end - job->read_time_ms;

        pthread_mutex_lock(&g_stats.mutex);

        g_stats.sum_turnaround_ms += turnaround;

        if (g_stats.job_count == 0) {
            g_stats.min_turnaround_ms = turnaround;
            g_stats.max_turnaround_ms = turnaround;
        } else {
            if (turnaround < g_stats.min_turnaround_ms)
                g_stats.min_turnaround_ms = turnaround;
            if (turnaround > g_stats.max_turnaround_ms)
                g_stats.max_turnaround_ms = turnaround;
        }
        g_stats.job_count++;

        pthread_mutex_unlock(&g_stats.mutex);

        // -----------------------------
        // Mark job finished
        // -----------------------------
        pthread_mutex_lock(&g_jobs_mutex);
        g_jobs_in_progress--;
        if (g_jobs_in_progress == 0)
            pthread_cond_signal(&g_jobs_zero_cond);
        pthread_mutex_unlock(&g_jobs_mutex);

        free(job->line);
        free(job);
    }

    if (logf) fclose(logf);
    return NULL;
}



// ============================================================================
// INITIALIZATION
// ============================================================================

int init_system(int num_threads, int num_counters, int log_enabled)
{
    g_num_threads  = num_threads;
    g_num_counters = num_counters;
    g_log_enabled  = log_enabled ? 1 : 0;

    g_start_time_ms = now_ms();

    g_job_queue.head = NULL;
    g_job_queue.tail = NULL;

    pthread_mutex_init(&g_job_queue.mutex, NULL);
    pthread_cond_init(&g_job_queue.has_jobs, NULL);

    pthread_mutex_init(&g_stats.mutex, NULL);

    // Initialize per-counter mutexes
    for (int i = 0; i < g_num_counters; i++)
        pthread_mutex_init(&g_counter_mutex[i], NULL);

    // Create counter files
    for (int i = 0; i < g_num_counters; i++) {
        char fname[32];
        sprintf(fname, "count%02d.txt", i);
        FILE *f = fopen(fname, "w");
        if (!f) {
            report_syscall_error("fopen");
            return -1;
        }
        fprintf(f, "0\n");
        fclose(f);
    }

    // Create worker threads
    g_worker_threads = malloc(sizeof(pthread_t) * g_num_threads);
    if (!g_worker_threads) {
        report_syscall_error("malloc");
        return -1;
    }

    for (int i = 0; i < g_num_threads; i++) {
        int rc = pthread_create(&g_worker_threads[i], NULL,
                                worker_thread_main, (void *)(long)i);
        if (rc != 0) {
            errno = rc;
            report_syscall_error("pthread_create");
            return -1;
        }
    }

    return 0;
}



// ============================================================================
// ADD A JOB TO THE QUEUE
// ============================================================================

int enqueue_job(const char *line, long long read_time_ms)
{
    Job *job = malloc(sizeof(Job));
    if (!job) {
        report_syscall_error("malloc");
        return -1;
    }

    job->line = strdup(line);
    if (!job->line) {
        free(job);
        report_syscall_error("strdup");
        return -1;
    }

    job->read_time_ms = read_time_ms;
    job->next = NULL;

    // Add to queue
    pthread_mutex_lock(&g_job_queue.mutex);

    if (g_job_queue.tail == NULL) {
        g_job_queue.head = job;
        g_job_queue.tail = job;
    } else {
        g_job_queue.tail->next = job;
        g_job_queue.tail = job;
    }

    pthread_mutex_unlock(&g_job_queue.mutex);

    // Increase pending job count
    pthread_mutex_lock(&g_jobs_mutex);
    g_jobs_in_progress++;
    pthread_mutex_unlock(&g_jobs_mutex);

    // Wake one worker
    pthread_cond_signal(&g_job_queue.has_jobs);

    return 0;
}



// ============================================================================
// WAIT FOR ALL JOBS
// ============================================================================

void dispatcher_wait_for_all_jobs(void)
{
    pthread_mutex_lock(&g_jobs_mutex);

    while (g_jobs_in_progress > 0)
        pthread_cond_wait(&g_jobs_zero_cond, &g_jobs_mutex);

    pthread_mutex_unlock(&g_jobs_mutex);
}



// ============================================================================
// Generate stats.txt
// ============================================================================

int write_stats_file(const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f) {
        report_syscall_error("fopen");
        return -1;
    }

    long long total = now_ms() - g_start_time_ms;

    pthread_mutex_lock(&g_stats.mutex);
    long long sum   = g_stats.sum_turnaround_ms;
    long long min   = g_stats.min_turnaround_ms;
    long long max   = g_stats.max_turnaround_ms;
    long long count = g_stats.job_count;
    pthread_mutex_unlock(&g_stats.mutex);

    double avg = (count > 0) ? (double)sum / (double)count : 0.0;

    fprintf(f, "total running time: %lld milliseconds\n", total);
    fprintf(f, "sum of jobs turnaround time: %lld milliseconds\n", sum);
    fprintf(f, "min job turnaround time: %lld milliseconds\n", (count ? min : 0));
    fprintf(f, "average job turnaround time: %f milliseconds\n", avg);
    fprintf(f, "max job turnaround time: %lld milliseconds\n", (count ? max : 0));

    fclose(f);
    return 0;
}



// ============================================================================
// CLEANUP
// ============================================================================

void shutdown_system(void)
{
    // Wait for threads to finish
    if (g_worker_threads) {
        for (int i = 0; i < g_num_threads; i++)
            pthread_join(g_worker_threads[i], NULL);

        free(g_worker_threads);
        g_worker_threads = NULL;
    }

    // Destroy mutexes/conds
    pthread_mutex_destroy(&g_stats.mutex);
    pthread_mutex_destroy(&g_job_queue.mutex);
    pthread_cond_destroy(&g_job_queue.has_jobs);

    pthread_mutex_destroy(&g_jobs_mutex);
    pthread_cond_destroy(&g_jobs_zero_cond);

    for (int i = 0; i < g_num_counters; i++)
        pthread_mutex_destroy(&g_counter_mutex[i]);
}
