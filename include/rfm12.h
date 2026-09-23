/*
 * -------------------------------------------------------------------------
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * rfm12.h
 *
 * RFM12B radio driver public API.
 * -------------------------------------------------------------------------
 */

#ifndef RFM12_H
#define RFM12_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * -------------------------------------------------------------------------
 *
 * Data Types
 * 
 * -------------------------------------------------------------------------
 */

#pragma region Data Types

// // API return result type

typedef enum
{
    RFM12_OK = 0,

    // General errors
    RFM12_ERROR_INVALID_HANDLE,
    RFM12_ERROR_INVALID_ARGUMENT,
    RFM12_ERROR_INVALID_CONFIGURATION,
    RFM12_ERROR_NO_INSTANCE_AVAILABLE,
    RFM12_ERROR_NOT_INITIALIZED,

    // Catch-all
    RFM12_ERROR_UNKNOWN

} RFM12_result_t;

//
// Main RFM12 device structure
//

typedef struct RFM12 RFM12_t;

//
// Enable/Disable type
//

typedef bool RFM12_enable_t;

#define RFM12_ENABLE  ((RFM12_enable_t)true)
#define RFM12_DISABLE ((RFM12_enable_t)false)

// current rx/tx state
typedef enum
{
    RFM12_MODE_UNKNOWN = 0,
    RFM12_MODE_STANDBY,
    RFM12_MODE_IDLE,
    RFM12_MODE_RX,
    RFM12_MODE_TX,
    RFM12_MODE_SLEEP

} RFM12_mode_t;

//
// SPI transfer function type
//

typedef RFM12_result_t (*RFM12_spi_transfer16_fn)(void *context, uint16_t tx_word, uint16_t *rx_word);

#pragma endregion

/*--------------------- Configuration Setting Command -----------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  |
--------------------|-----------------|-------------------|--------------------
| 1  | 0  | 0  | 0  | 0  | 0  | 0 | 0 | el | ef | b1 | b0 | x3 | x2 | x1 | x0 |
\*---------------------------------------------------------------------------*/
// POR: 0x8008

#pragma region Configuration Setting Command

// Frequency Band
// Bits: | b1 | b0 |
typedef enum 
{
    RFM12_BAND_433  = 0b01,
    RFM12_BAND_868  = 0b10,
    RFM12_BAND_915  = 0b11
} RFM12_frequency_band_t;

// Crystal Load Capacitance [pF]
// Bits: | x3 | x2 | x1 | x0 |
typedef enum 
{
    RFM12_XTAL_CAP_8_5PF  = 0x00,
    RFM12_XTAL_CAP_9_0PF  = 0x01,
    RFM12_XTAL_CAP_9_5PF  = 0x02,
    RFM12_XTAL_CAP_10_0PF = 0x03,
    RFM12_XTAL_CAP_10_5PF = 0x04,
    RFM12_XTAL_CAP_11_0PF = 0x05,
    RFM12_XTAL_CAP_11_5PF = 0x06,
    RFM12_XTAL_CAP_12_0PF = 0x07,
    RFM12_XTAL_CAP_12_5PF = 0x08,
    RFM12_XTAL_CAP_13_0PF = 0x09,
    RFM12_XTAL_CAP_13_5PF = 0x0A,
    RFM12_XTAL_CAP_14_0PF = 0x0B,
    RFM12_XTAL_CAP_14_5PF = 0x0C,
    RFM12_XTAL_CAP_15_0PF = 0x0D,
    RFM12_XTAL_CAP_15_5PF = 0x0E,
    RFM12_XTAL_CAP_16_0PF = 0x0F
} RFM12_xtal_cap_t;

#pragma endregion

/*--------------------- Power Management Command      -----------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  |
--------------------|-----------------|-------------------|--------------------
| 1  | 0  | 0  | 0  | 0  | 0  | 1 | 0 | er | ebb| et | es | ex | eb | ew | dc |
\*---------------------------------------------------------------------------*/

#pragma region Power Management Command

#pragma endregion

/*--------------------- Frequency Setting Command     -----------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  |
--------------------|-----------------|-------------------|--------------------
| 1  | 0  | 1  | 0  | f11| f10| f9| f8| f7 | f6 | f5 | f4 | f3 | f2 | f1 | f0 |
\*---------------------------------------------------------------------------*/

