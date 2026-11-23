// Input control module: polls the joystick through the ADC every 10 ms and turns
// up/down into volume changes. The accelerometer will be read in the same loop.

#include "app/inputControl.h"
#include "app/audioMixer.h"
#include "app/timing.h"
#include "hal/adc.h"
#include "hal/joystick.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define POLL_PERIOD_MS 10
#define VOLUME_STEP 5

// Holding the joystick: wait HOLD_DELAY before the first repeat, then repeat
// every REPEAT_PERIOD. Counted in polls, since each pass of the loop is 10 ms.
#define HOLD_DELAY_POLLS (500 / POLL_PERIOD_MS)
#define REPEAT_PERIOD_POLLS (150 / POLL_PERIOD_MS)

static bool isInitialized = false;
static pthread_t inputThread;
static atomic_bool isStopRequested = false;

// Joystick hold state, only touched by the input thread
static bool isJoystickHeld = false;
static int pollsUntilRepeat = 0;

// Add delta to the volume, clamped to [0, 100], since AudioMixer_setVolume()
// rejects out-of-range values instead of clamping them
static void changeVolume(int delta)
{
	int oldVolume = AudioMixer_getVolume();
	int newVolume = oldVolume + delta;
	if (newVolume > AUDIOMIXER_MAX_VOLUME) {
		newVolume = AUDIOMIXER_MAX_VOLUME;
	}
	if (newVolume < 0) {
		newVolume = 0;
	}
	if (newVolume != oldVolume) {
		AudioMixer_setVolume(newVolume);
		printf("Volume: %d\n", newVolume);
	}
}

// One press changes the volume once; holding repeats after a short delay
static void handleJoystick(void)
{
	Joystick_direction direction = Joystick_getDirection();
	if (direction == JOYSTICK_NONE) {
		// Released: the next push acts right away
		isJoystickHeld = false;
		return;
	}

	int delta = (direction == JOYSTICK_UP) ? VOLUME_STEP : -VOLUME_STEP;
	if (!isJoystickHeld) {
		// New press
		isJoystickHeld = true;
		pollsUntilRepeat = HOLD_DELAY_POLLS;
		changeVolume(delta);
	} else if (--pollsUntilRepeat <= 0) {
		// Still held long enough: repeat
		pollsUntilRepeat = REPEAT_PERIOD_POLLS;
		changeVolume(delta);
	}
}

static void *inputThreadFn(void *pArg)
{
	(void)pArg;
	while (!atomic_load(&isStopRequested)) {
		handleJoystick();
		// TODO: read the accelerometer here, and Period_markEvent(PERIOD_EVENT_ACCEL)
		Timing_sleepForMs(POLL_PERIOD_MS);
	}
	return NULL;
}

void InputControl_init(void)
{
	assert(!isInitialized);
	Adc_init();

	isJoystickHeld = false;
	atomic_store(&isStopRequested, false);
	if (pthread_create(&inputThread, NULL, inputThreadFn, NULL) != 0) {
		fprintf(stderr, "Failed to create input control thread!\n");
		exit(EXIT_FAILURE);
	}
	isInitialized = true;
}

void InputControl_cleanup(void)
{
	assert(isInitialized);
	atomic_store(&isStopRequested, true);
	pthread_join(inputThread, NULL);
	// Only close the ADC once the thread can no longer read it
	Adc_cleanup();
	isInitialized = false;
	printf("Input control thread cleaned up successfully.\n");
}
