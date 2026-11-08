// shutdown.h
// The shutdown module: it lets any thread ask the program to end, and lets
// main block until that happens.

#ifndef SHUTDOWN_H
#define SHUTDOWN_H

// Ask the program to shut down. This is safe to call from any thread, and
// safe to call more than once.
void Shutdown_request(void);

// Block the calling thread until Shutdown_request() is called.
void Shutdown_waitForRequest(void);

#endif
