// Reporter module: once per second, moves the sampler's current data into
// its history, counts the light dips in it, prints two lines of statistics
// and samples, and stores the dip count.

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include "app/reporter.h"
#include "app/periodTimer.h"
#include "app/sampler.h"
#include "app/timing.h"
#include "app/ledControl.h"

// Both are offsets below the current average light level (in volts).
// A dip triggers at the first, and can't retrigger until back above the second.
#define DIP_TRIGGER_V 0.10
#define DIP_RESET_V 0.07

#define NUM_SAMPLES_TO_PRINT 10

static pthread_t reporterThread;
static bool isRunning = false;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int dipCount = 0;

static bool isThreadRunning(void);
static int countDips(const double *pSamples, int size, double avg);
static void printStatsLine(int size, double avg, int dips, const Period_statistics_t *pStats);
static void printSamplesLine(const double *pSamples, int size);

static void *reporterThreadFn(void *pArg)
{
	(void)pArg;
	while (isThreadRunning()) {
		// Once a second, swap the sampler's buffers so the second that just
		// ended becomes the history
		Timing_sleepForMs(MS_PER_SECOND);
		Sampler_moveCurrentDataToHistory();

		// Timing stats for the samples taken during that same second
		Period_statistics_t stats;
		Period_getStatisticsAndClear(PERIOD_EVENT_SAMPLE_LIGHT, &stats);

		// Work on a copy, so the sampler isn't blocked while this prints
		int size = 0;
		double *pHistory = Sampler_getHistory(&size);
		double average = Sampler_getAverageReading();

		int count = countDips(pHistory, size, average);
		printStatsLine(size, average, count, &stats);
		printSamplesLine(pHistory, size);
		free(pHistory);

		// Save the count for the UDP "dips" command
		pthread_mutex_lock(&lock);
		dipCount = count;
		pthread_mutex_unlock(&lock);
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

int Reporter_getDipCount(void)
{
	pthread_mutex_lock(&lock);
	int count = dipCount;
	pthread_mutex_unlock(&lock);
	return count;
}

static bool isThreadRunning(void)
{
	pthread_mutex_lock(&lock);
	bool shouldRun = isRunning;
	pthread_mutex_unlock(&lock);
	return shouldRun;
}

static int countDips(const double *pSamples, int size, double avg)
{
	assert(pSamples != NULL || size == 0);
	int dips = 0;
	bool isInDip = false;
	for (int i = 0; i < size; i++) {
		// Hysteresis: a dip starts 0.10 V below the average, and the light has
		// to come back up to 0.07 V below it before another dip can start
		if (!isInDip && pSamples[i] <= avg - DIP_TRIGGER_V) {
			dips++;
			isInDip = true;
		} else if (isInDip && pSamples[i] >= avg - DIP_RESET_V) {
			isInDip = false;
		}
	}
	return dips;
}

// Line 1 contains samples per second, LED frequency (Hz), smoothed average, dips,
// and sample timing jitter
static void printStatsLine(int size, double avg, int dips, const Period_statistics_t *pStats)
{
	assert(pStats != NULL);
	printf("#Smpl/s = %4d   Flash @ %3dHz   avg = %5.3fV   dips = %3d   "
	       "Smpl ms[%6.3f, %6.3f] avg %6.3f/%d\n",
	       size, LedControl_getFrequencyHz(), avg, dips,
	       pStats->minPeriodInMs, pStats->maxPeriodInMs,
	       pStats->avgPeriodInMs, pStats->numSamples);
}

// Line 2 contains up to NUM_SAMPLES_TO_PRINT samples, evenly spaced across
// the history
static void printSamplesLine(const double *pSamples, int size)
{
	assert(pSamples != NULL || size == 0);
	int numToPrint = (size < NUM_SAMPLES_TO_PRINT) ? size : NUM_SAMPLES_TO_PRINT;
	for (int i = 0; i < numToPrint; i++) {
		int index = i * size / numToPrint;
		printf(" %4d:%5.3f", index, pSamples[index]);
	}
	printf("\n");
}
