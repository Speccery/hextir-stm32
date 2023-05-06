/*
    HEXTIr-SD - Texas Instruments HEX-BUS SD Mass Storage Device
    Copyright Jim Brain and RETRO Innovations, 2017

    This code is a modification of the file from the following project:

    sd2iec - SD/MMC to Commodore serial bus interface/controller
    Copyright (C) 2007-2017  Ingo Korb <ingo@akana.de>

    Inspired by MMC2IEC by Lars Pontoppidan et al.

    FAT filesystem access based on code from ChaN and Jim Brain, see ff.c|h.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; version 2 of the License only.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    spi.c: Low-level SPI routines, AVR version

*/

#include <avr/io.h>
#include "config.h"
#include "spi.h"

#ifdef STM32
#include "main.h"
#include <memory.h>
extern SPI_HandleTypeDef hspi2;

void spi_set_divisor(const uint8_t div) {
	// BUGBUG MISSING
}

void spi_set_speed(spi_speed_t speed) {
	// BUGBUG MISSING
  if (speed == SPI_SPEED_FAST) {
    // spi_set_divisor(SPI_DIVISOR_FAST);
  } else {
    // spi_set_divisor(SPI_DIVISOR_SLOW);
  }
}

void spi_init(spi_speed_t speed) {
	// BUGBUG MISSING
  /* set up SPI I/O pins */
}

/* Simple and braindead, just like the AVR's SPI unit */
static uint8_t spi_exchange_byte(uint8_t output) {
	uint8_t txbuf[4], rxbuf[4];
	txbuf[0] = output;
	HAL_SPI_TransmitReceive(&hspi2, txbuf, rxbuf, 1, 100);
	return rxbuf[0];
}

void spi_tx_byte(uint8_t data) {
  spi_exchange_byte(data);
}

uint8_t spi_rx_byte(void) {
  return spi_exchange_byte(0xff);
}

/**
 * From original sources:
 * write = 0: write data from vdata. Discard returned data!
 * write = 1: send 0xFF bytes. Store returned data to buffer.
 * Thus the meaning of write seems to be inverted, i.e. active zero.
 */
void spi_exchange_block(void *vdata, unsigned int length, uint8_t write) {
	uint8_t rxbuf[16];
	uint8_t buf_ff[16];
	memset(buf_ff, 0xff, sizeof(buf_ff));	// Fill buf_ff with 0xFF
	uint8_t *txbuf  = (uint8_t *)vdata;
	while(length > 0) {
		int block_size = length > 16 ? 16 : length;
		if(!write) {
			HAL_SPI_TransmitReceive(&hspi2, txbuf, rxbuf, block_size, 1000);
			// discard data written to rxbuf
		} else {
			// Notice that below we transmit from buf_ff and store returned data to txbuf i.e. the input buffer.
			// Thus thus is a read operation.
			HAL_SPI_TransmitReceive(&hspi2, buf_ff, txbuf, block_size, 1000);
		}
		txbuf += block_size;
		length -= block_size;
	}
}

/* Receive a data block */
void spi_tx_block(const void *data, unsigned int length) {
  spi_exchange_block((void *)data, length, 0);
}

/* Receive a single byte */
uint8_t spi_rx_byte(void);

/* Receive a data block */
void spi_rx_block(void *data, unsigned int length) {
  spi_exchange_block(data, length, 1);
}


void sdcard_set_ss(uint8_t state) {
	HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, state == 0 ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

#else

/* interrupts disabled, SPI enabled, MSB first, master mode */
/* leading edge rising, sample on leading edge, clock bits cleared */
#define SPCR_VAL 0b01010000

/* set up SPSR+SPCR according to the divisor */
/* compiles to 3-4 instructions */
static inline __attribute__((always_inline)) void spi_set_divisor(const uint8_t div) {
  if (div == 2 || div == 8 || div == 32) {
    SPSR = _BV(SPI2X);
  } else {
    SPSR = 0;
  }

  if (div == 2 || div == 4) {
    SPCR = SPCR_VAL;
  } else if (div == 8 || div == 16) {
    SPCR = SPCR_VAL | _BV(SPR0);
  } else if (div == 32 || div == 64) {
    SPCR = SPCR_VAL | _BV(SPR1);
  } else { // div == 128
    SPCR = SPCR_VAL | _BV(SPR0) | _BV(SPR1);
  }
}

void spi_set_speed(spi_speed_t speed) {
  if (speed == SPI_SPEED_FAST) {
    spi_set_divisor(SPI_DIVISOR_FAST);
  } else {
    spi_set_divisor(SPI_DIVISOR_SLOW);
  }
}

void spi_init(spi_speed_t speed) {
  /* set up SPI I/O pins */
  SPI_PORT = (SPI_PORT & ~SPI_MASK) | SPI_SCK | SPI_SS | SPI_MISO;
  SPI_DDR  = (SPI_DDR  & ~SPI_MASK) | SPI_SCK | SPI_SS | SPI_MOSI;

  /* enable and initialize SPI */
  if (speed == SPI_SPEED_FAST) {
    spi_set_divisor(SPI_DIVISOR_FAST);
  } else {
    spi_set_divisor(SPI_DIVISOR_SLOW);
  }

  /* Clear buffers, just to be sure */
  (void) SPSR;
  (void) SPDR;
}

/* Simple and braindead, just like the AVR's SPI unit */
static uint8_t spi_exchange_byte(uint8_t output) {
  SPDR = output;
  loop_until_bit_is_set(SPSR, SPIF);
  return SPDR;
}

void spi_tx_byte(uint8_t data) {
  spi_exchange_byte(data);
}

uint8_t spi_rx_byte(void) {
  return spi_exchange_byte(0xff);
}

void spi_exchange_block(void *vdata, unsigned int length, uint8_t write) {
  uint8_t *data = (uint8_t*)vdata;

  while (length--) {
    if (!write)
      SPDR = *data;
    else
      SPDR = 0xff;

    loop_until_bit_is_set(SPSR, SPIF);

    if (write)
      *data = SPDR;
    else
      (void) SPDR;
    data++;
  }
}

#endif
