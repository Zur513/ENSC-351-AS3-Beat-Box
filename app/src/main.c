// Beat box: plays drum beats controlled via joystick, accelerometer, and web interface

#include "app/drumBeat.h"
#include "app/audioMixer.h"
#include "app/timing.h"
#include "app/periodTimer.h"
#include "app/reporter.h"
#include "app/udpServer.h"
#include "app/shutdown.h"
#include "app/tempoControl.h"
#include "app/inputControl.h"

int main(void)
{
	Period_init();
	AudioMixer_init();
	DrumBeat_init();
	UdpServer_init();
	Reporter_init();
	TempoControl_init();
	InputControl_init();

	// Sleep here until the UDP "stop" command asks the program to end
	Shutdown_waitForRequest();

	// Stop in the reverse order so no module is still using one that has already stopped
	InputControl_cleanup();
	TempoControl_cleanup();
	Reporter_cleanup();
	UdpServer_cleanup();
	DrumBeat_cleanup();
	AudioMixer_cleanup();
	Period_cleanup();
	return 0;
}
