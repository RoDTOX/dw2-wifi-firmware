/**
 * @file dw2_protocol.h
 * @brief Enhanced DW2 BLE payload: BTHome V2 compatible + rich security & real-time telemetry.
 */

#ifndef __DW2_PROTOCOL_H__
#define __DW2_PROTOCOL_H__

#include <stdint.h>

#define DW2_BLE_COMPANY_ID          (0xFFFFU)
#define DW2_BLE_MAGIC_0             (0x44U)     /* 'D' */
#define DW2_BLE_MAGIC_1             (0x57U)     /* 'W' */
#define DW2_BLE_PROTO_VERSION       (0x02U)     /* Version 2: Epoch time + BTHome compatible */

/* Flag bits */
#define DW2_FLAG_DOOR_OPEN          (1U << 0)   /* 1 = Open, 0 = Closed */
#define DW2_FLAG_HEARTBEAT          (1U << 1)   /* 1 = Periodic heartbeat */
#define DW2_FLAG_BUTTON_TEST        (1U << 2)   /* 1 = Triggered by S1 button (tamper/test) */
#define DW2_FLAG_DOOR_AJAR          (1U << 3)   /* 1 = Door left open > 45s warning */
#define DW2_FLAG_TIME_SYNCED        (1U << 4)   /* 1 = Epoch timestamp valid, 0 = relative ticks */
#define DW2_FLAG_LOW_BATTERY        (1U << 5)   /* 1 = Battery < 2.3 V warning */
#define DW2_FLAG_WIFI_FALLBACK      (1U << 6)   /* 1 = Wi-Fi fallback armed/enabled */
#define DW2_FLAG_RTC_BEACON         (1U << 7)   /* 1 = Master RTC authoritative time beacon */

/**
 * @brief 20-byte Manufacturer Specific Payload:
 * Magic(2B) + Ver(1B) + Flags(1B) + Counter(4B) + Epoch(4B) + Vbat(2B) + OpenDuration(2B) + BTHomeObject(2B)
 */
typedef struct __attribute__((packed))
{
    uint16_t company_id;        /* 0xFFFF */
    uint8_t  magic[2];          /* 'D', 'W' */
    uint8_t  version;           /* 0x02 */
    uint8_t  flags;             /* DW2_FLAG_* */
    uint32_t event_counter;     /* Rolling 32-bit monotonic counter (anti-replay) */
    uint32_t epoch_sec;         /* Real-time UNIX Epoch timestamp */
    uint16_t battery_mv;        /* Battery voltage in mV (e.g. 3197) */
    uint16_t open_duration_sec; /* Door open duration in seconds (on close event) */
    /* BTHome V2 object: BTHOME_ID_DOOR (0x1A), 1 byte value (0=closed, 1=open) */
    uint8_t  bthome_id;         /* 0x1A (BTHome Door sensor) */
    uint8_t  bthome_val;        /* 0 or 1 */
} dw2_adv_payload_t;

/**
 * @brief Flash log entry format (16 bytes aligned per event, ~8,192 entries in 128 KB circular buffer).
 */
typedef struct __attribute__((packed))
{
    uint32_t epoch_sec;
    uint32_t event_counter;
    uint16_t battery_mv;
    uint16_t open_duration_sec;
    uint8_t  flags;
    uint8_t  reserved[2];
    uint8_t  checksum;          /* XOR of first 15 bytes */
} dw2_log_entry_t;

/**
 * @brief Persistent configuration stored in Flash sector (4 KB).
 */
typedef struct __attribute__((packed))
{
    uint32_t magic;               /* 0x44574346 = "DWCF" */
    uint16_t version;             /* 0x0001 */
    uint8_t  wifi_fallback_en;    /* 0 = disabled, 1 = enabled */
    uint8_t  led_mode;            /* 0 = stealth, 1 = 24h install mode, 2 = always blink */
    uint16_t ble_ack_timeout_ms;  /* e.g. 1000 ms */
    uint16_t door_ajar_sec;       /* e.g. 45 s (0 = disabled) */
    char     wifi_ssid[32];       /* Wi-Fi SSID */
    char     wifi_pass[64];       /* Wi-Fi Password */
    char     mqtt_host[32];       /* MQTT broker IP / hostname */
    uint16_t mqtt_port;           /* MQTT port (1883) */
    char     mqtt_topic[32];      /* MQTT topic prefix */
    uint32_t checksum;            /* XOR checksum of struct */
} dw2_nv_config_t;

#endif /* __DW2_PROTOCOL_H__ */
