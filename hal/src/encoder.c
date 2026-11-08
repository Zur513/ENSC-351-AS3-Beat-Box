// Encoder module: decodes the rotary encoder's quadrature signal from gpiod
// edge events in a background thread.

#include "hal/encoder.h"
#include <assert.h>
#include <dirent.h>
#include <gpiod.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Header pins 29 and 31, found by name so the chip and offsets aren't hard-coded
#define LINE_A_NAME "GPIO6"
#define LINE_B_NAME "GPIO5"
#define NUM_LINES 2
#define CONSUMER_NAME "encoder"
#define GPIO_DEV_DIR "/dev"

// Quadrature states per detent (24 detents per full rotation)
#define STEPS_PER_DETENT 4
// Bits of the quadrature state: state = (A << 1) | B
#define STATE_BIT_A 0x2
#define STATE_BIT_B 0x1
// How long the thread waits for an edge before checking whether to stop
#define EVENT_TIMEOUT_NS 100000000LL
#define EVENT_BUFFER_SIZE 16

static struct gpiod_chip *pChip = NULL;
static struct gpiod_line_request *pRequest = NULL;
static struct gpiod_edge_event_buffer *pEventBuffer = NULL;
static unsigned int offsetA;
static unsigned int offsetB;

static pthread_t encoderThread;
static bool isRunning = false;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int position = 0;

// Decoder state, only touched by the encoder thread
static int prevState = 0;
static int steps = 0;

// Index = (previous state << 2) | new state, where state = (A << 1) | B.
// A valid move gives +1 (clockwise) or -1 (counter-clockwise). No change or
// a skipped state (a bounce, or a missed edge) gives 0.
// If the knob counts backwards, swap LINE_A_NAME and LINE_B_NAME.
static const int8_t STEP_TABLE[16] = {
	 0, +1, -1,  0,
	-1,  0,  0, +1,
	+1,  0,  0, -1,
	 0, -1, +1,  0,
};

static bool isThreadRunning(void);
static void *encoderThreadFn(void *pArg);
static void handleEdge(struct gpiod_edge_event *pEvent);
static void addToPosition(int detents);
static void openChipWithLines(void);
static bool tryChip(const char *pPath);
static void requestLines(void);

static void *encoderThreadFn(void *pArg)
{
	(void)pArg;
	int a = gpiod_line_request_get_value(pRequest, offsetA) == GPIOD_LINE_VALUE_ACTIVE;
	int b = gpiod_line_request_get_value(pRequest, offsetB) == GPIOD_LINE_VALUE_ACTIVE;
	prevState = (a ? STATE_BIT_A : 0) | (b ? STATE_BIT_B : 0);
	steps = 0;

	while (isThreadRunning()) {
		// Returns 0 on a timeout, so the loop can check isThreadRunning()
		int result = gpiod_line_request_wait_edge_events(pRequest, EVENT_TIMEOUT_NS);
		if (result < 0) {
			perror("encoder: wait for edge events");
			exit(EXIT_FAILURE);
		}
		if (result > 0) {
			int numEvents = gpiod_line_request_read_edge_events(pRequest, pEventBuffer,
			                                                    EVENT_BUFFER_SIZE);
			for (int i = 0; i < numEvents; i++) {
				handleEdge(gpiod_edge_event_buffer_get_event(pEventBuffer, i));
			}
		}
	}
	return NULL;
}

// Update the quadrature state from one edge, and count a detent every
// STEPS_PER_DETENT steps in the same direction
static void handleEdge(struct gpiod_edge_event *pEvent)
{
	assert(pEvent != NULL);
	int bit = (gpiod_edge_event_get_line_offset(pEvent) == offsetA) ? STATE_BIT_A : STATE_BIT_B;
	// Use the edge type for the new value instead of reading the line, since
	// the line may have changed again by now
	bool isRising = gpiod_edge_event_get_event_type(pEvent) == GPIOD_EDGE_EVENT_RISING_EDGE;
	int state = isRising ? (prevState | bit) : (prevState & ~bit);

	steps += STEP_TABLE[(prevState << 2) | state];
	prevState = state;
	if (abs(steps) >= STEPS_PER_DETENT) {
		addToPosition(steps / STEPS_PER_DETENT);
		steps = 0;
	}
}

