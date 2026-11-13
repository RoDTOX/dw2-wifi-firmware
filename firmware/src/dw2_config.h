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
#define DW2_DOOR_DEBOUNCE_MS        (40U)       /* 40 ms: fast response, catches quick magnet flicks reliably */
#define DW2_BUTTON_DEBOUNCE_MS      (30U)

/* S1 multi-tier press windows (press -> release, ms) */
#define DW2_BTN_SHORT_MAX_MS        (5000U)     /* < 5 s: Connectable OTA */
#define DW2_BTN_REBOOT_MIN_MS       (8000U)     /* 8-10 s: hard reboot */
#define DW2_BTN_REBOOT_MAX_MS       (10000U)
#define DW2_BTN_FACTORY_MIN_MS      (15000U)    /* >= 15 s: factory reset */
#define DW2_BTN_CUE_TOLERANCE_MS    (50U)

/* LED pattern timings (ms) - crisp & distinct human-visible pulses */
#define DW2_LED_BLINK_SHORT_MS      (80U)       /* 80 ms ON: solid, visible blink */
#define DW2_LED_BLINK_GAP_MS        (120U)      /* 120 ms OFF: clear separation between blinks */
#define DW2_LED_BLINK_LONG_MS       (250U)

/* Door Ajar alert: trigger after 45 s if door remains OPEN */
#define DW2_DOOR_AJAR_TIMEOUT_SEC   (45U)
#define DW2_DOOR_AJAR_REPEAT_SEC    (15U)

/* Secret Magnetic Knock: 5 comfortable transitions with up to 2.5s gap */
#define DW2_MAGNET_KNOCK_MAX_GAP_MS (2500U)     /* 2.5 seconds allowed between flicks - comfortable human pace */
#define DW2_MAGNET_KNOCK_THRESHOLD  (5U)        /* 5 flicks to trigger Connectable OTA mode */

/* BLE Advertising parameters - Broadcast Burst */
#define DW2_BLE_ADV_INTERVAL_MIN    (0x0020U)   /* 20 ms (32 * 0.625 ms) */
#define DW2_BLE_ADV_INTERVAL_MAX    (0x0030U)   /* 30 ms (48 * 0.625 ms) */
#define DW2_BLE_BURST_DURATION_MS   (500U)      /* 500 ms burst duration */

/* BLE Advertising parameters - Connectable Sync & OTA Mode */
#define DW2_BLE_CONN_ADV_INTERVAL_MIN (0x0040U) /* 40 ms */
#define DW2_BLE_CONN_ADV_INTERVAL_MAX (0x0060U) /* 60 ms */
#define DW2_BLE_CONNECTABLE_TIMEOUT_MS (180000U) /* 3 minutes connectable window on Secret Knock */

/* Periodic heartbeat timer: 4 hours */
#define DW2_HEARTBEAT_PERIOD_MS     (14400000U)

/* Low battery threshold: 2.3 V (2300 mV) */
#define DW2_VBAT_LOW_THRESHOLD_MV   (2300U)

/* AuxADC battery measurement */
#define DW2_VBAT_AVG_COUNT          (16U)

/* Smart sleep enable */
#define DW2_SMART_SLEEP_EN          (1U)

/* 128 KB SPI Flash Circular Blackbox Log */
#define DW2_FLASH_LOG_BASE_ADDR     (0x000E0000U)   /* Safe region above OTA banks (ends at 0x000FFFFF) */
#define DW2_FLASH_LOG_SIZE          (0x00020000U)   /* 128 KB = 131,072 bytes */
#define DW2_FLASH_LOG_SECTOR_SIZE   (4096U)         /* 4 KB sector erase size */
#define DW2_FLASH_LOG_ENTRY_SIZE    (16U)           /* sizeof(dw2_log_entry_t) */
#define DW2_FLASH_LOG_MAX_ENTRIES   (DW2_FLASH_LOG_SIZE / DW2_FLASH_LOG_ENTRY_SIZE) /* 8,192 entries */
#define DW2_FLASH_LOG_SECTORS       (DW2_FLASH_LOG_SIZE / DW2_FLASH_LOG_SECTOR_SIZE) /* 32 sectors */

/* Secondary RAM Blackbox circular buffer capacity */
#define DW2_RAM_LOG_CAPACITY        (64U)

#endif /* __DW2_CONFIG_H__ */
