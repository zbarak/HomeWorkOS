// ============================================================================
// func.h  — Shared declarations for dispatcher and worker code
// ============================================================================

#ifndef FUNC_H
#define FUNC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>   // for isspace()

#define MAX_LINE      1024
#define MAX_COUNTERS  100
#define MAX_THREADS   4096

/* --------------------------------------------------------------------------
   Structures
   -------------------------------------------------------------------------- */

// One job = one full "worker ..." line read by the dispatcher
typedef struct Job {
    char *line;               // malloc'ed copy of the line
    long long read_time_ms;   // time dispatcher read/enqueued this job
    struct Job *next;         // linked-list queue pointer
} Job;

// Queue shared between dispatcher and worker threads
typedef struct JobQueue {
    Job *head;
    Job *tail;
    pthread_mutex_t mutex;    // protects access to queue
    pthread_cond_t  has_jobs; // workers sleep here if queue empty
} JobQueue;

// Statistics about job turnaround time
typedef struct Stats {
    long long sum_turnaround_ms;
    long long min_turnaround_ms;
    long long max_turnaround_ms;
    long long job_count;
    pthread_mutex_t mutex;    // protects stats updates
} Stats;

/* --------------------------------------------------------------------------
   Global variables (defined in func.c, only declared here)
   -------------------------------------------------------------------------- */

extern JobQueue g_job_queue;
extern Stats    g_stats;

extern long long g_start_time_ms;

extern int g_jobs_in_progress;
extern pthread_mutex_t g_jobs_mutex;
extern pthread_cond_t  g_jobs_zero_cond;

extern int g_dispatcher_done;

extern int g_log_enabled;
extern int g_num_counters;
extern int g_num_threads;

/* *** IMPORTANT ***  
   We also must declare the counter mutex array here,
   because func.c defines it AND worker_thread_main() uses it.
*/
extern pthread_mutex_t g_counter_mutex[MAX_COUNTERS];

/* --------------------------------------------------------------------------
   Time helpers
   -------------------------------------------------------------------------- */

// Return current time (ms)
long long now_ms(void);

// Return milliseconds since program start
long long since_start_ms(void);

/* --------------------------------------------------------------------------
   Utilities
   -------------------------------------------------------------------------- */

// Sleep for given milliseconds
void msleep_ms(int ms);

// Print syscall error with "errno"
void report_syscall_error(const char *syscall_name);

/* --------------------------------------------------------------------------
   Initialization / shutdown
   -------------------------------------------------------------------------- */

int init_system(int num_threads, int num_counters, int log_enabled);

void shutdown_system(void);

/* --------------------------------------------------------------------------
   Dispatcher-side helpers
   -------------------------------------------------------------------------- */

int enqueue_job(const char *line, long long read_time_ms);

void dispatcher_wait_for_all_jobs(void);

int write_stats_file(const char *filename);

#endif