#pragma region Frequency Setting Command
/*--------------------------------------------------------------------------*\

 Fc = 10 * C1 * (C2 + (F / 4000)) [MHz] : 96 <= F <= 3903

 --------------------------------------------------------
 Band   | C1 | C2 | | Min Freq  | Max Freq  | Freq Step |
 -------|----|----| |-----------|-----------|-----------|
 433    | 1  | 43 | | 430.2400  | 439.7575  | 2.5 kHz   |
 868    | 2  | 43 | | 860.4800  | 879.5150  | 5.0 kHz   |
 915    | 3  | 30 | | 900.7200  | 929.2725  | 7.5 kHz   |
 --------------------------------------------------------

\*--------------------------------------------------------------------------*/

typedef uint32_t RFM12_frequency_hz_t;

#pragma endregion

/*--------------------- Data Rate Command             -----------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  |
--------------------|-----------------|-------------------|--------------------
| 1  | 1  | 0  | 0  | 0  | 1  | 1 | 0 | cs | r6 | r5 | r4 | r3 | r2 | r1 | r0 |
\*---------------------------------------------------------------------------*/

#pragma region Data Rate Command
typedef enum
{
    RFM12_DATA_RATE_1200 = 0,
    RFM12_DATA_RATE_2400,
    RFM12_DATA_RATE_4800,
    RFM12_DATA_RATE_9600,
    RFM12_DATA_RATE_19200,
    RFM12_DATA_RATE_28800,
    RFM12_DATA_RATE_38400,
    RFM12_DATA_RATE_57600,
    RFM12_DATA_RATE_115200,

    RFM12_DATA_RATE_COUNT

} RFM12_data_rate_t;

#pragma endregion

/*--------------------- Receiver Control Command      -----------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  |
--------------------|-----------------|-------------------|--------------------
| 1  | 0  | 0  | 1  | 0  | p16| d1| d0| i2 | i1 | i0 | g1 | g0 | r2 | r1 | r0 |
\*---------------------------------------------------------------------------*/

#pragma region Receiver Control Command

// Bits:[10] (p16)
typedef enum
{
    RFM12_PIN16_INTERRUPT_IN    = 0b0,
    RFM12_PIN16_VDI_OUT         = 0b1
} RFM12_pin16_function_t;

// Bits:[9:8] (d1..d0)
typedef enum
{
    RFM12_VDI_FAST   = 0b00,
    RFM12_VDI_MEDIUM = 0b01,
    RFM12_VDI_SLOW   = 0b10,
    RFM12_VDI_ALWAYS = 0b11

} RFM12_vdi_mode_t;

// Bits:[7:5] (i2..i0)
typedef enum
{
    RFM12_RX_BANDWIDTH_400_KHZ = 0b001,
    RFM12_RX_BANDWIDTH_340_KHZ = 0b010,
    RFM12_RX_BANDWIDTH_270_KHZ = 0b011,
    RFM12_RX_BANDWIDTH_200_KHZ = 0b100,
    RFM12_RX_BANDWIDTH_134_KHZ = 0b101,
    RFM12_RX_BANDWIDTH_67_KHZ  = 0b110
} RFM12_rx_bandwidth_t;

// G_lna
// Bits:[4:3] (g1..g0)
typedef enum 
{
    RFM12_LNA_GAIN_0_DB      = 0b00,     //   0 dB
    RFM12_LNA_GAIN_M6_DB     = 0b01,     //  -6 dB
    RFM12_LNA_GAIN_M14_DB    = 0b10,     // -14 dB
    RFM12_LNA_GAIN_M20_DB    = 0b11      // -20 dB
} RFM12_lna_gain_t;

// RSSI_th = RSSI_setth + G_lna
// Bits:[2:0] (r2..r0) RSSI_setth
typedef enum
{
    RFM12_RSSI_THRESHOLD_M103_DBM = 0b000,      // -103 dBm
    RFM12_RSSI_THRESHOLD_M97_DBM  = 0b001,      // -97  dBm
    RFM12_RSSI_THRESHOLD_M91_DBM  = 0b010,      // -91  dBm
    RFM12_RSSI_THRESHOLD_M85_DBM  = 0b011,      // -85  dBm
    RFM12_RSSI_THRESHOLD_M79_DBM  = 0b100,      // -79  dBm
    RFM12_RSSI_THRESHOLD_M73_DBM  = 0b101       // -73  dBm
} RFM12_rssi_threshold_t;

#pragma endregion

/*--------------------- Data Filter Command           -----------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  |
--------------------|-----------------|-------------------|--------------------
| 1  | 1  | 0  | 0  | 0  | 0  | 1 | 0 | al | ml | 1  | s  | 1  | f2 | f1 | f0 |
\*---------------------------------------------------------------------------*/

#pragma region Data Filter Command

