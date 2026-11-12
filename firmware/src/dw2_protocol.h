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

/**
 * @brief 16-byte Manufacturer Specific Payload:
 * Magic(2B) + Ver(1B) + Flags(1B) + Counter(4B) + Epoch(4B) + Vbat(2B) + BTHomeObject(2B)
 */
typedef struct __attribute__((packed))
{
    uint16_t company_id;    /* 0xFFFF */
    uint8_t  magic[2];      /* 'D', 'W' */
    uint8_t  version;       /* 0x02 */
    uint8_t  flags;         /* DW2_FLAG_* */
    uint32_t event_counter; /* Rolling 32-bit monotonic counter (anti-replay) */
    uint32_t epoch_sec;     /* Real-time UNIX Epoch timestamp */
    uint16_t battery_mv;    /* Battery voltage in mV (e.g. 3197) */
    /* BTHome V2 object: BTHOME_ID_DOOR (0x1A), 1 byte value (0=closed, 1=open) */
    uint8_t  bthome_id;     /* 0x1A (BTHome Door sensor) */
    uint8_t  bthome_val;    /* 0 or 1 */
} dw2_adv_payload_t;

/**
 * @brief Flash log entry format (12 bytes per event, 5000+ entries stored in flash).
 */
typedef struct __attribute__((packed))
{
    uint32_t epoch_sec;
    uint32_t event_counter;
    uint16_t battery_mv;
    uint8_t  flags;
    uint8_t  checksum;      /* XOR of first 11 bytes */
} dw2_log_entry_t;

#endif /* __DW2_PROTOCOL_H__ */
