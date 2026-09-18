/*
 * -------------------------------------------------------------------------
 *
 * SPDX-License-Identifier: BSD-3-Clause
 * 
 * rfm12.c
 *
 * RFM12B radio driver implementation.
 * -------------------------------------------------------------------------
 */



#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "rfm12.h"

#ifndef RFM12_NUM_RADIOS
#define RFM12_NUM_RADIOS 1
#endif

/*
 * -------------------------------------------------------------------------
 *
 * RFM12B Command Prefixes Defines
 * 
 * -------------------------------------------------------------------------
 */

#pragma region Command Prefix Defines

#define RFM12_CMD_CONFIG_SETTING            0x8000U
#define RFM12_CMD_POWER_MANAGEMENT          0x8200U
#define RFM12_CMD_FREQUENCY_SETTING         0xA000U
#define RFM12_CMD_DATA_RATE                 0xC600U
#define RFM12_CMD_RECEIVER_CONTROL          0x9000U
#define RFM12_CMD_DATA_FILTER               0xC228U
#define RFM12_CMD_FIFO_RESET_MODE           0xCA00U
#define RFM12_CMD_SYNC_PATTERN              0xCE00U
#define RFM12_CMD_RECEIVER_FIFO_READ        0xB000U
#define RFM12_CMD_AFC                       0xC400U
#define RFM12_CMD_TX_CONFIG                 0x9800U
#define RFM12_CMD_PLL_SETTING               0xCC12U
#define RFM12_CMD_TX_REGISTER_WRITE         0xB800U
#define RFM12_CMD_WAKEUP_TIMER              0xE000U
#define RFM12_CMD_LOW_DUTY_CYCLE            0xC800U
#define RFM12_CMD_LOW_BATTERY_CLOCK_DIVIDER 0xC000U
#define RFM12_CMD_STATUS_READ               0x0000U
#define RFM12_CMD_SOFTWARE_RESET            0xFE00U

/*
 * Known-valid FIFO and Reset Mode Command used only to enable sensitive
 * reset immediately before issuing the software-reset command. The FIFO
 * interrupt level is eight bits; all other configurable fields, including
 * the reset-mode bit (dr), are zero.
 */
#define RFM12_CMD_ENABLE_SENSITIVE_RESET     0xCA80U

#pragma endregion

/*
 * -------------------------------------------------------------------------
 *
 * RFM12B Command Shifts and Masks Defines
 * 
 * -------------------------------------------------------------------------
 */

#pragma region Command Shifts and Masks

#define RFM12_FREQUENCY_F_MASK              0x0FFFU
#define RFM12_DATA_RATE_CS_SHIFT            7U
#define RFM12_DATA_RATE_R_MASK              0x007FU

#define RFM12_STATUS_FFIT_RGIT_MASK         (1U << 15)
#define RFM12_STATUS_FFOV_RGUR_MASK         (1U << 13)
#define RFM12_STATUS_POR_MASK               (1U << 14)
#define RFM12_STATUS_WKUP_MASK              (1U << 12)
#define RFM12_STATUS_EXT_MASK               (1U << 11)
#define RFM12_STATUS_LBD_MASK               (1U << 10)
#define RFM12_STATUS_FFEM_MASK              (1U << 9)
#define RFM12_STATUS_ATS_MASK               (1U << 8)
#define RFM12_STATUS_RSSI_MASK              (1U << 7)
#define RFM12_STATUS_DQD_MASK               (1U << 6)
#define RFM12_STATUS_CRL_MASK               (1U << 5)
#define RFM12_STATUS_ATGL_MASK              (1U << 4)
#define RFM12_STATUS_AFC_OFFSET_MASK        0x000FU

#pragma endregion

/*
 * -------------------------------------------------------------------------
 *
 * Private Data Types
 * 
 * -------------------------------------------------------------------------
 */

 #pragma region Private Data Types

#define RFM12_LINK_SYNC_BYTE_1  0x2DU

// command update flags

typedef enum
{
    RFM12_DIRTY_CONFIG_SETTING    = (1u << 0),
    RFM12_DIRTY_POWER_MGMT        = (1u << 1),
    RFM12_DIRTY_FREQUENCY         = (1u << 2),
    RFM12_DIRTY_DATA_RATE         = (1u << 3),
    RFM12_DIRTY_RX_CONTROL        = (1u << 4),
    RFM12_DIRTY_DATA_FILTER       = (1u << 5),
    RFM12_DIRTY_FIFO_RESET        = (1u << 6),
    RFM12_DIRTY_SYNC_PATTERN      = (1u << 7),
    RFM12_DIRTY_AFC               = (1u << 8),
    RFM12_DIRTY_TX_CONFIG         = (1u << 9),
    RFM12_DIRTY_PLL_SETTING       = (1u << 10),
    RFM12_DIRTY_WAKEUP_TIMER      = (1u << 11),
    RFM12_DIRTY_LOW_DUTY_CYCLE    = (1u << 12),
    RFM12_DIRTY_LOW_BATTERY_CLOCK = (1u << 13)

} RFM12_dirty_flag_t;

#define RFM12_DIRTY_ALL_CONFIGURATION    \
    (RFM12_DIRTY_CONFIG_SETTING    |     \
     RFM12_DIRTY_POWER_MGMT        |     \
     RFM12_DIRTY_FREQUENCY         |     \
     RFM12_DIRTY_DATA_RATE         |     \
     RFM12_DIRTY_RX_CONTROL        |     \
     RFM12_DIRTY_DATA_FILTER       |     \
     RFM12_DIRTY_FIFO_RESET        |     \
     RFM12_DIRTY_SYNC_PATTERN      |     \
     RFM12_DIRTY_AFC               |     \
     RFM12_DIRTY_TX_CONFIG         |     \
     RFM12_DIRTY_PLL_SETTING       |     \
     RFM12_DIRTY_WAKEUP_TIMER      |     \
     RFM12_DIRTY_LOW_DUTY_CYCLE    |     \
     RFM12_DIRTY_LOW_BATTERY_CLOCK)

// HAL (Hardware Abstraction Layer) structure

typedef struct
{
    RFM12_spi_transfer16_fn spi_transfer16;
    void                   *context;

} RFM12_HAL_t;

// Configuration Setting Command Structure

typedef struct
{
    RFM12_enable_t          tx_data_register_enable;    // el
    RFM12_enable_t          rx_fifo_enable;             // ef
    RFM12_frequency_band_t  band;                       // b[1:0]
    RFM12_xtal_cap_t        xtal_cap;                   // x[3:0]   
} RFM12_config_setting_t;

// Power Management Command

typedef struct
{
    RFM12_enable_t receiver;                // er
    RFM12_enable_t baseband;                // ebb
    RFM12_enable_t transmitter;             // et
    RFM12_enable_t synthesizer;             // es
    RFM12_enable_t crystal_oscillator;      // ex
    RFM12_enable_t low_battery_detector;    // eb
    RFM12_enable_t wakeup_timer;            // ew
    RFM12_enable_t clock_output;            // dc
} RFM12_power_management_t;

// Frequency Setting Command

// helper struct for band specific frequency limits
typedef struct
{
    RFM12_frequency_hz_t min_frequency_hz;
    RFM12_frequency_hz_t step_hz;
    uint16_t min_F;
    uint16_t max_F;
} RFM12_band_info_t;

// Data Rate Command

typedef struct
{
    uint32_t bits_per_second;
    uint8_t  cs;
    uint8_t  r;
} RFM12_data_rate_info_t;

// Receiver Control Command

typedef struct
{
    RFM12_pin16_function_t  pin16_function;
    RFM12_vdi_mode_t        vdi_mode;
    RFM12_rx_bandwidth_t    rx_bandwidth;
    RFM12_lna_gain_t        lna_gain;
    RFM12_rssi_threshold_t  rssi_threshold;
} RFM12_receiver_control_t;

// Data Filter Command

typedef struct
{
    RFM12_clock_recovery_mode_t      clock_recovery_mode;
    RFM12_clock_recovery_speed_t     clock_recovery_speed;
    RFM12_filter_type_t              filter_type;
    RFM12_dqd_threshold_t            dqd_threshold;
    
} RFM12_data_filter_t;

// FIFO and Reset Mode Command

typedef struct
{
    RFM12_fifo_interrupt_level_t  fifo_interrupt_level;
    RFM12_sync_pattern_length_t   sync_pattern_length;
    RFM12_fifo_fill_start_t       fifo_fill_start;
    RFM12_enable_t                fifo_enable;
    RFM12_reset_mode_t            reset_mode;

} RFM12_fifo_reset_mode_t;

// AFC Command

