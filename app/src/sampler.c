// sampler.c
// Module to sample light levels in the background (uses a thread).
//
// It continuously samples the light level, and stores it internally.
// It provides access to the samples it recorded during the _previous_
// complete second.
//
// The application will do a number of actions each second which must
// be synchronized (such as computing dips and printing to the screen).
// To make easy to work with the data, the app must call
// Sampler_moveCurrentDataToHistory() each second to trigger this
// module to move the current samples into the history.

#include "app/sampler.h"
#include "app/periodTimer.h"
#include "app/timing.h"
#include "hal/adc.h"
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_PERIOD_MS 1
#define LIGHT_SENSOR_CHANNEL 2
// 1ms of sleep + SPI time gives 500-900 samples per second, so this leaves headroom
#define MAX_SAMPLES_PER_SECOND 2000
#define PREV_SAMPLE_WEIGHT 0.999
#define NEW_SAMPLE_WEIGHT 0.001

#define SAMPLER_RT_PRIORITY 50

static double smoothedAvg;
static bool hasFirstSample = false;
// Two buffers that swap roles each second: the thread fills pCurrent while
// the rest of the program reads pHistory
static double bufferA[MAX_SAMPLES_PER_SECOND];
static double bufferB[MAX_SAMPLES_PER_SECOND];
static double *pCurrent = bufferA;
static double *pHistory = bufferB;
static int currentSize = 0;
static int historySize = 0;

static long long numSamplesTaken = 0;

static pthread_t samplerThread;
static bool isRunning = false;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static bool isThreadRunning(void);
static void *samplerThreadFn(void *pArg);

static void *samplerThreadFn(void *pArg)
{
	(void)pArg;
	// Real-time priority stops the other threads from delaying samples (needs root)
	struct sched_param param = { .sched_priority = SAMPLER_RT_PRIORITY };
	if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &param) != 0) {
		fprintf(stderr, "Sampler: no real-time priority (maybe you aren't root), timing will be looser\n");
	}
	while (isThreadRunning()) {
		double voltage = Adc_readVoltage(LIGHT_SENSOR_CHANNEL);

		pthread_mutex_lock(&lock);
		// Drop samples past the end of the buffer instead of overflowing it
		// (this shouldn't happen at about 1000 samples per second)
		if (currentSize < MAX_SAMPLES_PER_SECOND) {
			pCurrent[currentSize++] = voltage;
		}
		// Exponential smoothing, with the first sample as the starting average
		smoothedAvg = hasFirstSample
		    ? (PREV_SAMPLE_WEIGHT * smoothedAvg) + (NEW_SAMPLE_WEIGHT * voltage)
		    : voltage;
		numSamplesTaken++;
		pthread_mutex_unlock(&lock);

		hasFirstSample = true;
		Period_markEvent(PERIOD_EVENT_SAMPLE_LIGHT);
		Timing_sleepForMs(SAMPLE_PERIOD_MS);
	}
	return NULL;
}

void Sampler_init(void)
{
	pthread_mutex_lock(&lock);
	if (isRunning) {
		pthread_mutex_unlock(&lock);
		fprintf(stderr, "The sampler thread is already running.\n");
		return;
	}
	isRunning = true;
	pthread_mutex_unlock(&lock);

	Adc_init();
	int result = pthread_create(&samplerThread, NULL, samplerThreadFn, NULL);
	if (result != 0) {
		fprintf(stderr, "Failed to create sampler thread!\n");
		pthread_mutex_lock(&lock);
		isRunning = false;
		pthread_mutex_unlock(&lock);
		Adc_cleanup();
	}
}

void Sampler_cleanup(void)
{
	// Signal the sampler thread to stop
	pthread_mutex_lock(&lock);
	if (!isRunning) {
		pthread_mutex_unlock(&lock);
		return; // Thread wasn't running
	}
	isRunning = false;
	pthread_mutex_unlock(&lock);

	pthread_join(samplerThread, NULL);
	Adc_cleanup();
	printf("Sample thread cleaned up successfully.\n");
}

void Sampler_moveCurrentDataToHistory(void)
{
	pthread_mutex_lock(&lock);
	// Swap the pointers instead of copying the samples
	double *pTemp = pCurrent;
	pCurrent = pHistory;
	pHistory = pTemp;
	historySize = currentSize;
	currentSize = 0;
	pthread_mutex_unlock(&lock);
}

int Sampler_getHistorySize(void)
{
	pthread_mutex_lock(&lock);
	int size = historySize;
	pthread_mutex_unlock(&lock);
	return size;
}

double *Sampler_getHistory(int *pSize)
{
	assert(pSize != NULL);
	// Return a copy, so the caller can use it without holding the lock
	// (the caller frees it)
	pthread_mutex_lock(&lock);
	double *pCopy = malloc(historySize * sizeof(double));
	if (pCopy != NULL) {
		memcpy(pCopy, pHistory, historySize * sizeof(double));
	}
	*pSize = (pCopy != NULL) ? historySize : 0;
	pthread_mutex_unlock(&lock);
	return pCopy;
}

double Sampler_getAverageReading(void)
{
	pthread_mutex_lock(&lock);
	double avg = smoothedAvg;
	pthread_mutex_unlock(&lock);
	return avg;
}

long long Sampler_getNumSamplesTaken(void)
{
	pthread_mutex_lock(&lock);
	long long num = numSamplesTaken;
	pthread_mutex_unlock(&lock);
	return num;
}

static bool isThreadRunning(void)
{
	pthread_mutex_lock(&lock);
	bool shouldRun = isRunning;
	pthread_mutex_unlock(&lock);
	return shouldRun;
}