// Bits: [7] (al)
typedef enum
{
    RFM12_CLOCK_RECOVERY_MANUAL = 0b0,
    RFM12_CLOCK_RECOVERY_AUTO   = 0b1

} RFM12_clock_recovery_mode_t;

// Bits: [6] (ml)
// Used only when clock recovery is set to manual mode.
typedef enum
{
    RFM12_CLOCK_RECOVERY_SLOW = 0b0,
    RFM12_CLOCK_RECOVERY_FAST = 0b1

} RFM12_clock_recovery_speed_t;

// Bits: [4] (s)
typedef enum
{
    RFM12_FILTER_DIGITAL   = 0b0,
    RFM12_FILTER_ANALOG_RC = 0b1

} RFM12_filter_type_t;

// Bits: [2:0] (f2..f0)
// Data Quality Detector threshold.
//
// Recommended operating range: 5..7
// Hardware-supported range:   0..7
//
// DQDpar = 4 x (deviation - TX/RX offset) / bit rate
typedef enum
{
    RFM12_DQD_THRESHOLD_0 = 0b000,
    RFM12_DQD_THRESHOLD_1 = 0b001,
    RFM12_DQD_THRESHOLD_2 = 0b010,
    RFM12_DQD_THRESHOLD_3 = 0b011,
    RFM12_DQD_THRESHOLD_4 = 0b100,
    RFM12_DQD_THRESHOLD_5 = 0b101,
    RFM12_DQD_THRESHOLD_6 = 0b110,
    RFM12_DQD_THRESHOLD_7 = 0b111

} RFM12_dqd_threshold_t;

#pragma endregion

/*--------------------- FIFO and Reset Mode Command   -----------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  |
--------------------|-----------------|-------------------|--------------------
| 1  | 1  | 0  | 0  | 0  | 0  | 1 | 0 | f3 | f2 | f1 | f0 | sp | al | ff | dr |
\*---------------------------------------------------------------------------*/

#pragma region FIFO and Reset Mode Command

// Bits: [7:4] (f3..f0)
/* Receiver FIFO interrupt level: 1..15 */
typedef uint8_t RFM12_fifo_interrupt_level_t;

// Bits: [3] (sp)
typedef enum
{
    RFM12_SYNC_PATTERN_2BYTE = 0b0,
    RFM12_SYNC_PATTERN_1BYTE = 0b1

} RFM12_sync_pattern_length_t;

// Bits: [2] (al)
typedef enum
{
    RFM12_FIFO_FILL_AFTER_SYNC = 0b0,
    RFM12_FIFO_FILL_ALWAYS     = 0b1

} RFM12_fifo_fill_start_t;

// Bits: [0] (dr)
typedef enum
{
    RFM12_RESET_MODE_SENSITIVE     = 0b0,
    RFM12_RESET_MODE_NON_SENSITIVE = 0b1

} RFM12_reset_mode_t;

#pragma endregion

/*--------------------- Sync Pattern Command      ---------------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  |
------------------------------------|------------------------------------------
| 1  | 1  | 0  | 0  | 1  | 1  | 1 | 0 | b7 | b6 | b5 | b4 | b3 | b2 | b1 | b0 |
\*---------------------------------------------------------------------------*/

#pragma region Sync Pattern Command

typedef uint8_t RFM12_sync_pattern_t;

#pragma endregion

/*--------------------- AFC Command                  --------------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7  | 6  | 5   | 4   | 3  | 2  | 1  | 0  |
--------------------|-----------------|----|----|-----|-----|----|----|----|----
| 1  | 1  | 0  | 0  | 0  | 1  | 0 | 0 | a1 | a0 | rl1 | rl0 | st | fi | oe | en |
\*-----------------------------------------------------------------------------*/

#pragma region AFC Command

// Bits: [7:6] (a1:a0)
typedef enum
{
    RFM12_AFC_MODE_OFF                     = 0b00,
    RFM12_AFC_MODE_ON                      = 0b01,
    RFM12_AFC_MODE_ON_AFTER_RECEIVING      = 0b10,
    RFM12_AFC_MODE_KEEP_OFFSET_ON_RECEIVE  = 0b11

} RFM12_afc_mode_t;

// Bits: [5:4] (rl1:rl0)
typedef enum
{
    RFM12_AFC_RANGE_UNRESTRICTED = 0b00,
    RFM12_AFC_RANGE_15_TO_16     = 0b01,
    RFM12_AFC_RANGE_7_TO_8       = 0b10,
    RFM12_AFC_RANGE_3_TO_4       = 0b11

} RFM12_afc_range_t;

#pragma endregion

