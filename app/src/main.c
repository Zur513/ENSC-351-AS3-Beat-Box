// Beat box: plays drum beats controlled via joystick, accelerometer, and web interface

#include <unistd.h> //remove later and use any sleeps in the other modules
#include "app/audioMixer.h"
#include "app/timing.h"
#include "app/sampler.h"
#include "app/periodTimer.h"
#include "app/reporter.h"
#include "app/udpServer.h"
#include "app/shutdown.h"
#include "app/ledControl.h"

int main(void)
{
	wavedata_t sound;
	// Start the modules (each one except the period timer runs its own thread)
	AudioMixer_init();
	AudioMixer_readWaveFileIntoMemory("beatbox-wav-files/100060__menegass__gui-drum-splash-hard.wav", &sound);

	//Period_init();
	//Sampler_init();
	//Reporter_init();
	//UdpServer_init();
	//LedControl_init();

	// Sleep here until the UDP "stop" command asks the program to end
	//Shutdown_waitForRequest();

	// Stop in roughly the reverse order, so no module is still using one
	// that has already stopped
	AudioMixer_queueSound(&sound);
	Timing_sleepForMs(0.5 * 1000); // sleep for 0.9 seconds to allow the sound to play before cleanup
	AudioMixer_queueSound(&sound);
	sleep(2);

	AudioMixer_cleanup();
	AudioMixer_freeWaveFileData(&sound);
	//LedControl_cleanup();
	//UdpServer_cleanup();
	//Reporter_cleanup();
	//Sampler_cleanup();
	//Period_cleanup();
	return 0;
}
