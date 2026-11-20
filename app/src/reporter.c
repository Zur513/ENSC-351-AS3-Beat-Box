// Reporter module: collects and reports statistical and setting information about beatbox

#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include "app/reporter.h"
#include "app/periodTimer.h"
#include "app/timing.h"
#include "app/drumBeat.h"
#include "app/audioMixer.h"

static pthread_t reporterThread;
static bool isRunning = false;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static bool isThreadRunning(void);
static void printStatsLine(int mode, int tempo, int volume,
		const Period_statistics_t *pAudioStats,
		const Period_statistics_t *pAccelStats);

static void *reporterThreadFn(void *pArg)
{
	(void)pArg;
	while (isThreadRunning()) {
		Timing_sleepForMs(MS_PER_SECOND);
		Period_statistics_t audioStats;
		Period_statistics_t accelStats;
		Period_getStatisticsAndClear(PERIOD_EVENT_AUDIO, &audioStats);
		Period_getStatisticsAndClear(PERIOD_EVENT_ACCEL, &accelStats);
		printStatsLine(DrumBeat_getMode(), DrumBeat_getTempo(),
				AudioMixer_getVolume(), &audioStats, &accelStats);
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

static void printStatsLine(int mode, int tempo, int volume,
		const Period_statistics_t *pAudioStats,
		const Period_statistics_t *pAccelStats)
{
	printf("M%d %dbpm vol:%d   "
			"Audio[%.3f, %.3f] avg %.3f/%d     "
			"Accel[%.3f, %.3f] avg %.3f/%d\n",
			mode, tempo, volume,
			pAudioStats->minPeriodInMs, pAudioStats->maxPeriodInMs,
			pAudioStats->avgPeriodInMs, pAudioStats->numSamples,
			pAccelStats->minPeriodInMs, pAccelStats->maxPeriodInMs,
			pAccelStats->avgPeriodInMs, pAccelStats->numSamples);
}
