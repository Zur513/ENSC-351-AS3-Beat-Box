// shutdown.c handles shutdown requests by threads (gracefully)

#include "app/shutdown.h"
#include <stdbool.h>
#include <pthread.h>

static bool isShutdownRequested = false;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t requestedCondition = PTHREAD_COND_INITIALIZER;

void Shutdown_request(void)
{
	pthread_mutex_lock(&lock);
	isShutdownRequested = true;
	// Wake every thread waiting in Shutdown_waitForRequest()
	pthread_cond_broadcast(&requestedCondition);
	pthread_mutex_unlock(&lock);
}

void Shutdown_waitForRequest(void)
{
	pthread_mutex_lock(&lock);
	// Loop because pthread_cond_wait() can return without a broadcast
	while (!isShutdownRequested) {
		pthread_cond_wait(&requestedCondition, &lock);
	}
	pthread_mutex_unlock(&lock);
}
