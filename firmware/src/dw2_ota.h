/**
 * @file dw2_ota.h
 * @brief Over-The-Air (OTA) firmware upgrade & BLE GATT Service for Sonoff DW2.
 *
 * Exposes standard Opulinks OTA GATT service (0xAAAA) with characteristics:
 * - Data In (0xBBB0): Client write (firmware packets & commands)
 * - Data Out (0xBBB1): Device notification (ACKs, version, log stream)
 */

#ifndef __DW2_OTA_H__
#define __DW2_OTA_H__

#include <stdint.h>
#include <stdbool.h>

#include "ble_gatt_if.h"
#include "msg.h"

/* Opulinks Service & Characteristic UUIDs */
#define DW2_OTA_UUID_SERVICE        (0xAAAAU)
#define DW2_OTA_UUID_DATA_IN        (0xBBB0U)
#define DW2_OTA_UUID_DATA_OUT       (0xBBB1U)

/* Protocol Command IDs */
#define DW2_REQ_OTA_VERSION         (0x0100U)
#define DW2_RSP_OTA_VERSION         (0x1100U)
#define DW2_REQ_OTA_UPGRADE         (0x0101U)
#define DW2_RSP_OTA_UPGRADE         (0x1101U)
#define DW2_REQ_OTA_RAW             (0x0102U)
#define DW2_RSP_OTA_RAW             (0x1102U)
#define DW2_REQ_OTA_END             (0x0103U)
#define DW2_RSP_OTA_END             (0x1103U)

/* Extended Blackbox Log & Time Sync commands */
#define DW2_REQ_TIME_SYNC           (0x0300U)
#define DW2_RSP_TIME_SYNC           (0x1300U)
#define DW2_REQ_LOG_DUMP            (0x0301U)
#define DW2_RSP_LOG_DUMP            (0x1301U)

/* OTA Status Codes */
enum
{
    DW2_OTA_STATUS_SUCCESS = 0,
    DW2_OTA_STATUS_NOT_ACTIVE,
    DW2_OTA_STATUS_HW_FAILURE,
    DW2_OTA_STATUS_IN_PROGRESS,
    DW2_OTA_STATUS_INVALID_LEN,
    DW2_OTA_STATUS_CHECKSUM,
    DW2_OTA_STATUS_MEM_EXCEED
};

typedef struct __attribute__((packed))
{
    uint16_t type;
    uint16_t data_len;
    uint8_t  data[];
} dw2_ota_pkt_hdr_t;

void Dw2_Ota_Init(TASK task);
void Dw2_Ota_RegisterServices(void);
void Dw2_Ota_SetConnectionHandle(uint16_t conn_hdl);
void Dw2_Ota_OnDisconnect(void);
void Dw2_Ota_HandleAccessWrite(LE_GATT_MSG_ACCESS_WRITE_IND_T *ind);
void Dw2_Ota_HandleAccessRead(LE_GATT_MSG_ACCESS_READ_IND_T *ind);
bool Dw2_Ota_IsUpgradeInProgress(void);

#endif /* __DW2_OTA_H__ */
