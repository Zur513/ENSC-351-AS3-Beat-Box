// Reporter module

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include "app/reporter.h"
#include "app/periodTimer.h"
#include "app/timing.h"
#include "app/tempoControl.h"

static pthread_t reporterThread;
static bool isRunning = false;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static bool isThreadRunning(void);
static void printStatsLine(void);

static void *reporterThreadFn(void *pArg)
{
	(void)pArg;
	while (isThreadRunning()) {
		Timing_sleepForMs(MS_PER_SECOND);
		Period_statistics_t stats;
		Period_getStatisticsAndClear(PERIOD_EVENT_AUDIO, &stats);
		printStatsLine();
	}
	return NULL;
}

void Reporter_init(void)
{
	pthread_mutex_lock(&lock);
	if (isRunning) {
		pthread_mutex_unlock(&lock);
		fprintf(stderr, "The reporter thread is already running.\n");
		return;
	}
	isRunning = true;
	pthread_mutex_unlock(&lock);

	int result = pthread_create(&reporterThread, NULL, reporterThreadFn, NULL);
	if (result != 0) {
		fprintf(stderr, "Failed to create reporter thread!\n");
		pthread_mutex_lock(&lock);
		isRunning = false;
		pthread_mutex_unlock(&lock);
	}
}

void Reporter_cleanup(void)
{
	// Signal the reporter thread to stop
	pthread_mutex_lock(&lock);
	if (!isRunning) {
		pthread_mutex_unlock(&lock);
		return; // Thread wasn't running
	}
	isRunning = false;
	pthread_mutex_unlock(&lock);

	pthread_join(reporterThread, NULL);
	printf("Reporter thread cleaned up successfully.\n");
}

static bool isThreadRunning(void)
{
	pthread_mutex_lock(&lock);
	bool shouldRun = isRunning;
	pthread_mutex_unlock(&lock);
	return shouldRun;
}

static void printStatsLine(void)
{
	printf("M%d %dbpm vol:%d Audio[%2.3f, %2.3f] avg %2.3f/%d Accel[%2.3f, %2.3f] avg %2.3f/%d\n",
		   0, 120, 1, 1.1, 1.1, 1.1, 1, 1.1, 1.1, 1.1, 1);
}
