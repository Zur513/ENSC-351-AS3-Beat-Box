// Tempo control module: polls the rotary encoder in a background thread,
// changes drumBeat's tempo by 5 BPM per click, and moves to the next beat mode
// on each press of the knob

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

#define POLL_PERIOD_MS 10
#define TEMPO_STEP_BPM 5

static bool isInitialized = false;
static pthread_t tempoThread;
static atomic_bool isStopRequested = false;

static void *tempoThreadFn(void *pArg)
{
	(void)pArg;
	int lastPosition = Encoder_getPosition();
	int lastPressCount = Encoder_getPressCount();

	while (!atomic_load(&isStopRequested)) {
		// Apply the change in position (not the position itself), since the
		// tempo can also be set over UDP and gets clamped by drumBeat
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
