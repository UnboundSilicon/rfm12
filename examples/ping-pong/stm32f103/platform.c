/*
 * -------------------------------------------------------------------------
 * platform.c
 *
 * STM32F103 Blue Pill platform implementation for the
 * RFM12B PING/PONG example.
 * -------------------------------------------------------------------------
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "stm32f1xx.h"

#include "platform.h"


#define BUTTON_DEBOUNCE_MS      25U
#define UART_BAUD               115200UL


static volatile bool button_pressed;
static volatile bool button_press_event;
static uint8_t debounce_count;


/*
 * -------------------------------------------------------------------------
 * Clock
 *
 * Blue Pill 8 MHz HSE -> PLL x9 -> 72 MHz.
 * APB1 = 36 MHz
 * APB2 = 72 MHz
 * -------------------------------------------------------------------------
 */

static void clock_initialize(void)
{
    RCC->CR |= RCC_CR_HSION;

    while((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }

    RCC->CFGR =
        (RCC->CFGR & ~RCC_CFGR_SW) |
        RCC_CFGR_SW_HSI;

    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
    {
    }

    RCC->CR &= ~RCC_CR_PLLON;

    while((RCC->CR & RCC_CR_PLLRDY) != 0U)
    {
    }

    RCC->CR &= ~(RCC_CR_HSEON | RCC_CR_HSEBYP);
    RCC->CR |= RCC_CR_HSEON;

    while((RCC->CR & RCC_CR_HSERDY) == 0U)
    {
    }

    FLASH->ACR =
        FLASH_ACR_PRFTBE |
        FLASH_ACR_LATENCY_2;

    RCC->CFGR =
        (RCC->CFGR &
         ~(RCC_CFGR_PLLSRC |
           RCC_CFGR_PLLXTPRE |
           RCC_CFGR_PLLMULL |
           RCC_CFGR_HPRE |
           RCC_CFGR_PPRE1 |
           RCC_CFGR_PPRE2)) |
        RCC_CFGR_PLLSRC |
        RCC_CFGR_PLLMULL9 |
        RCC_CFGR_HPRE_DIV1 |
        RCC_CFGR_PPRE1_DIV2 |
        RCC_CFGR_PPRE2_DIV1;

    RCC->CR |= RCC_CR_PLLON;

    while((RCC->CR & RCC_CR_PLLRDY) == 0U)
    {
    }

    RCC->CFGR =
        (RCC->CFGR & ~RCC_CFGR_SW) |
        RCC_CFGR_SW_PLL;

    while((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
    {
    }

    SystemCoreClockUpdate();
}


/*
 * -------------------------------------------------------------------------
 * USART1
 *
 * PA9 TX
 * 115200 baud
 * -------------------------------------------------------------------------
 */

static void uart_initialize(void)
{
    /*
     * Enable GPIOA and USART1 clocks.
     */
    RCC->APB2ENR |=
        RCC_APB2ENR_IOPAEN |
        RCC_APB2ENR_USART1EN;

    (void)RCC->APB2ENR;

    /*
     * PA9 = alternate-function push-pull, 50 MHz.
     *
     * PA9 occupies bits 7:4 of GPIOA->CRH.
     */
    GPIOA->CRH =
        (GPIOA->CRH & ~(0xFUL << 4U)) |
        (0xBUL << 4U);

    USART1->CR1 = 0U;
    USART1->CR2 = 0U;
    USART1->CR3 = 0U;

    /*
     * USART1 is on APB2 at 72 MHz.
     */
    USART1->BRR =
        (SystemCoreClock + (UART_BAUD / 2U)) /
        UART_BAUD;

    USART1->CR1 =
        USART_CR1_TE |
        USART_CR1_UE;
}


static void uart_putchar(uint8_t data)
{
    while((USART1->SR & USART_SR_TXE) == 0U)
    {
    }

    USART1->DR = data;
}


/*
 * newlib stdout implementation used by printf().
 */
int _write(int file, char *data, int length)
{
    int index;

    (void)file;

    for(index = 0; index < length; index++)
    {
        if(data[index] == '\n')
        {
            uart_putchar((uint8_t)'\r');
        }

        uart_putchar((uint8_t)data[index]);
    }

    return length;
}


/*
 * -------------------------------------------------------------------------
 * SPI1
 *
 * PA4 - RFM12 chip select
 * PA5 - SCK
 * PA6 - MISO
 * PA7 - MOSI
 *
 * SPI1 clock = PCLK2 / 32 = 2.25 MHz.
 * -------------------------------------------------------------------------
 */

static void spi_initialize(void)
{
    /*
     * Enable GPIOA and SPI1 clocks.
     */
    RCC->APB2ENR |=
        RCC_APB2ENR_IOPAEN |
        RCC_APB2ENR_SPI1EN;

    (void)RCC->APB2ENR;

    /*
     * PA4 = general-purpose push-pull output.
     * PA5 = alternate-function push-pull output.
     * PA6 = floating input.
     * PA7 = alternate-function push-pull output.
     */
    GPIOA->CRL =
        (GPIOA->CRL &
         ~((0xFUL << 16U) |
           (0xFUL << 20U) |
           (0xFUL << 24U) |
           (0xFUL << 28U))) |
        (0x3UL << 16U) |
        (0xBUL << 20U) |
        (0x4UL << 24U) |
        (0xBUL << 28U);

    /*
     * RFM12 deselected.
     */
    GPIOA->BSRR = (1UL << 4U);

    /*
     * SPI master, mode 0, MSB first, software NSS.
     *
     * BR = 100 -> PCLK2 / 32.
     */
    SPI1->CR1 =
        SPI_CR1_MSTR |
        SPI_CR1_BR_2 |
        SPI_CR1_SSM |
        SPI_CR1_SSI;

    SPI1->CR2 = 0U;

    SPI1->CR1 |= SPI_CR1_SPE;
}


static uint8_t spi_transfer8(uint8_t data)
{
    while((SPI1->SR & SPI_SR_TXE) == 0U)
    {
    }

    *((volatile uint8_t *)&SPI1->DR) = data;

    while((SPI1->SR & SPI_SR_RXNE) == 0U)
    {
    }

    return *((volatile uint8_t *)&SPI1->DR);
}


/*
 * -------------------------------------------------------------------------
 * Button / SysTick
 *
 * PB1 input with pull-up.
 * Button is active low.
 *
 * SysTick generates one interrupt per millisecond.
 * -------------------------------------------------------------------------
 */

static void button_initialize(void)
{
    uint32_t reload;

    /*
     * Enable GPIOB.
     */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN;

    (void)RCC->APB2ENR;

    /*
     * PB1 = input with pull-up/pull-down.
     *
     * CNF=10, MODE=00 -> 0x8.
     */
    GPIOB->CRL =
        (GPIOB->CRL & ~(0xFUL << 4U)) |
        (0x8UL << 4U);

    /*
     * ODR=1 selects the pull-up.
     */
    GPIOB->BSRR = (1UL << 1U);

    button_pressed =
        ((GPIOB->IDR & (1UL << 1U)) == 0U);

    button_press_event = false;
    debounce_count = 0U;

    /*
     * Configure SysTick for 1 ms.
     */
    reload = SystemCoreClock / 1000UL;

    SysTick->CTRL = 0U;
    SysTick->LOAD = reload - 1U;
    SysTick->VAL = 0U;

    SysTick->CTRL =
        SysTick_CTRL_CLKSOURCE_Msk |
        SysTick_CTRL_TICKINT_Msk |
        SysTick_CTRL_ENABLE_Msk;
}


/*
 * -------------------------------------------------------------------------
 * Public platform interface
 * -------------------------------------------------------------------------
 */

void platform_initialize(void)
{
    __disable_irq();

    clock_initialize();
    uart_initialize();
    spi_initialize();
    button_initialize();
}


void platform_start_interrupts(void)
{
    __enable_irq();
}


void platform_delay_ms(uint16_t milliseconds)
{
    if(milliseconds == 0U)
    {
        return;
    }

    /*
     * COUNTFLAG operates even while interrupts are disabled, allowing
     * this function to be used during application initialization.
     */
    SysTick->VAL = 0U;

    while(milliseconds > 0U)
    {
        while((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0U)
        {
        }

        milliseconds--;
    }
}


bool platform_button_pressed(void)
{
    bool event;

    __disable_irq();

    event = button_press_event;
    button_press_event = false;

    __enable_irq();

    return event;
}


RFM12_result_t platform_rfm12_spi_transfer(
    void *context,
    uint16_t tx,
    uint16_t *rx)
{
    uint16_t received;

    (void)context;

    if(rx == NULL)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    /*
     * Select RFM12.
     */
    GPIOA->BRR = (1UL << 4U);

    received =
        (uint16_t)spi_transfer8((uint8_t)(tx >> 8U))
        << 8U;

    received |=
        (uint16_t)spi_transfer8((uint8_t)tx);

    /*
     * Wait for the final bit to leave the SPI peripheral before
     * releasing chip select.
     */
    while((SPI1->SR & SPI_SR_BSY) != 0U)
    {
    }

    /*
     * Deselect RFM12.
     */
    GPIOA->BSRR = (1UL << 4U);

    *rx = received;

    return RFM12_OK;
}


/*
 * -------------------------------------------------------------------------
 * SysTick interrupt
 *
 * Sample the button once per millisecond and generate one event after
 * the input has remained in the new state for 25 ms.
 * -------------------------------------------------------------------------
 */

void SysTick_Handler(void)
{
    const bool raw_pressed =
        ((GPIOB->IDR & (1UL << 1U)) == 0U);

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