/*--------------------- TX Configuration Control Command --------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8  | 7  | 6  | 5  | 4  | 3 | 2  | 1  | 0  |
---------------------|-------------------|-------------------------------------
| 1  | 0  | 0  | 1  | 1  | 0  | 0 | mp | m3 | m2 | m1 | m0 | 0 | p2 | p1 | p0 |
\*---------------------------------------------------------------------------*/
// POR: 0x9800

#pragma region TX Configuration Control Command

typedef enum
{
    // Logic 0 → Lower frequency
    // Logic 1 → Higher frequency
    RFM12_TX_FSK_POLARITY_NORMAL = 0,

    // Logic 0 → Higher frequency
    // Logic 1 → Lower frequency
    RFM12_TX_FSK_POLARITY_INVERTED = 1

} RFM12_tx_fsk_polarity_t;

// Bits: [7:4] (m3:m0)
// Frequency deviation
typedef enum
{
    RFM12_TX_FSK_DEVIATION_15KHZ  = 0b0000,
    RFM12_TX_FSK_DEVIATION_30KHZ  = 0b0001,
    RFM12_TX_FSK_DEVIATION_45KHZ  = 0b0010,
    RFM12_TX_FSK_DEVIATION_60KHZ  = 0b0011,
    RFM12_TX_FSK_DEVIATION_75KHZ  = 0b0100,
    RFM12_TX_FSK_DEVIATION_90KHZ  = 0b0101,
    RFM12_TX_FSK_DEVIATION_105KHZ = 0b0110,
    RFM12_TX_FSK_DEVIATION_120KHZ = 0b0111,
    RFM12_TX_FSK_DEVIATION_135KHZ = 0b1000,
    RFM12_TX_FSK_DEVIATION_150KHZ = 0b1001,
    RFM12_TX_FSK_DEVIATION_165KHZ = 0b1010,
    RFM12_TX_FSK_DEVIATION_180KHZ = 0b1011,
    RFM12_TX_FSK_DEVIATION_195KHZ = 0b1100,
    RFM12_TX_FSK_DEVIATION_210KHZ = 0b1101,
    RFM12_TX_FSK_DEVIATION_225KHZ = 0b1110,
    RFM12_TX_FSK_DEVIATION_240KHZ = 0b1111

} RFM12_tx_fsk_deviation_t;

// Bit: [3] is always 0

// Bits: [2:0] (p2:p0)
// TX output power (attenuation from maximum output)
typedef enum
{
    RFM12_TX_POWER_0DB        = 0b000,  // Maximum output
    RFM12_TX_POWER_MINUS_3DB  = 0b001,
    RFM12_TX_POWER_MINUS_6DB  = 0b010,
    RFM12_TX_POWER_MINUS_9DB  = 0b011,
    RFM12_TX_POWER_MINUS_12DB = 0b100,
    RFM12_TX_POWER_MINUS_15DB = 0b101,
    RFM12_TX_POWER_MINUS_18DB = 0b110,
    RFM12_TX_POWER_MINUS_21DB = 0b111

} RFM12_tx_power_t;

#pragma endregion

/*--------------------- PLL Setting Command                 -----------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9  | 8  | 7 | 6   | 5   | 4 | 3   | 2    | 1 | 0   |
----------------------------------------|---|-----|-----|---|-----|------|---|-----|
| 1  | 1  | 0  | 0  | 1  | 1  | 0  | 0  | 0 | ob1 | ob0 | 1 | dly | ddit | 1 | bw0 |
\*---------------------------------------------------------------------------------*/

/*

 * The RFM12B datasheet recommends retaining the POR defaults for typical
 * applications. Changes to PLL settings may affect the transmitted RF
 * spectrum and should be verified with appropriate RF test equipment.
  
 */

#pragma region PLL Setting Command

// Bits: [6:5] (ob1:ob0)
// PLL output buffer current
typedef enum
{
    RFM12_PLL_OUTPUT_BUFFER_CURRENT_0 = 0b00,
    RFM12_PLL_OUTPUT_BUFFER_CURRENT_1 = 0b01,
    RFM12_PLL_OUTPUT_BUFFER_CURRENT_2 = 0b10,
    RFM12_PLL_OUTPUT_BUFFER_CURRENT_3 = 0b11

} RFM12_pll_output_buffer_current_t;

// Bit: [0] (bw0)
// PLL loop bandwidth
typedef enum
{
    RFM12_PLL_BANDWIDTH_NORMAL = 0b0,   // Normal bandwidth (default)
    RFM12_PLL_BANDWIDTH_HIGH   = 0b1    // Increased bandwidth

} RFM12_pll_bandwidth_t;

#pragma endregion

