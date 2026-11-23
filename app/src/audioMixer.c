// Audio mixer: plays wave files in real time through ALSA
// mixing any sounds that overlap into one playback buffer

#include "app/audioMixer.h"
#include "app/periodTimer.h"
#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <limits.h>
#include <alloca.h> // needed for mixer

static snd_pcm_t *handle;

//since running from root causes the USB default audio to go somewhere else (not our configured default)
//we need to open the USB card directly here in audioMixer.c
#define PCM_DEVICE_USB      "plughw:Device"   // USB adapter by card ID; "plug" converts 44.1 kHz mono for it
#define PCM_DEVICE_FALLBACK "default"

#define DEFAULT_VOLUME 80

#define SAMPLE_RATE 44100
#define NUM_CHANNELS 1
// How much audio ALSA buffers (0.05 seconds), which sets the playback latency
#define PLAYBACK_LATENCY_US 50000
#define SAMPLE_SIZE (sizeof(short)) 			// bytes per sample
// Sample size note: This works for mono files because each sample ("frame') is 1 value.
// If using stereo files then a frame would be two samples.

static unsigned long playbackBufferSize = 0;
static short *playbackBuffer = NULL;


// Currently active (waiting to be played) sound bites
#define MAX_SOUND_BITES 30
typedef struct {
	// A pointer to a previously allocated sound bite (wavedata_t struct).
	// Note that many different sound-bite slots could share the same pointer
	// (overlapping cymbal crashes, for example)
	wavedata_t *pSound;

	// The offset into the pData of pSound. Indicates how much of the
	// sound has already been played (and hence where to start playing next).
	int location;
} playbackSound_t;
static playbackSound_t soundBites[MAX_SOUND_BITES];

// Playback threading
static void *playbackThread(void *pArg);
static bool isStopping = false;
static pthread_t playbackThreadId;
static pthread_mutex_t audioMutex = PTHREAD_MUTEX_INITIALIZER;

// Separate from audioMutex so a slow ALSA mixer write never stalls playback
static pthread_mutex_t volumeMutex = PTHREAD_MUTEX_INITIALIZER;
static int volume = 0;