typedef struct
{
    RFM12_afc_mode_t   mode;
    RFM12_afc_range_t  range_limit;
    RFM12_enable_t     store_offset;
    RFM12_enable_t     fine_mode;
    RFM12_enable_t     output_register_enable;
    RFM12_enable_t     enable;

} RFM12_afc_config_t;

// TX Configuration Command

typedef struct
{
    RFM12_tx_fsk_polarity_t     polarity;
    RFM12_tx_fsk_deviation_t    deviation;
    RFM12_tx_power_t            power;

} RFM12_tx_configuration_t;

// PLL Setting Command

typedef struct
{
    RFM12_pll_output_buffer_current_t output_buffer_current;
    RFM12_enable_t                    delay_enable;
    RFM12_enable_t                    dithering_disable;
    RFM12_pll_bandwidth_t             bandwidth;

} RFM12_pll_setting_t;

// Wake-Up Timer Command

typedef struct
{
    RFM12_wakeup_prescaler_t  prescaler;
    RFM12_wakeup_multiplier_t multiplier;

} RFM12_wakeup_timer_settings_t;

// Low Duty-Cycle Command

typedef struct
{
    RFM12_low_duty_cycle_d_t    d;
    RFM12_enable_t              enable;

} RFM12_low_duty_cycle_config_t;

// Low Battery / Clock Divider Command

typedef struct
{
    RFM12_clock_output_frequency_t clock_frequency;
    RFM12_low_battery_threshold_t  low_battery_threshold;

} RFM12_low_battery_clock_config_t;

// Main RFM12 device structure

struct RFM12
{
    RFM12_config_setting_t              config_setting;
    RFM12_power_management_t            power_management;
    RFM12_frequency_hz_t                frequency_hz;
    RFM12_data_rate_t                   data_rate;
    RFM12_receiver_control_t            receiver_control;
    RFM12_data_filter_t                 data_filter;
    RFM12_fifo_reset_mode_t             fifo_reset_mode;
    RFM12_sync_pattern_t                sync_pattern;
    RFM12_afc_config_t                  afc;
    RFM12_tx_configuration_t            tx_configuration;
    RFM12_pll_setting_t                 pll_setting;
    RFM12_wakeup_timer_settings_t       wakeup_timer;
    RFM12_low_duty_cycle_config_t       low_duty_cycle;
    RFM12_low_battery_clock_config_t    low_battery_clock;

    RFM12_HAL_t hal;

    uint32_t dirty;
    RFM12_mode_t mode;
};

#pragma endregion

// Default Structs

#pragma region Command Default Data

// 1. Configuration Setting Command

static const RFM12_config_setting_t rfm12_config_setting_default =
{
    .tx_data_register_enable = RFM12_DISABLE,
    .rx_fifo_enable          = RFM12_DISABLE,
    .band                    = RFM12_BAND_433,
    .xtal_cap                = RFM12_XTAL_CAP_12_5PF
};

// 2. Power Management Command

static const RFM12_power_management_t rfm12_power_management_default =
{
    .receiver             = RFM12_DISABLE,  // er
    .baseband             = RFM12_DISABLE,  // ebb
    .transmitter          = RFM12_DISABLE,  // et
    .synthesizer          = RFM12_DISABLE,  // es
    .crystal_oscillator   = RFM12_ENABLE,   // ex
    .low_battery_detector = RFM12_DISABLE,  // eb
    .wakeup_timer         = RFM12_DISABLE,  // ew
    .clock_output         = RFM12_ENABLE    // dc is inverted
};

// 3. Frequency Setting Command

static const RFM12_frequency_hz_t rfm12_frequency_default = 430240000UL;

// 4. Data Rate Command

static const RFM12_data_rate_t rfm12_data_rate_default = RFM12_DATA_RATE_9600;

// 5. Receiver Control Command

static const RFM12_receiver_control_t rfm12_receiver_control_default =
{
    .pin16_function = RFM12_PIN16_INTERRUPT_IN,
    .vdi_mode       = RFM12_VDI_FAST,
    .rx_bandwidth   = RFM12_RX_BANDWIDTH_200_KHZ,
    .lna_gain       = RFM12_LNA_GAIN_0_DB,
    .rssi_threshold = RFM12_RSSI_THRESHOLD_M103_DBM
};

// 6. Data Filter Command

static const RFM12_data_filter_t rfm12_data_filter_default =
{
    .clock_recovery_mode  = RFM12_CLOCK_RECOVERY_MANUAL,
    .clock_recovery_speed = RFM12_CLOCK_RECOVERY_SLOW,
    .filter_type          = RFM12_FILTER_DIGITAL,
    .dqd_threshold        = RFM12_DQD_THRESHOLD_4
};

// 7. FIFO and Reset Mode Command

static const RFM12_fifo_reset_mode_t rfm12_fifo_reset_mode_default =
{
    .fifo_interrupt_level = 8U,
    .sync_pattern_length  = RFM12_SYNC_PATTERN_2BYTE,
    .fifo_fill_start      = RFM12_FIFO_FILL_AFTER_SYNC,
    .fifo_enable          = RFM12_DISABLE,
    .reset_mode           = RFM12_RESET_MODE_SENSITIVE
};

// 8. Sync Pattern Command

static const RFM12_sync_pattern_t rfm12_sync_pattern_default = 0xD4U;

// 10. AFC Command

static const RFM12_afc_config_t rfm12_afc_default =
{
    .mode                   = RFM12_AFC_MODE_KEEP_OFFSET_ON_RECEIVE,
    .range_limit            = RFM12_AFC_RANGE_3_TO_4,
    .store_offset           = RFM12_DISABLE,
    .fine_mode              = RFM12_ENABLE,
    .output_register_enable = RFM12_ENABLE,
    .enable                 = RFM12_ENABLE
};

// 11. TX Configuration Control Command

static const RFM12_tx_configuration_t rfm12_tx_configuration_default =
{
    .polarity  = RFM12_TX_FSK_POLARITY_NORMAL,
    .deviation = RFM12_TX_FSK_DEVIATION_15KHZ,
    .power     = RFM12_TX_POWER_0DB
};

// 12. PLL Setting Command

static const RFM12_pll_setting_t rfm12_pll_setting_default =
{
    .output_buffer_current = RFM12_PLL_OUTPUT_BUFFER_CURRENT_3,
    .delay_enable          = RFM12_DISABLE,
    .dithering_disable     = RFM12_ENABLE,
    .bandwidth             = RFM12_PLL_BANDWIDTH_HIGH
};

// 14. Wake-Up Timer Command

static const RFM12_wakeup_timer_settings_t rfm12_wakeup_timer_default =
{
    .prescaler = 1U,
    .multiplier = 150U
};

// 15. Low Duty-Cycle Command

static const RFM12_low_duty_cycle_config_t rfm12_low_duty_cycle_default =
{
    .d      = 7U,
    .enable = RFM12_DISABLE
};

// 16. Low Battery Detector and Microcontroller Clock Divider Command

static const RFM12_low_battery_clock_config_t rfm12_low_battery_clock_default =
{
    .clock_frequency       = RFM12_CLOCK_OUTPUT_1MHZ,
    .low_battery_threshold = RFM12_LOW_BATTERY_THRESHOLD_2_25V
};

#pragma endregion

// Helper Structs

#pragma region Helper Structs

static const RFM12_band_info_t rfm12_band_info[] =
{
    [RFM12_BAND_433] = { 430240000UL, 2500UL, 96, 3903 },
    [RFM12_BAND_868] = { 860480000UL, 5000UL, 96, 3903 },
    [RFM12_BAND_915] = { 900720000UL, 7500UL, 96, 3903 },
};

static const RFM12_data_rate_info_t rfm12_data_rate_table[RFM12_DATA_RATE_COUNT] =
{
    [RFM12_DATA_RATE_1200]   = { 1200U,   1U, 0x1EU },
    [RFM12_DATA_RATE_2400]   = { 2400U,   1U, 0x11U },
    [RFM12_DATA_RATE_4800]   = { 4800U,   0U, 0x47U },
    [RFM12_DATA_RATE_9600]   = { 9600U,   0U, 0x23U },
    [RFM12_DATA_RATE_19200]  = { 19200U,  0U, 0x11U },
    [RFM12_DATA_RATE_28800]  = { 28800U,  0U, 0x0BU },
    [RFM12_DATA_RATE_38400]  = { 38400U,  0U, 0x08U },
    [RFM12_DATA_RATE_57600]  = { 57600U,  0U, 0x05U },
    [RFM12_DATA_RATE_115200] = { 115200U, 0U, 0x02U }
};

#pragma endregion

// Instance Management

#pragma region Instance Management

