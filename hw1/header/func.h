#ifndef FUNC_H
#define FUNC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#define MAX_LINE      1024     // max line length from cmdfile
#define MAX_COUNTERS  100      // as in the assignment
#define MAX_THREADS   4096     // as in the assignment

/* ---------- Core data structures ---------- */

// One job = one "worker ..." line from the command file
typedef struct Job {
    char *line;               // full original line (malloc'ed copy)
    long long read_time_ms;   // time when dispatcher enqueued it
    struct Job *next;         // linked-list queue
} Job;

// Shared job queue between dispatcher and worker threads
typedef struct JobQueue {
    Job *head;
    Job *tail;
    pthread_mutex_t mutex;    // protects head/tail
    pthread_cond_t has_jobs;  // workers sleep here when queue is empty
} JobQueue;

// Global statistics for jobs
typedef struct Stats {
    long long sum_turnaround_ms;
    long long min_turnaround_ms;
    long long max_turnaround_ms;
    long long job_count;
    pthread_mutex_t mutex;    // protects the stats fields
} Stats;

/* ---------- Global variables ---------- */

extern JobQueue g_job_queue;            // global job queue
extern Stats    g_stats;                // global stats

extern long long g_start_time_ms;       // time when program started

extern int  g_jobs_in_progress;         // jobs enqueued but not finished
extern pthread_mutex_t g_jobs_mutex;    // protects g_jobs_in_progress
extern pthread_cond_t  g_jobs_zero_cond;// dispatcher waits here for 0 jobs

extern int g_dispatcher_done;           // set to 1 when no more jobs will arrive

extern int g_log_enabled;               // 1 = logs on, 0 = logs off
extern int g_num_counters;              // number of counter files
extern int g_num_threads;               // number of worker threads

/* ---------- Time helpers ---------- */

// Return current time in milliseconds (monotonic clock)
long long now_ms(void);

// Return "milliseconds since program start" (now_ms() - g_start_time_ms)
long long since_start_ms(void);

/* ---------- Utility ---------- */
// Sleep for the given number of milliseconds (no busy waiting)
void msleep_ms(int ms);

// Print a syscall error with errno (prefix: "hw2")
void report_syscall_error(const char *syscall_name);

/* ---------- Initialization / cleanup ---------- */

// Initialize globals, job queue, stats, counter files and create worker threads.
// Returns 0 on success, -1 on failure.
int init_system(int num_threads, int num_counters, int log_enabled);

// Shut down system: signal workers to exit, join them, destroy mutexes/conds, etc.
void shutdown_system(void);

/* ---------- Dispatcher-side helpers ---------- */

// Enqueue a new job line into the job queue.
// Copies the line (so caller can reuse/free its buffer).
// Returns 0 on success, -1 on failure (e.g. malloc).
int enqueue_job(const char *line, long long read_time_ms);

// Block until all current jobs (g_jobs_in_progress == 0) are finished.
void dispatcher_wait_for_all_jobs(void);

// Write statistics to stats.txt (or another filename if you want).
// Returns 0 on success, -1 on failure.
int write_stats_file(const char *filename);

#endif // FUNC_H
