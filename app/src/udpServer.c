// udpServer.c
// UDP server module: listens on port 12345 in a background thread and replies
// to text commands

#include "app/udpServer.h"
#include "app/shutdown.h"
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

static pthread_t udpServerThread;
static bool isRunning = false;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static int socketFd = -1;

static void *udpServerThreadFn(void *pArg);

static bool handleCommand(const char *pCommand, const struct sockaddr_in *pSender);
static void handleHelp(const struct sockaddr_in *pSender);
static void handleStop(const struct sockaddr_in *pSender);

static void sendReply(const char *pMessage, const struct sockaddr_in *pSender);

static const char HELP_MESSAGE[] =
	"Accepted command examples:\n"
	"stop     -- cause the server program to end.\n"
	"<enter>  -- repeat last command.\n";

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
		// recvfrom() returns 0 (or an error) once UdpServer_cleanup() shuts
		// the socket down
		if (bytesRead <= 0) {
			isStopRequested = true;
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

static bool handleCommand(const char *pCommand, const struct sockaddr_in *pSender)
{
	assert(pCommand != NULL && pSender != NULL);
	bool isStop = false;
	if (strcmp(pCommand, "help") == 0 || strcmp(pCommand, "?") == 0) {
		handleHelp(pSender);
	} else if (strcmp(pCommand, "stop") == 0) {
		handleStop(pSender);
		isStop = true;
	} else {
		sendReply("Unknown command. Type 'help' for the list of commands.\n", pSender);
	}
	return isStop;
}

static void handleHelp(const struct sockaddr_in *pSender)
{
	sendReply(HELP_MESSAGE, pSender);
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
