// Copyright 2025 Azoteq (Pty) Ltd
// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdbool.h>
#include "azoteq_iqs9150.h"
#include "pointing_device_internal.h"
#include "wait.h"
#include "print.h"
#include <gpio.h>

// #define IQS9150_INIT_H

#define SETUP

#define AZOTEQ_IQS9150_USE_DEFAULT_SETTINGS

// Bit manipulation macro
#ifndef BIT
#define BIT(n) (1U << (n))
#endif

#ifndef GENMASK
#define GENMASK(h, l) (((~0UL) - (1UL << (l)) + 1) & (~0UL >> (32 - 1 - (h))))
#endif

// Register definitions based on 9150 Linux driver
#define IQS9150_PROD_NUM                    0x1000
#define IQS9150_STATUS                      0x1018
#define IQS9150_REG_BUF_START               0x115C
#define IQS9150_REG_BUF_LEN                 (0x14F0 - IQS9150_REG_BUF_START)
#define IQS9150_SETTINGS_MINOR              0x1178
#define IQS9150_SETTINGS_MAJOR              0x1179
#define IQS9150_RATE_IDLE_TOUCH             0x11A4
#define IQS9150_TIMEOUT_COMMS               0x11B8
#define IQS9150_CONTROL                     0x11BC
#define IQS9150_CONFIG                      0x11BE
#define IQS9150_OTHER                       0x11C0
#define IQS9150_ALP_SETUP                   0x11C5
#define IQS9150_NUM_CONTACTS                0x11E5
#define IQS9150_X_RES                       0x11E6
#define IQS9150_Y_RES                       0x11E8
#define IQS9150_END_COMMS                   0xEEEE

#define IQS9150_9151_MM_INFO_FLAGS                       0x1020
#define IQS9150_9151_MM_TRACKPAD_FLAGS                   0x1022

// Control register bits
#define IQS9150_CONTROL_SUSPEND             BIT(11)
// #define IQS9150_CONTROL_ACK_RESET           BIT(7)
#define IQS9150_CONTROL_ACK_RESET           0x82
//0x80 is ack reset on 11BC
//0x82 is ack reset + idle mode on 11BC
#define IQS9150_CONTROL_ATI_ALP             BIT(6)
#define IQS9150_CONTROL_ATI_TP              BIT(5)

// Config register bits
#define IQS9150_CONFIG_EVENT_MASK           GENMASK(15, 9)
#define IQS9150_CONFIG_EVENT_ATI            BIT(11)
#define IQS9150_CONFIG_EVENT_MODE           BIT(8)
#define IQS9150_CONFIG_FORCED_COMMS         BIT(4)

// Status info bits
#define IQS9150_INFO_GLOBAL_TOUCH           BIT(9)
#define IQS9150_INFO_SHOW_RESET             BIT(7)
#define IQS9150_INFO_ALP_ATI_AGAIN          BIT(6)
#define IQS9150_INFO_ALP_ATI_ERROR          BIT(5)
#define IQS9150_INFO_TP_ATI_AGAIN           BIT(4)
#define IQS9150_INFO_TP_ATI_ERROR           BIT(3)
#define IQS9150_INFO_CHARGE_MODE            GENMASK(2, 0)
#define IQS9150_INFO_CHARGE_MODE_LP1        3

// Other register bits
#define IQS9150_OTHER_SW_ENABLE             BIT(15)
#define IQS9150_ALP_SETUP_ENABLE            BIT(7)

// Communication constants
#define IQS9150_COMMS_ERROR                 0xEEEE
#define IQS9150_COMMS_RETRY_MS              50
#define IQS9150_NUM_RETRIES                 5
#define IQS9150_MAX_LEN                     256
#define IQS9150_MAX_CONTACTS                7

#define IQS915X_ADDR        (0x56 << 1)       // 7-bit I2C address
#define IQS915X_PROD_REG    0x1000
#define I2C_TIMEOUT_MS     200

// define register map

// Driver interface structure
const pointing_device_driver_t azoteq_iqs9150_pointing_device_driver = {
    .init       = azoteq_iqs9150_init,
    .get_report = azoteq_iqs9150_get_report,
    // .set_cpi    = azoteq_iqs9150_set_cpi,
    // .get_cpi    = azoteq_iqs9150_get_cpi,
};

// Global variables
// static uint16_t azoteq_iqs9150_product_number = AZOTEQ_IQS9150_UNKNOWN;
// static azoteq_iqs9150_comms_mode_t azoteq_iqs9150_comms_mode = AZOTEQ_IQS9150_COMMS_MODE_WAIT;
// static azoteq_iqs9150_ver_info_t azoteq_iqs9150_ver_info = {0};

static inline void bb_delay(void) {
    wait_us(5); // ~100kHz
}

