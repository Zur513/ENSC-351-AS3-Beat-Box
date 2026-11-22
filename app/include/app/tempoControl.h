// Tempo control module: reads the rotary encoder in a background thread and
// changes the tempo to what the encoder selects.
//
// The tempo starts at 120 BPM. Each clockwise click increases by 5 BPM and each
// counter-clockwise click subtracts 5 BPM, between 40 BPM (off) and 300 BPM.

#ifndef TEMPO_CONTROL_H
#define TEMPO_CONTROL_H

// Start the encoder and the background thread.
void TempoControl_init(void);

// Stop the thread and release the encoder
void TempoControl_cleanup(void);

#endif
