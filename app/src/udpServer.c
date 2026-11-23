// UDP server module: listens on port 12345 in a background thread and replies
// to text commands

#include "app/udpServer.h"
#include "app/shutdown.h"
#include "app/audioMixer.h"
#include "app/drumBeat.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_PACKET_SIZE 1500
#define UDP_PORT 12345

#define MAX_REPLY_SIZE 1500

// Longest command word read from a packet (longer words are cut off, and so
// won't match any command)
#define MAX_COMMAND_LENGTH 15
// Turns MAX_COMMAND_LENGTH into the text "15" so it can go in sscanf's format
#define STRINGIFY(x) #x
#define TO_STRING(x) STRINGIFY(x)

static pthread_t udpServerThread;
static bool isRunning = false;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static int socketFd = -1;

static void *udpServerThreadFn(void *pArg);
static bool isThreadRunning(void);

static bool handleCommand(const char *pCommand, const struct sockaddr_in *pSender);
static void handleHelp(const struct sockaddr_in *pSender);
static void handleTempo(bool hasValue, int value, const struct sockaddr_in *pSender);
static void handleMode(bool hasValue, int value, const struct sockaddr_in *pSender);
static void handleVolume(bool hasValue, int value, const struct sockaddr_in *pSender);
static void handlePlay(bool hasValue, int value, const struct sockaddr_in *pSender);
static void handleStop(const struct sockaddr_in *pSender);

static void sendReply(const char *pMessage, const struct sockaddr_in *pSender);

static const char HELP_MESSAGE[] =
	"Accepted command examples:\n"
	"help -- show this message.\n"
	"? -- show this message.\n"
	"tempo n -- set tempo to n BPM.\n"
	"mode n -- set mode to n.\n"
	"volume n -- set volume to n.\n"
	"play n -- play sound n.\n"
	"stop -- cause the server program to end.\n"
	"<enter> -- repeat last command.\n";

static void *udpServerThreadFn(void *pArg)
{
	(void)pArg;
	bool isStopRequested = false;
	char buffer[MAX_PACKET_SIZE + 1];
	char lastCommand[MAX_PACKET_SIZE + 1] = "";
	while (!isStopRequested) {
		struct sockaddr_in sender;
		socklen_t senderLength = sizeof(sender);
		// Blocks until a packet arrives
		ssize_t bytesRead = recvfrom(socketFd,
									 buffer,
									 MAX_PACKET_SIZE,
									 0,
									 (struct sockaddr *)&sender,
									 &senderLength);
		// recvfrom() returns 0 once UdpServer_cleanup() shuts the socket down,
		// but an empty packet from a client also returns 0. So only stop if
		// cleanup asked us to (or on a real error), and ignore empty packets.
		if (bytesRead <= 0) {
			if (!isThreadRunning()) {
				isStopRequested = true;
			} else if (bytesRead < 0) {
				perror("UDP recvfrom");
				isStopRequested = true;
			}
		} else {
			buffer[bytesRead] = '\0';
			// we have to strip the newline netcat adds
			buffer[strcspn(buffer, "\r\n")] = '\0';
			if (buffer[0] == '\0') {
				// we will never encounter a repeated call to stop, so we can
				// disregard the output.
				(void)handleCommand(lastCommand, &sender);
			} else {
				isStopRequested = handleCommand(buffer, &sender);
				// These buffers are the same size, no overflow worries.
				strcpy(lastCommand, buffer);
			}
		}
	}
	return NULL;
}

void UdpServer_init(void)
{
	pthread_mutex_lock(&lock);
	if (isRunning) {
		pthread_mutex_unlock(&lock);
		fprintf(stderr, "The udpServer thread is already running.\n");
		return;
	}
	isRunning = true;
	pthread_mutex_unlock(&lock);

	socketFd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketFd < 0) {
		perror("UDP socket");
		exit(EXIT_FAILURE);
	}
	struct sockaddr_in address = {0};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_ANY);
	address.sin_port = htons(UDP_PORT);
	if (bind(socketFd, (struct sockaddr *)&address, sizeof(address)) < 0) {
		perror("UDP bind");
		exit(EXIT_FAILURE);
	}

	int result = pthread_create(&udpServerThread, NULL, udpServerThreadFn, NULL);
	if (result != 0) {
		fprintf(stderr, "Failed to create udpServer thread!\n");
		close(socketFd);
		pthread_mutex_lock(&lock);
		isRunning = false;
		pthread_mutex_unlock(&lock);
	}
}

