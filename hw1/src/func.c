#include "../header/func.h"
//Global Variables
JobQueue g_job_queue;
Stats g_stats;

long long g_start_time_ms = 0;
int g_jobs_in_progress = 0;
pthread_mutex_t g_jobs_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  g_jobs_zero_cond = PTHREAD_COND_INITIALIZER;

int g_dispacher_done = 0;
int g_log_enabled = 0;
int g_num_counters = 0;
int g_num_threads = 0;

//To be added - array of pthread_t for worker threads (needed in shutdown_system)

static pthreads_t *g_worker_threads = NULL; //To be malloc'd in init_system

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

// Forward declaration for worker thread function (Zohar will implement later)
static void *worker_thread_main(void *arg);

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
    if(pthreads_cond_init(&g_job_queue.has_jobs, NULL) != 0){ //if cond init failed - report
        report_syscall_error("pthreads_cond_init");
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
    g_dispacher_done = 0;
    
    //TODO: create counter files here (we'll add this in a later step)
    
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
            report_syscall_error("pthreads_create");
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
            pthreads_join(g_worker_threads[i], NULL); //This waits for worker thread i to finish
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
        report_syscall_error("strup"); //if strdup failed - report and free memory
        free(job);
        return -1;
    }        
    job->read_time_ms = read_time_ms;
    job->next = NULL;

    //Update queue:
    if(pthread_mutex_lock(&g_job_queue.mutex) != 0) { //locks the queue mutex so others threads cant access it
        report_syscall_error("pthreads_mutex_lock"); //if locking failed - report and free.
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


// Temporary stub worker function (Person B will replace this)
static void *worker_thread_main(void *arg)
{
    int thread_index = (int)(long)arg;
    (void)thread_index; // silence unused warning for now

    // For now, worker just exits immediately.
    // Zohar will implement: dequeue jobs, log start/end, update stats, etc.
    return NULL;
}


