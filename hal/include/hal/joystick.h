// Joystick module: reads the joystick's Y axis through the ADC and reports
// whether it's pushed up, down, or not at all.
// It uses adc.c, so only call it from the thread that owns the ADC.

#ifndef JOYSTICK_H
#define JOYSTICK_H

typedef enum {
	JOYSTICK_NONE,
	JOYSTICK_UP,
	JOYSTICK_DOWN
} Joystick_direction;

// Read the joystick once and return which way it's pushed.
// Adc_init() must be called first.
Joystick_direction Joystick_getDirection(void);

#endif // JOYSTICK_H
