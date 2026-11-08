// Hardware Abstraction Layer for receiving signals from the MCP3208 ADC
// over SPI on the BeagleY-AI board

#include "hal/adc.h"
#include <assert.h>
#include <fcntl.h>
#include <linux/spi/spidev.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define SPI_DEVICE "/dev/spidev0.0"
#define SPI_SPEED_HZ 500000
#define SPI_MSG_LEN 3
#define SPI_BITS_PER_WORD 8
#define NUM_CHANNELS 8
#define ADC_MAX_RAW 4095 // Max ADC reading (at 3.3V)
#define ADC_MAX_VOLTAGE 3.3 // Max ADC voltage (when returning 4095)

// Allow module to check if it has been initialized once
static bool isInitialized = false;
static int spiFd = -1;

double Adc_readVoltage(int channel)
{
	assert(isInitialized);
	assert(channel >= 0 && channel < NUM_CHANNELS);
	uint8_t tx[SPI_MSG_LEN] = {
		0x06 | ((channel >> 2) & 0x01), // start bit, single-ended, Data2
		(channel & 0x03) << 6, // Data1, Data0
		0x00,
	};
	uint8_t rx[SPI_MSG_LEN] = {0};
	struct spi_ioc_transfer transfer = {
		.tx_buf = (uintptr_t)tx,
		.rx_buf = (uintptr_t)rx,
		.len = SPI_MSG_LEN,
		.speed_hz = SPI_SPEED_HZ,
		.bits_per_word = SPI_BITS_PER_WORD,
	};
	if (ioctl(spiFd, SPI_IOC_MESSAGE(1), &transfer) < 0) {
		perror("SPI transfer");
		exit(EXIT_FAILURE);
	}
	// The 12 bit result is the low 4 bits of the second byte, then the third byte
	int rawReading = ((rx[1] & 0x0F) << 8) | rx[2];

	return rawReading * (ADC_MAX_VOLTAGE / ADC_MAX_RAW);
	//printf("Channel %d: raw = %d\n", channel, rawReading); //for debugging
	//return rawReading * (ADC_MAX_VOLTAGE / ADC_MAX_RAW);
}

void Adc_init(void)
{
	assert(!isInitialized);
	spiFd = open(SPI_DEVICE, O_RDWR);
	if (spiFd < 0) {
		perror(SPI_DEVICE);
		exit(EXIT_FAILURE);
	}
	// The MCP3208 uses SPI mode 0 (the clock idles low)
	uint8_t mode = SPI_MODE_0;
	if (ioctl(spiFd, SPI_IOC_WR_MODE, &mode) < 0) {
		perror("SPI set mode error");
		exit(EXIT_FAILURE);
	}
	isInitialized = true;
}

void Adc_cleanup(void)
{
	assert(isInitialized);
	close(spiFd);
	spiFd = -1;
	isInitialized = false;
}