void UdpServer_cleanup(void)
{
	// Signal the udpServer thread to stop
	pthread_mutex_lock(&lock);
	if (!isRunning) {
		pthread_mutex_unlock(&lock);
		return; // Thread wasn't running
	}
	isRunning = false;
	pthread_mutex_unlock(&lock);

	// The thread is usually blocked in recvfrom(), so shutting the socket
	// down is what makes it return
	shutdown(socketFd, SHUT_RDWR);
	pthread_join(udpServerThread, NULL);
	close(socketFd);
	printf("UdpServer thread cleaned up successfully.\n");
}

static bool isThreadRunning(void)
{
	pthread_mutex_lock(&lock);
	bool shouldRun = isRunning;
	pthread_mutex_unlock(&lock);
	return shouldRun;
}

static bool handleCommand(const char *pCommand, const struct sockaddr_in *pSender)
{
	assert(pCommand != NULL && pSender != NULL);
	bool isStop = false;

	// Rip apart the command
	char word[MAX_COMMAND_LENGTH + 1] = "";
	int value = 0;
	int numParsed = sscanf(pCommand, "%" TO_STRING(MAX_COMMAND_LENGTH) "s %d",
	                       word, &value);

	if (strcmp(word, "help") == 0 || strcmp(word, "?") == 0) {
		handleHelp(pSender);
	} else if (strcmp(word, "stop") == 0) {
		handleStop(pSender);
		isStop = true;
	} else if (strcmp(word, "tempo") == 0) {
		handleTempo(numParsed == 2, value, pSender);
	} else if (strcmp(word, "mode") == 0) {
		handleMode(numParsed == 2, value, pSender);
	} else if (strcmp(word, "volume") == 0) {
		handleVolume(numParsed == 2, value, pSender);
	} else if (strcmp(word, "play") == 0) {
		handlePlay(numParsed == 2, value, pSender);
	} else {
		sendReply("Unknown command. Type 'help' for the list of commands.\n", pSender);
	}
	return isStop;
}

static void handleHelp(const struct sockaddr_in *pSender)
{
	sendReply(HELP_MESSAGE, pSender);
}

static void handleTempo(bool hasValue, int value, const struct sockaddr_in *pSender)
{
	if (hasValue) {
		DrumBeat_setTempo(value);
	}
	char reply[MAX_REPLY_SIZE];
	snprintf(reply, sizeof(reply), "%d\n", DrumBeat_getTempo());
	sendReply(reply, pSender);
}

static void handleMode(bool hasValue, int value, const struct sockaddr_in *pSender)
{
	if (hasValue && (value >= 0) && (value < DRUMBEAT_NUM_MODES)) {
		DrumBeat_setMode(value);
	}
	char reply[MAX_REPLY_SIZE];
	snprintf(reply, sizeof(reply), "%d\n", DrumBeat_getMode());
	sendReply(reply, pSender);
}

static void handleVolume(bool hasValue, int value, const struct sockaddr_in *pSender)
{
	if (hasValue && (value <= AUDIOMIXER_MAX_VOLUME)) {
		AudioMixer_setVolume(value);
	}
	char reply[MAX_REPLY_SIZE];
	snprintf(reply, sizeof(reply), "%d\n", AudioMixer_getVolume());
	sendReply(reply, pSender);
}

static void handlePlay(bool hasValue, int value, const struct sockaddr_in *pSender)
{
	char reply[MAX_REPLY_SIZE];
	if (hasValue && (value >= 0) && (value < DRUMBEAT_NUM_SOUNDS)) {
		DrumBeat_playSound(value);
		snprintf(reply, sizeof(reply), "Playing %d\n", value);
	} else if (hasValue) {
		snprintf(reply, sizeof(reply), "%d is unknown!\n", value);
	} else {
		snprintf(reply, sizeof(reply), "Usage: play N (0 - 2)\n");
	}
	sendReply(reply, pSender);
}

static void handleStop(const struct sockaddr_in *pSender)
{
	sendReply("Program terminating.\n", pSender);
	Shutdown_request();
}

static void sendReply(const char *pMessage, const struct sockaddr_in *pSender)
{
	sendto(socketFd,
		   pMessage,
		   strlen(pMessage),
		   0,
		   (const struct sockaddr *)pSender,
		   sizeof(*pSender));
}
