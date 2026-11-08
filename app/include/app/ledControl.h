// LED control module: reads the rotary encoder in a background thread and
// flashes the LED emitter at the frequency it selects.
//
// The LED starts at 10 Hz. Each clockwise click adds 1 Hz and each
// counter-clockwise click subtracts 1 Hz, between 0 Hz (off) and 500 Hz.
// Frequencies below the PWM hardware's minimum are flashed in software.

#ifndef LED_CONTROL_H
#define LED_CONTROL_H

// Start the encoder, the PWM output, and the background thread.
void LedControl_init(void);

// Stop the thread, turn the LED off, and release the encoder and PWM.
void LedControl_cleanup(void);

// Return the current flash frequency in Hz (0 means off). Safe to call from
// any thread.
int LedControl_getFrequencyHz(void);

#endif
