// Light dip detector: samples a light sensor, flashes an LED, and serves stats over UDP

#include "app/sampler.h"
#include "app/periodTimer.h"
#include "app/reporter.h"
#include "app/udpServer.h"
#include "app/shutdown.h"
#include "app/ledControl.h"

int main(void)
{
	// Start the modules (each one except the period timer runs its own thread)
	Period_init();
	Sampler_init();
	Reporter_init();
	UdpServer_init();
	LedControl_init();

	// Sleep here until the UDP "stop" command asks the program to end
	Shutdown_waitForRequest();

	// Stop in roughly the reverse order, so no module is still using one
	// that has already stopped
	LedControl_cleanup();
	UdpServer_cleanup();
	Reporter_cleanup();
	Sampler_cleanup();
	Period_cleanup();
	return 0;
}