void Encoder_init(void)
{
	pthread_mutex_lock(&lock);
	if (isRunning) {
		pthread_mutex_unlock(&lock);
		fprintf(stderr, "The encoder thread is already running.\n");
		return;
	}
	isRunning = true;
	position = 0;
	pthread_mutex_unlock(&lock);

	openChipWithLines();
	requestLines();
	pEventBuffer = gpiod_edge_event_buffer_new(EVENT_BUFFER_SIZE);
	if (pEventBuffer == NULL) {
		perror("encoder: edge event buffer");
		exit(EXIT_FAILURE);
	}

	if (pthread_create(&encoderThread, NULL, encoderThreadFn, NULL) != 0) {
		fprintf(stderr, "Failed to create encoder thread!\n");
		exit(EXIT_FAILURE);
	}
}

void Encoder_cleanup(void)
{
	pthread_mutex_lock(&lock);
	if (!isRunning) {
		pthread_mutex_unlock(&lock);
		return;
	}
	isRunning = false;
	pthread_mutex_unlock(&lock);

	// The thread sees the change in EVENT_TIMEOUT_NS
	pthread_join(encoderThread, NULL);
	gpiod_edge_event_buffer_free(pEventBuffer);
	gpiod_line_request_release(pRequest);
	gpiod_chip_close(pChip);
	pEventBuffer = NULL;
	pRequest = NULL;
	pChip = NULL;
	printf("Encoder thread cleaned up successfully.\n");
}

int Encoder_getPosition(void)
{
	pthread_mutex_lock(&lock);
	int currentPosition = position;
	pthread_mutex_unlock(&lock);
	return currentPosition;
}

static bool isThreadRunning(void)
{
	pthread_mutex_lock(&lock);
	bool shouldRun = isRunning;
	pthread_mutex_unlock(&lock);
	return shouldRun;
}

static void addToPosition(int detents)
{
	pthread_mutex_lock(&lock);
	position += detents;
	pthread_mutex_unlock(&lock);
}

// Search every /dev/gpiochipN for the one with both named lines, and keep it
// open in pChip with the line offsets in offsetA and offsetB
static void openChipWithLines(void)
{
	DIR *pDir = opendir(GPIO_DEV_DIR);
	if (pDir == NULL) {
		perror(GPIO_DEV_DIR);
		exit(EXIT_FAILURE);
	}
	struct dirent *pEntry = readdir(pDir);
	bool isFound = false;
	while (!isFound && pEntry != NULL) {
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", GPIO_DEV_DIR, pEntry->d_name);
		isFound = gpiod_is_gpiochip_device(path) && tryChip(path);
		pEntry = readdir(pDir);
	}
	closedir(pDir);

	if (!isFound) {
		fprintf(stderr, "encoder: no GPIO chip has lines %s and %s (check gpioinfo)\n",
		        LINE_A_NAME, LINE_B_NAME);
		exit(EXIT_FAILURE);
	}
}

// Open the chip at pPath, and keep it if it has both lines
static bool tryChip(const char *pPath)
{
	assert(pPath != NULL);
	struct gpiod_chip *pCandidate = gpiod_chip_open(pPath);
	if (pCandidate == NULL) {
		return false;
	}
	int foundA = gpiod_chip_get_line_offset_from_name(pCandidate, LINE_A_NAME);
	int foundB = gpiod_chip_get_line_offset_from_name(pCandidate, LINE_B_NAME);
	if (foundA < 0 || foundB < 0) {
		gpiod_chip_close(pCandidate);
		return false;
	}
	pChip = pCandidate;
	offsetA = foundA;
	offsetB = foundB;
	return true;
}

// Request both lines as inputs that report rising and falling edges
static void requestLines(void)
{
	assert(pChip != NULL);
	struct gpiod_line_settings *pSettings = gpiod_line_settings_new();
	struct gpiod_line_config *pLineConfig = gpiod_line_config_new();
	struct gpiod_request_config *pRequestConfig = gpiod_request_config_new();
	if (pSettings == NULL || pLineConfig == NULL || pRequestConfig == NULL) {
		perror("encoder: gpiod config");
		exit(EXIT_FAILURE);
	}
	gpiod_line_settings_set_direction(pSettings, GPIOD_LINE_DIRECTION_INPUT);
	gpiod_line_settings_set_edge_detection(pSettings, GPIOD_LINE_EDGE_BOTH);

	const unsigned int offsets[NUM_LINES] = { offsetA, offsetB };
	gpiod_line_config_add_line_settings(pLineConfig, offsets, NUM_LINES, pSettings);
	gpiod_request_config_set_consumer(pRequestConfig, CONSUMER_NAME);

	pRequest = gpiod_chip_request_lines(pChip, pRequestConfig, pLineConfig);
	gpiod_request_config_free(pRequestConfig);
	gpiod_line_config_free(pLineConfig);
	gpiod_line_settings_free(pSettings);
	if (pRequest == NULL) {
		perror("encoder: request lines (run with sudo?)");
		exit(EXIT_FAILURE);
	}
}