static inline void bb_scl_high(void) {
    setPinInputHigh(I2C1_SCL_PIN);
    bb_delay();
}

static inline void bb_scl_low(void) {
    setPinOutput(I2C1_SCL_PIN);
    writePinLow(I2C1_SCL_PIN);
    bb_delay();
}

static inline void bb_sda_high(void) {
    setPinInputHigh(I2C1_SDA_PIN);
    bb_delay();
}

static inline void bb_sda_low(void) {
    setPinOutput(I2C1_SDA_PIN);
    writePinLow(I2C1_SDA_PIN);
    bb_delay();
}

/* ---- Public transport API ---- */

void iqs9150_i2c_init(void) {
    bb_sda_high();
    bb_scl_high();
}

void iqs9150_i2c_start(void) {
    bb_sda_high();
    bb_scl_high();
    bb_sda_low();
    bb_scl_low();
}

void iqs9150_i2c_stop(void) {
    bb_sda_low();
    bb_scl_high();
    bb_sda_high();
}

bool iqs9150_i2c_write_u8(uint8_t byte) {
    for (int i = 7; i >= 0; i--) {
        (byte & (1 << i)) ? bb_sda_high() : bb_sda_low();
        bb_scl_high();
        bb_scl_low();
    }

    /* ACK */
    bb_sda_high();
    bb_scl_high();
    bool ack = !readPin(I2C1_SDA_PIN);
    bb_scl_low();

    return ack;
}

uint8_t iqs9150_i2c_read_u8(bool ack) {
    uint8_t v = 0;

    bb_sda_high();
    for (int i = 7; i >= 0; i--) {
        bb_scl_high();
        if (readPin(I2C1_SDA_PIN)) {
            v |= (1 << i);
        }
        bb_scl_low();
    }

    ack ? bb_sda_low() : bb_sda_high();
    bb_scl_high();
    bb_scl_low();
    bb_sda_high();

    return v;
}

static bool iqs9150_i2c_write_reg16_addr(uint16_t reg) {
    return iqs9150_i2c_write_u8(0xAC) &&           // address + write
           iqs9150_i2c_write_u8(reg & 0xFF) &&     // LSB first
           iqs9150_i2c_write_u8(reg >> 8);         // MSB
}

bool iqs9150_i2c_write_reg16(uint16_t reg, uint16_t value) {
    iqs9150_i2c_start();

    if (!iqs9150_i2c_write_reg16_addr(reg))
        goto fail;

    if (!iqs9150_i2c_write_u8(value & 0xFF))   // DATA LSB
        goto fail;

    if (!iqs9150_i2c_write_u8(value >> 8))     // DATA MSB
        goto fail;

    iqs9150_i2c_stop();
    return true;

fail:
    iqs9150_i2c_stop();
    return false;
}

bool iqs9150_i2c_read_reg16(uint16_t reg, uint16_t *value) {
    uint8_t lsb, msb;

    iqs9150_i2c_start();
    if (!iqs9150_i2c_write_reg16_addr(reg)) goto fail;

    iqs9150_i2c_start();
    if (!iqs9150_i2c_write_u8(0xAD)) goto fail;

    lsb = iqs9150_i2c_read_u8(true);
    msb = iqs9150_i2c_read_u8(false);

    *value = (msb << 8) | lsb;
    iqs9150_i2c_stop();
    return true;

fail:
    iqs9150_i2c_stop();
    return false;
}

static void wait_for_rdy_low(void) {
    while (readPin(IQS9150_RDY_PIN)) {
        wait_ms(1);
    }
}

static i2c_status_t azoteq_iqs9150_init_status = 1;

static bool iqs9150_read_rel_xy(int16_t *x, int16_t *y) {
    uint16_t raw;

    wait_for_rdy_low();

    if (!iqs9150_i2c_read_reg16(0x1014, &raw))
        return false;
    *x = (int16_t)raw;

    // wait_for_rdy_low();

    if (!iqs9150_i2c_read_reg16(0x1016, &raw))
        return false;
    *y = (int16_t)raw;

    return true;
}

