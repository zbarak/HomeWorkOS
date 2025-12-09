#include "../header/func.h"
//Global Variables
JobQueue g_job_queue;
Stats g_stats;

long long g_start_time_ms = 0;
int g_jobs_in_progress = 0;
pthread_mutex_t g_jobs_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  g_jobs_zero_cond = PTHREAD_COND_INITIALIZER;

int g_dispatcher_done = 0;
int g_log_enabled = 0;
int g_num_counters = 0;
int g_num_threads = 0;

//To be added - array of pthread_t for worker threads (needed in shutdown_system)

pthread_t *g_worker_threads = NULL; //To be malloc'd in init_system

//Time Assitances

long long now_ms(void){         // This function returns current time in milliseconds using CLOCK_MONOTONIC.
    struct timespec ts;         //timespec is a special time variable used to store time specific things
    if(clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        report_syscall_error("clock_gettime");                //Just in case of failure
        return 0;
    }
    return(long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000; //convert seconds + nanoseconds to milliseconds.
}

long long since_start_ms(void){     //This function simply returns the delta between the current time, and the start time in milliseconds.
    if(g_start_time_ms == 0){           //Hasnt been initialized yet, so we return 0
        return 0;                   
    }
    long long now = now_ms();
    return now - g_start_time_ms;
}

void msleep_ms(int ms){             //This function pauses the program for 'ms' milliseconds
    if(ms <= 0) return;     //'ms' value must be positive
    usleep((useconds_t)ms * 1000);  //it uses the function 'usleep' which converts 'ms' into a special-
                                    //unsinged integer in time types, and multiplies by 1000 because usleep must get nanoseconds
}

void report_syscall_error(const char *syscall_name){ //This function simply prints the syscall that failed and its error number.
    fprintf(stderr, "hw2: %s failed, errno is %d\n", syscall_name, errno);
}

// ========== Initialization / cleanup ==========


// =========================================================
//  WORKER THREAD MAIN
// =========================================================
static void *worker_thread_main(void *arg)
{
    int thread_index = (int)(long)arg;

    // ---------------------------------------
    // 1. Open thread log file (if enabled)
    // ---------------------------------------
    FILE *logf = NULL;
    char filename[32];

    if (g_log_enabled) {
        sprintf(filename, "thread%02d.txt", thread_index);

        logf = fopen(filename, "w");
        if (!logf) {
            report_syscall_error("fopen");
            // worker continues, but without logging
        }
    }

    // ---------------------------------------
    // 2. Worker main loop
    // ---------------------------------------
    while (1) {

        // ========== (A) DEQUEUE A JOB ==========
        if (pthread_mutex_lock(&g_job_queue.mutex) != 0) {
            report_syscall_error("pthread_mutex_lock");
            return NULL;
        }

        // Wait while queue empty *and* dispatcher not done
        while (g_job_queue.head == NULL && g_dispatcher_done == 0) {
            int rc = pthread_cond_wait(&g_job_queue.has_jobs, &g_job_queue.mutex);
            if (rc != 0) {
                errno = rc;
                report_syscall_error("pthread_cond_wait");
                pthread_mutex_unlock(&g_job_queue.mutex);
                return NULL;
            }
        }

        // If dispatcher done and queue empty -> exit
        if (g_job_queue.head == NULL && g_dispatcher_done == 1) {
            pthread_mutex_unlock(&g_job_queue.mutex);
            break; // exit the worker thread
        }

        // Pop job from queue
        Job *job = g_job_queue.head;
        g_job_queue.head = job->next;
        if (g_job_queue.head == NULL)
            g_job_queue.tail = NULL;

        pthread_mutex_unlock(&g_job_queue.mutex);
        // ========== Job dequeued ==========

        // ---------------------------------------
        // 3. Log job START
        // ---------------------------------------
        long long start_time_ms = since_start_ms();

        if (logf) {
            fprintf(logf,
                    "TIME %lld: START job %s\n",
                    start_time_ms,
                    job->line);
            fflush(logf);
        }

        // ---------------------------------------
        // 4. PARSE the job line into basic commands
        // ---------------------------------------
        // Commands separated by ";"
        char *to_free_line = strdup(job->line);
        char *line_copy = to_free_line;
        if (!line_copy) {
            report_syscall_error("strdup");
            // still finish job (just no commands)
        }

        char *basic_cmds[128];
        int basic_count = 0;

        if (line_copy) {
            char *token = strtok(line_copy, ";");

            while (token && basic_count < 128) {
                // Trim spaces
                while (*token == ' ') token++;

                basic_cmds[basic_count++] = token;
                token = strtok(NULL, ";");
            }
        }

        // ---------------------------------------
        // 5. Check for "repeat x"
        // ---------------------------------------
        int repeat_count = 1;
        int first_cmd = 0;

        if (basic_count > 0) {
            if (strncmp(basic_cmds[0], "worker", 6) == 0) {
                first_cmd = 1; 
            }
        }
        if (first_cmd < basic_count && strncmp(basic_cmds[first_cmd], "repeat", 6) == 0) {

            char *p = basic_cmds[first_cmd] + 6;
            while (*p == ' ') p++;
            repeat_count = atoi(p);

            first_cmd++; // skip the repeat command
        }

        // ---------------------------------------
        // 6. Execute commands repeat_count times
        // ---------------------------------------
        for (int r = 0; r < repeat_count; r++) {
            for (int i = first_cmd; i < basic_count; i++) {

                char *cmd = basic_cmds[i];

                // Trim spaces again (safe)
                while (*cmd == ' ') cmd++;

                // ========== msleep x ==========
                if (strncmp(cmd, "msleep", 6) == 0) {
                    long long ms = atoll(cmd + 6);
                    msleep_ms((int)ms);
                }

                // ========== increment x ==========
                else if (strncmp(cmd, "increment", 9) == 0) {

                    int cid = atoi(cmd + 9);
                    if (cid >= 0 && cid < g_num_counters) {
                        char cfname[32];
                        sprintf(cfname, "count%02d.txt", cid);

                        FILE *cf = fopen(cfname, "r+");
                        if (cf) {
                            long long value = 0;
                            fscanf(cf, "%lld", &value);

                            value++;

                            fseek(cf, 0, SEEK_SET);
                            fprintf(cf, "%lld\n", value);
                            fflush(cf);
                            fclose(cf);
                        }
                        else {
                            report_syscall_error("fopen");
                        }
                    }
                }

                // ========== decrement x ==========
                else if (strncmp(cmd, "decrement", 9) == 0) {

                    int cid = atoi(cmd + 9);
                    if (cid >= 0 && cid < g_num_counters) {
                        char cfname[32];
                        sprintf(cfname, "count%02d.txt", cid);

                        FILE *cf = fopen(cfname, "r+");
                        if (cf) {
                            long long value = 0;
                            fscanf(cf, "%lld", &value);

                            value--;

                            fseek(cf, 0, SEEK_SET);
                            fprintf(cf, "%lld\n", value);
                            fflush(cf);
                            fclose(cf);
                        }
                        else {
                            report_syscall_error("fopen");
                        }
                    }
                }

                // ========== Unknown command ==========
                else {
                    fprintf(stderr, "hw2: invalid worker command in job: %s\n", cmd);
                }
            }
        }

        // Free duplicated line copy
        if (to_free_line) free(to_free_line);

        // ---------------------------------------
        // 7. Log END of job
        // ---------------------------------------
        long long end_time_ms = since_start_ms();

        if (logf) {
            fprintf(logf,
                    "TIME %lld: END job %s\n",
                    end_time_ms,
                    job->line);
            fflush(logf);
        }

        // ---------------------------------------
        // 8. Update turnaround-time stats
        // ---------------------------------------
        long long turnaround = end_time_ms - job->read_time_ms;

        if (pthread_mutex_lock(&g_stats.mutex) == 0) {

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
        }

        // ---------------------------------------
        // 9. Decrease jobs_in_progress
        // ---------------------------------------
        if (pthread_mutex_lock(&g_jobs_mutex) == 0) {

            g_jobs_in_progress--;

            if (g_jobs_in_progress == 0) {
                pthread_cond_signal(&g_jobs_zero_cond);
            }

            pthread_mutex_unlock(&g_jobs_mutex);
        }

        // ---------------------------------------
        // 10. Free job memory
        // ---------------------------------------
        free(job->line);
        free(job);
    }

    // ---------------------------------------
    // 11. Thread exit
    // ---------------------------------------
    if (logf) fclose(logf);
    return NULL;
}

// init_system()
// Initialize global data structures, create counters and worker threads.
// Returns 0 on success, -1 on error.
int init_system(int num_threads, int num_counters, int log_enabled){
    if(num_threads <= 0 || num_threads > MAX_THREADS || num_counters <= 0 || num_counters > MAX_COUNTERS){ //Making sure no value is incorrect
        fprintf(stderr, "hw2: invalid num_threads or num_counters\n");
        return -1;
    }
    //If all good, init global vars
    g_num_threads = num_threads;
    g_num_counters = num_counters;
    g_log_enabled = log_enabled ? 1:0;
    //init global start time
    g_start_time_ms = now_ms();
    //Init Job queue
    g_job_queue.head = NULL;
    g_job_queue.tail = NULL;
    if(pthread_mutex_init(&g_job_queue.mutex, NULL) != 0){ //if mutex init failed - report
        report_syscall_error("pthread_mutex_init");
        return -1;
    }
    if(pthread_cond_init(&g_job_queue.has_jobs, NULL) != 0){ //if cond init failed - report
        report_syscall_error("pthread_cond_init");
        pthread_mutex_destroy(&g_job_queue.mutex);
        return -1;
    }
    //Init stats:
    g_stats.sum_turnaround_ms = 0;
    g_stats.min_turnaround_ms = 0;  // (we'll handle "no jobs" case specially)
    g_stats.max_turnaround_ms = 0;
    g_stats.job_count         = 0;
    if (pthread_mutex_init(&g_stats.mutex, NULL) != 0) { //if mutex init failed - report
        report_syscall_error("pthread_mutex_init");
        // destroy what we already initialized
        pthread_cond_destroy(&g_job_queue.has_jobs);
        pthread_mutex_destroy(&g_job_queue.mutex);
        return -1;
    }
    g_jobs_in_progress = 0; //init counters
    g_dispatcher_done = 0;
    
    //TODO: create counter files here (we'll add this in a later step)
    for (int i = 0; i < g_num_counters; i++) 
    {
        char fname[32];
        sprintf(fname, "count%02d.txt", i);
        FILE *f = fopen(fname, "w");
        if (!f) { report_syscall_error("fopen"); return -1; }
        fprintf(f, "0\n");
        fclose(f);
    }

    //Allocate array for worker thread IDs:
    g_worker_threads = malloc(sizeof(pthread_t) * g_num_threads);
    if(!g_worker_threads) { //if malloc failed - report and destroy
        report_syscall_error("malloc");
        pthread_mutex_destroy(&g_stats.mutex);
        pthread_cond_destroy(&g_job_queue.has_jobs);
        pthread_mutex_destroy(&g_job_queue.mutex);
        return -1;
    }
    for(int i = 0; i < g_num_threads; i++){
        int rc = pthread_create(&g_worker_threads[i], NULL, worker_thread_main, (void *)(long)i);
        /* 
        Short explanation for what the above line does:
        &g_worker_threads[i] — where the created thread ID will be stored

        NULL — default thread attributes (No need to change)

        worker_thread_main — the function the new thread will run, will be updated later per thread (according to what it needs to do)

        (void *)(long)i — pass the thread index i to the thread as a void * argument
        (trick to pass an integer)

        pthread_create returns 0 on success, non-zero error code on failure.
        */
        if(rc != 0) { //if the thread creation failed - report
            errno = rc;
            report_syscall_error("pthread_create");
            return -1;
        }
    }
    return 0;
}
// shutdown_system()
// Joins worker threads, destroys mutexes/conds, frees memory.
void shutdown_system(void){
    // Join workers
    if(g_worker_threads){
        for(int i = 0; i < g_num_threads; i++){
            pthread_join(g_worker_threads[i], NULL); //This waits for worker thread i to finish
                                                      //and NULL means we dont care for its return value (This avoids zombie threads)
        }
        free(g_worker_threads); //frees the threads memory
        g_worker_threads = NULL;//makes sure it no longer accidentally has a value
    }
    // Destroy stats mutex
    pthread_mutex_destroy(&g_stats.mutex);

    // Destroy queue mutex/cond
    pthread_cond_destroy(&g_job_queue.has_jobs);
    pthread_mutex_destroy(&g_job_queue.mutex);

    // Destroy jobs mutex/cond
    pthread_cond_destroy(&g_jobs_zero_cond);
    pthread_mutex_destroy(&g_jobs_mutex);
}

// enqueue_job()
// Create a new Job object from a line and enqueue it.
int enqueue_job(const char *line, long long read_time_ms){
    Job *job = malloc(sizeof(Job)); //allocate memory for Job
    if(!job){   //if malloc failed - report
        report_syscall_error("malloc");
        return -1;
    }
    job->line = strdup(line); //strdup(line) creates a new heap-allocated copy of the string line.
                              //It returns a pointer to that newly allocated string.
    if(!job->line){
        report_syscall_error("strdup"); //if strdup failed - report and free memory
        free(job);
        return -1;
    }        
    job->read_time_ms = read_time_ms;
    job->next = NULL;

    //Update queue:
    if(pthread_mutex_lock(&g_job_queue.mutex) != 0) { //locks the queue mutex so others threads cant access it
        report_syscall_error("pthread_mutex_lock"); //if locking failed - report and free.
        free(job->line);
        free(job);
        return -1;
    }
    //we now add the next job to queue 
    if (g_job_queue.tail == NULL) { //if the queue is empty add this job only
        g_job_queue.head = job;
        g_job_queue.tail = job;
    } 
    else {
        g_job_queue.tail->next = job; //otherwise add it to the tail
        g_job_queue.tail = job;
    }
    pthread_mutex_unlock(&g_job_queue.mutex); //we finished updating the queue, unlock it for other threads use.
    // Update jobs_in_progress
    if (pthread_mutex_lock(&g_jobs_mutex) != 0) {
        report_syscall_error("pthread_mutex_lock");
        // We won't undo enqueue here; homework-level OK.
        return -1;
    }
    g_jobs_in_progress++;
    pthread_mutex_unlock(&g_jobs_mutex);

    // Wake one worker
    pthread_cond_signal(&g_job_queue.has_jobs);

    return 0;
}


// dispatcher_wait_for_all_jobs()
// Block until g_jobs_in_progress becomes 0.
void dispatcher_wait_for_all_jobs(void)
{
    if (pthread_mutex_lock(&g_jobs_mutex) != 0) {
        report_syscall_error("pthread_mutex_lock");
        return;
    }

    while (g_jobs_in_progress > 0) {
        int rc = pthread_cond_wait(&g_jobs_zero_cond, &g_jobs_mutex);
        if (rc != 0) {
            errno = rc;
            report_syscall_error("pthread_cond_wait");
            break;
        }
    }

    pthread_mutex_unlock(&g_jobs_mutex);
}


// write_stats_file()
// Writes stats to the given filename.
// NOTE: workers still need to update g_stats for this to be meaningful.
int write_stats_file(const char *filename)
{
    FILE *f = fopen(filename, "w");
    if (!f) {
        report_syscall_error("fopen");
        return -1;
    }

    long long total_run_ms = now_ms() - g_start_time_ms;

    // Copy stats under lock
    long long sum_ms, min_ms, max_ms, count;
    if (pthread_mutex_lock(&g_stats.mutex) != 0) {
        report_syscall_error("pthread_mutex_lock");
        fclose(f);
        return -1;
    }

    sum_ms  = g_stats.sum_turnaround_ms;
    min_ms  = g_stats.min_turnaround_ms;
    max_ms  = g_stats.max_turnaround_ms;
    count   = g_stats.job_count;

    pthread_mutex_unlock(&g_stats.mutex);

    double avg = 0.0;
    if (count > 0) {
        avg = (double)sum_ms / (double)count;
    }

    fprintf(f, "total running time: %lld milliseconds\n", total_run_ms);
    fprintf(f, "sum of jobs turnaround time: %lld milliseconds\n", sum_ms);
    fprintf(f, "min job turnaround time: %lld milliseconds\n", (count > 0 ? min_ms : 0));
    fprintf(f, "average job turnaround time: %f milliseconds\n", avg);
    fprintf(f, "max job turnaround time: %lld milliseconds\n", (count > 0 ? max_ms : 0));

    fclose(f);
    return 0;
}