static RFM12_t rfm12_instances[RFM12_NUM_RADIOS];
static uint8_t rfm12_instance_count = 0;

#pragma endregion

/*
 * -------------------------------------------------------------------------
 *
 * Private Function Prototypes
 * 
 * -------------------------------------------------------------------------
 */

#pragma region Private Function Prototypes

static void rfm12_initialize_instance(RFM12_t *dev);

static RFM12_result_t rfm12_exchange_command(RFM12_t *dev, uint16_t command, uint16_t *response);
static RFM12_result_t rfm12_send_command(RFM12_t *dev, uint16_t command);

static RFM12_result_t rfm12_validate_frequency_configuration(const RFM12_t *dev);
static RFM12_result_t rfm12_wakeup_ms_to_rm(uint32_t wakeup_ms, uint8_t *r, uint8_t *m);

static uint16_t rfm12_encode_config_setting(const RFM12_t *dev);
static uint16_t rfm12_encode_power_management(const RFM12_t *dev);
static uint16_t rfm12_encode_frequency_setting(const RFM12_t *dev);
static uint16_t rfm12_encode_data_rate(const RFM12_t *dev);
static uint16_t rfm12_encode_receiver_control(const RFM12_t *dev);
static uint16_t rfm12_encode_data_filter(const RFM12_t *dev);
static uint16_t rfm12_encode_fifo_reset_mode(const RFM12_t *dev);
static uint16_t rfm12_encode_sync_pattern(const RFM12_t *dev);
static uint16_t rfm12_encode_afc_command(const RFM12_t *dev);
static uint16_t rfm12_encode_tx_configuration(const RFM12_t *dev);
static uint16_t rfm12_encode_pll_setting(const RFM12_t *dev);
static uint16_t rfm12_encode_wakeup_timer(const RFM12_t *dev);
static uint16_t rfm12_encode_low_duty_cycle(const RFM12_t *dev);
static uint16_t rfm12_encode_low_battery_clock_config(const RFM12_t *dev);


#pragma endregion

/*
 * -------------------------------------------------------------------------
 *
 * Private Driver Infrastructure Functions
 * 
 * -------------------------------------------------------------------------
 */

#pragma region Private Driver Infrastructure Functions

static void rfm12_initialize_instance(RFM12_t *dev)
{
    dev->config_setting     = rfm12_config_setting_default;
    dev->power_management   = rfm12_power_management_default;
    dev->frequency_hz       = rfm12_frequency_default;
    dev->data_rate          = rfm12_data_rate_default;
    dev->receiver_control   = rfm12_receiver_control_default;
    dev->data_filter        = rfm12_data_filter_default;
    dev->fifo_reset_mode    = rfm12_fifo_reset_mode_default;
    dev->sync_pattern       = rfm12_sync_pattern_default;
    dev->afc                = rfm12_afc_default;
    dev->tx_configuration   = rfm12_tx_configuration_default;
    dev->pll_setting        = rfm12_pll_setting_default;
    dev->wakeup_timer       = rfm12_wakeup_timer_default;
    dev->low_duty_cycle     = rfm12_low_duty_cycle_default;
    dev->low_battery_clock  = rfm12_low_battery_clock_default;

    dev->hal.spi_transfer16 = NULL;
    dev->hal.context        = NULL;
    
    dev->dirty              = RFM12_DIRTY_ALL_CONFIGURATION;
    dev->mode               = RFM12_MODE_UNKNOWN;
}

static RFM12_result_t rfm12_exchange_command(RFM12_t *dev, uint16_t command, uint16_t *response)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (response == NULL)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    if (dev->hal.spi_transfer16 == NULL)
    {
        return RFM12_ERROR_NOT_INITIALIZED;
    }

    return dev->hal.spi_transfer16(dev->hal.context, command, response);
}

static RFM12_result_t rfm12_send_command(RFM12_t *dev, uint16_t command)
{
    uint16_t response;

    return rfm12_exchange_command(dev, command, &response);
}

#pragma endregion

/*
 * -------------------------------------------------------------------------
 *
 * Public API Functions
 * 
 * -------------------------------------------------------------------------
 */

#pragma region Public API Functions

RFM12_result_t rfm12_get_instance(RFM12_t **instance)
{
    RFM12_t *dev;

    if (instance == NULL)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    *instance = NULL;

    if (rfm12_instance_count >= RFM12_NUM_RADIOS)
    {
        return RFM12_ERROR_NO_INSTANCE_AVAILABLE;
    }

    dev = &rfm12_instances[rfm12_instance_count++];

    rfm12_initialize_instance(dev);

    *instance = dev;

    return RFM12_OK;
}

RFM12_result_t rfm12_configure_hal(RFM12_t *dev, RFM12_spi_transfer16_fn spi_transfer16, void *context)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (spi_transfer16 == NULL)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->hal.spi_transfer16 = spi_transfer16;
    dev->hal.context        = context;

    return RFM12_OK;
}