/*--------------------- Wake-Up Timer Command          -----------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9  | 8  | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  |
--------------------|-------------------------|--------------------------------
| 1  | 1  | 1  | r4 | r3 | r2 | r1 | r0 | m7 | m6 | m5 | m4 | m3 | m2 | m1 | m0 |
\*---------------------------------------------------------------------------*/

#pragma region Wake-Up Timer Command

/* Wake-up timer prescaler R: 0..31 */
typedef uint8_t RFM12_wakeup_prescaler_t;

/* Wake-up timer multiplier M: 0..255 */
typedef uint8_t RFM12_wakeup_multiplier_t;

#pragma endregion

/*--------------------- Low Duty-Cycle Command         -----------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7  | 6  | 5  | 4  | 3  | 2  | 1  | 0  |
-------------------------------------------------------------------------------
| 1  | 1  | 0  | 0  | 1  | 0  | 0 | 0 | d6 | d5 | d4 | d3 | d2 | d1 | d0 | en |
\*---------------------------------------------------------------------------*/

#pragma region Low Duty-Cycle Command

// Bits: [7:1] (d6:d0)
/* Low-duty-cycle D field: 0..127 */
typedef uint8_t RFM12_low_duty_cycle_d_t;

#pragma endregion

/*---------------- Low Battery / Clock Divider Command ----------------------*\
| 15 | 14 | 13 | 12 | 11 | 10 | 9 | 8 | 7  | 6  | 5  | 4 | 3  | 2  | 1  | 0  |
|----|----|----|----|----|----|---|---|----|----|----|---|----|----|----|----|
| 1  | 1  | 0  | 0  | 0  | 0  | 0 | 0 | d2 | d1 | d0 | 0 | v3 | v2 | v1 | v0 |
\*---------------------------------------------------------------------------*/

#pragma region Low Battery / Clock Divider Command

/*
 * Bits [7:5] — d2:d0
 *
 * Microcontroller clock output frequency.
 */
typedef enum
{
    RFM12_CLOCK_OUTPUT_1MHZ    = 0b000,
    RFM12_CLOCK_OUTPUT_1_25MHZ = 0b001,
    RFM12_CLOCK_OUTPUT_1_66MHZ = 0b010,
    RFM12_CLOCK_OUTPUT_2MHZ    = 0b011,
    RFM12_CLOCK_OUTPUT_2_5MHZ  = 0b100,
    RFM12_CLOCK_OUTPUT_3_33MHZ = 0b101,
    RFM12_CLOCK_OUTPUT_5MHZ    = 0b110,
    RFM12_CLOCK_OUTPUT_10MHZ   = 0b111

} RFM12_clock_output_frequency_t;

/*
 * Bits [3:0] — v3:v0
 *
 * Low-battery detector threshold:
 *
 *     VLB = 2.25 V + (value × 0.10 V)
 */
typedef enum
{
    RFM12_LOW_BATTERY_THRESHOLD_2_25V = 0b0000,
    RFM12_LOW_BATTERY_THRESHOLD_2_35V = 0b0001,
    RFM12_LOW_BATTERY_THRESHOLD_2_45V = 0b0010,
    RFM12_LOW_BATTERY_THRESHOLD_2_55V = 0b0011,
    RFM12_LOW_BATTERY_THRESHOLD_2_65V = 0b0100,
    RFM12_LOW_BATTERY_THRESHOLD_2_75V = 0b0101,
    RFM12_LOW_BATTERY_THRESHOLD_2_85V = 0b0110,
    RFM12_LOW_BATTERY_THRESHOLD_2_95V = 0b0111,
    RFM12_LOW_BATTERY_THRESHOLD_3_05V = 0b1000,
    RFM12_LOW_BATTERY_THRESHOLD_3_15V = 0b1001,
    RFM12_LOW_BATTERY_THRESHOLD_3_25V = 0b1010,
    RFM12_LOW_BATTERY_THRESHOLD_3_35V = 0b1011,
    RFM12_LOW_BATTERY_THRESHOLD_3_45V = 0b1100,
    RFM12_LOW_BATTERY_THRESHOLD_3_55V = 0b1101,
    RFM12_LOW_BATTERY_THRESHOLD_3_65V = 0b1110,
    RFM12_LOW_BATTERY_THRESHOLD_3_75V = 0b1111

} RFM12_low_battery_threshold_t;

#pragma endregion

