## Yet Another RFM12 Library?

So... the RFM12 is an old module.

Years ago I purchased a number of these, both the 433MHz and 915MHz versions, for various embedded projects, even going so far as to designing a few prototype PCBs around them. Like many of my hobby projects, they eventually found their way into a parts bin after little more than basic bring-up and proof-of-concept testing.  Let's just say that sometimes I prefer to learn how to use a module more than actually using the module in some type of project.

After recently moving into a new house, I've started planning several wireless projects for home automation, sensors, and other low-power devices, and for most projects, there's no reason why I couldn't use one of these modules.

My original code was written specifically for AVR microcontrollers, and like most existing RFM12 code, it's basically just a collection of functions that transmit 16-bit command words over SPI, hardcoded for a specific MCU. Add to that, the application is expected to understand register layouts, bit fields, and device-specific "magic numbers" in order to configure and use the radio. And then there's the lack of a proper API to abstract the register layout, bitfields, and SPI transactions.  Additionally, I plan on using STM32s and ESPs, in addition to AVRs, which would require a complete rewrite for each MCU.

That's where this project comes in.

The goal of my RFM12 Driver Library is to provide a modern, hardware independent, and well-documented C library that abstracts the device's low-level command interface into a clean, intuitive API. Instead of thinking in terms of register values and bit masks, the application should be able to think in terms of radio configuration, data transmission, and reception.

Details below 👇👇👇

## RFM12 Driver Library

A portable, hardware independent driver library for the HopeRF RFM12/RFM12B ISM-band transceiver modules.

## Goals

Most existing RFM12 code, including [HopeRF's own example code](datasheets/RFM12B_code.pdf), is tightly coupled to a specific microcontroller and consists primarily of functions that write 16-bit "magic number" command values to the SPI bus. This library takes a different approach.

The goal is to provide a clean, maintainable, and portable driver that:

* Separates radio logic from hardware-specific code
* Uses a Hardware Abstraction Layer (HAL) for portability
* Supports AVR, STM32, ESP32, and other microcontrollers
* Eliminates "magic numbers" from application code
* Provides a well-defined API for configuration and operation
* Allows multiple radio instances through a context-based design
* Supports both polling and interrupt-driven operation

## Design Philosophy

At its core, the RFM12 is simply a 16-bit SPI command/response device. Nearly every interaction with the radio consists of transmitting a 16-bit command word while some commands are simultaneously receiving a 16-bit status or data word. 

The application should configure the radio using meaningful functions and data structures, leaving the library responsible for encoding the appropriate bit fields and communicating with the hardware.

This:

```c
rfm12_xfer(0x80D7);
rfm12_xfer(0xA640);
rfm12_xfer(0xC647);
```

becomes this:

```c
rfm12_set_band(radio, RFM12_BAND_433MHZ);
rfm12_set_frequency(radio, 433920000UL);
rfm12_set_bitrate(radio, RFM12_DATA_RATE_9600);
```

The result is code that is much easier to read, easier to maintain, and easier to port to new hardware, while still exposing the full functionality of the RFM12 when needed.


## Key Features

* Portable C implementation
* Hardware abstraction layer
* Configurable SPI interface
* Optional interrupt support
* Register and command abstraction
* Frequency, bitrate, and power configuration helpers
* FIFO management
* Packet transmit and receive support
* Support for multiple radio instances
* Minimal RAM footprint
* No dynamic memory allocation

## HAL-Based Design

The driver depends only on one user-provided function and an arbitrary context pointer:

```
typedef RFM12_result_t (*RFM12_spi_transfer16_fn) (
    void *context, 
    uint16_t  tx_word, 
    uint16_t *rx_word
);

```

This allows the same library source code to run on:

* AVR ATmega devices
* STM32 devices
* ESP32 devices
* RP2040 devices
* Any platform with an SPI peripheral

## Context-Based API

Each radio instance is represented by a context structure.

```
rfm12_t *radio;
```

The context stores:

* HAL callbacks
* Hardware-specific user data
* Current configuration
* Runtime state
* Driver status information

This approach allows multiple RFM12 modules to coexist in the same circuit / application while keeping the API reentrant and portable.

## Example Usage

```

RFM12_t *radio;
RFM12_result_t result;

result = rfm12_get_instance(&radio);

if(result != RFM12_OK)
{
    handle_error(result);
}

result = rfm12_configure_hal(
    radio,
    my_spi_transfer16,
    &my_spi_context
);

if(result != RFM12_OK)
{
    handle_error(result);
}

rfm12_set_frequency_band(radio, RFM12_BAND_433);
rfm12_set_frequency(radio, 433920000UL);
rfm12_set_data_rate(radio, RFM12_DATA_RATE_4800);

result = rfm12_apply_to_radio(radio);

if(result != RFM12_OK)
{
    handle_error(result);
}

```

## Project Status

Work in progress.

Current focus:

* Core HAL architecture
* Command abstraction layer
* Radio initialization
* Transmit/receive framework
* Interrupt-driven operation

## License

BSD License
