// Timing module: helpers for delaying the calling thread.

#ifndef TIMING_H
#define TIMING_H

#define MS_PER_SECOND 1000

// Block the calling thread for `delayInMs` milliseconds.
void Timing_sleepForMs(long long delayInMs);

// Block the calling thread for `delayInNs` nanoseconds.
void Timing_sleepForNs(long long delayInNs);

#endif