RFM12_result_t rfm12_apply_to_radio(RFM12_t *dev)
{
    uint16_t command;
    RFM12_result_t result;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    /*
     * The Frequency Setting Command depends on the band selected by the
     * Configuration Setting Command. Validate the combination before
     * sending any pending commands.
     */
    if ((dev->dirty & (RFM12_DIRTY_CONFIG_SETTING | RFM12_DIRTY_FREQUENCY)) != 0U)
    {
        result = rfm12_validate_frequency_configuration(dev);

        if (result != RFM12_OK)
        {
            return result;
        }
    }

    if ((dev->dirty & RFM12_DIRTY_CONFIG_SETTING) != 0U)
    {
        command = rfm12_encode_config_setting(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_CONFIG_SETTING;
    }

    if ((dev->dirty & RFM12_DIRTY_FREQUENCY) != 0U)
    {
        command = rfm12_encode_frequency_setting(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_FREQUENCY;
    }

    if ((dev->dirty & RFM12_DIRTY_DATA_RATE) != 0U)
    {
        command = rfm12_encode_data_rate(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_DATA_RATE;
    }

    if ((dev->dirty & RFM12_DIRTY_RX_CONTROL) != 0U)
    {
        command = rfm12_encode_receiver_control(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_RX_CONTROL;
    }

    if ((dev->dirty & RFM12_DIRTY_DATA_FILTER) != 0U)
    {
        command = rfm12_encode_data_filter(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_DATA_FILTER;
    }

    if ((dev->dirty & RFM12_DIRTY_FIFO_RESET) != 0U)
    {
        command = rfm12_encode_fifo_reset_mode(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_FIFO_RESET;
    }

    if ((dev->dirty & RFM12_DIRTY_SYNC_PATTERN) != 0U)
    {
        command = rfm12_encode_sync_pattern(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_SYNC_PATTERN;
    }

    if ((dev->dirty & RFM12_DIRTY_AFC) != 0U)
    {
        command = rfm12_encode_afc_command(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_AFC;
    }

    if ((dev->dirty & RFM12_DIRTY_TX_CONFIG) != 0U)
    {
        command = rfm12_encode_tx_configuration(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_TX_CONFIG;
    }

    if ((dev->dirty & RFM12_DIRTY_PLL_SETTING) != 0U)
    {
        command = rfm12_encode_pll_setting(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_PLL_SETTING;
    }

    if ((dev->dirty & RFM12_DIRTY_WAKEUP_TIMER) != 0U)
    {
        command = rfm12_encode_wakeup_timer(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_WAKEUP_TIMER;
    }

    if ((dev->dirty & RFM12_DIRTY_LOW_DUTY_CYCLE) != 0U)
    {
        command = rfm12_encode_low_duty_cycle(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_LOW_DUTY_CYCLE;
    }

    if ((dev->dirty & RFM12_DIRTY_LOW_BATTERY_CLOCK) != 0U)
    {
        command = rfm12_encode_low_battery_clock_config(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_LOW_BATTERY_CLOCK;
    }

    /*
     * Apply power management last so that the receiver or transmitter
     * is not enabled until the remaining configuration is complete.
     */
    if ((dev->dirty & RFM12_DIRTY_POWER_MGMT) != 0U)
    {
        command = rfm12_encode_power_management(dev);
        result = rfm12_send_command(dev, command);

        if (result != RFM12_OK)
        {
            return result;
        }

        dev->dirty &= ~RFM12_DIRTY_POWER_MGMT;
    }

    return RFM12_OK;
}

RFM12_result_t rfm12_software_reset(RFM12_t *dev)
{
    RFM12_result_t result;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    /*
     * Sensitive reset must be enabled in the radio before FE00h is sent.
     * Send a fixed command directly so pending staged configuration is not
     * applied and the application's desired FIFO/reset settings are not
     * modified.
     */
    result = rfm12_send_command(dev, RFM12_CMD_ENABLE_SENSITIVE_RESET);

    if (result != RFM12_OK)
    {
        return result;
    }

    /* The temporary command above changed the radio's FIFO configuration. */
    dev->dirty |= RFM12_DIRTY_FIFO_RESET;

    result = rfm12_send_command(dev, RFM12_CMD_SOFTWARE_RESET);

    if (result != RFM12_OK)
    {
        return result;
    }

    /*
     * Reset restores the radio's hardware defaults. Preserve the staged
     * application configuration and HAL, but require every command to be
     * reapplied after the radio has completed its reset sequence.
     */
    dev->dirty = RFM12_DIRTY_ALL_CONFIGURATION;
    dev->mode = RFM12_MODE_UNKNOWN;

    return RFM12_OK;
}

RFM12_result_t rfm12_get_mode(const RFM12_t *dev, RFM12_mode_t *mode)
{
    if(dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if(mode == NULL)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    *mode = dev->mode;

    return RFM12_OK;
}

const char *rfm12_result_string(RFM12_result_t result)
{
    switch (result)
    {
        case RFM12_OK:
            return "OK";

        case RFM12_ERROR_INVALID_HANDLE:
            return "Invalid radio handle";

        case RFM12_ERROR_INVALID_ARGUMENT:
            return "Invalid argument";

        case RFM12_ERROR_INVALID_CONFIGURATION:
            return "Invalid radio configuration";

        case RFM12_ERROR_NO_INSTANCE_AVAILABLE:
            return "No radio instance available";

        case RFM12_ERROR_NOT_INITIALIZED:
            return "Radio not initialized";

        case RFM12_ERROR_UNKNOWN:
            return "Unknown error";

        default:
            return "Unrecognized result code";
    }
}

RFM12_result_t rfm12_get_sync_bytes(const RFM12_t *dev, uint8_t *buffer, uint8_t buffer_size, uint8_t *length)
{
    if(dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if((buffer == NULL) || (length == NULL))
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    if(dev->fifo_reset_mode.sync_pattern_length ==
       RFM12_SYNC_PATTERN_2BYTE)
    {
        if(buffer_size < 2U)
        {
            return RFM12_ERROR_INVALID_ARGUMENT;
        }

        buffer[0] = RFM12_LINK_SYNC_BYTE_1;
        buffer[1] = dev->sync_pattern;
        *length = 2U;
    }
    else
    {
        if(buffer_size < 1U)
        {
            return RFM12_ERROR_INVALID_ARGUMENT;
        }

        buffer[0] = dev->sync_pattern;
        *length = 1U;
    }

    return RFM12_OK;
}

#pragma endregion

//
// Datasheet Hardware Command Implementation Functions
//

/****************************************************************************\

            1. Configuration Setting Command

\****************************************************************************/

#pragma region Configuration Setting Command

RFM12_result_t rfm12_set_tx_data_register_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
        return RFM12_ERROR_INVALID_HANDLE;

    dev->config_setting.tx_data_register_enable = enable;

    dev->dirty |= RFM12_DIRTY_CONFIG_SETTING;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_rx_fifo_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
        return RFM12_ERROR_INVALID_HANDLE;

    dev->config_setting.rx_fifo_enable = enable;

    dev->dirty |= RFM12_DIRTY_CONFIG_SETTING;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_frequency_band(RFM12_t *dev, RFM12_frequency_band_t band)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (band)
    {
        case RFM12_BAND_433:
        case RFM12_BAND_868:
        case RFM12_BAND_915:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }
    
    dev->config_setting.band = band;
    dev->dirty |= RFM12_DIRTY_CONFIG_SETTING;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_xtal_cap(RFM12_t *dev, RFM12_xtal_cap_t cap)
{
    if (dev == NULL)
    {   
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (cap > RFM12_XTAL_CAP_16_0PF)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->config_setting.xtal_cap = cap;
    dev->dirty |= RFM12_DIRTY_CONFIG_SETTING;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_config_setting(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->config_setting = rfm12_config_setting_default;
    dev->dirty |= RFM12_DIRTY_CONFIG_SETTING;

    return RFM12_OK;
}

static uint16_t rfm12_encode_config_setting(const RFM12_t *dev)
{
    
    const RFM12_config_setting_t *config;
    config = &dev->config_setting;

    uint16_t command = RFM12_CMD_CONFIG_SETTING;

    command |= ((uint16_t)config->tx_data_register_enable << 7);
    command |= ((uint16_t)config->rx_fifo_enable          << 6);
    command |= ((uint16_t)config->band                    << 4);
    command |=  (uint16_t)config->xtal_cap;

    return command;
}
#pragma endregion

/****************************************************************************\

            2. Power Management Command

\****************************************************************************/

#pragma region Power Management Command

static RFM12_result_t rfm12_set_power_bit(RFM12_t *dev, RFM12_enable_t *field, RFM12_enable_t enable)
{
    if (*field != enable)
    {
        *field = enable;
        dev->dirty |= RFM12_DIRTY_POWER_MGMT;
    }

    return RFM12_OK;
}

RFM12_result_t rfm12_set_receiver_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    RFM12_result_t result;

    if(dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    result = rfm12_set_power_bit(dev, &dev->power_management.receiver, enable);

    if(result == RFM12_OK)
    {
        dev->mode = RFM12_MODE_UNKNOWN;
    }

    return result;
}

RFM12_result_t rfm12_set_baseband_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    RFM12_result_t result;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }
    
    result = rfm12_set_power_bit(dev, &dev->power_management.baseband, enable);
    
    if(result == RFM12_OK)
    {
        dev->mode = RFM12_MODE_UNKNOWN;
    }
    return result;
}

RFM12_result_t rfm12_set_transmitter_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    RFM12_result_t result;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }
    result = rfm12_set_power_bit(dev, &dev->power_management.transmitter, enable);

    if(result == RFM12_OK)
    {
        dev->mode = RFM12_MODE_UNKNOWN;
    }
    return result;
}

RFM12_result_t rfm12_set_synthesizer_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    RFM12_result_t result;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }
    result = rfm12_set_power_bit(dev, &dev->power_management.synthesizer, enable);

    if(result == RFM12_OK)
    {
        dev->mode = RFM12_MODE_UNKNOWN;
    }
    return result;
}

RFM12_result_t rfm12_set_crystal_oscillator_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    RFM12_result_t result;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }
    result = rfm12_set_power_bit(dev, &dev->power_management.crystal_oscillator, enable);

    if(result == RFM12_OK)
    {
        dev->mode = RFM12_MODE_UNKNOWN;
    }
    return result;
}

RFM12_result_t rfm12_set_low_battery_detector_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }
    return rfm12_set_power_bit(dev, &dev->power_management.low_battery_detector, enable);
}

RFM12_result_t rfm12_set_wakeup_timer_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }
    return rfm12_set_power_bit(dev, &dev->power_management.wakeup_timer, enable);
}

RFM12_result_t rfm12_set_clock_output_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }
    return rfm12_set_power_bit(dev, &dev->power_management.clock_output, enable);
}

// helper functions

RFM12_result_t rfm12_reset_power_management(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->power_management = rfm12_power_management_default;
    dev->dirty |= RFM12_DIRTY_POWER_MGMT;

    return RFM12_OK;
}

RFM12_result_t rfm12_enter_rx_mode(RFM12_t *dev)
{
    RFM12_result_t result;
    uint16_t command;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->power_management.receiver           = RFM12_ENABLE;
    dev->power_management.baseband           = RFM12_ENABLE;
    dev->power_management.transmitter        = RFM12_DISABLE;
    dev->power_management.synthesizer        = RFM12_ENABLE;
    dev->power_management.crystal_oscillator = RFM12_ENABLE;

    command = rfm12_encode_power_management(dev);

    result = rfm12_send_command(dev, command);

    if(result == RFM12_OK)
    {
        dev->dirty &= ~RFM12_DIRTY_POWER_MGMT;
        dev->mode = RFM12_MODE_RX;
    }

    return result;
}

RFM12_result_t rfm12_enter_tx_mode(RFM12_t *dev)
{
    RFM12_result_t result;
    uint16_t command;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->power_management.receiver           = RFM12_DISABLE;
    dev->power_management.baseband           = RFM12_DISABLE;
    dev->power_management.transmitter        = RFM12_ENABLE;
    dev->power_management.synthesizer        = RFM12_ENABLE;
    dev->power_management.crystal_oscillator = RFM12_ENABLE;

    command = rfm12_encode_power_management(dev);

    result = rfm12_send_command(dev, command);

    if(result == RFM12_OK)
    {
        dev->dirty &= ~RFM12_DIRTY_POWER_MGMT;
        dev->mode = RFM12_MODE_TX;
    }

    return result;
}

RFM12_result_t rfm12_enter_idle_mode(RFM12_t *dev)
{
    RFM12_result_t result;
    uint16_t command;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->power_management.receiver           = RFM12_DISABLE;
    dev->power_management.baseband           = RFM12_DISABLE;
    dev->power_management.transmitter        = RFM12_DISABLE;
    dev->power_management.synthesizer        = RFM12_ENABLE;
    dev->power_management.crystal_oscillator = RFM12_ENABLE;

    command = rfm12_encode_power_management(dev);

    result = rfm12_send_command(dev, command);

    if(result == RFM12_OK)
    {
        dev->dirty &= ~RFM12_DIRTY_POWER_MGMT;
        dev->mode = RFM12_MODE_IDLE;
    }

    return result;

}

RFM12_result_t rfm12_enter_sleep_mode(RFM12_t *dev)
{
    RFM12_result_t result;
    uint16_t command;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->power_management.receiver           = RFM12_DISABLE;
    dev->power_management.baseband           = RFM12_DISABLE;
    dev->power_management.transmitter        = RFM12_DISABLE;
    dev->power_management.synthesizer        = RFM12_DISABLE;
    dev->power_management.crystal_oscillator = RFM12_DISABLE;

    command = rfm12_encode_power_management(dev);

    result = rfm12_send_command(dev, command);

    if(result == RFM12_OK)
    {
        dev->dirty &= ~RFM12_DIRTY_POWER_MGMT;
        dev->mode = RFM12_MODE_SLEEP;
    }

    return result;
}

static uint16_t rfm12_encode_power_management(const RFM12_t *dev)
{

    const RFM12_power_management_t *power;
    power = &dev->power_management;

    uint16_t command = RFM12_CMD_POWER_MANAGEMENT;

    command |= ((uint16_t)power->receiver             << 7);
    command |= ((uint16_t)power->baseband             << 6);
    command |= ((uint16_t)power->transmitter          << 5);
    command |= ((uint16_t)power->synthesizer          << 4);
    command |= ((uint16_t)power->crystal_oscillator   << 3);
    command |= ((uint16_t)power->low_battery_detector << 2);
    command |= ((uint16_t)power->wakeup_timer         << 1);

    /* dc bit is inverted */
    command |= (uint16_t)
        (power->clock_output == RFM12_DISABLE);

    return command;
}
#pragma endregion

/****************************************************************************\

            3. Frequency Setting Command

\****************************************************************************/

#pragma region Frequency Setting Command

RFM12_result_t rfm12_set_frequency(RFM12_t *dev, RFM12_frequency_hz_t frequency_hz)
{
    const RFM12_band_info_t *band_info;
    RFM12_frequency_band_t band;
    uint32_t max_frequency_hz;
    uint32_t offset_hz;
    uint32_t step_count;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    band = dev->config_setting.band;

    if ((band != RFM12_BAND_433) &&
        (band != RFM12_BAND_868) &&
        (band != RFM12_BAND_915))
    {
        return RFM12_ERROR_INVALID_CONFIGURATION;
    }

    band_info = &rfm12_band_info[band];

    max_frequency_hz =
        band_info->min_frequency_hz +
        ((uint32_t)(band_info->max_F - band_info->min_F) *
         band_info->step_hz);

    if ((frequency_hz < band_info->min_frequency_hz) ||
        (frequency_hz > max_frequency_hz))
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    /*
     * Convert the requested frequency to the nearest frequency that the
     * RFM12 can generate in the currently selected band.
     */

    offset_hz = frequency_hz - band_info->min_frequency_hz;

    step_count = (offset_hz + (band_info->step_hz / 2UL)) / band_info->step_hz;

    dev->frequency_hz = band_info->min_frequency_hz + (step_count * band_info->step_hz);

    dev->dirty |= RFM12_DIRTY_FREQUENCY;

    return RFM12_OK;
}


RFM12_result_t rfm12_get_frequency(const RFM12_t  *dev, RFM12_frequency_hz_t *frequency_hz)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (frequency_hz == NULL)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    *frequency_hz = dev->frequency_hz;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_frequency(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->frequency_hz = rfm12_frequency_default;
    dev->dirty |= RFM12_DIRTY_FREQUENCY;

    return RFM12_OK;
}

static RFM12_result_t rfm12_validate_frequency_configuration(const RFM12_t *dev)
{
    const RFM12_band_info_t *band_info;
    RFM12_frequency_band_t band;
    uint32_t max_frequency_hz;

    band = dev->config_setting.band;

    if ((band != RFM12_BAND_433) &&
        (band != RFM12_BAND_868) &&
        (band != RFM12_BAND_915))
    {
        return RFM12_ERROR_INVALID_CONFIGURATION;
    }

    band_info = &rfm12_band_info[band];

    max_frequency_hz =
        band_info->min_frequency_hz +
        ((uint32_t)(band_info->max_F - band_info->min_F) *
         band_info->step_hz);

    if ((dev->frequency_hz < band_info->min_frequency_hz) ||
        (dev->frequency_hz > max_frequency_hz))
    {
        return RFM12_ERROR_INVALID_CONFIGURATION;
    }

    return RFM12_OK;
}

static uint16_t rfm12_encode_frequency_setting(const RFM12_t *dev)
{
    const RFM12_band_info_t *band_info;
    RFM12_frequency_band_t band;
    uint32_t frequency_offset_hz;
    uint16_t f_value;

    band = dev->config_setting.band;
    band_info = &rfm12_band_info[band];

    frequency_offset_hz = dev->frequency_hz - band_info->min_frequency_hz;

    f_value = band_info->min_F + (uint16_t)(frequency_offset_hz / band_info->step_hz);

    return RFM12_CMD_FREQUENCY_SETTING | (f_value & RFM12_FREQUENCY_F_MASK);
}

#pragma endregion

/****************************************************************************\

            4. Data Rate Command

\****************************************************************************/

#pragma region Data Rate Command

RFM12_result_t rfm12_set_data_rate(RFM12_t *dev, RFM12_data_rate_t rate)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if ((unsigned int)rate >= (unsigned int)RFM12_DATA_RATE_COUNT)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->data_rate = rate;
    dev->dirty |= RFM12_DIRTY_DATA_RATE;

    return RFM12_OK;
}

RFM12_result_t rfm12_get_data_rate(const RFM12_t *dev, RFM12_data_rate_t *rate)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (rate == NULL)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    *rate = dev->data_rate;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_data_rate(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->data_rate = rfm12_data_rate_default;
    dev->dirty |= RFM12_DIRTY_DATA_RATE;

    return RFM12_OK;
}

static uint16_t rfm12_encode_data_rate(const RFM12_t *dev)
{
    const RFM12_data_rate_info_t *rate_info;
    rate_info = &rfm12_data_rate_table[dev->data_rate];

    uint16_t command = RFM12_CMD_DATA_RATE;

    command |= (uint16_t)rate_info->cs << RFM12_DATA_RATE_CS_SHIFT;
    command |= (uint16_t)rate_info->r & RFM12_DATA_RATE_R_MASK;

    return command;
}

#pragma endregion

/****************************************************************************\

           5. Receiver Control Command

\****************************************************************************/

#pragma region Receiver Control Command

RFM12_result_t rfm12_set_pin16_function(RFM12_t *dev, RFM12_pin16_function_t pin16)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (pin16)
    {
        case RFM12_PIN16_INTERRUPT_IN:
        case RFM12_PIN16_VDI_OUT:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->receiver_control.pin16_function = pin16;
    dev->dirty |= RFM12_DIRTY_RX_CONTROL;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_vdi_mode(RFM12_t *dev, RFM12_vdi_mode_t mode)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (mode)
    {
        case RFM12_VDI_FAST:
        case RFM12_VDI_MEDIUM:
        case RFM12_VDI_SLOW:
        case RFM12_VDI_ALWAYS:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->receiver_control.vdi_mode = mode;
    dev->dirty |= RFM12_DIRTY_RX_CONTROL;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_rx_bandwidth(RFM12_t *dev, RFM12_rx_bandwidth_t bandwidth)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (bandwidth)
    {
        case RFM12_RX_BANDWIDTH_400_KHZ:
        case RFM12_RX_BANDWIDTH_340_KHZ:
        case RFM12_RX_BANDWIDTH_270_KHZ:
        case RFM12_RX_BANDWIDTH_200_KHZ:
        case RFM12_RX_BANDWIDTH_134_KHZ:
        case RFM12_RX_BANDWIDTH_67_KHZ:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->receiver_control.rx_bandwidth = bandwidth;
    dev->dirty |= RFM12_DIRTY_RX_CONTROL;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_lna_gain(RFM12_t *dev, RFM12_lna_gain_t gain)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (gain)
    {
        case RFM12_LNA_GAIN_0_DB:
        case RFM12_LNA_GAIN_M6_DB:
        case RFM12_LNA_GAIN_M14_DB:
        case RFM12_LNA_GAIN_M20_DB:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->receiver_control.lna_gain = gain;
    dev->dirty |= RFM12_DIRTY_RX_CONTROL;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_rssi_threshold(RFM12_t *dev, RFM12_rssi_threshold_t threshold)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (threshold)
    {
        case RFM12_RSSI_THRESHOLD_M103_DBM:
        case RFM12_RSSI_THRESHOLD_M97_DBM:
        case RFM12_RSSI_THRESHOLD_M91_DBM:
        case RFM12_RSSI_THRESHOLD_M85_DBM:
        case RFM12_RSSI_THRESHOLD_M79_DBM:
        case RFM12_RSSI_THRESHOLD_M73_DBM:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->receiver_control.rssi_threshold = threshold;
    dev->dirty |= RFM12_DIRTY_RX_CONTROL;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_receiver_control(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->receiver_control = rfm12_receiver_control_default;

    dev->dirty |= RFM12_DIRTY_RX_CONTROL;

    return RFM12_OK;
}

static uint16_t rfm12_encode_receiver_control(const RFM12_t *dev)
{
    const RFM12_receiver_control_t *receiver_control;
    receiver_control = &dev->receiver_control;

    uint16_t command = RFM12_CMD_RECEIVER_CONTROL;

    command |= (((uint16_t)receiver_control->pin16_function & 0x01U) << 10);
    command |= (((uint16_t)receiver_control->vdi_mode & 0x03U) << 8);
    command |= (((uint16_t)receiver_control->rx_bandwidth & 0x07U) << 5);
    command |= (((uint16_t)receiver_control->lna_gain & 0x03U) << 3);
    command |= (((uint16_t)receiver_control->rssi_threshold & 0x07U) << 0);

    return command;
}

#pragma endregion

/****************************************************************************\

           6. Data Filter Command

\****************************************************************************/

#pragma region Data Filter Command

RFM12_result_t rfm12_set_clock_recovery_mode(RFM12_t *dev, RFM12_clock_recovery_mode_t mode)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (mode)
    {
        case RFM12_CLOCK_RECOVERY_MANUAL:
        case RFM12_CLOCK_RECOVERY_AUTO:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->data_filter.clock_recovery_mode = mode;

    dev->dirty |= RFM12_DIRTY_DATA_FILTER;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_clock_recovery_speed(RFM12_t *dev, RFM12_clock_recovery_speed_t speed)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (speed)
    {
        case RFM12_CLOCK_RECOVERY_SLOW:
        case RFM12_CLOCK_RECOVERY_FAST:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->data_filter.clock_recovery_speed = speed;

    dev->dirty |= RFM12_DIRTY_DATA_FILTER;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_filter_type(RFM12_t *dev, RFM12_filter_type_t filter_type)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (filter_type)
    {
        case RFM12_FILTER_DIGITAL:
        case RFM12_FILTER_ANALOG_RC:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->data_filter.filter_type = filter_type;

    dev->dirty |= RFM12_DIRTY_DATA_FILTER;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_dqd_threshold(RFM12_t *dev, RFM12_dqd_threshold_t threshold)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (threshold)
    {
        case RFM12_DQD_THRESHOLD_0:
        case RFM12_DQD_THRESHOLD_1:
        case RFM12_DQD_THRESHOLD_2:
        case RFM12_DQD_THRESHOLD_3:
        case RFM12_DQD_THRESHOLD_4:
        case RFM12_DQD_THRESHOLD_5:
        case RFM12_DQD_THRESHOLD_6:
        case RFM12_DQD_THRESHOLD_7:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->data_filter.dqd_threshold = threshold;

    dev->dirty |= RFM12_DIRTY_DATA_FILTER;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_data_filter(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->data_filter = rfm12_data_filter_default;
    dev->dirty |= RFM12_DIRTY_DATA_FILTER;

    return RFM12_OK;
}


static uint16_t rfm12_encode_data_filter(const RFM12_t *dev)
{
    const RFM12_data_filter_t *data_filter;
    data_filter = &dev->data_filter;

    uint16_t command = RFM12_CMD_DATA_FILTER;

    command |= (uint16_t)data_filter->clock_recovery_mode  << 7;
    command |= (uint16_t)data_filter->clock_recovery_speed << 6;
    command |= (uint16_t)data_filter->filter_type          << 4;
    command |= (uint16_t)data_filter->dqd_threshold;

    return command;
}

#pragma endregion

/****************************************************************************\

            7. FIFO and Reset Mode Command

\****************************************************************************/

#pragma region FIFO and Reset Mode Command

RFM12_result_t rfm12_set_fifo_interrupt_level(RFM12_t *dev, RFM12_fifo_interrupt_level_t level)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    /* Valid FIFO interrupt level: 1-15 bits */
    if ((level < 1U) || (level > 15U))
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->fifo_reset_mode.fifo_interrupt_level = level;
    dev->dirty |= RFM12_DIRTY_FIFO_RESET;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_sync_pattern_length(RFM12_t *dev, RFM12_sync_pattern_length_t length)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (length)
    {
        case RFM12_SYNC_PATTERN_2BYTE:
        case RFM12_SYNC_PATTERN_1BYTE:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->fifo_reset_mode.sync_pattern_length = length;
    dev->dirty |= RFM12_DIRTY_FIFO_RESET;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_fifo_fill_start(RFM12_t *dev, RFM12_fifo_fill_start_t mode)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (mode)
    {
        case RFM12_FIFO_FILL_AFTER_SYNC:
        case RFM12_FIFO_FILL_ALWAYS:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->fifo_reset_mode.fifo_fill_start = mode;
    dev->dirty |= RFM12_DIRTY_FIFO_RESET;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_fifo_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->fifo_reset_mode.fifo_enable = enable;
    dev->dirty |= RFM12_DIRTY_FIFO_RESET;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_reset_mode(RFM12_t *dev, RFM12_reset_mode_t mode)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (mode)
    {
        case RFM12_RESET_MODE_SENSITIVE:
        case RFM12_RESET_MODE_NON_SENSITIVE:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->fifo_reset_mode.reset_mode = mode;
    dev->dirty |= RFM12_DIRTY_FIFO_RESET;

    return RFM12_OK;
}

// Helper Functions
#define RFM12_FIFO_FILL_ENABLE_MASK ((uint16_t)1U << 1)

RFM12_result_t rfm12_restart_sync_recognition(RFM12_t *dev)
{
    RFM12_result_t result;
    uint16_t command;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (dev->fifo_reset_mode.fifo_enable != RFM12_ENABLE)
    {
        return RFM12_ERROR_INVALID_CONFIGURATION;
    }

    command = rfm12_encode_fifo_reset_mode(dev);

    /*
     * Clear FIFO fill enable to stop FIFO filling and restart the
     * synchronization-pattern recognition state.
     */
    result = rfm12_send_command(dev, command & ~RFM12_FIFO_FILL_ENABLE_MASK);

    if (result != RFM12_OK)
    {
        return result;
    }

    /*
     * Set FIFO fill enable again. The receiver now waits for a new
     * synchronization pattern before filling the FIFO.
     */
    result = rfm12_send_command(dev, command);

    if (result != RFM12_OK)
    {
        return result;
    }

    dev->dirty &= ~RFM12_DIRTY_FIFO_RESET;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_fifo_reset_mode(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->fifo_reset_mode = rfm12_fifo_reset_mode_default;
    dev->dirty |= RFM12_DIRTY_FIFO_RESET;

    return RFM12_OK;
}

static uint16_t rfm12_encode_fifo_reset_mode(const RFM12_t *dev)
{
    const RFM12_fifo_reset_mode_t *fifo;
    fifo = &dev->fifo_reset_mode;

    uint16_t command = RFM12_CMD_FIFO_RESET_MODE;

    command |= ((uint16_t)fifo->fifo_interrupt_level << 4);
    command |= ((uint16_t)fifo->sync_pattern_length  << 3);
    command |= ((uint16_t)fifo->fifo_fill_start      << 2);
    command |= ((uint16_t)fifo->fifo_enable          << 1);
    command |=  (uint16_t)fifo->reset_mode;

    return command;
}

#pragma endregion

/****************************************************************************\

           8. Sync Pattern Command

\****************************************************************************/

#pragma region Sync Pattern Command

RFM12_result_t rfm12_set_sync_pattern(RFM12_t *dev, RFM12_sync_pattern_t pattern)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->sync_pattern = pattern;
    dev->dirty |= RFM12_DIRTY_SYNC_PATTERN;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_sync_pattern(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->sync_pattern = rfm12_sync_pattern_default;
    dev->dirty |= RFM12_DIRTY_SYNC_PATTERN;

    return RFM12_OK;
}

static uint16_t rfm12_encode_sync_pattern(const RFM12_t *dev)
{
    const RFM12_sync_pattern_t *sync_pattern;
    sync_pattern = &dev->sync_pattern;

    uint16_t command = RFM12_CMD_SYNC_PATTERN;

    command |= (uint16_t)(*sync_pattern);

    return command;
}

#pragma endregion

/****************************************************************************\

          9. Receiver FIFO Read Command

\****************************************************************************/

#pragma region Receiver FIFO Read Command

RFM12_result_t rfm12_read_fifo(RFM12_t *dev, uint8_t *data)
{
    RFM12_result_t result;
    uint16_t rx_data;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (data == NULL)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    result = rfm12_exchange_command(dev, RFM12_CMD_RECEIVER_FIFO_READ, &rx_data);

    if (result != RFM12_OK)
    {
        return result;
    }

    *data = (uint8_t)(rx_data & 0x00FFU);

    return RFM12_OK;
}

#pragma endregion

/****************************************************************************\

            10. AFC Command

\****************************************************************************/

#pragma region AFC Command

RFM12_result_t rfm12_set_afc_mode(RFM12_t *dev, RFM12_afc_mode_t mode)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (mode)
    {
        case RFM12_AFC_MODE_OFF:
        case RFM12_AFC_MODE_ON:
        case RFM12_AFC_MODE_ON_AFTER_RECEIVING:
        case RFM12_AFC_MODE_KEEP_OFFSET_ON_RECEIVE:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->afc.mode = mode;
    dev->dirty |= RFM12_DIRTY_AFC;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_afc_range_limit(RFM12_t *dev, RFM12_afc_range_t range_limit)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    switch (range_limit)
    {
        case RFM12_AFC_RANGE_UNRESTRICTED:
        case RFM12_AFC_RANGE_15_TO_16:
        case RFM12_AFC_RANGE_7_TO_8:
        case RFM12_AFC_RANGE_3_TO_4:
            break;

        default:
            return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->afc.range_limit = range_limit;
    dev->dirty |= RFM12_DIRTY_AFC;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_afc_store_offset(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->afc.store_offset = enable;
    dev->dirty |= RFM12_DIRTY_AFC;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_afc_fine_mode(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->afc.fine_mode = enable;
    dev->dirty |= RFM12_DIRTY_AFC;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_afc_output_register_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->afc.output_register_enable = enable;
    dev->dirty |= RFM12_DIRTY_AFC;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_afc_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->afc.enable = enable;
    dev->dirty |= RFM12_DIRTY_AFC;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_afc(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->afc = rfm12_afc_default;
    dev->dirty |= RFM12_DIRTY_AFC;

    return RFM12_OK;
}

static uint16_t rfm12_encode_afc_command(const RFM12_t *dev)
{
    const RFM12_afc_config_t *afc;
    afc = &dev->afc;

    uint16_t command = RFM12_CMD_AFC;

    command |= ((uint16_t)afc->mode                   << 6);
    command |= ((uint16_t)afc->range_limit            << 4);
    command |= ((uint16_t)afc->store_offset           << 3);
    command |= ((uint16_t)afc->fine_mode              << 2);
    command |= ((uint16_t)afc->output_register_enable << 1);
    command |=  (uint16_t)afc->enable;

    return command;
}

#pragma endregion

/****************************************************************************\

            11. TX Configuration Control Command

\****************************************************************************/

#pragma region TX Configuration Control Command

RFM12_result_t rfm12_set_tx_fsk_polarity(RFM12_t *dev, RFM12_tx_fsk_polarity_t polarity)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (polarity > RFM12_TX_FSK_POLARITY_INVERTED)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->tx_configuration.polarity = polarity;

    dev->dirty |= RFM12_DIRTY_TX_CONFIG;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_tx_fsk_deviation(RFM12_t *dev, RFM12_tx_fsk_deviation_t deviation)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (deviation > RFM12_TX_FSK_DEVIATION_240KHZ)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->tx_configuration.deviation = deviation;

    dev->dirty |= RFM12_DIRTY_TX_CONFIG;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_tx_power(RFM12_t *dev, RFM12_tx_power_t power)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (power > RFM12_TX_POWER_MINUS_21DB)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->tx_configuration.power = power;

    dev->dirty |= RFM12_DIRTY_TX_CONFIG;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_tx_configuration(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->tx_configuration = rfm12_tx_configuration_default;
    dev->dirty |= RFM12_DIRTY_TX_CONFIG;

    return RFM12_OK;
}

static uint16_t rfm12_encode_tx_configuration(const RFM12_t *dev)
{
    const RFM12_tx_configuration_t *config;
    config = &dev->tx_configuration;

    uint16_t command = RFM12_CMD_TX_CONFIG;

    command |= ((uint16_t)config->polarity  << 8);
    command |= ((uint16_t)config->deviation << 4);
    command |=  (uint16_t)config->power;

    return command;
}

#pragma endregion

/****************************************************************************\

            12. PLL Setting Command

\****************************************************************************/

#pragma region PLL Setting Command

RFM12_result_t rfm12_set_pll_output_buffer_current(RFM12_t *dev, RFM12_pll_output_buffer_current_t current)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (current > RFM12_PLL_OUTPUT_BUFFER_CURRENT_3)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->pll_setting.output_buffer_current = current;
    dev->dirty |= RFM12_DIRTY_PLL_SETTING;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_pll_delay_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->pll_setting.delay_enable = enable;
    dev->dirty |= RFM12_DIRTY_PLL_SETTING;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_pll_dithering_disable(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->pll_setting.dithering_disable = enable;
    dev->dirty |= RFM12_DIRTY_PLL_SETTING;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_pll_bandwidth(RFM12_t *dev, RFM12_pll_bandwidth_t bandwidth)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (bandwidth > RFM12_PLL_BANDWIDTH_HIGH)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->pll_setting.bandwidth = bandwidth;
    dev->dirty |= RFM12_DIRTY_PLL_SETTING;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_pll_setting(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->pll_setting = rfm12_pll_setting_default;
    dev->dirty |= RFM12_DIRTY_PLL_SETTING;

    return RFM12_OK;
}

static uint16_t rfm12_encode_pll_setting(const RFM12_t *dev)
{

    const RFM12_pll_setting_t *config;
    config = &dev->pll_setting;

    uint16_t command = RFM12_CMD_PLL_SETTING;

    command |= ((uint16_t)config->output_buffer_current << 5);
    command |= ((uint16_t)config->delay_enable          << 3);
    command |= ((uint16_t)config->dithering_disable     << 2);
    command |=  (uint16_t)config->bandwidth;

    return command;
}

#pragma endregion

/****************************************************************************\
 
            13. Transmitter Register Write Command

\****************************************************************************/

#pragma region Transmitter Register Write Command

RFM12_result_t rfm12_write_tx_register(RFM12_t *dev, uint8_t data)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    return rfm12_send_command(dev, RFM12_CMD_TX_REGISTER_WRITE | data);
}

#pragma endregion

/****************************************************************************\
 
            14. Wake-Up Timer Command

\****************************************************************************/

#pragma region Wake-Up Timer Command

RFM12_result_t rfm12_set_wakeup_timer(RFM12_t *dev, RFM12_wakeup_prescaler_t prescaler, RFM12_wakeup_multiplier_t multiplier)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (prescaler > 31U)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->wakeup_timer.prescaler = prescaler;
    dev->wakeup_timer.multiplier = multiplier;

    dev->dirty |= RFM12_DIRTY_WAKEUP_TIMER;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_wakeup_period_ms(RFM12_t *dev, uint32_t wakeup_ms)
{
    RFM12_result_t result;
    uint8_t r;
    uint8_t m;

    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    result = rfm12_wakeup_ms_to_rm(wakeup_ms, &r, &m);
    if (result != RFM12_OK)
    {
        return result;
    }

    return rfm12_set_wakeup_timer(dev, r, m);
}

static RFM12_result_t rfm12_wakeup_ms_to_rm(uint32_t wakeup_ms, uint8_t *r, uint8_t *m)
{
    uint8_t  best_r = 0U;
    uint8_t  best_m = 0U;
    uint64_t best_error = UINT64_MAX;
    uint64_t requested_scaled;

    if ((r == NULL) || (m == NULL))
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    if (wakeup_ms == 0U)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    /*
     * All timer values are represented in hundredths of a millisecond:
     *
     *     requested_scaled = requested milliseconds * 100
     *
     *     actual_scaled = M * 103 * 2^R
     */
    requested_scaled = (uint64_t)wakeup_ms * 100ULL;

    for (uint8_t current_r = 0U; current_r <= 31U; current_r++)
    {
        uint64_t scaled_step;
        uint64_t candidate_m;
        uint64_t actual_scaled;
        uint64_t error;

        scaled_step = 103ULL * (1ULL << current_r);

        /*
         * Rounded division:
         *
         *     M = requested_scaled / scaled_step
         */
        candidate_m =
            (requested_scaled + (scaled_step / 2ULL)) / scaled_step;

        if ((candidate_m == 0ULL) || (candidate_m > 255ULL))
        {
            continue;
        }

        actual_scaled = candidate_m * scaled_step;

        error =
            (actual_scaled > requested_scaled)
                ? (actual_scaled - requested_scaled)
                : (requested_scaled - actual_scaled);

        if (error < best_error)
        {
            best_error = error;
            best_r = current_r;
            best_m = (uint8_t)candidate_m;

            if (error == 0ULL)
            {
                break;
            }
        }
    }

    if (best_error == UINT64_MAX)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    *r = best_r;
    *m = best_m;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_wakeup_timer(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->wakeup_timer = rfm12_wakeup_timer_default;
    dev->dirty |= RFM12_DIRTY_WAKEUP_TIMER;

    return RFM12_OK;
}

static uint16_t rfm12_encode_wakeup_timer(const RFM12_t *dev)
{
    const RFM12_wakeup_timer_settings_t *wakeup_timer;
    wakeup_timer = &dev->wakeup_timer;

    uint16_t command = RFM12_CMD_WAKEUP_TIMER;

    command |= ((uint16_t)wakeup_timer->prescaler << 8);
    command |=  (uint16_t)wakeup_timer->multiplier;

    return command;
}

#pragma endregion

/****************************************************************************\
 
            15. Low Duty-Cycle Command

\****************************************************************************/

#pragma region Low Duty-Cycle Command

#define RFM12_LOW_DUTY_CYCLE_D_MAX  127U

RFM12_result_t rfm12_set_low_duty_cycle_d(RFM12_t *dev, RFM12_low_duty_cycle_d_t d)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (d > RFM12_LOW_DUTY_CYCLE_D_MAX)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->low_duty_cycle.d = d;
    dev->dirty |= RFM12_DIRTY_LOW_DUTY_CYCLE;

    return RFM12_OK;
}


RFM12_result_t rfm12_set_low_duty_cycle_enable(RFM12_t *dev, RFM12_enable_t enable)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->low_duty_cycle.enable = enable;
    dev->dirty |= RFM12_DIRTY_LOW_DUTY_CYCLE;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_low_duty_cycle(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->low_duty_cycle = rfm12_low_duty_cycle_default;
    dev->dirty |= RFM12_DIRTY_LOW_DUTY_CYCLE;

    return RFM12_OK;
}

static uint16_t rfm12_encode_low_duty_cycle(const RFM12_t *dev)
{
    const RFM12_low_duty_cycle_config_t *low_duty_cycle;
    low_duty_cycle = &dev->low_duty_cycle;

    uint16_t command = RFM12_CMD_LOW_DUTY_CYCLE;

    command |= ((uint16_t)low_duty_cycle->d      << 1);
    command |=  (uint16_t)low_duty_cycle->enable;

    return command;
}

#pragma endregion

/****************************************************************************\
 
            16. Low Battery Detector Command

\****************************************************************************/

#pragma region Low Battery Detector Command

RFM12_result_t rfm12_set_clock_output_frequency(RFM12_t *dev, RFM12_clock_output_frequency_t frequency)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (frequency > RFM12_CLOCK_OUTPUT_10MHZ)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    dev->low_battery_clock.clock_frequency = frequency;

    dev->dirty |= RFM12_DIRTY_LOW_BATTERY_CLOCK;

    return RFM12_OK;
}

RFM12_result_t rfm12_set_low_battery_threshold(RFM12_t *dev, RFM12_low_battery_threshold_t threshold)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (threshold > RFM12_LOW_BATTERY_THRESHOLD_3_75V)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }


    dev->low_battery_clock.low_battery_threshold = threshold;

    dev->dirty |= RFM12_DIRTY_LOW_BATTERY_CLOCK;

    return RFM12_OK;
}

RFM12_result_t rfm12_reset_low_battery_clock(RFM12_t *dev)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    dev->low_battery_clock = rfm12_low_battery_clock_default;
    dev->dirty |= RFM12_DIRTY_LOW_BATTERY_CLOCK;

    return RFM12_OK;
}

static uint16_t rfm12_encode_low_battery_clock_config(const RFM12_t *dev)
{
    const RFM12_low_battery_clock_config_t *config;
    config = &dev->low_battery_clock;

    uint16_t command = RFM12_CMD_LOW_BATTERY_CLOCK_DIVIDER;

    command |= ((uint16_t)config->clock_frequency      << 5);
    command |=  (uint16_t)config->low_battery_threshold;

    return command;
}

#pragma endregion

/****************************************************************************\
 
            17. Status Read Command

\****************************************************************************/

#pragma region Status Read Command

RFM12_result_t rfm12_read_status(RFM12_t *dev, RFM12_status_word_t *status)
{
    if (dev == NULL)
    {
        return RFM12_ERROR_INVALID_HANDLE;
    }

    if (status == NULL)
    {
        return RFM12_ERROR_INVALID_ARGUMENT;
    }

    return rfm12_exchange_command(dev, RFM12_CMD_STATUS_READ, status);

}

// 
// Helper Functions
//

/* -------------------------------------------------------------------------
 * Mode-dependent status bits
 * ------------------------------------------------------------------------- */


/* Bit 15: FFIT while receiving */
bool rfm12_status_rx_fifo_interrupt(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_FFIT_RGIT_MASK) != 0U);
}

/* Bit 15: RGIT while transmitting */
bool rfm12_status_tx_register_ready(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_FFIT_RGIT_MASK) != 0U);
}

/* Bit 13: FFOV while receiving */
bool rfm12_status_rx_fifo_overflow(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_FFOV_RGUR_MASK) != 0U);
}

/* Bit 13: RGUR while transmitting */
bool rfm12_status_tx_register_underrun(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_FFOV_RGUR_MASK) != 0U);
}


/* -------------------------------------------------------------------------
 * Interrupt and event status bits
 * ------------------------------------------------------------------------- */

/* Bit 14: POR */
bool rfm12_status_power_on_reset(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_POR_MASK) != 0U);
}

/* Bit 12: WKUP */
bool rfm12_status_wakeup_timer_expired(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_WKUP_MASK) != 0U);
}

/* Bit 11: EXT */
bool rfm12_status_external_interrupt(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_EXT_MASK) != 0U);
}

