// Reporter module: prints one status line per second from a background thread.
//
// The line shows the beat mode, tempo, and volume, plus timing statistics for
// refilling the audio buffer and sampling the accelerometer, for example:
// M1 120bpm vol:80   Audio[16.283, 16.942] avg 16.667/61     Accel[...] avg .../100
//
// Call init after Period_init(), AudioMixer_init(), and DrumBeat_init(), and
// cleanup before them.

#ifndef REPORTER_H
#define REPORTER_H

// Start the reporting thread.
void Reporter_init(void);

// Stop the reporting thread and wait for it to finish.
void Reporter_cleanup(void);

#endif
