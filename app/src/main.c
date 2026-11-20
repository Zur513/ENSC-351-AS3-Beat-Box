// Beat box: plays drum beats controlled via joystick, accelerometer, and web interface

#include "app/drumBeat.h"
#include "app/audioMixer.h"
#include "app/timing.h"
#include "app/periodTimer.h"
#include "app/reporter.h"
#include "app/udpServer.h"
#include "app/shutdown.h"
#include "app/tempoControl.h"

int main(void)
{
	wavedata_t sound;

	AudioMixer_init();
	DrumBeat_init();
	UdpServer_init();

	// Load sound
	AudioMixer_readWaveFileIntoMemory("beatbox-wav-files/100060__menegass__gui-drum-splash-hard.wav", &sound);
	
	AudioMixer_queueSound(&sound);

	// sleep for 0.5 seconds to allow the sound to play before cleanup
	Timing_sleepForMs(0.5 * 1000);
	AudioMixer_queueSound(&sound);

	// Sleep here until the UDP "stop" command asks the program to end
	Shutdown_waitForRequest();

	// Stop in the reverse order so no module is still using one that has already stopped
	UdpServer_cleanup();
	DrumBeat_cleanup();
	AudioMixer_cleanup();

	// Unload sound
	AudioMixer_freeWaveFileData(&sound);

	return 0;
}