/* Bit 10: LBD */
bool rfm12_status_low_battery(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_LBD_MASK) != 0U);
}

/* -------------------------------------------------------------------------
 * Receiver and FIFO state bits
 * ------------------------------------------------------------------------- */



/* Bit 9: FFEM */
bool rfm12_status_rx_fifo_empty(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_FFEM_MASK) != 0U);
}

/* Bit 8: ATS */
bool rfm12_status_antenna_tuning_signal(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_ATS_MASK) != 0U);
}

/* Bit 7: RSSI */
bool rfm12_status_rssi_detected(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_RSSI_MASK) != 0U);
}

/* Bit 6: DQD */
bool rfm12_status_data_quality_detected(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_DQD_MASK) != 0U);
}

/* Bit 5: CRL */
bool rfm12_status_clock_recovery_locked(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_CRL_MASK) != 0U);
}

/* -------------------------------------------------------------------------
 * AFC status and measurement
 * ------------------------------------------------------------------------- */

/* Bit 4: ATGL */
bool rfm12_status_afc_cycle_toggle(RFM12_status_word_t status)
{
    return ((status & RFM12_STATUS_ATGL_MASK) != 0U);
}

/* Bits 3:0: signed AFC offset value */
int8_t rfm12_status_afc_offset_steps(RFM12_status_word_t status)
{
    int8_t offset;

    offset = (int8_t)(status & RFM12_STATUS_AFC_OFFSET_MASK);

    /* Sign-extend 4-bit two's-complement value */
    if ((offset & 0x08) != 0)
    {
        offset |= (int8_t)0xF0;
    }

    return offset;
}

#pragma endregion
