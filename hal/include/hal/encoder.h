// Encoder module: reads the rotary encoder in a background thread (uses gpiod).
//
// The thread waits for edge events on the encoder's two quadrature lines and
// keeps a running count of detents ("clicks"). Clockwise clicks add one and
// counter-clockwise clicks subtract one. Callers find out how far the knob has
// turned by comparing two readings of Encoder_getPosition().

#ifndef ENCODER_H
#define ENCODER_H

// Find the encoder's GPIO lines, request them, and start the thread.
// Exits the program if the lines can't be found or requested.
void Encoder_init(void);

// Stop the thread and release the GPIO lines.
void Encoder_cleanup(void);

// Return the number of detents turned since Encoder_init() (clockwise minus
// counter-clockwise). It can be negative.
int Encoder_getPosition(void);

#endif
