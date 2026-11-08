// Reporter module: once per second (in a background thread), moves the
// sampler's current data into its history, counts the light dips in that
// history, prints stats and sampling info, and records the result for
// other modules to read.

#ifndef REPORTER_H
#define REPORTER_H

// Begin/end the background thread that analyzes each second of samples.
// Sampler_init() and Period_init() must be called first.
void Reporter_init(void);
void Reporter_cleanup(void);

// Get the number of dips found in the previous complete second.
int Reporter_getDipCount(void);

#endif
