/*
 * -------------------------------------------------------------------------
 * main.c
 *
 * RFM12B simple PING/PONG example
 * -------------------------------------------------------------------------
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "rfm12.h"
#include "platform.h"


#define MESSAGE_LENGTH          4U

#define PREAMBLE_BYTE           0xAAU
#define PREAMBLE_LENGTH         3U

#define SYNC_BYTE_1             0x2DU
#define SYNC_BYTE_2             0xD4U

#define FIFO_INTERRUPT_LEVEL    8U

#define POSTAMBLE_BYTE          0xAAU
#define POSTAMBLE_LENGTH        2U


static const uint8_t ping_message[MESSAGE_LENGTH] = { 'P', 'I', 'N', 'G' };
static const uint8_t pong_message[MESSAGE_LENGTH] = { 'P', 'O', 'N', 'G' };


static void configure_radio(RFM12_t *radio);
static void transmit_message(RFM12_t *radio, const uint8_t *message);
static void transmit_byte(RFM12_t *radio, uint8_t data);
static void process_message(RFM12_t *radio, const uint8_t *message);
static void fatal_rfm12_error(RFM12_result_t radio_result);
static void print_radio_status(RFM12_status_word_t status);

static void configure_radio(RFM12_t *radio)
{
    /*
     * Data paths
     */
    rfm12_set_tx_data_register_enable(radio, RFM12_ENABLE);
    rfm12_set_rx_fifo_enable(radio, RFM12_ENABLE);
    rfm12_set_fifo_enable(radio, RFM12_ENABLE);
    rfm12_set_reset_mode(radio, RFM12_RESET_MODE_NON_SENSITIVE);
    rfm12_set_clock_recovery_mode(radio, RFM12_CLOCK_RECOVERY_AUTO);

    /*
     * RF configuration
     */
    rfm12_set_frequency_band(radio, RFM12_BAND_915);
    rfm12_set_frequency(radio, 920002500UL);
    rfm12_set_xtal_cap(radio, RFM12_XTAL_CAP_13_0PF);

    /*
     * Modulation
     */
    rfm12_set_data_rate(radio, RFM12_DATA_RATE_4800);
    rfm12_set_rx_bandwidth(radio, RFM12_RX_BANDWIDTH_67_KHZ);
    rfm12_set_tx_fsk_deviation(radio, RFM12_TX_FSK_DEVIATION_45KHZ);

    /*
     * TX/RX levels
     */
    rfm12_set_tx_power(radio, RFM12_TX_POWER_0DB);
    rfm12_set_lna_gain(radio, RFM12_LNA_GAIN_0_DB);
    rfm12_set_rssi_threshold(radio, RFM12_RSSI_THRESHOLD_M103_DBM);
    rfm12_set_dqd_threshold(radio, RFM12_DQD_THRESHOLD_5);

    /*
     * RX framing
     *
     * FFIT after 8 bits = one received byte.
     * In two-byte sync mode the sync sequence is 0x2D,0xD4.
     */
    rfm12_set_fifo_interrupt_level(radio, (RFM12_fifo_interrupt_level_t)FIFO_INTERRUPT_LEVEL);
    rfm12_set_sync_pattern_length(radio, RFM12_SYNC_PATTERN_2BYTE);
    rfm12_set_fifo_fill_start(radio, RFM12_FIFO_FILL_AFTER_SYNC);
    rfm12_set_sync_pattern(radio, SYNC_BYTE_2);
}

static void transmit_message(RFM12_t *radio, const uint8_t *message)
{
    uint8_t i;
    RFM12_status_word_t status;

    /*
     * Stop receiving and enable the transmitter.
     */
    rfm12_enter_tx_mode(radio);

    /*
     * Preamble
     */
    for(i = 0U; i < PREAMBLE_LENGTH; i++)
    {
        transmit_byte(radio, PREAMBLE_BYTE);
    }

    /*
     * Sync pattern
     */
    transmit_byte(radio, SYNC_BYTE_1);
    transmit_byte(radio, SYNC_BYTE_2);

    /*
     * Four-byte application message.
     */
    for(i = 0U; i < MESSAGE_LENGTH; i++)
    {
        transmit_byte(radio, message[i]);
    }

    /*
     * Postamble
     */
    for(i = 0U; i < POSTAMBLE_LENGTH; i++)
    {
        transmit_byte(radio, POSTAMBLE_BYTE);
    }

    /*
     * Wait until the final postamble byte has advanced
     * through the TX register before disabling TX.
     */
    for(;;)
    {
        if(rfm12_read_status(radio, &status) != RFM12_OK)
        {
            continue;
        }

        if(rfm12_status_tx_register_ready(status))
        {
            break;
        }
    }

    /*
     * Return immediately to receive mode and wait for
     * a new sync pattern.
     */
    rfm12_enter_rx_mode(radio);
    rfm12_restart_sync_recognition(radio);
}

static void transmit_byte(RFM12_t *radio, uint8_t data)
{
    RFM12_status_word_t status;

    for(;;)
    {
        if(rfm12_read_status(radio, &status) != RFM12_OK)
        {
            continue;
        }

        if(rfm12_status_tx_register_ready(status))
        {
            break;
        }
    }

    rfm12_write_tx_register(radio, data);
}

