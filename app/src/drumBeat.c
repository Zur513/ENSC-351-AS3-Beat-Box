// Drum beat module: plays the current beat pattern in a background thread
// (one step every half-beat), and owns the beat mode and the tempo

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include "app/drumBeat.h"
#include "app/audioMixer.h"
#include "app/timing.h"

#define HALF_BEATS_PER_MEASURE 8
#define HALF_BEATS_PER_BEAT 2
#define SECONDS_PER_MINUTE 60

static atomic_bool isStopping = false;
static atomic_int currentMode = DRUMBEAT_MODE_ROCK;
static atomic_int currentTempoBPM = DRUMBEAT_DEFAULT_BPM;

static pthread_t beatThreadId;

static const char *soundFiles[DRUMBEAT_NUM_SOUNDS] = {
	[DRUMBEAT_SOUND_BASE] = "beatbox-wav-files/100051__menegass__gui-drum-bd-hard.wav",
	[DRUMBEAT_SOUND_HIHAT] = "beatbox-wav-files/100053__menegass__gui-drum-cc.wav",
	[DRUMBEAT_SOUND_SNARE] = "beatbox-wav-files/100059__menegass__gui-drum-snare-soft.wav",
};
static wavedata_t sounds[DRUMBEAT_NUM_SOUNDS];

DrumBeat_mode DrumBeat_getMode(void)
{
	return atomic_load(&currentMode);
}

// Which sounds play on each half-beat. Each row is one half-beat, and the
// columns are in DrumBeat_sound order (base, hi-hat, snare).
// The rock beat is the table on page 1 of the handout.
static const bool rockPattern[HALF_BEATS_PER_MEASURE][DRUMBEAT_NUM_SOUNDS] = {
	{ true,  true,  false },	// 1
	{ false, true,  false },	// 1.5
	{ false, true,  true  },	// 2
	{ false, true,  false },	// 2.5
	{ true,  true,  false },	// 3
	{ false, true,  false },	// 3.5
	{ false, true,  true  },	// 4
	{ false, true,  false },	// 4.5
};

// Our own beat: hi-hat on every half-beat, snare on 1 and 3, and base on
// 2, 2.5, 3.5, and 4
static const bool customPattern[HALF_BEATS_PER_MEASURE][DRUMBEAT_NUM_SOUNDS] = {
	{ false, true,  true  },	// 1
	{ false, true,  false },	// 1.5
	{ true,  true,  false },	// 2
	{ true,  true,  false },	// 2.5
	{ false, true,  true  },	// 3
	{ true,  true,  false },	// 3.5
	{ true,  true,  false },	// 4
	{ false, true,  false },	// 4.5
};

// Play every sound marked true in this half-beat's row
static void playStep(const bool step[DRUMBEAT_NUM_SOUNDS])
{
	for (int s = 0; s < DRUMBEAT_NUM_SOUNDS; s++) {
		if (step[s]) {
			DrumBeat_playSound((DrumBeat_sound)s);
		}
	}
}

static void *beatThread(void *pArg)
{
	(void)pArg;
	// Position within one 4-beat measure (0 - 7)
	int halfBeat = 0;

	while (!atomic_load(&isStopping)) {
		switch (DrumBeat_getMode()) {
		case DRUMBEAT_MODE_ROCK:
			playStep(rockPattern[halfBeat]);
			break;
		case DRUMBEAT_MODE_CUSTOM:
			playStep(customPattern[halfBeat]);
			break;
		case DRUMBEAT_MODE_NONE:
			// Fall through
		default:
			break;
		}
		// Move to the next step, wrapping 7 -> 0
		halfBeat = (halfBeat + 1) % HALF_BEATS_PER_MEASURE;

		// Sleep for half a beat. The tempo is read fresh every loop, so
		// changes apply right away.
		long long halfBeatMs = SECONDS_PER_MINUTE * MS_PER_SECOND
		                       / (DrumBeat_getTempo() * HALF_BEATS_PER_BEAT);
		Timing_sleepForMs(halfBeatMs);
	}
	return NULL;
}

void DrumBeat_init(void)
{
	for (int i = 0; i < DRUMBEAT_NUM_SOUNDS; i++) {
		AudioMixer_readWaveFileIntoMemory(soundFiles[i], &sounds[i]);
	}
	atomic_store(&isStopping, false);
	if (pthread_create(&beatThreadId, NULL, beatThread, NULL) != 0) {
		fprintf(stderr, "Failed to create drum beat thread!\n");
		exit(EXIT_FAILURE);
	}
}

void DrumBeat_cleanup(void)
{
	atomic_store(&isStopping, true);
	pthread_join(beatThreadId, NULL);
	// Stop the mixer before freeing, since queued sounds still point at
	// these wave files
	AudioMixer_stopAllSounds();
	for (int i = 0; i < DRUMBEAT_NUM_SOUNDS; i++) {
		AudioMixer_freeWaveFileData(&sounds[i]);
	}
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
}
