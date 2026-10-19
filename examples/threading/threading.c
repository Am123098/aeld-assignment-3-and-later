#define _GNU_SOURCE 
#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
    struct thread_data* t_data = (struct thread_data*)thread_param;

    t_data->thread_complete_success = false;

    if (t_data->wait_to_obtain_ms > 0) usleep(t_data->wait_to_obtain_ms * 1000);

    if (pthread_mutex_lock(t_data->mutex) != 0) {
        ERROR_LOG("'pthread_mutex_lock()' failed!!");
        return thread_param;
    }

    if (t_data->wait_to_release_ms > 0) usleep(t_data->wait_to_release_ms * 1000);

    if (pthread_mutex_unlock(t_data->mutex) != 0) {
        ERROR_LOG("'pthread_mutex_unlock()' failed!!");
        return thread_param;
    }
    
    t_data->thread_complete_success = true;

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data* t_data = (struct thread_data*)malloc(sizeof(struct thread_data));
    if (t_data == NULL) {
        ERROR_LOG("Failed to allocate memory for 'thread_data' struct!!");
        return false; 
    }

    t_data->mutex = mutex;
    t_data->wait_to_obtain_ms = wait_to_obtain_ms;
    t_data->wait_to_release_ms = wait_to_release_ms;
    t_data->thread_complete_success = false;

    if (pthread_create(thread, NULL, threadfunc, t_data) != 0) {
        ERROR_LOG("Failed to spawn a thread!!");
        free(t_data);
        return false;
    }

    return true;
}

