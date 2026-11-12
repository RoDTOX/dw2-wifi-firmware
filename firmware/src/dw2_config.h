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

/* LED power-saving: only blink on door events during the first 24 hours (86,400 s) from boot */
#define DW2_LED_DOOR_ACTIVE_SEC     (86400U)

/* Door Ajar alert: trigger after 45 s if door remains OPEN */
#define DW2_DOOR_AJAR_TIMEOUT_MS    (45000U)
#define DW2_DOOR_AJAR_REPEAT_MS     (10000U)

/* Time synchronization window on cold boot or button long-press: 5 minutes */
#define DW2_TIME_SYNC_WINDOW_MS     (300000U)

/* BLE Adaptive Burst:
 * Phase 1: 300 ms fast burst at 20 ms interval
 * Phase 2: 500 ms safe burst at 100 ms interval
 * Total = 800 ms
 */
#define DW2_ADV_BURST_FAST_MS       (300U)
#define DW2_ADV_BURST_TOTAL_MS      (800U)
#define DW2_BLE_ADV_FAST_MIN        (0x0020U)   /* 20 ms */
#define DW2_BLE_ADV_FAST_MAX        (0x0030U)   /* 30 ms */
#define DW2_BLE_ADV_SAFE_MIN        (0x00A0U)   /* 100 ms */
#define DW2_BLE_ADV_SAFE_MAX        (0x00A0U)   /* 100 ms */

/* Periodic heartbeat timer: 4 hours */
#define DW2_HEARTBEAT_PERIOD_MS     (14400000U)

/* Low battery threshold: 2.3 V (2300 mV) */
#define DW2_VBAT_LOW_THRESHOLD_MV   (2300U)

/* AuxADC battery measurement */
#define DW2_VBAT_AVG_COUNT          (16U)

/* Flash Configuration Sector: 4 KB at 0x000DF000 */
#define DW2_FLASH_CONFIG_ADDR       (0x000DF000U)

/* Flash Logger in BoyaMicro 25Q80 (1 MB):
 * 128 KB Circular Ring Buffer at 0x000E0000 -> holds 8,192 events (16 bytes each)!
 */
#define DW2_FLASH_LOG_BASE_ADDR     (0x000E0000U)
#define DW2_FLASH_LOG_SIZE          (0x00020000U)   /* 128 KB */
#define DW2_FLASH_LOG_MAX_ENTRIES   (8192U)

#endif /* __DW2_CONFIG_H__ */
