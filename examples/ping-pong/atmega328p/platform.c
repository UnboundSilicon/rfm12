/*
 * -------------------------------------------------------------------------
 * platform.c
 *
 * ATmega328P platform implementation for the RFM12B PING/PONG example.
 * -------------------------------------------------------------------------
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>
#include <util/delay.h>

#include "platform.h"


#define BUTTON_DEBOUNCE_MS      25U
#define UART_BAUD               115200UL


static volatile bool button_pressed;
static volatile bool button_press_event;
static uint8_t debounce_count;


static int uart_putchar(char c, FILE *stream)
{
    (void)stream;

    if(c == '\n')
    {
        uart_putchar('\r', stream);
    }

    while((UCSR0A & (1U << UDRE0)) == 0U)
    {
    }

    UDR0 = (uint8_t)c;

    return 0;
}


static FILE uart_stdout =
    FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);


static void uart_initialize(void)
{
    /*
     * 115200 baud at 16 MHz.
     */
    UBRR0 = 8U;

    UCSR0B = (1U << TXEN0);

    UCSR0C =
        (1U << UCSZ01) |
        (1U << UCSZ00);

    stdout = &uart_stdout;
}


static void spi_initialize(void)
{
    /*
     * PB2 - RFM12 chip select
     * PB3 - MOSI
     * PB4 - MISO
     * PB5 - SCK
     */
    DDRB |=
        (1U << PB2) |
        (1U << PB3) |
        (1U << PB5);

    DDRB &= ~(1U << PB4);

    /*
     * RFM12 deselected.
     */
    PORTB |= (1U << PB2);

    /*
     * SPI master, mode 0, MSB first.
     *
     * 16 MHz / 8 = 2 MHz SPI clock.
     */
    SPCR =
        (1U << SPE) |
        (1U << MSTR) |
        (1U << SPR0);

    SPSR = (1U << SPI2X);
}


static void button_initialize(void)
{
    /*
     * PD2 input with internal pull-up.
     * Button connects PD2 to ground when pressed.
     */
    DDRD &= ~(1U << PD2);
    PORTD |= (1U << PD2);

    button_pressed = ((PIND & (1U << PD2)) == 0U);
    button_press_event = false;
    debounce_count = 0U;

    /*
     * Timer2 CTC mode.
     *
     * 16 MHz / 64 = 250 kHz
     * 250 counts = 1 ms
     */
    TCCR2A = (1U << WGM21);
    TCCR2B = (1U << CS22);
    OCR2A = 249U;
    TIMSK2 = (1U << OCIE2A);
}


void platform_initialize(void)
{
    cli();

    uart_initialize();
    spi_initialize();
    button_initialize();
}


void platform_start_interrupts(void)
{
    sei();
}


void platform_delay_ms(uint16_t milliseconds)
{
    while(milliseconds > 0U)
    {
        _delay_ms(1.0);
        milliseconds--;
    }
}


bool platform_button_pressed(void)
{
    bool event;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        event = button_press_event;
        button_press_event = false;
    }

    return event;
}


RFM12_result_t platform_rfm12_spi_transfer(
    void *context,
    uint16_t tx,
    uint16_t *rx)
{
    (void)context;

    if(rx == NULL)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    /*
     * Select RFM12.
     */
    PORTB &= ~(1U << PB2);

    /*
     * Transfer high byte.
     */
    SPDR = (uint8_t)(tx >> 8U);

    while((SPSR & (1U << SPIF)) == 0U)
    {
    }

    *rx = (uint16_t)SPDR << 8U;

    /*
     * Transfer low byte.
     */
    SPDR = (uint8_t)tx;

    while((SPSR & (1U << SPIF)) == 0U)
    {
    }

    *rx |= (uint16_t)SPDR;

    /*
     * Deselect RFM12.
     */
    PORTB |= (1U << PB2);

    return RFM12_OK;
}


ISR(TIMER2_COMPA_vect)
{
    const bool raw_pressed =
        ((PIND & (1U << PD2)) == 0U);

    if(raw_pressed == button_pressed)
    {
        debounce_count = 0U;
        return;
    }

    if(debounce_count < BUTTON_DEBOUNCE_MS)
    {
        debounce_count++;
    }

    if(debounce_count == BUTTON_DEBOUNCE_MS)
    {
        button_pressed = raw_pressed;
        debounce_count = 0U;

        if(button_pressed)
        {
            button_press_event = true;
        }
    }
}