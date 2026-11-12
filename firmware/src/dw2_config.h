/**
 * @file dw2_config.h
 * @brief Hardware pin mapping and runtime configuration for Sonoff DW2-Wi-Fi (OPL1000 A2).
 */

#ifndef __DW2_CONFIG_H__
#define __DW2_CONFIG_H__

#include "hal_vic.h"

/* Hardware GPIO mapping (DW2-WIFI V1.2, verified against reference blewifi) */
#define DW2_BUTTON_IO_PORT          (GPIO_IDX_04)
#define DW2_MAGNETIC_IO_PORT        (GPIO_IDX_05)
#define DW2_BATTERY_IO_PORT         (GPIO_IDX_02)
#define DW2_LED_IO_PORT             (GPIO_IDX_21)

/* Polarity: DW2 Hall sensor pulls IO5 LOW when magnet is near (CLOSED), HIGH when far (OPEN) */
#define DW2_DOOR_CLOSED_LEVEL       (GPIO_LEVEL_LOW)
#define DW2_BUTTON_PRESSED_LEVEL    (GPIO_LEVEL_LOW)

/* Debounce and LED timings (ms) */
#define DW2_DOOR_DEBOUNCE_MS        (200U)
#define DW2_BUTTON_DEBOUNCE_MS      (30U)
#define DW2_LED_BLINK_MS            (80U)

/* BLE advertisement burst configuration */
#define DW2_BLE_ADV_INTERVAL_MIN    (0x0020U)   /* 20 ms (units of 0.625 ms) */
#define DW2_BLE_ADV_INTERVAL_MAX    (0x0030U)   /* 30 ms */
#define DW2_BLE_BURST_DURATION_MS   (1500U)     /* 1.5 s burst per event */
#define DW2_HEARTBEAT_PERIOD_MS     (14400000U) /* 4 hours */

/* Power saving */
#define DW2_SMART_SLEEP_EN          (1)

/* AuxADC battery measurement */
#define DW2_VBAT_AVG_COUNT          (16U)

#endif /* __DW2_CONFIG_H__ */
