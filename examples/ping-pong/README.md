# RFM12 PING/PONG Example

## Overview

This example demonstrates basic use of the RFM12 driver by configuring the radio and performing simple transmit and receive operations.

Two radios communicate using four-byte `PING` and `PONG` messages. Pressing the button on either device transmits a `PING`. When the other device receives the message, it transmits a `PONG` response and returns to receive mode.

## Example Targets

The PING/PONG application is hardware-independent and shares the same `main.c` between targets. Platform-specific code provides the MCU initialization, SPI interface, timing, and button handling required by the application.

The example currently includes implementations for:

* **ATmega328P** — tested using an Arduino Nano
* **STM32F103C8** — tested using a Blue Pill

## Platform Interface

`platform.h` defines the small MCU-specific interface required by the shared application. Porting the example to another MCU requires implementing these functions for the target platform:

| Function                        | Purpose                                                                     |
| ------------------------------- | --------------------------------------------------------------------------- |
| `platform_initialize()`         | Initializes the MCU peripherals required by the example.                    |
| `platform_start_interrupts()`   | Enables interrupts after application and radio initialization are complete. |
| `platform_delay_ms()`           | Provides a blocking millisecond delay.                                      |
| `platform_button_pressed()`     | Returns a debounced, one-shot button press event.                           |
| `platform_rfm12_spi_transfer()` | Provides the 16-bit SPI transfer function used by the RFM12 HAL.            |

The platform implementation is also responsible for routing the application's `printf()` output to a serial port.

MCU peripheral configuration, GPIO assignments, and other hardware-specific details remain entirely within the platform implementation.

## How It Works

At startup, the application initializes the platform and obtains an RFM12 instance using `rfm12_get_instance()`. The platform-specific SPI transfer function is then attached to the instance through the RFM12 HAL. The application configures the radio through the driver API, applies the configuration to the radio, and enters receive mode.

The application then continuously checks for two events:

* A button press causes the radio to enter transmit mode and send a four-byte `PING` message. After transmission, the radio returns to receive mode.
* Received bytes are collected from the RFM12 FIFO. When four bytes have been received, the message is compared with `PING` and `PONG`. A received `PING` causes a `PONG` response to be transmitted, while a received `PONG` is reported and the radio remains in receive mode.

The example uses polling for radio receive status while button debouncing is handled by the platform-specific implementation.

### Serial Output

The example reports initialization and PING/PONG activity through a serial port. The platform-specific README files describe the serial interface used by each target.

A device initiating a PING will produce output similar to:

```text
RFM12 PING/PONG example ready
Sending PING
Received PONG
```

The responding device will produce:

```text
RFM12 PING/PONG example ready
Received PING
Sending PONG
```

This example intentionally keeps communication simple and does not implement CRC checking, retries, or a response timeout.

## Radio Configuration

Both radios must use the same RF and modulation settings to communicate. The example configures the RFM12 for:

* 915 MHz frequency band
* 920.0025 MHz carrier frequency
* 4800 bps data rate
* 67 kHz receiver bandwidth
* 45 kHz FSK deviation
* 0 dB transmit power
* 0 dB LNA gain
* -103 dBm RSSI threshold
* Two-byte `0x2D 0xD4` synchronization pattern
* FIFO filling beginning after synchronization
* 8-bit FIFO interrupt threshold

The configuration is built using the RFM12 setter functions and then written to the radio with `rfm12_apply_to_radio()`.

## On-Air Format

The application writes the following byte sequence to the RFM12 transmitter:

```text
AA AA AA | 2D D4 | P I N G | AA AA
AA AA AA | 2D D4 | P O N G | AA AA
```

* **Preamble — `AA AA AA`**
  The alternating bit pattern allows the receiver's clock recovery circuitry to synchronize with the incoming data stream. The RFM12 transmitter may generate additional alternating preamble bits while the transmitter starts.

* **Synchronization pattern — `2D D4`**
  Marks the beginning of the message. The RFM12 is configured for two-byte synchronization, with the receiver FIFO beginning to fill only after the synchronization pattern has been recognized.

* **Message — `PING` or `PONG`**
  The example transmits a fixed four-byte application message. Because the message length is known, no length field or additional framing is required.

* **TX flush — `AA AA`**
  After the four-byte message, the application writes two additional bytes before leaving transmit mode. These bytes provide the transmitter enough time to shift the final payload byte completely over the air before the power amplifier is disabled. The trailing bytes themselves are not part of the application message and are not required to be completely transmitted.

The transmit sequence follows the guidance in the RFM12 datasheet. The datasheet recommends an alternating `0xAA` or `0x55` preamble followed by the synchronization pattern before application data, and requires sufficient time after loading the final data byte for that byte to be completely transmitted before disabling the power amplifier.

## Directory Structure

The application code is shared between all targets. Each target directory contains only the platform-specific implementation and build configuration required for that MCU.

```text id="l2dvzh"
ping-pong/
├── README.md
├── main.c
├── platform.h
├── atmega328p/
│   ├── README.md
│   ├── platform.c
│   └── Makefile
└── stm32f103/
    ├── README.md
    ├── platform.c
    └── Makefile
```

`main.c` contains the platform-independent RFM12 configuration and PING/PONG application logic. `platform.h` defines the small interface required by the application, while each `platform.c` implements that interface using the peripherals of its target MCU.

## Building and Hardware Setup

Build instructions, MCU peripheral configuration, pin assignments, and hardware connections are documented separately for each example target:

* [ATmega328P](atmega328p/README.md) — tested using an Arduino Nano
* [STM32F103C8](stm32f103/README.md) — tested using a Blue Pill
