// Tempo control module: Reads rotary encoder thread in background and adjusts DrumBeats tempo by +- 5 BPM per click,
// and cycles the beat mode on each press of the knob

#include "app/tempoControl.h"
#include "app/timing.h"
#include "hal/encoder.h"
#include "app/drumBeat.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

//#define START_TEMPO_BPM 120
//#define MIN_TEMPO_BPM 40
//#define MAX_TEMPO_BPM 300
#define POLL_PERIOD_MS 10
#define TEMPO_STEP_BPM 5

static bool isInitialized = false;
static pthread_t tempoThread;
static atomic_bool isStopRequested = false;
//static atomic_int currentTempoBPM = START_TEMPO_BPM;

//static int clampTempo(int tempoBPM);
// static void applyTempo(int tempoBPM);

static void *tempoThreadFn(void *pArg)
{
	(void)pArg;
	//int tempoBPM = START_TEMPO_BPM;
	int lastPosition = Encoder_getPosition();
	int lastPressCount = Encoder_getPressCount();

	while (!atomic_load(&isStopRequested)) {
		// Apply the change in position (not the position itself), so the
		// tempo starts rising on the first clockwise click after 0 BPM
		int position = Encoder_getPosition();
		int clicks = position - lastPosition;
		lastPosition = position;

		if (clicks != 0) {
			DrumBeat_setTempo(DrumBeat_getTempo() + clicks * TEMPO_STEP_BPM);
		}

		// Each (debounced) press of the knob moves to the next mode:
		// ROCK(1) -> CUSTOM(2) -> NONE(0) -> ROCK(1) ...
		int pressCount = Encoder_getPressCount();
		if (pressCount != lastPressCount) {
			lastPressCount = pressCount;
			DrumBeat_mode next = (DrumBeat_getMode() + 1) % DRUMBEAT_NUM_MODES;
			DrumBeat_setMode(next);
		}

		Timing_sleepForMs(POLL_PERIOD_MS);
	}
	return NULL;
}

void TempoControl_init(void)
{
	assert(!isInitialized);
	Encoder_init();
	
	//atomic_store(&currentTempoBPM, START_TEMPO_BPM);
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
