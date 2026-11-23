// Input control module: polls the joystick and the accelerometer through the ADC
// every 10 ms. Joystick up/down changes the volume; a sharp move along an
// accelerometer axis ("air drumming") plays that axis's drum sound.

#include "app/inputControl.h"
#include "app/audioMixer.h"
#include "app/drumBeat.h"
#include "app/periodTimer.h"
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

// Accelerometer: about 0.36 V per g on this board (measured: a 1 g tilt moved each axis ~0.35-0.37 V)
#define ACCEL_V_PER_G 0.36
// After a hit, ignore that axis for this long, so the slow-down at the end of
// the same air-drum stroke doesn't play a second sound. Short enough for ~100 BPM.
#define ACCEL_DEBOUNCE_POLLS (150 / POLL_PERIOD_MS)
#define ACCEL_REARM_FRACTION 0.5 //only rearms once the axis has calmed down below this fraction of its threshold

#define NUM_AXES 3


// Holding the joystick: wait HOLD_DELAY before the first repeat, then repeat
// every REPEAT_PERIOD. Counted in polls, since each pass of the loop is 10 ms.
#define HOLD_DELAY_POLLS (500 / POLL_PERIOD_MS)
#define REPEAT_PERIOD_POLLS (150 / POLL_PERIOD_MS)

typedef struct {
	int channel;
	double restV;           // voltage with the board flat and still (measured with adcprobe)
	double thresholdG; 		// how far from rest (in g) counts as a hit
	DrumBeat_sound sound;
	int pollsUntilReady; 	//debounce countdown after a hit
	bool isArmed; 			//false until the axis calms down after a hit
	const char *name;       // for the message printed on a hit
} AccelAxis;

// Only touched by the input thread. Rest voltages were measured flat on the desk;
// Z's includes the 1 g from gravity.
static AccelAxis axes[NUM_AXES] = {
	{ .channel = 5, .restV = 2.02, .thresholdG = 0.8, .sound = DRUMBEAT_SOUND_BASE,  .isArmed = true, .name = "Z" }, // vertical drum = bass
	{ .channel = 6, .restV = 1.64, .thresholdG = 1.2, .sound = DRUMBEAT_SOUND_SNARE, .isArmed = true, .name = "X" },
	{ .channel = 7, .restV = 1.65, .thresholdG = 1.2, .sound = DRUMBEAT_SOUND_HIHAT, .isArmed = true, .name = "Y" },
};

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

// Check one axis: play its sound on a hard enough move, then debounce
static void handleAccelerometerAxis(AccelAxis *pAxis)
{
	double g = (Adc_readVoltage(pAxis->channel) - pAxis->restV) / ACCEL_V_PER_G;
	double magnitudeG = (g < 0) ? -g : g;

	if (pAxis->pollsUntilReady > 0) {
		// Still debouncing after a hit
		pAxis->pollsUntilReady--;
		return;
	}
	if (!pAxis->isArmed) {
		// Wait for the axis to calm down before allowing the next hit
		if (magnitudeG < pAxis->thresholdG * ACCEL_REARM_FRACTION) {
			pAxis->isArmed = true;
		}
		return;
	}
	if (magnitudeG > pAxis->thresholdG) {
		DrumBeat_playSound(pAxis->sound);
		printf("Air drum %s: %.1f g\n", pAxis->name, g);
		pAxis->pollsUntilReady = ACCEL_DEBOUNCE_POLLS;
		pAxis->isArmed = false;
	}
}

// Read every axis once (one accelerometer "sample" for the reporter's stats)
static void handleAccelerometer(void)
{
	for (int i = 0; i < NUM_AXES; i++) {
		handleAccelerometerAxis(&axes[i]);
	}
	Period_markEvent(PERIOD_EVENT_ACCEL);
}

static void *inputThreadFn(void *pArg)
{
	(void)pArg;
	while (!atomic_load(&isStopRequested)) {
		handleJoystick();
		handleAccelerometer();
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
