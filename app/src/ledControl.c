// LED control module: turns encoder clicks into a flash frequency for the LED.

#include "app/ledControl.h"
#include "app/timing.h"
#include "hal/encoder.h"
#include "hal/pwm.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define START_FREQUENCY_HZ 10
#define MIN_FREQUENCY_HZ 0
#define MAX_FREQUENCY_HZ 500
#define POLL_PERIOD_MS 10
#define HALF_PERIODS_PER_CYCLE 2

static bool isInitialized = false;
static pthread_t ledThread;
static atomic_bool isStopRequested = false;
static atomic_int currentFrequencyHz = START_FREQUENCY_HZ;

// Software flashing state, only touched by the LED thread
static bool isSoftwareLedOn = false;
static int ticksSinceToggle = 0;

static void *ledThreadFn(void *pArg);
static int clampFrequency(int frequencyHz);
static bool isSoftwareFrequency(int frequencyHz);
static void applyFrequency(int frequencyHz);
static void updateSoftwareFlashing(int frequencyHz);

static void *ledThreadFn(void *pArg)
{
	(void)pArg;
	int frequencyHz = START_FREQUENCY_HZ;
	int lastPosition = Encoder_getPosition();

	while (!atomic_load(&isStopRequested)) {
		// Apply the change in position (not the position itself), so the
		// frequency starts rising on the first clockwise click after 0 Hz
		int position = Encoder_getPosition();
		int newFrequencyHz = clampFrequency(frequencyHz + position - lastPosition);
		lastPosition = position;

		// Only touch the LED when the frequency changes, so the flashing
		// isn't interrupted
		if (newFrequencyHz != frequencyHz) {
			frequencyHz = newFrequencyHz;
			atomic_store(&currentFrequencyHz, frequencyHz);
			applyFrequency(frequencyHz);
		}
		updateSoftwareFlashing(frequencyHz);
		Timing_sleepForMs(POLL_PERIOD_MS);
	}
	return NULL;
}

void LedControl_init(void)
{
	assert(!isInitialized);
	Encoder_init();
	Pwm_init();
	Pwm_setFrequency(START_FREQUENCY_HZ);

	atomic_store(&currentFrequencyHz, START_FREQUENCY_HZ);
	atomic_store(&isStopRequested, false);
	if (pthread_create(&ledThread, NULL, ledThreadFn, NULL) != 0) {
		fprintf(stderr, "Failed to create LED control thread!\n");
		exit(EXIT_FAILURE);
	}
	isInitialized = true;
}

void LedControl_cleanup(void)
{
	assert(isInitialized);
	atomic_store(&isStopRequested, true);
	pthread_join(ledThread, NULL);
	Pwm_cleanup();
	Encoder_cleanup();
	isInitialized = false;
	printf("LED control thread cleaned up successfully.\n");
}

int LedControl_getFrequencyHz(void)
{
	return atomic_load(&currentFrequencyHz);
}

static int clampFrequency(int frequencyHz)
{
	int clamped = frequencyHz;
	if (clamped < MIN_FREQUENCY_HZ) {
		clamped = MIN_FREQUENCY_HZ;
	}
	if (clamped > MAX_FREQUENCY_HZ) {
		clamped = MAX_FREQUENCY_HZ;
	}
	return clamped;
}

// Frequencies the PWM hardware can't reach are flashed by toggling the LED
static bool isSoftwareFrequency(int frequencyHz)
{
	return frequencyHz > 0 && frequencyHz < PWM_MIN_FREQUENCY_HZ;
}

static void applyFrequency(int frequencyHz)
{
	if (isSoftwareFrequency(frequencyHz)) {
		// Start a new software cycle with the LED on
		isSoftwareLedOn = true;
		ticksSinceToggle = 0;
		Pwm_setLevel(isSoftwareLedOn);
	} else {
		Pwm_setFrequency(frequencyHz);
	}
}

// Toggle the LED every half period while flashing in software
static void updateSoftwareFlashing(int frequencyHz)
{
	if (isSoftwareFrequency(frequencyHz)) {
		int ticksPerHalfPeriod = (MS_PER_SECOND / POLL_PERIOD_MS)
		                         / (HALF_PERIODS_PER_CYCLE * frequencyHz);
		ticksSinceToggle++;
		if (ticksSinceToggle >= ticksPerHalfPeriod) {
			ticksSinceToggle = 0;
			isSoftwareLedOn = !isSoftwareLedOn;
			Pwm_setLevel(isSoftwareLedOn);
		}
	}
}
