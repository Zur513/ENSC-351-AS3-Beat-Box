// drumBeat.h
// Drum beat module which plays the current beat in a background thread, and plays
// single drum sounds on request. This module owns the beat mode and the tempo.
// Every function aside from init/cleanup is safe to call from any thread.

#ifndef DRUM_BEAT_H
#define DRUM_BEAT_H

// match what the web UI sends ("mode 0", "mode 1", "mode 2")
typedef enum {
	DRUMBEAT_MODE_NONE = 0,
	DRUMBEAT_MODE_ROCK = 1,
	DRUMBEAT_MODE_CUSTOM = 2,
	DRUMBEAT_NUM_MODES
} DrumBeat_mode;

// match what the web UI sends ("play 0" base, "play 1" hi-hat, "play 2" snare)
typedef enum {
	DRUMBEAT_SOUND_BASE = 0,
	DRUMBEAT_SOUND_HIHAT = 1,
	DRUMBEAT_SOUND_SNARE = 2,
	DRUMBEAT_NUM_SOUNDS
} DrumBeat_sound;

#define DRUMBEAT_MIN_BPM 40
#define DRUMBEAT_MAX_BPM 300
#define DRUMBEAT_DEFAULT_BPM 120

// Call this after AudioMixer_init(), and cleanup before AudioMixer_cleanup().
// init loads the wave files and starts the beat thread in ROCK mode
void DrumBeat_init(void);
void DrumBeat_cleanup(void);

DrumBeat_mode DrumBeat_getMode(void);
// the mode must be a valid DrumBeat_mode, callers should check first (assert fails otherwise)
void DrumBeat_setMode(DrumBeat_mode mode);

// Out-of-range values are clamped: [MIN_BPM, MAX_BPM]
int DrumBeat_getTempo(void);
void DrumBeat_setTempo(int bpm);

// Play one sound once on top of the beat
// The sound must be a valid DrumBeat_sound, callers should check first (assert fails otherwise)
void DrumBeat_playSound(DrumBeat_sound sound);

#endif
