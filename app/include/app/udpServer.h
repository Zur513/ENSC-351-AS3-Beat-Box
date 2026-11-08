// udpServer.h
// UDP server module: listens on port 12345 in a background thread and replies
// to text commands:
// help: Return a brief summary/list of supported commands
// ?: Same as help
// count: Return the total number of light samples taken so far
// length: Return how many samples were captured during the previous second
// dips: Return how many dips were detected during the previous second's samples
// history: Return all the data samples from the previous second
// stop: Calls Shutdown_request()
// blank line: Repeats the last command

#ifndef UDP_SERVER_H
#define UDP_SERVER_H

// Open the socket and start listening thread
void UdpServer_init(void);

// Join the listening thread and close the socket.
// Only call this after a shutdown has been requested!
void UdpServer_cleanup(void);

#endif