void AudioMixer_init(void)
{
	AudioMixer_setVolume(DEFAULT_VOLUME);

	// Initialize the currently active sound-bites being played
	for (int i = 0; i < MAX_SOUND_BITES; i++) {
		soundBites[i].pSound = NULL;
	}

	// Open the PCM output: the USB adapter directly, so it works as root (sudo)
	// without PipeWire; fall back to "default" if that card isn't available
	int err = snd_pcm_open(&handle, PCM_DEVICE_USB, SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		fprintf(stderr, "AudioMixer: can't open %s (%s), trying %s\n", PCM_DEVICE_USB, snd_strerror(err), PCM_DEVICE_FALLBACK);
		err = snd_pcm_open(&handle, PCM_DEVICE_FALLBACK, SND_PCM_STREAM_PLAYBACK, 0);
	}
	if (err < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	// Configure parameters of PCM output
	err = snd_pcm_set_params(handle,
			SND_PCM_FORMAT_S16_LE,
			SND_PCM_ACCESS_RW_INTERLEAVED,
			NUM_CHANNELS,
			SAMPLE_RATE,
			1,			// Allow software resampling
			PLAYBACK_LATENCY_US);
	if (err < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	// Allocate this software's playback buffer to be the same size as the
	// the hardware's playback buffers for efficient data transfers.
	// ..get info on the hardware buffers:
	unsigned long unusedBufferSize = 0;
	snd_pcm_get_params(handle, &unusedBufferSize, &playbackBufferSize);
	// ..allocate playback buffer:
	playbackBuffer = malloc(playbackBufferSize * sizeof(*playbackBuffer));

	// Launch playback thread:
	pthread_create(&playbackThreadId, NULL, playbackThread, NULL);
}


// Client code must call AudioMixer_freeWaveFileData to free dynamically allocated data.
void AudioMixer_readWaveFileIntoMemory(const char *pFileName, wavedata_t *pSound)
{
	assert(pSound);

	// The PCM data in a wave file starts after the header:
	const int PCM_DATA_OFFSET = 44;

	// Open the wave file
	FILE *pFile = fopen(pFileName, "r");
	if (pFile == NULL) {
		fprintf(stderr, "ERROR: Unable to open file %s.\n", pFileName);
		exit(EXIT_FAILURE);
	}

	// Get file size
	fseek(pFile, 0, SEEK_END);
	int sizeInBytes = ftell(pFile) - PCM_DATA_OFFSET;
	pSound->numSamples = sizeInBytes / SAMPLE_SIZE;

	// Search to the start of the data in the file
	fseek(pFile, PCM_DATA_OFFSET, SEEK_SET);

	// Allocate space to hold all PCM data
	pSound->pData = malloc(sizeInBytes);
	if (pSound->pData == 0) {
		fprintf(stderr, "ERROR: Unable to allocate %d bytes for file %s.\n",
				sizeInBytes, pFileName);
		exit(EXIT_FAILURE);
	}

	// Read PCM data from wave file into memory
	int samplesRead = fread(pSound->pData, SAMPLE_SIZE, pSound->numSamples, pFile);
	if (samplesRead != pSound->numSamples) {
		fprintf(stderr, "ERROR: Unable to read %d samples from file %s (read %d).\n",
				pSound->numSamples, pFileName, samplesRead);
		exit(EXIT_FAILURE);
	}
	fclose(pFile);
}

void AudioMixer_freeWaveFileData(wavedata_t *pSound)
{
	pSound->numSamples = 0;
	free(pSound->pData);
	pSound->pData = NULL;
}

void AudioMixer_queueSound(wavedata_t *pSound)
{
	// Ensure we are only being asked to play "good" sounds:
	assert(pSound->numSamples > 0);
	assert(pSound->pData);

	bool foundFreeSlot = false;
	pthread_mutex_lock(&audioMutex);
	for (int i = 0; i < MAX_SOUND_BITES; i++) {
		if (soundBites[i].pSound == NULL) {
			soundBites[i].pSound = pSound;
			soundBites[i].location = 0;
			foundFreeSlot = true;
			break;
		}
	}
	pthread_mutex_unlock(&audioMutex);
	if (!foundFreeSlot) {
		fprintf(stderr, "ERROR: No free slots found in soundBites array\n");
	}
}

void AudioMixer_cleanup(void)
{
	printf("Stopping audio...\n");

	// Stop the PCM generation thread
	isStopping = true;
	pthread_join(playbackThreadId, NULL);

	// Shutdown the PCM output, allowing any pending sound to play out (drain)
	snd_pcm_drain(handle);
	snd_pcm_close(handle);

	// Free playback buffer
	// (note that any wave files read into wavedata_t records must be freed
	//  in addition to this by calling AudioMixer_freeWaveFileData() on that struct.)
	free(playbackBuffer);
	playbackBuffer = NULL;

	printf("Done stopping audio...\n");
	fflush(stdout);
}

int AudioMixer_getVolume(void)
{
	// Return the cached volume
	pthread_mutex_lock(&volumeMutex);
	int val = volume;
	pthread_mutex_unlock(&volumeMutex);
	return val;
}

// Function copied from:
// http://stackoverflow.com/questions/6787318/set-alsa-master-volume-from-c-code
// Written by user "trenki".
void AudioMixer_setVolume(int newVolume)
{
	// Ensure volume is reasonable; If so, cache it for later getVolume() calls.
	if (newVolume < 0 || newVolume > AUDIOMIXER_MAX_VOLUME) {
		printf("ERROR: Volume must be between 0 and 100.\n");
		return;
	}

	// Hold the lock across the hardware write too, so the cached value
	// and the speaker can't end up disagreeing when two threads race
	pthread_mutex_lock(&volumeMutex);
	volume = newVolume;

	long min;
	long max;
	snd_mixer_t *mixerHandle;
	snd_mixer_selem_id_t *sid;
	// USB adapter's card ID ("default" is PipeWire, which has no "Speaker")
	const char *card = "hw:Device";
	// const char *selem_name = "PCM";	// For ZEN cape
	const char *selem_name = "Speaker";	// For USB Audio

	snd_mixer_open(&mixerHandle, 0);
	snd_mixer_attach(mixerHandle, card);
	snd_mixer_selem_register(mixerHandle, NULL, NULL);
	snd_mixer_load(mixerHandle);

	snd_mixer_selem_id_alloca(&sid);
	snd_mixer_selem_id_set_index(sid, 0);
	snd_mixer_selem_id_set_name(sid, selem_name);
	snd_mixer_elem_t* elem = snd_mixer_find_selem(mixerHandle, sid);
	assert(elem != NULL && "Mixer Control not found - check card/selem_name");
	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_set_playback_volume_all(elem, newVolume * max / AUDIOMIXER_MAX_VOLUME);

	snd_mixer_close(mixerHandle);
	pthread_mutex_unlock(&volumeMutex);
}

// Fill the pBuff array with new PCM values to output.
//    pBuff: buffer to fill with new PCM data from sound bites.
//    size: the number of *values* (not bytes) to store into pBuff
static void fillPlaybackBuffer(short *pBuff, int size)
{
	// Start from silence, then add each sound bite on top
	int bytesPerSample = sizeof(*pBuff);
	memset(pBuff, 0, size * bytesPerSample);

	pthread_mutex_lock(&audioMutex);

	for (int i = 0; i < MAX_SOUND_BITES; i++) {
		wavedata_t *pSound = soundBites[i].pSound;
		if (soundBites[i].pSound == NULL) {
			continue;
		}
		// Work on a local copy of the location, and write it back after
		int loc = soundBites[i].location;
		int numSamples = soundBites[i].pSound->numSamples;
		short *pData = pSound->pData;

		for (int j = 0; j < size && loc < numSamples; j++) {
			int sum = pBuff[j] + pData[loc];
			if (sum > SHRT_MAX) {
				sum = SHRT_MAX;
			} else if (sum < SHRT_MIN) {
				sum = SHRT_MIN;
			}
			pBuff[j] = (short)sum;
			loc++;
		}

		soundBites[i].location = loc;
		if (loc >= numSamples) {
			// Finished playing this sound bite, free the slot
			soundBites[i].pSound = NULL;
		}
	}
	pthread_mutex_unlock(&audioMutex);
}

static void *playbackThread(void *pArg)
{
	(void)pArg;
	while (!isStopping) {
		// Generate next block of audio
		fillPlaybackBuffer(playbackBuffer, playbackBufferSize);

		// Mark an audio event
		Period_markEvent(PERIOD_EVENT_AUDIO);

		// Output the audio
		snd_pcm_sframes_t frames = snd_pcm_writei(handle,
				playbackBuffer, playbackBufferSize);

		// Check for (and handle) possible error conditions on output
		if (frames < 0) {
			fprintf(stderr, "AudioMixer: writei() returned %li\n", frames);
			frames = snd_pcm_recover(handle, frames, 1);
		}
		if (frames < 0) {
			fprintf(stderr, "ERROR: Failed writing audio with snd_pcm_writei(): %li\n",
					frames);
			exit(EXIT_FAILURE);
		}
		if (frames > 0 && frames < (snd_pcm_sframes_t)playbackBufferSize) {
			printf("Short write (expected %lu, wrote %li)\n",
					playbackBufferSize, frames);
		}
	}

	return NULL;
}

void AudioMixer_stopAllSounds(void)
{
	pthread_mutex_lock(&audioMutex);
	for (int i = 0; i < MAX_SOUND_BITES; i++) {
		soundBites[i].pSound = NULL;
	}
	pthread_mutex_unlock(&audioMutex);
}
