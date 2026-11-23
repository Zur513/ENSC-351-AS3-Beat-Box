// Joystick module: turns the Y-axis voltage from the ADC into up/down/none.
// Adapted from my AS1 joystick module, but reads through adc.c instead of
// opening the SPI device itself, so only one module owns /dev/spidev0.0.

#include "hal/joystick.h"
#include "hal/adc.h"

// CH0 is the Y axis: high voltage = up, low voltage = down (centre is ~1.65 V)
#define Y_CHANNEL 0

// From the AS1 raw thresholds (3500 and 600 out of 4095 at 3.3 V).
// The gap around the centre ignores small wobbles of the stick.
#define UP_THRESHOLD_V   2.82
#define DOWN_THRESHOLD_V 0.48

Joystick_direction Joystick_getDirection(void)
{
	double y = Adc_readVoltage(Y_CHANNEL);
	if (y > UP_THRESHOLD_V) {
		return JOYSTICK_UP;
	}
	if (y < DOWN_THRESHOLD_V) {
		return JOYSTICK_DOWN;
	}
	return JOYSTICK_NONE;
}
