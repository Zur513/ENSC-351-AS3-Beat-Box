// PWM module: drives the LED emitter through the kernel's PWM files.

#include "hal/pwm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define PWM_DIR "/dev/hat/pwm/GPIO12"
#define MAX_PATH_LENGTH 128
#define NS_PER_SECOND 1000000000LL
// 50% duty cycle: on for half of each period
#define PERIOD_TO_DUTY_DIVISOR 2

// Period used at startup and for software flashing. It must be one the
// hardware accepts (the check happens when the output is enabled).
#define INIT_FREQUENCY_HZ 10
#define INIT_PERIOD_NS (NS_PER_SECOND / INIT_FREQUENCY_HZ)

// currentFrequencyHz while Pwm_setLevel() holds the output on or off
#define NOT_HARDWARE_FLASHING -1

static bool isInitialized = false;
static long long currentPeriodNs = 0;
static long long currentDutyNs = 0;
static int currentFrequencyHz = NOT_HARDWARE_FLASHING;

static void writeValue(const char *pFileName, long long value);

void Pwm_init(void)
{
	assert(!isInitialized);
	writeValue("enable", 0);
	writeValue("duty_cycle", 0);
	writeValue("period", INIT_PERIOD_NS);
	writeValue("enable", 1);
	currentDutyNs = 0;
	currentPeriodNs = INIT_PERIOD_NS;
	currentFrequencyHz = 0;
	isInitialized = true;
}

void Pwm_setFrequency(int frequencyHz)
{
	assert(isInitialized);
	assert(frequencyHz == 0 || frequencyHz >= PWM_MIN_FREQUENCY_HZ);

	if (frequencyHz != currentFrequencyHz) {
		if (frequencyHz == 0) {
			// A duty cycle of 0 holds the pin low (the period stays as it is)
			writeValue("duty_cycle", 0);
			currentDutyNs = 0;
		} else {
			long long newPeriodNs = NS_PER_SECOND / frequencyHz;
			long long newDutyNs = newPeriodNs / PERIOD_TO_DUTY_DIVISOR;

			// The kernel requires duty <= period after every write, so the
			// order depends on whether the period grows or shrinks
			if (newPeriodNs < currentPeriodNs) {
				writeValue("duty_cycle", newDutyNs);
				writeValue("period", newPeriodNs);
			} else {
				writeValue("period", newPeriodNs);
				writeValue("duty_cycle", newDutyNs);
			}
			currentPeriodNs = newPeriodNs;
			currentDutyNs = newDutyNs;
		}
		currentFrequencyHz = frequencyHz;
	}
}

void Pwm_setLevel(bool isOn)
{
	assert(isInitialized);
	// The period stays as it is (always a valid one). A duty cycle of 0 is a
	// constant low, and one equal to the period is a constant high.
	long long newDutyNs = isOn ? currentPeriodNs : 0;
	currentFrequencyHz = NOT_HARDWARE_FLASHING;
	if (newDutyNs != currentDutyNs) {
		writeValue("duty_cycle", newDutyNs);
		currentDutyNs = newDutyNs;
	}
}

void Pwm_cleanup(void)
{
	assert(isInitialized);
	writeValue("duty_cycle", 0);
	writeValue("enable", 0);
	currentDutyNs = 0;
	currentFrequencyHz = NOT_HARDWARE_FLASHING;
	isInitialized = false;
}

// Write a number to one of the PWM files, and exit if the kernel rejects it
static void writeValue(const char *pFileName, long long value)
{
	assert(pFileName != NULL);
	char path[MAX_PATH_LENGTH];
	snprintf(path, sizeof(path), "%s/%s", PWM_DIR, pFileName);

	FILE *pFile = fopen(path, "w");
	if (pFile == NULL) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	// The kernel checks the value when the file is written and closed
	if (fprintf(pFile, "%lld", value) < 0 || fclose(pFile) != 0) {
		perror(path);
		exit(EXIT_FAILURE);
	}
}
