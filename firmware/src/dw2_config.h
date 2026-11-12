/**
 * @file dw2_config.h
 * @brief Hardware pin mapping and runtime configuration for Sonoff DW2-Wi-Fi (OPL1000 A2).
 */

#ifndef __DW2_CONFIG_H__
#define __DW2_CONFIG_H__

#include "hal_vic.h"

/* Hardware GPIO mapping (DW2-WIFI V1.2) */
#define DW2_BUTTON_IO_PORT          (GPIO_IDX_04)
#define DW2_MAGNETIC_IO_PORT        (GPIO_IDX_05)
#define DW2_BATTERY_IO_PORT         (GPIO_IDX_02)
#define DW2_LED_IO_PORT             (GPIO_IDX_21)

/* Polarity */
#define DW2_DOOR_CLOSED_LEVEL       (GPIO_LEVEL_LOW)
#define DW2_BUTTON_PRESSED_LEVEL    (GPIO_LEVEL_LOW)

/* Debounce & timings (ms) */
#define DW2_DOOR_DEBOUNCE_MS        (150U)
#define DW2_BUTTON_DEBOUNCE_MS      (30U)

/* LED pattern timings (ms) */
#define DW2_LED_BLINK_SHORT_MS      (30U)
#define DW2_LED_BLINK_LONG_MS       (250U)

/* Door Ajar alert: trigger after 45 s if door remains OPEN */
#define DW2_DOOR_AJAR_TIMEOUT_SEC   (45U)
#define DW2_DOOR_AJAR_REPEAT_SEC    (15U)

/* Secret Magnetic Knock: 5 rapid transitions within < 1200 ms */
#define DW2_MAGNET_KNOCK_MAX_GAP_MS (1200U)
#define DW2_MAGNET_KNOCK_THRESHOLD  (5U)

/* BLE Advertising parameters */
#define DW2_BLE_ADV_INTERVAL_MIN    (0x0020U)   /* 20 ms (32 * 0.625 ms) */
#define DW2_BLE_ADV_INTERVAL_MAX    (0x0030U)   /* 30 ms (48 * 0.625 ms) */
#define DW2_BLE_BURST_DURATION_MS   (500U)      /* 500 ms burst duration */

/* Periodic heartbeat timer: 4 hours */
#define DW2_HEARTBEAT_PERIOD_MS     (14400000U)

/* Low battery threshold: 2.3 V (2300 mV) */
#define DW2_VBAT_LOW_THRESHOLD_MV   (2300U)

/* AuxADC battery measurement */
#define DW2_VBAT_AVG_COUNT          (16U)

/* Smart sleep enable */
#define DW2_SMART_SLEEP_EN          (1U)

/* RAM Blackbox circular buffer capacity */
#define DW2_RAM_LOG_CAPACITY        (64U)

#endif /* __DW2_CONFIG_H__ */
