// UDP server module: listens on port 12345 in a background thread and replies
// to text commands from the Node.js server. Each reply is one packet.
// mode N:   set the beat (0 none, 1 rock, 2 custom) and reply with the mode
// volume N: set the volume (0 - 100) and reply with the volume
// tempo N:  set the tempo (40 - 300 BPM) and reply with the tempo
// play N:   play one drum sound (0 base, 1 hi-hat, 2 snare)
// stop:     calls Shutdown_request()
// Sending a command without a number only queries the current value.

#ifndef UDP_SERVER_H
#define UDP_SERVER_H

// Open the socket and start listening thread
void UdpServer_init(void);

// Join the listening thread and close the socket.
// Only call this after a shutdown has been requested!
void UdpServer_cleanup(void);

#endif
