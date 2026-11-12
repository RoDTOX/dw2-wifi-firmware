/**
 * @file dw2_protocol.h
 * @brief Shared BLE Manufacturer Specific Data payload format for DW2 -> ESP32.
 */

#ifndef __DW2_PROTOCOL_H__
#define __DW2_PROTOCOL_H__

#include <stdint.h>

#define DW2_BLE_COMPANY_ID          (0xFFFFU)   /* Custom / internal test company ID */
#define DW2_BLE_MAGIC_0             (0x44U)     /* 'D' */
#define DW2_BLE_MAGIC_1             (0x57U)     /* 'W' */
#define DW2_BLE_PROTO_VERSION       (0x01U)

/* Flag bits in dw2_adv_payload_t.flags */
#define DW2_FLAG_DOOR_OPEN          (1U << 0)   /* 1 = open, 0 = closed */
#define DW2_FLAG_HEARTBEAT          (1U << 1)   /* 1 = periodic heartbeat, 0 = state change */
#define DW2_FLAG_BUTTON_TEST        (1U << 2)   /* 1 = triggered by S1 button */

typedef struct __attribute__((packed))
{
    uint16_t company_id;    /* Little-endian: DW2_BLE_COMPANY_ID */
    uint8_t  magic[2];      /* 'D', 'W' */
    uint8_t  version;       /* DW2_BLE_PROTO_VERSION */
    uint8_t  flags;         /* DW2_FLAG_* bitmask */
    uint16_t event_counter; /* Monotonic counter for deduplication */
    uint16_t battery_mv;    /* Battery voltage in mV (e.g. 2950 = 2.950 V) */
} dw2_adv_payload_t;

#endif /* __DW2_PROTOCOL_H__ */
