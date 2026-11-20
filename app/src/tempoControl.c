// Tempo control module: turns encoder clicks into a BPM for playback.

#include "app/tempoControl.h"
#include "app/timing.h"
#include "hal/encoder.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define START_TEMPO_BPM 120
#define MIN_TEMPO_BPM 40
#define MAX_TEMPO_BPM 300
#define POLL_PERIOD_MS 10

static bool isInitialized = false;
static pthread_t tempoThread;
static atomic_bool isStopRequested = false;
static atomic_int currentTempoBPM = START_TEMPO_BPM;

static int clampTempo(int tempoBPM);
// static void applyTempo(int tempoBPM);

static void *tempoThreadFn(void *pArg)
{
	(void)pArg;
	int tempoBPM = START_TEMPO_BPM;
	int lastPosition = Encoder_getPosition();

	while (!atomic_load(&isStopRequested)) {
		// Apply the change in position (not the position itself), so the
		// tempo starts rising on the first clockwise click after 0 BPM
		int position = Encoder_getPosition();
		int newTempoBPM = clampTempo(tempoBPM + position - lastPosition);
		lastPosition = position;

		if (newTempoBPM != tempoBPM) {
			tempoBPM = newTempoBPM;
			atomic_store(&currentTempoBPM, tempoBPM);
			// applyTempo(tempoBPM);
		}
		Timing_sleepForMs(POLL_PERIOD_MS);
	}
	return NULL;
}

void TempoControl_init(void)
{
	assert(!isInitialized);
	Encoder_init();
	
	atomic_store(&currentTempoBPM, START_TEMPO_BPM);
	atomic_store(&isStopRequested, false);
	if (pthread_create(&tempoThread, NULL, tempoThreadFn, NULL) != 0) {
		fprintf(stderr, "Failed to create tempo control thread!\n");
		exit(EXIT_FAILURE);
	}
	isInitialized = true;
}

void TempoControl_cleanup(void)
{
	assert(isInitialized);
	atomic_store(&isStopRequested, true);
	pthread_join(tempoThread, NULL);
	Encoder_cleanup();
	isInitialized = false;
	printf("Tempo control thread cleaned up successfully.\n");
}

int TempoControl_getTempoBPM(void)
{
	return atomic_load(&currentTempoBPM);
}

static int clampTempo(int tempoBPM)
{
	int clamped = tempoBPM;
	if (clamped < MIN_TEMPO_BPM) {
		clamped = MIN_TEMPO_BPM;
	}
	if (clamped > MAX_TEMPO_BPM) {
		clamped = MAX_TEMPO_BPM;
	}
	return clamped;
}

/* static void applyTempo(int tempoBPM) */
/* { */
/* 	if (isSoftwareTempo(tempoBPM)) { */
/* 		// Start a new software cycle with the LED on */
/* 		isSoftwareLedOn = true; */
/* 		ticksSinceToggle = 0; */
/* 		Pwm_setLevel(isSoftwareLedOn); */
/* 	} else { */
/* 		Pwm_setTempo(tempoBPM); */
/* 	} */
/* } */
