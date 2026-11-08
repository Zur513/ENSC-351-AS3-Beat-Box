// ADC module: reads voltages from the MCP3208 12-bit A/D converter over SPI
// (/dev/spidev0.0) It isn't thread-safe, so we can only use it from one thread

#ifndef ADC_H
#define ADC_H

// Open and configure the SPI device. Must be called before any reads.
void Adc_init(void);

// Read one single-ended channel (0 to 7) and return its voltage (0 to 3.3V).
double Adc_readVoltage(int channel);

// Close the SPI device.
void Adc_cleanup(void);

#endif
