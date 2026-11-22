// drumBeat.c: Handles the drum beat settings (mode and tempo).

#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include "app/drumBeat.h"
#include "app/audioMixer.h"
#include "app/timing.h"

#define HALF_BEATS_PER_MEASURE 8

static atomic_bool stopping = false;
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

//which sounds play on each half-beat.
// Column order matches the DrumBeat_sound enum
static const bool rockPattern[HALF_BEATS_PER_MEASURE][DRUMBEAT_NUM_SOUNDS] = {//each row is one line of the table on page 1 of the handout. 
    { true,  true,  false },  // 1
    { false, true,  false },  // 1.5
    { false, true,  true  },  // 2
    { false, true,  false },  // 2.5
    { true,  true,  false },  // 3
    { false, true,  false },  // 3.5
    { false, true,  true  },  // 4
    { false, true,  false },  // 4.5
};

static const bool customPattern[HALF_BEATS_PER_MEASURE][DRUMBEAT_NUM_SOUNDS] = {
    { true,  true,  false }, 
	{ false, true,  false }, 
	{ true, true,  false  }, 
	{ true, true,  false },
    { false,  true,  true }, 
	{ true, true,  false }, 
	{ true, true,  false  }, 
	{ false, true,  true },
};

static void playStep(const bool step[DRUMBEAT_NUM_SOUNDS]) { //switch on the mode and play the sounds for this halfBeat
    for (int s = 0; s < DRUMBEAT_NUM_SOUNDS; s++) {
        if (step[s]) { //check if this index is marked true and if yes play the sound
            DrumBeat_playSound((DrumBeat_sound)s);
        }
    }
}

static void* beatThread(void *arg) 
{
    (void)arg; 
    int halfBeat = 0; // 0-7 = position within one 4-beat measure

    while(!atomic_load(&stopping)) {
        switch (DrumBeat_getMode()) {// different options for beats
            case DRUMBEAT_MODE_ROCK:
            playStep(rockPattern[halfBeat]);
            break;
            case DRUMBEAT_MODE_CUSTOM:
            playStep(customPattern[halfBeat]);
            break;
            case DRUMBEAT_MODE_NONE:
            default:
            break;
        }
        halfBeat = (halfBeat +1) % HALF_BEATS_PER_MEASURE; //move halfBeat to the next step, wrapping 7 -> 0

        long long halfBeatMs = 60LL * 1000 / DrumBeat_getTempo() / 2;
        Timing_sleepForMs(halfBeatMs); //sleep for half a beat: 60 / BPM / 2 seconds, in ms, reading the tempo fresh every loop so changes apply right away
    }
    return NULL;
} 

void DrumBeat_init(void)
{
	for (int i = 0; i < DRUMBEAT_NUM_SOUNDS; i++) {
		AudioMixer_readWaveFileIntoMemory((char *)soundFiles[i], &sounds[i]); // Read the sound files into memory
	}
	atomic_store(&stopping, false);
	pthread_create(&beatThreadId, NULL, beatThread, NULL);
}

void DrumBeat_cleanup(void)
{
	atomic_store(&stopping, true);
	pthread_join(beatThreadId, NULL);
	AudioMixer_stopAllSounds();
	for ( int i = 0; i < DRUMBEAT_NUM_SOUNDS; i++) {
		AudioMixer_freeWaveFileData(&sounds[i]); // Free the memory allocated for the sound files
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