/*--------------------------- Status Read Command ---------------------------*\
| 15        | 14  | 13        | 12   | 11  | 10  | 9    | 8   |
| RGIT/FFIT | POR | RGUR/FFOV | WKUP | EXT | LBD | FFEM | ATS |
|-----------|-----|-----------|------|-----|-----|------|-----|
| 7         | 6   | 5         | 4    | 3   | 2   | 1    |  0  |
| RSSI      | DQD | CRL       | ATGL |   AFC offset           |
\*---------------------------------------------------------------------------*/

#pragma region Status Read Command

typedef uint16_t RFM12_status_word_t;

#pragma endregion

//
// Function Prototypes
//

#pragma region API Prototypes

//
// API Functions
//

/**
 * Obtain a statically allocated RFM12 driver instance.
 *
 * Each successful call obtains the next available instance from the
 * compile-time pool of RFM12_NUM_RADIOS instances. The returned instance
 * remains valid for the lifetime of the program and must not be freed.
 *
 * Instances cannot be released or reused. After all instances have been
 * obtained, the function returns RFM12_ERROR_NO_INSTANCE_AVAILABLE and
 * sets *instance to NULL.
 *
 * This function is intended to be called during application initialization
 * and is not thread-safe.
 */
RFM12_result_t rfm12_get_instance(                  RFM12_t **instance);

RFM12_result_t rfm12_configure_hal(                 RFM12_t *dev, RFM12_spi_transfer16_fn function, void *context);
RFM12_result_t rfm12_apply_to_radio(                RFM12_t *dev);

/**
 * Immediately issue the radio's software-reset sequence.
 *
 * This enables sensitive reset and then sends FE00h without applying or
 * modifying staged configuration. On success, all staged configuration is
 * marked pending so it can be restored with rfm12_apply_to_radio() after the
 * radio has completed its reset/startup delay.
 */
RFM12_result_t rfm12_software_reset(                RFM12_t *dev);

const char *rfm12_result_string(                    RFM12_result_t result);
RFM12_result_t rfm12_get_sync_bytes(          const RFM12_t *dev, uint8_t *buffer, uint8_t buffer_size, uint8_t *length);
RFM12_result_t rfm12_get_mode(                const RFM12_t *dev, RFM12_mode_t *mode);



/*
 * Configuration setters and command-reset functions update the driver's
 * staged configuration and mark the corresponding command as pending. They
 * do not communicate with the radio. Call rfm12_apply_to_radio() to send
 * pending configuration commands.
 *
 * The following operations perform immediate SPI transfers:
 *
 *   rfm12_apply_to_radio()
 *   rfm12_software_reset()
 *   rfm12_enter_standby_mode()
 *   rfm12_enter_rx_mode()
 *   rfm12_enter_tx_mode()
 *   rfm12_enter_idle_mode()
 *   rfm12_enter_sleep_mode()
 *   rfm12_restart_sync_recognition()
 *   rfm12_read_fifo()
 *   rfm12_write_tx_register()
 *   rfm12_read_status()
 *
 * Mode-entry functions write the complete Power Management Command and
 * therefore also apply any pending settings belonging to that command.
 *
 * rfm12_restart_sync_recognition() writes the complete FIFO and Reset Mode
 * Command and therefore applies any pending settings belonging to that
 * command.
 */

// 1. Configuration Setting Command