void azoteq_iqs9150_init(void) {
    uint16_t product = 0;
    uint16_t config  = 0;

    azoteq_iqs9150_init_status = I2C_STATUS_ERROR;

    uprintf("\n====================================\n");
    uprintf("=== IQS9150 INIT BEGIN ===\n");
    uprintf("====================================\n");

    /* STEP 0: Ensure I2C bus idle */
    uprintf("[STEP 0] Initialising bitbanged I2C lines\n");
    iqs9150_i2c_init();
    uprintf("[STEP 0] SDA=%ld SCL=%ld\n",
            readPin(I2C1_SDA_PIN),
            readPin(I2C1_SCL_PIN));

    /* STEP 1: Wait for communication window */
    uprintf("[STEP 1] Waiting for RDY LOW (comms window)\n");
    wait_for_rdy_low();
    uprintf("[STEP 1] RDY LOW detected\n");

    /* STEP 2: Read product ID */
    uprintf("[STEP 2] Reading PRODUCT ID @ 0x1000\n");
    if (!iqs9150_i2c_read_reg16(0x1000, &product)) {
        uprintf("[ERROR] PRODUCT ID read failed\n");
        return;
    }

    uprintf("[STEP 2] PRODUCT ID = 0x%04X\n", product);

    if (product == 0x076A) {
        uprintf("[STEP 2] Detected IQS9150\n");
    } else if (product == 0x09BC) {
        uprintf("[STEP 2] Detected IQS9151\n");
    } else {
        uprintf("[ERROR] Unknown product ID\n");
        return;
    }

    /* STEP 3: Clear SHOW_RESET (mandatory after reset) */
    uprintf("[STEP 3] Clearing SHOW_RESET via CONTROL (0x11BC)\n");
    wait_for_rdy_low();

    uprintf("[STEP 3] Writing CONTROL = 0x0080 (ACK_RESET)\n");
    if (!iqs9150_i2c_write_reg16(0x11BC, 0x0080)) {
        uprintf("[ERROR] Failed to clear SHOW_RESET\n");
        return;
    }
    uprintf("[STEP 3] SHOW_RESET cleared\n");

    /* STEP 4: Enable forced comms, disable event mode */
    uprintf("[STEP 4] Reading CONFIG register (0x11BE)\n");
    wait_for_rdy_low();

    if (!iqs9150_i2c_read_reg16(0x11BE, &config)) {
        uprintf("[ERROR] Failed to read CONFIG\n");
        return;
    }

    uprintf("[STEP 4] CONFIG before = 0x%04X\n", config);

    config |=  (1 << 4);  /* FORCED_COMMS */
    config &= ~(1 << 8);  /* EVENT_MODE = streaming */

    uprintf("[STEP 4] CONFIG after  = 0x%04X\n", config);
    uprintf("[STEP 4]   FORCED_COMMS = %d\n", !!(config & (1 << 4)));
    uprintf("[STEP 4]   EVENT_MODE   = %d (0 = streaming)\n", !!(config & (1 << 8)));

    if (!iqs9150_i2c_write_reg16(0x11BE, config)) {
        uprintf("[ERROR] Failed to write CONFIG\n");
        return;
    }

    uprintf("[STEP 4] CONFIG written successfully\n");

    /* STEP 5: Disable low-power timeouts */
    uprintf("[STEP 5] Disabling LP timeouts via TIMEOUT_COMMS (0x11B8)\n");
    wait_for_rdy_low();

    if (!iqs9150_i2c_write_reg16(0x11B8, 0x00FF)) {
        uprintf("[ERROR] Failed to write TIMEOUT_COMMS\n");
        return;
    }

    uprintf("[STEP 5] TIMEOUT_COMMS set to 0x00FF\n");

    /* STEP 6: Set resolution */
    uprintf("[STEP 6] Setting resolution\n");
    wait_for_rdy_low();
    uprintf("[STEP 6] Writing X_RES = 2048\n");
    iqs9150_i2c_write_reg16(0x11E6, 2048);

    wait_for_rdy_low();
    uprintf("[STEP 6] Writing Y_RES = 2048\n");
    iqs9150_i2c_write_reg16(0x11E8, 2048);

    uprintf("[STEP 6] Resolution configured\n");

    /* STEP 7: End I2C session */
    uprintf("[STEP 7] Ending I2C session (0xEEEE)\n");
    wait_for_rdy_low();

    iqs9150_i2c_write_reg16(0xEEEE, 0x0000);
    uprintf("[STEP 7] Session ended\n");

    azoteq_iqs9150_init_status = I2C_STATUS_SUCCESS;

    uprintf("====================================\n");
    uprintf("=== IQS9150 INIT OK ===\n");
    uprintf("====================================\n");
}

report_mouse_t azoteq_iqs9150_get_report(report_mouse_t mouse_report) {
    report_mouse_t temp_report = mouse_report;

    int16_t x = 0, y = 0;

    if (iqs9150_read_rel_xy(&x, &y)) {
        /* Clamp to HID range */
        if (x > 127)  x = 127;
        if (x < -127) x = -127;
        if (y > 127)  y = 127;
        if (y < -127) y = -127;

        temp_report.x = (int8_t)x;
        temp_report.y = (int8_t)y;

        // /* OPTIONAL: throttled debug */
        // static uint32_t last_print;
        // if (timer_elapsed32(last_print) > 200) {
        //     uprintf("REL_X=%d REL_Y=%d RDY=%ld\n",
        //             x, y, readPin(IQS9150_RDY_PIN));
        //     last_print = timer_read32();
        // }
    }

    return temp_report;
}