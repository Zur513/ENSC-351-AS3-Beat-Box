// PWM module: flashes the LED emitter with the hardware PWM on header pin 32
// (GPIO12). It uses the files in /dev/hat/pwm/GPIO12, which only exist after
// running `sudo beagle-pwm-export --pin hat-32`.
// Only use it from one thread.

#ifndef PWM_H
#define PWM_H

#include <stdbool.h>

// Lowest frequency the hardware flashes at reliably. Periods longer than about
// 400 ms are rejected (even though the documentation gives a lower limit of
// 0.233 Hz), so slower flashing has to be done in software with Pwm_setLevel().
#define PWM_MIN_FREQUENCY_HZ 3

// Enable the PWM output, starting with the LED off.
// Exits the program if the PWM files can't be written.
void Pwm_init(void);

// Flash at the given frequency in hardware: 0 (off), or PWM_MIN_FREQUENCY_HZ
// and above. Setting the current frequency again does nothing, so the
// flashing isn't interrupted.
void Pwm_setFrequency(int frequencyHz);

// Hold the output on or off. For flashing slower than PWM_MIN_FREQUENCY_HZ,
// call this from a timing loop.
void Pwm_setLevel(bool isOn);

// Turn the output off and disable it.
void Pwm_cleanup(void);

#endif
