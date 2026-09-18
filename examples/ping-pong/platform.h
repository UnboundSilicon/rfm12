/*
 * -------------------------------------------------------------------------
 * platform.h
 *
 * Platform interface for the RFM12B PING/PONG example
 * -------------------------------------------------------------------------
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdbool.h>
#include <stdint.h>

#include "rfm12.h"


void platform_initialize(void);

void platform_start_interrupts(void);

void platform_delay_ms(uint16_t milliseconds);

bool platform_button_pressed(void);

RFM12_result_t platform_rfm12_spi_transfer(
    void *context,
    uint16_t tx,
    uint16_t *rx);


#endif /* PLATFORM_H */