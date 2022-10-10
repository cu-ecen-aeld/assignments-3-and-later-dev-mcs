#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{
	if (thread_param) {

		struct thread_data* thread_func_args = (struct thread_data *) thread_param;
		
		if (!thread_func_args->mutex)
			return NULL;

		// Assume failure.
		thread_func_args->thread_complete_success = false;

		// Sleep for the obtain delay.
		struct timeval delay;
		delay.tv_sec = thread_func_args->wait_to_obtain_ms / 1000;
		delay.tv_usec = (thread_func_args->wait_to_obtain_ms % 1000) * 1000;
		select(0, 0, 0, 0, &delay);
	
		if (pthread_mutex_lock(thread_func_args->mutex) == 0) {

			// wait for the release delay.
			delay.tv_sec = thread_func_args->wait_to_release_ms / 1000;
        	delay.tv_usec = (thread_func_args->wait_to_release_ms % 1000) * 1000;
        	select(0, 0, 0, 0, &delay);

			if (pthread_mutex_unlock(thread_func_args->mutex) == 0) {
				thread_func_args->thread_complete_success = true;
			}
		}
	}

    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
	if (!thread || !mutex )
		return false;

	// Assume failure.
	bool status = false;

	struct thread_data* thread_func_args = (struct thread_data*) malloc(sizeof(struct thread_data));

	if (thread_func_args) {

		thread_func_args->mutex= mutex;
		thread_func_args->wait_to_obtain_ms = wait_to_obtain_ms;
		thread_func_args->wait_to_release_ms = wait_to_release_ms;
		thread_func_args->thread_complete_success= false;
	
		if (pthread_create(thread, 0, threadfunc, thread_func_args) == 0) {
			
			status = true;
		}
		else {
			ERROR_LOG("Thread create failed");
		}
	}
	else {
		ERROR_LOG("malloc failed for struct thread_datastruct thread_data");
	}

    return status;
}