RFM12_result_t rfm12_set_tx_data_register_enable(   RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_rx_fifo_mode_enable(       RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_frequency_band(            RFM12_t *dev, RFM12_frequency_band_t band);
RFM12_result_t rfm12_set_xtal_cap(                  RFM12_t *dev, RFM12_xtal_cap_t cap);
RFM12_result_t rfm12_reset_config_setting(          RFM12_t *dev);

// 2. Power Management Command

RFM12_result_t rfm12_set_receiver_enable(             RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_baseband_enable(             RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_transmitter_enable(          RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_synthesizer_enable(          RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_crystal_oscillator_enable(   RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_low_battery_detector_enable( RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_wakeup_timer_enable(         RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_clock_output_enable(         RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_reset_power_management(          RFM12_t *dev);

// Helper Functions for Power Management Command

RFM12_result_t rfm12_enter_rx_mode(                   RFM12_t *dev);
RFM12_result_t rfm12_enter_tx_mode(                   RFM12_t *dev);
RFM12_result_t rfm12_enter_idle_mode(                 RFM12_t *dev);
RFM12_result_t rfm12_enter_standby_mode(              RFM12_t *dev);
RFM12_result_t rfm12_enter_sleep_mode(                RFM12_t *dev);

// 3. Frequency Setting Command

RFM12_result_t rfm12_set_frequency(                 RFM12_t *dev, RFM12_frequency_hz_t frequency_hz);
RFM12_result_t rfm12_get_frequency(           const RFM12_t *dev, RFM12_frequency_hz_t *frequency_hz);
RFM12_result_t rfm12_reset_frequency(               RFM12_t *dev);

// 4. Data Rate Command

RFM12_result_t rfm12_set_data_rate(                 RFM12_t *dev, RFM12_data_rate_t rate);
RFM12_result_t rfm12_get_data_rate(           const RFM12_t *dev, RFM12_data_rate_t *rate);
RFM12_result_t rfm12_reset_data_rate(               RFM12_t *dev);

// 5. Receiver Control Command

RFM12_result_t rfm12_set_pin16_function(            RFM12_t *dev, RFM12_pin16_function_t pin16);
RFM12_result_t rfm12_set_vdi_mode(                  RFM12_t *dev, RFM12_vdi_mode_t mode);
RFM12_result_t rfm12_set_rx_bandwidth(              RFM12_t *dev, RFM12_rx_bandwidth_t bandwidth);
RFM12_result_t rfm12_set_lna_gain(                  RFM12_t *dev, RFM12_lna_gain_t gain);
RFM12_result_t rfm12_set_rssi_threshold(            RFM12_t *dev, RFM12_rssi_threshold_t threshold);
RFM12_result_t rfm12_reset_receiver_control(        RFM12_t *dev);

// 6. Data Filter Command

RFM12_result_t rfm12_set_clock_recovery_mode(       RFM12_t *dev, RFM12_clock_recovery_mode_t mode);
RFM12_result_t rfm12_set_clock_recovery_speed(      RFM12_t *dev, RFM12_clock_recovery_speed_t speed);
RFM12_result_t rfm12_set_filter_type(               RFM12_t *dev, RFM12_filter_type_t filter_type);
RFM12_result_t rfm12_set_dqd_threshold(             RFM12_t *dev, RFM12_dqd_threshold_t threshold);
RFM12_result_t rfm12_reset_data_filter(             RFM12_t *dev);

// 7. FIFO and Reset Mode Command

RFM12_result_t rfm12_set_fifo_interrupt_level(      RFM12_t *dev, RFM12_fifo_interrupt_level_t level);
RFM12_result_t rfm12_set_sync_pattern_length(       RFM12_t *dev, RFM12_sync_pattern_length_t length);
RFM12_result_t rfm12_set_fifo_fill_start(           RFM12_t *dev, RFM12_fifo_fill_start_t mode);
RFM12_result_t rfm12_set_rx_fifo_fill_enable(       RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_reset_mode(                RFM12_t *dev, RFM12_reset_mode_t mode);
RFM12_result_t rfm12_reset_fifo_reset_mode(         RFM12_t *dev);

// Helper Function For FIFO and Reset Mode Command

/**
 * Restart receiver synchronization-pattern recognition.
 *
 * The RFM12B datasheet specifies that synchronization-pattern recognition
 * is restarted by clearing and then setting the FIFO fill-enable bit.
 *
 * The configured FIFO fill-enable field must already be enabled. This
 * function writes the FIFO and Reset Mode Command twice without modifying
 * the staged FIFO configuration stored in the device.
 * 
 * Note: 
 * Any pending FIFO and Reset Mode settings are applied as part of this
 * operation.
 */

RFM12_result_t rfm12_restart_sync_recognition(      RFM12_t *dev);

// 8. Sync Pattern Command

RFM12_result_t rfm12_set_sync_pattern(              RFM12_t *dev, RFM12_sync_pattern_t pattern);
RFM12_result_t rfm12_reset_sync_pattern(            RFM12_t *dev);

// 9. Receiver FIFO Read Command

RFM12_result_t rfm12_read_fifo(                     RFM12_t *dev, uint8_t *data);

// 10. AFC Command

RFM12_result_t rfm12_set_afc_mode(                  RFM12_t *dev, RFM12_afc_mode_t mode);
RFM12_result_t rfm12_set_afc_range_limit(           RFM12_t *dev, RFM12_afc_range_t range_limit);
RFM12_result_t rfm12_set_afc_store_offset(          RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_afc_fine_mode(             RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_afc_output_register_enable(RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_afc_enable(                RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_reset_afc(                     RFM12_t *dev);

// 11. TX Configuration Control Command

RFM12_result_t rfm12_set_tx_fsk_polarity(           RFM12_t *dev, RFM12_tx_fsk_polarity_t polarity);
RFM12_result_t rfm12_set_tx_fsk_deviation(          RFM12_t *dev, RFM12_tx_fsk_deviation_t deviation);
RFM12_result_t rfm12_set_tx_power(                  RFM12_t *dev, RFM12_tx_power_t power);
RFM12_result_t rfm12_reset_tx_configuration(        RFM12_t *dev);

// 12. PLL Setting Command

RFM12_result_t rfm12_set_pll_output_buffer_current( RFM12_t *dev, RFM12_pll_output_buffer_current_t current);
RFM12_result_t rfm12_set_pll_delay_enable(          RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_pll_dithering_disable(     RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_set_pll_bandwidth(             RFM12_t *dev, RFM12_pll_bandwidth_t bandwidth);
RFM12_result_t rfm12_reset_pll_setting(             RFM12_t *dev);

// 13. Transmitter Register Write Command

RFM12_result_t rfm12_write_tx_register(             RFM12_t *dev, uint8_t data);

// 14. Wake-Up Timer Command

RFM12_result_t rfm12_set_wakeup_timer(              RFM12_t *dev, RFM12_wakeup_prescaler_t prescaler, RFM12_wakeup_multiplier_t multiplier);
RFM12_result_t rfm12_set_wakeup_period_ms(          RFM12_t *dev, uint32_t wakeup_ms);
RFM12_result_t rfm12_reset_wakeup_timer(            RFM12_t *dev);

// 15. Low Duty-Cycle Command

RFM12_result_t rfm12_set_low_duty_cycle_d(          RFM12_t *dev, RFM12_low_duty_cycle_d_t d);
RFM12_result_t rfm12_set_low_duty_cycle_enable(     RFM12_t *dev, RFM12_enable_t enable);
RFM12_result_t rfm12_reset_low_duty_cycle(          RFM12_t *dev);

// 16. Low Battery Detector and Microcontroller Clock Divider Command

RFM12_result_t rfm12_set_clock_output_frequency(    RFM12_t *dev, RFM12_clock_output_frequency_t frequency);
RFM12_result_t rfm12_set_low_battery_threshold(     RFM12_t *dev, RFM12_low_battery_threshold_t threshold);
RFM12_result_t rfm12_reset_low_battery_clock(       RFM12_t *dev);

// 17. Status Read Command

RFM12_result_t rfm12_read_status(RFM12_t *dev, RFM12_status_word_t *status);

//
// Status Read Helper Functions
//

/* -------------------------------------------------------------------------
 * Mode-dependent status bits
 * ------------------------------------------------------------------------- */

/* Bit 15: FFIT while receiving */
bool rfm12_status_rx_fifo_interrupt(RFM12_status_word_t status);

/* Bit 15: RGIT while transmitting */
bool rfm12_status_tx_register_ready(RFM12_status_word_t status);

/* Bit 13: FFOV while receiving */
bool rfm12_status_rx_fifo_overflow(RFM12_status_word_t status);

/* Bit 13: RGUR while transmitting */
bool rfm12_status_tx_register_underrun(RFM12_status_word_t status);

/* -------------------------------------------------------------------------
 * Interrupt and event status bits
 * ------------------------------------------------------------------------- */

/* Bit 14: POR */
bool rfm12_status_power_on_reset(RFM12_status_word_t status);

/* Bit 12: WKUP */
bool rfm12_status_wakeup_timer_expired(RFM12_status_word_t status);

/* Bit 11: EXT */
bool rfm12_status_external_interrupt(RFM12_status_word_t status);

/* Bit 10: LBD */
bool rfm12_status_low_battery(RFM12_status_word_t status);

/* -------------------------------------------------------------------------
 * Receiver and FIFO state bits
 * ------------------------------------------------------------------------- */

/* Bit 9: FFEM */
bool rfm12_status_rx_fifo_empty(RFM12_status_word_t status);

/* Bit 8: ATS */
bool rfm12_status_antenna_tuning_signal(RFM12_status_word_t status);

/* Bit 7: RSSI */
bool rfm12_status_rssi_detected(RFM12_status_word_t status);

/* Bit 6: DQD */
bool rfm12_status_data_quality_detected(RFM12_status_word_t status);

/* Bit 5: CRL */
bool rfm12_status_clock_recovery_locked(RFM12_status_word_t status);

/* -------------------------------------------------------------------------
 * AFC status and measurement
 * ------------------------------------------------------------------------- */

/* Bit 4: ATGL */
bool rfm12_status_afc_cycle_toggle(RFM12_status_word_t status);

/*
 * Bits [3:0]: signed four-bit AFC offset, sign-extended to int8_t.
 */
int8_t rfm12_status_afc_offset_steps(RFM12_status_word_t status);

#pragma endregion

#ifdef __cplusplus
}
#endif /* (__cplusplus) */

#endif /* RFM12_H */