static void process_message(RFM12_t *radio, const uint8_t *message)
{
    if(memcmp(message, ping_message, MESSAGE_LENGTH) == 0)
    {
        printf("Received PING\n");
        printf("Sending PONG\n");

        transmit_message(radio, pong_message);
    }
    else if(memcmp(message, pong_message, MESSAGE_LENGTH) == 0)
    {
        printf("Received PONG\n");
    }
    else
    {
        printf("Received unknown message\n");
    }
}

static void print_radio_status(RFM12_status_word_t status)
{
    typedef struct
    {
        bool active;
        const char *name;
    } RFM12_status_flag_t;

    const RFM12_status_flag_t flags[] =
    {
        { rfm12_status_tx_register_ready(status),    "RGIT/FFIT" },
        { rfm12_status_power_on_reset(status),       "POR"       },
        { rfm12_status_rx_fifo_overflow(status),     "RGUR/FFOV" },
        { rfm12_status_wakeup_timer_expired(status), "WKUP"      },
        { rfm12_status_external_interrupt(status),   "EXT"       },
        { rfm12_status_low_battery(status),          "LBD"       },
        { rfm12_status_rx_fifo_empty(status),        "FFEM"      },
        { rfm12_status_antenna_tuning_signal(status), "ATS"      },
        { rfm12_status_rssi_detected(status),        "RSSI"      },
        { rfm12_status_data_quality_detected(status), "DQD"      },
        { rfm12_status_clock_recovery_locked(status), "CRL"      },
        { rfm12_status_afc_cycle_toggle(status),     "ATGL"      }
    };

    size_t index;

    printf("RFM12 Status = 0x%04X\n", status);

    for(index = 0U; index < (sizeof(flags) / sizeof(flags[0])); index++)
    {
        printf("  %-10s : %s\n",
               flags[index].name,
               flags[index].active ? "ON" : "OFF");
    }

    printf("  AFC Offset : %d\n", (int)rfm12_status_afc_offset_steps(status));
}

static void fatal_rfm12_error(RFM12_result_t radio_result)
{
    printf("RFM12 error: %s\n", rfm12_result_string(radio_result));

    for(;;)
    {
    }
}


int main(void)
{
    RFM12_t *radio = NULL;
    RFM12_result_t radio_result;
    RFM12_status_word_t radio_status;

    uint8_t rx_buffer[MESSAGE_LENGTH];
    uint8_t rx_count = 0U;

    platform_initialize();

    /*
     * Allow the RFM12 power-on reset sequence to complete.
     */
    platform_delay_ms(250U);

    radio_result = rfm12_get_instance(&radio);
    if(radio_result != RFM12_OK)
    {
        fatal_rfm12_error(radio_result);
    }

    radio_result = rfm12_configure_hal(radio, platform_rfm12_spi_transfer, NULL);

    if(radio_result != RFM12_OK)
    {
        fatal_rfm12_error(radio_result);
    }

    /*
     * Reset the radio to a known hardware state.
     */
    radio_result = rfm12_software_reset(radio);
    if(radio_result != RFM12_OK)
    {
        fatal_rfm12_error(radio_result);
    }

    /*
     * Allow the RFM12 reset sequence to complete.
     */
    platform_delay_ms(250U);

    /*
     * Verify SPI communication and display the initial radio status.
     */
    radio_result = rfm12_read_status(radio, &radio_status);
    if(radio_result != RFM12_OK)
    {
        fatal_rfm12_error(radio_result);
    }

    print_radio_status(radio_status);

    configure_radio(radio);

    radio_result = rfm12_apply_to_radio(radio);
    if(radio_result != RFM12_OK)
    {
        fatal_rfm12_error(radio_result);
    }

    radio_result = rfm12_enter_rx_mode(radio);
    if(radio_result != RFM12_OK)
    {
        fatal_rfm12_error(radio_result);
    }

    rfm12_restart_sync_recognition(radio);

    printf("RFM12 PING/PONG example ready\n");

    for(;;)
    {
        RFM12_status_word_t status;

        /*
         * A new button press initiates a PING.
         */
        if(platform_button_pressed())
        {
            printf("Sending PING\n");

            rx_count = 0U;
            transmit_message(radio, ping_message);
        }

        /*
         * Poll the RFM12 status register for received data.
         */
        if(rfm12_read_status(radio, &status) != RFM12_OK)
        {
            continue;
        }

        /*
         * Recover from an RX FIFO overflow by discarding any
         * partial message and waiting for a new sync pattern.
         */
        if(rfm12_status_rx_fifo_overflow(status))
        {
            rx_count = 0U;
            rfm12_restart_sync_recognition(radio);

            continue;
        }

        /*
         * FFIT indicates that the configured FIFO threshold
         * has been reached. The example uses an 8-bit threshold,
         * so one byte is available.
         */
        if(rfm12_status_rx_fifo_interrupt(status))
        {
            uint8_t data;

            if(rfm12_read_fifo(radio, &data) == RFM12_OK)
            {
                rx_buffer[rx_count++] = data;

                if(rx_count == MESSAGE_LENGTH)
                {
                    /*
                     * Four bytes define a complete message.
                     * Restart synchronization before processing it.
                     */
                    rfm12_restart_sync_recognition(radio);

                    process_message(radio, rx_buffer);

                    rx_count = 0U;
                }
            }
        }
    }
}
