// Input control module: one background thread that polls the ADC-based inputs.
//
// It owns the ADC (calls Adc_init/Adc_cleanup), since the ADC must only be used
// from one thread. Right now it reads the joystick: up/down changes the volume
// by 5, once per press, repeating while held. The accelerometer goes here too.
//
// Call init after AudioMixer_init() and DrumBeat_init(), and cleanup before them.

#ifndef INPUT_CONTROL_H
#define INPUT_CONTROL_H

void InputControl_init(void);
void InputControl_cleanup(void);

#endif
