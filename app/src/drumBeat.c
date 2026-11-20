// drumBeat.c: Handles the drum beat settings (mode and tempo).

#include <stdatomic.h>
#include <stdio.h>
#include <assert.h>
#include "app/drumBeat.h"

static atomic_int currentMode = DRUMBEAT_MODE_ROCK;
static atomic_int currentTempoBPM = DRUMBEAT_DEFAULT_BPM;

void DrumBeat_init(void)
{}

void DrumBeat_cleanup(void)
{}

DrumBeat_mode DrumBeat_getMode(void)
{
	return atomic_load(&currentMode);
}

void DrumBeat_setMode(DrumBeat_mode mode)
{
	assert((mode >= 0) && (mode < DRUMBEAT_NUM_MODES));
	atomic_store(&currentMode, mode);
}

int DrumBeat_getTempo(void)
{
	return atomic_load(&currentTempoBPM);
}

void DrumBeat_setTempo(int bpm)
{
	// Clamp
	if (bpm <= DRUMBEAT_MIN_BPM) {
		bpm = DRUMBEAT_MIN_BPM;
	} else if (bpm >= DRUMBEAT_MAX_BPM) {
		bpm = DRUMBEAT_MAX_BPM;
	}
	atomic_store(&currentTempoBPM, bpm);
}

void DrumBeat_playSound(DrumBeat_sound sound)
{
	assert((sound >= 0) && (sound < DRUMBEAT_NUM_SOUNDS));
	printf("Playing sound %d\n", sound);
}
