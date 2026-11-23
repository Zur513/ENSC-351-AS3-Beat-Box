// Input control module: one background thread that polls the ADC-based inputs
// every 10 ms.
//
// It owns the ADC (calls Adc_init/Adc_cleanup), since the ADC must only be used
// from one thread. Joystick up/down changes the volume by 5, once per press,
// repeating while held. A sharp move along one of the accelerometer's axes
// ("air drumming") plays that axis's drum sound.
//
// Call init after AudioMixer_init() and DrumBeat_init(), and cleanup before them.

#ifndef INPUT_CONTROL_H
#define INPUT_CONTROL_H

// Open the ADC and start the polling thread.
void InputControl_init(void);

// Stop the thread and close the ADC.
void InputControl_cleanup(void);

#endif
