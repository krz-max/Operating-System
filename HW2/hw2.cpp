#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <chrono>

typedef struct {
    pthread_t thread_id;
    int thread_num;
    int sched_policy;
    int sched_priority;
    pthread_barrier_t* barrier;
    float time_wait;
} thread_info_t;


void Delay30S()
{

}
void* threadFunction(void* arg) {
    thread_info_t* thread_info = (thread_info_t*)arg;

    

    // Set CPU affinity for the thread
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);  // Assuming you want to set affinity to CPU 0
    if (sched_setaffinity(0, sizeof(cpuset), &cpuset) == -1) {
        fprintf(stderr, "Failed to set CPU affinity for thread %d\n", thread_info->thread_num);
        return NULL;
    }

    // Set scheduling policy and priority for real-time threads
    if (thread_info->sched_policy == SCHED_FIFO) {
        struct sched_param param;
        param.sched_priority = thread_info->sched_priority;
        if (sched_setscheduler(0, SCHED_FIFO, &param) == -1) {
            fprintf(stderr, "Failed to set scheduling policy for thread %d\n", thread_info->thread_num);
            return NULL;
        }
    }
    int wait_time = thread_info->time_wait*1000;
    // Wait until all threads are ready
    pthread_barrier_wait(thread_info->barrier);

    // Do the task
    for (int i = 0; i < 3; i++) {
        printf("Thread %d is running\n", thread_info->thread_num);

        // Busy work for time_wait seconds

        auto Start = std::chrono::high_resolution_clock::now();
	    while (1)
	    {
		    auto End = std::chrono::high_resolution_clock::now();
		    std::chrono::duration<double, std::milli> Elapsed = End - Start;
		    if (Elapsed.count() >= wait_time)
			    break;
	    }
    }

    // Exit the function
    return NULL;
}

int main(int argc, char* argv[]) {
    int numThreads = 0;
    thread_info_t* thread_info;

    float timeWait = 0.0f;
    int opt;
    while ((opt = getopt(argc, argv, "n:s:p:t:")) != -1) {
        switch (opt) {
            case 'n':
                numThreads = atoi(optarg);
                thread_info = (thread_info_t*)malloc(numThreads * sizeof(thread_info_t));
                if (thread_info == NULL) {
                    fprintf(stderr, "Failed to allocate memory for thread info.\n");
                    return 1;
                }
                break;
            case 't':
                timeWait = atof(optarg);
                break;
            case 's': {
                char* policyStr = strdup(optarg);
                char* token = strtok(policyStr, ",");
                int i = 0;
                while (token != NULL && i < numThreads) {
                    thread_info[i].sched_policy = (strcmp(token, "NORMAL") == 0) ? SCHED_OTHER : SCHED_FIFO;
                    token = strtok(NULL, ",");
                    i++;
                }
                free(policyStr);
                break;
            }
            case 'p': {
                char* priorityStr = strdup(optarg);
                char* token = strtok(priorityStr, ",");
                int i = 0;
                while (token != NULL && i < numThreads) {
                    thread_info[i].sched_priority = atoi(token);
                    token = strtok(NULL, ",");
                    i++;
                }
                free(priorityStr);
                break;
            }
            default:
                fprintf(stderr, "Usage: %s -n <num_threads> -s <policies> -p <priorities> -t <time_wait>\n", argv[0]);
                if (thread_info != NULL) {
                    free(thread_info);
                }
                return 1;
        }
    }

    if (numThreads == 0 || thread_info == NULL) {
        fprintf(stderr, "Error: Invalid or incomplete command-line arguments.\n");
        if (thread_info != NULL) {
            free(thread_info);
        }
        return 1;
    }

    // Create threads
    pthread_barrier_t barrier;
    if (pthread_barrier_init(&barrier, NULL, numThreads + 1) != 0) {
        fprintf(stderr, "Failed to initialize barrier.\n");
        free(thread_info);
        return 1;
    }

    for (int i = 0; i < numThreads; ++i) {
        thread_info[i].thread_num = i;
        thread_info[i].barrier = &barrier;
        thread_info[i].time_wait = timeWait;
        pthread_create(&thread_info[i].thread_id, NULL, threadFunction, (void*)&thread_info[i]);
    }
    




    // Wait for all threads to finish settings and start at once
    pthread_barrier_wait(&barrier);

    // Wait for all threads to finish
    for (int i = 0; i < numThreads; ++i) {
        pthread_join(thread_info[i].thread_id, NULL);
    }

    // Free allocated memory
    free(thread_info);

    // Destroy barrier
    pthread_barrier_destroy(&barrier);

    return 0;
}