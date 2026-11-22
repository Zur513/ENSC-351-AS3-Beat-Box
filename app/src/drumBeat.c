// drumBeat.c: Handles the drum beat settings (mode and tempo).

#include <stdatomic.h>
#include <stdio.h>
#include <assert.h>
#include "app/drumBeat.h"
#include "app/audioMixer.h"

static atomic_int currentMode = DRUMBEAT_MODE_ROCK;
static atomic_int currentTempoBPM = DRUMBEAT_DEFAULT_BPM;

static const char *soundFiles[DRUMBEAT_NUM_SOUNDS] = {
	[DRUMBEAT_SOUND_BASE] = "beatbox-wav-files/100051__menegass__gui-drum-bd-hard.wav",
	[DRUMBEAT_SOUND_HIHAT] = "beatbox-wav-files/100053__menegass__gui-drum-cc.wav",
	[DRUMBEAT_SOUND_SNARE] = "beatbox-wav-files/100059__menegass__gui-drum-snare-soft.wav",
};
static wavedata_t sounds[DRUMBEAT_NUM_SOUNDS];

void DrumBeat_init(void)
{
	for (int i = 0; i < DRUMBEAT_NUM_SOUNDS; i++) {
		AudioMixer_readWaveFileIntoMemory((char *)soundFiles[i], &sounds[i]); // Read the sound files into memory
	}
}

void DrumBeat_cleanup(void)
{
	for ( int i = 0; i < DRUMBEAT_NUM_SOUNDS; i++) {
		AudioMixer_freeWaveFileData(&sounds[i]); // Free the memory allocated for the sound files
	}
}

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
	AudioMixer_queueSound(&sounds[sound]);
	printf("Playing sound %d\n", sound);
}
