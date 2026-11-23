// Tempo control module: reads the rotary encoder in a background thread.
//
// Each clockwise click raises the tempo by 5 BPM and each counter-clockwise
// click lowers it by 5 BPM (drumBeat keeps it between 40 and 300 BPM).
// Each press of the knob moves to the next beat mode (rock, custom, then none).
//
// Call init after DrumBeat_init(), and cleanup before DrumBeat_cleanup().

#ifndef TEMPO_CONTROL_H
#define TEMPO_CONTROL_H

// Start the encoder and the background thread.
void TempoControl_init(void);

// Stop the thread and release the encoder.
void TempoControl_cleanup(void);

#endif
