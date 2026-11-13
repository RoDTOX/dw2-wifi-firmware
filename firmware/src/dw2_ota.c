/**
 * @file dw2_ota.c
 * @brief Over-The-Air (OTA) firmware upgrade & BLE GATT Service implementation for Sonoff DW2.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cmsis_os.h"
#include "hal_system.h"
#include "hal_flash.h"
#include "mw_ota.h"

#include "ble_gatt_if.h"
#include "ble_uuid.h"
#include "ble_gap_if.h"

#include "dw2_config.h"
#include "dw2_protocol.h"
#include "dw2_flash_log.h"
#include "dw2_ota.h"

#define DW2_LOG printf

extern void Dw2_SetEpochSec(uint32_t epoch);

/* Opulinks OTA global state from SDK */
extern T_MwOtaLayoutInfo g_tMwOtaLayoutInfo;
extern int8_t g_bMwOtaImageIdx;
extern uint32_t g_ulMwOtaPrepareHeaderAddr;
extern uint32_t g_ulMwOtaPrepareImageAddr;
extern uint32_t g_ulMwOtaPrepareWriteSize;
extern T_MwOtaFlashHeader g_tMwOtaPrepareHeaderInfo;
extern T_Hal_Flash_4KSectorAddrErase g_tMwOtaEraseFunc;
extern T_Hal_Flash_AddrProgram g_tMwOtaWriteFunc;
extern T_Hal_Flash_AddrRead g_tMwOtaReadFunc;

/* GATT Service UUIDs */
static UINT16 g_gap_svc_uuid  = ATT_SVC_GENERIC_ACCESS;

static UINT16 g_gap_dev_name_uuid      = ATT_CHAR_DEVICE_NAME;
static UINT8  g_gap_dev_name_char[]    = CHAR_DECL_UUID16_ATTR_VAL(LE_GATT_CHAR_PROP_RD, ATT_CHAR_DEVICE_NAME);
static UINT8  g_gap_dev_name_val[16]   = "DW2";

/* Opulinks OTA Service (0xAAAA) */
static UINT16 g_ota_svc_uuid           = DW2_OTA_UUID_SERVICE;
static UINT16 g_ota_data_in_uuid       = DW2_OTA_UUID_DATA_IN;
static UINT8  g_ota_data_in_char[]     = CHAR_DECL_UUID16_ATTR_VAL(LE_GATT_CHAR_PROP_WR_NO_RESP | LE_GATT_CHAR_PROP_WR, DW2_OTA_UUID_DATA_IN);
static UINT8  g_ota_data_in_val[256];

static UINT16 g_ota_data_out_uuid      = DW2_OTA_UUID_DATA_OUT;
static UINT8  g_ota_data_out_char[]    = CHAR_DECL_UUID16_ATTR_VAL(LE_GATT_CHAR_PROP_NTF, DW2_OTA_UUID_DATA_OUT);
static UINT8  g_ota_data_out_val[256];
static UINT16 g_ota_data_out_cccd      = 1;

/* Service DB Definitions */
enum
{
    GAP_IDX_SVC = 0,
    GAP_IDX_DEV_NAME_CHAR,
    GAP_IDX_DEV_NAME_VAL,
    GAP_IDX_TOP
};

static LE_GATT_ATTR_T g_gap_db[GAP_IDX_TOP] =
{
    [GAP_IDX_SVC]           = PRIMARY_SERVICE_DECL_UUID16(&g_gap_svc_uuid),
    [GAP_IDX_DEV_NAME_CHAR] = CHARACTERISTIC_DECL_UUID16(g_gap_dev_name_char),
    [GAP_IDX_DEV_NAME_VAL]  = CHARACTERISTIC_UUID16(&g_gap_dev_name_uuid, LE_GATT_PERMIT_READ, sizeof(g_gap_dev_name_val), 3, g_gap_dev_name_val)
};

void Dw2_Ota_SetDeviceName(const char *name)
{
    if (name)
    {
        strncpy((char *)g_gap_dev_name_val, name, sizeof(g_gap_dev_name_val) - 1);
        g_gap_dev_name_val[sizeof(g_gap_dev_name_val) - 1] = '\0';
        g_gap_db[GAP_IDX_DEV_NAME_VAL].len = (UINT16)strlen((char *)g_gap_dev_name_val);
    }
}

enum
{
    OTA_IDX_SVC = 0,
    OTA_IDX_DATA_IN_CHAR,
    OTA_IDX_DATA_IN_VAL,
    OTA_IDX_DATA_OUT_CHAR,
    OTA_IDX_DATA_OUT_VAL,
    OTA_IDX_DATA_OUT_CCCD,
    OTA_IDX_TOP
};

static LE_GATT_ATTR_T g_ota_db[OTA_IDX_TOP] =
{
    [OTA_IDX_SVC]           = PRIMARY_SERVICE_DECL_UUID16(&g_ota_svc_uuid),
    [OTA_IDX_DATA_IN_CHAR]  = CHARACTERISTIC_DECL_UUID16(g_ota_data_in_char),
    [OTA_IDX_DATA_IN_VAL]   = CHARACTERISTIC_UUID16(&g_ota_data_in_uuid, LE_GATT_PERMIT_AUTHOR_WRITE | LE_GATT_PERMIT_WRITE, sizeof(g_ota_data_in_val), 0, g_ota_data_in_val),
    [OTA_IDX_DATA_OUT_CHAR] = CHARACTERISTIC_DECL_UUID16(g_ota_data_out_char),
    [OTA_IDX_DATA_OUT_VAL]  = CHARACTERISTIC_UUID16(&g_ota_data_out_uuid, 0, sizeof(g_ota_data_out_val), 0, g_ota_data_out_val),
    [OTA_IDX_DATA_OUT_CCCD] = CHAR_CLIENT_CONFIG_DESCRIPTOR(LE_GATT_PERMIT_AUTHOR_READ | LE_GATT_PERMIT_AUTHOR_WRITE, &g_ota_data_out_cccd)
};

/* Standard Current Time Service (CTS 0x1805) & Current Time (0x2A2B) */
static UINT16 g_cts_svc_uuid       = ATT_SVC_CURRENT_TIME;
static UINT16 g_cts_char_uuid      = ATT_CHAR_CT_TIME;
static UINT8  g_cts_char_decl[]    = CHAR_DECL_UUID16_ATTR_VAL(LE_GATT_CHAR_PROP_RD | LE_GATT_CHAR_PROP_WR, ATT_CHAR_CT_TIME);
static UINT8  g_cts_time_val[10]   = {0};

enum
{
    CTS_IDX_SVC = 0,
    CTS_IDX_CHAR,
    CTS_IDX_VAL,
    CTS_IDX_TOP
};

static LE_GATT_ATTR_T g_cts_db[CTS_IDX_TOP] =
{
    [CTS_IDX_SVC]  = PRIMARY_SERVICE_DECL_UUID16(&g_cts_svc_uuid),
    [CTS_IDX_CHAR] = CHARACTERISTIC_DECL_UUID16(g_cts_char_decl),
    [CTS_IDX_VAL]  = CHARACTERISTIC_UUID16(&g_cts_char_uuid, LE_GATT_PERMIT_READ | LE_GATT_PERMIT_WRITE, sizeof(g_cts_time_val), sizeof(g_cts_time_val), g_cts_time_val)
};

static LE_GATT_SERVICE_T *g_p_gap_svc = NULL;
static LE_GATT_SERVICE_T *g_p_ota_svc = NULL;
static LE_GATT_SERVICE_T *g_p_cts_svc = NULL;

static uint16_t g_conn_hdl   = 0xFFFFU;
static uint16_t g_notify_hdl = 0xFFFFU;
static TASK g_parent_task = NULL;

/* Active OTA transfer context */
typedef struct
{
    bool     active;
    uint16_t rx_pkt_ack;
    uint16_t pkt_count;
    uint32_t total_size;
    uint32_t current_size;
    uint32_t current_chksum;
} dw2_ota_ctx_t;

static dw2_ota_ctx_t g_ota_ctx = {0};

static void Dw2_Ota_SendResponse(uint16_t type, const uint8_t *data, uint16_t len)
{
    if (g_conn_hdl == 0xFFFFU || g_notify_hdl == 0xFFFFU)
    {
        return;
    }

    uint8_t buf[256];
    uint16_t total_len = (uint16_t)(sizeof(dw2_ota_pkt_hdr_t) + len);
    if (total_len > sizeof(buf))
    {
        total_len = sizeof(buf);
    }

    dw2_ota_pkt_hdr_t *hdr = (dw2_ota_pkt_hdr_t *)buf;
    hdr->type     = type;
    hdr->data_len = len;
    if (len > 0 && data != NULL)
    {
        memcpy(hdr->data, data, len);
    }

    LeGattCharValNotify(g_conn_hdl, g_notify_hdl, total_len, buf);
}

void Dw2_Ota_Init(TASK task)
{
    MwOta_PreInitCold();

    /* Initialize OTA dual-bank layout */
    T_MwOtaLayoutInfo tLayout;
    tLayout.ulaHeaderAddr[0] = MW_OTA_HEADER_ADDR_1;
    tLayout.ulaHeaderAddr[1] = MW_OTA_HEADER_ADDR_2;
    tLayout.ulaImageAddr[0]  = MW_OTA_IMAGE_ADDR_1;
    tLayout.ulaImageAddr[1]  = MW_OTA_IMAGE_ADDR_2;
    tLayout.ulImageSize      = MW_OTA_IMAGE_SIZE;
    MwOta_Init(&tLayout, 0);

    g_parent_task = task;
    LeGattInit(task);
}

void Dw2_Ota_RegisterServices(void)
{
    g_p_gap_svc = LeGattRegisterService(g_gap_db, GAP_IDX_TOP);
    g_p_ota_svc = LeGattRegisterService(g_ota_db, OTA_IDX_TOP);
    g_p_cts_svc = LeGattRegisterService(g_cts_db, CTS_IDX_TOP);

    if (g_p_ota_svc)
    {
        g_notify_hdl = g_ota_db[OTA_IDX_DATA_OUT_VAL].handle;
        DW2_LOG("[OTA] Opulinks OTA GATT Service (0xAAAA) & Standard CTS (0x1805) registered! notify_hdl=%u\r\n", g_notify_hdl);
    }
    else
    {
        DW2_LOG("[OTA] Failed to register OTA GATT service!\r\n");
    }
}

void Dw2_Ota_SetConnectionHandle(uint16_t conn_hdl)
{
    g_conn_hdl = conn_hdl;
    DW2_LOG("[OTA] Connected! conn_hdl=%u\r\n", conn_hdl);
}

void Dw2_Ota_OnDisconnect(void)
{
    if (g_ota_ctx.active)
    {
        DW2_LOG("[OTA] Disconnect during OTA! Aborting...\r\n");
        MwOta_DataGiveUp();
        memset(&g_ota_ctx, 0, sizeof(g_ota_ctx));
    }
    g_conn_hdl = 0xFFFFU;
    DW2_LOG("[OTA] Disconnected.\r\n");
}

bool Dw2_Ota_IsUpgradeInProgress(void)
{
    return g_ota_ctx.active;
}

static void HandleOtaVersionReq(void)
{
    uint16_t pid = 0, cid = 0, fid = 0;
    uint8_t state = MwOta_VersionGet(&pid, &cid, &fid);

    uint8_t resp[7];
    resp[0] = (state == MW_OTA_OK) ? DW2_OTA_STATUS_SUCCESS : DW2_OTA_STATUS_HW_FAILURE;
    resp[1] = (uint8_t)(pid & 0xFF);
    resp[2] = (uint8_t)((pid >> 8) & 0xFF);
    resp[3] = (uint8_t)(cid & 0xFF);
    resp[4] = (uint8_t)((cid >> 8) & 0xFF);
    resp[5] = (uint8_t)(fid & 0xFF);
    resp[6] = (uint8_t)((fid >> 8) & 0xFF);

    DW2_LOG("[OTA] Version Req -> pid=0x%04X cid=0x%04X fid=0x%04X\r\n", pid, cid, fid);
    Dw2_Ota_SendResponse(DW2_RSP_OTA_VERSION, resp, sizeof(resp));
}

static void HandleOtaUpgradeReq(const uint8_t *data, uint16_t len)
{
    if (len < 26)
    {
        uint8_t st = DW2_OTA_STATUS_INVALID_LEN;
        Dw2_Ota_SendResponse(DW2_RSP_OTA_UPGRADE, &st, 1);
        return;
    }

    if (g_ota_ctx.active)
    {
        uint8_t st = DW2_OTA_STATUS_IN_PROGRESS;
        Dw2_Ota_SendResponse(DW2_RSP_OTA_UPGRADE, &st, 1);
        return;
    }

    uint16_t rx_pkt = (uint16_t)(data[0] | (data[1] << 8));
    T_MwOtaFlashHeader *hdr = (T_MwOtaFlashHeader *)&data[2];

    DW2_LOG("[OTA] Upgrade Req: total=%u B, chksum=0x%08X, rx_pkt_ack=%u\r\n",
            hdr->ulImageSize, hdr->ulImageSum, rx_pkt);

    /* Guarantee valid OTA layout and bank pointer even if cold boot lacked OTA headers */
    if (g_tMwOtaLayoutInfo.ulImageSize == 0)
    {
        g_tMwOtaLayoutInfo.ulaHeaderAddr[0] = MW_OTA_HEADER_ADDR_1;
        g_tMwOtaLayoutInfo.ulaHeaderAddr[1] = MW_OTA_HEADER_ADDR_2;
        g_tMwOtaLayoutInfo.ulaImageAddr[0]  = MW_OTA_IMAGE_ADDR_1;
        g_tMwOtaLayoutInfo.ulaImageAddr[1]  = MW_OTA_IMAGE_ADDR_2;
        g_tMwOtaLayoutInfo.ulImageSize      = MW_OTA_IMAGE_SIZE;
    }

    if (g_bMwOtaImageIdx == MW_OTA_IDX_INVALID)
    {
        g_bMwOtaImageIdx = 1; /* Target Bank 0 (0x00005000) for initial OTA */
    }

    if (g_tMwOtaEraseFunc == NULL)
    {
        g_tMwOtaEraseFunc = Hal_Flash_4KSectorAddrErase;
        g_tMwOtaWriteFunc = Hal_Flash_AddrProgram;
        g_tMwOtaReadFunc  = Hal_Flash_AddrRead;
    }

    uint8_t state = MwOta_Prepare(hdr->uwProjectId, hdr->uwChipId, hdr->uwFirmwareId, hdr->ulImageSize, hdr->ulImageSum);
    if (state == MW_OTA_OK)
    {
        g_ota_ctx.active         = true;
        g_ota_ctx.rx_pkt_ack     = rx_pkt;
        g_ota_ctx.pkt_count      = 0;
        g_ota_ctx.total_size     = hdr->ulImageSize;
        g_ota_ctx.current_size   = 0;
        g_ota_ctx.current_chksum = 0;

        uint8_t st = DW2_OTA_STATUS_SUCCESS;
        Dw2_Ota_SendResponse(DW2_RSP_OTA_UPGRADE, &st, 1);
    }
    else
    {
        DW2_LOG("[OTA] MwOta_Prepare failed! state=%u\r\n", state);
        uint8_t st = DW2_OTA_STATUS_HW_FAILURE;
        Dw2_Ota_SendResponse(DW2_RSP_OTA_UPGRADE, &st, 1);
    }
}

static void HandleOtaRawReq(const uint8_t *data, uint16_t len)
{
    if (!g_ota_ctx.active)
    {
        uint8_t st = DW2_OTA_STATUS_NOT_ACTIVE;
        Dw2_Ota_SendResponse(DW2_RSP_OTA_END, &st, 1);
        return;
    }

    uint8_t state = MwOta_DataIn((uint8_t *)data, len);
    if (state != MW_OTA_OK)
    {
        DW2_LOG("[OTA] MwOta_DataIn error! (written=%u/%u)\r\n", g_ota_ctx.current_size, g_ota_ctx.total_size);
        uint8_t st = DW2_OTA_STATUS_HW_FAILURE;
        Dw2_Ota_SendResponse(DW2_RSP_OTA_END, &st, 1);
        g_ota_ctx.active = false;
        return;
    }

    g_ota_ctx.current_size += len;
    g_ota_ctx.pkt_count++;

    /* Send ACK every rx_pkt_ack bursts or when complete */
    if ((g_ota_ctx.rx_pkt_ack > 0 && g_ota_ctx.pkt_count >= g_ota_ctx.rx_pkt_ack) ||
        (g_ota_ctx.current_size >= g_ota_ctx.total_size))
    {
        g_ota_ctx.pkt_count = 0;
        Dw2_Ota_SendResponse(DW2_RSP_OTA_RAW, NULL, 0);
    }
}

static uint32_t Dw2_CalculateFlashChecksum(uint32_t addr, uint32_t size)
{
    uint8_t buf[256];
    uint32_t sum = 0;
    uint32_t offset = 0;
    while (offset < size)
    {
        uint32_t chunk_len = (size - offset > sizeof(buf)) ? sizeof(buf) : (size - offset);
        if (Hal_Flash_AddrRead(SPI_IDX_0, addr + offset, 0, chunk_len, buf) != 0)
        {
            return 0;
        }
        for (uint32_t i = 0; i < chunk_len; i++)
        {
            sum += buf[i];
        }
        offset += chunk_len;
    }
    return sum;
}

static void HandleOtaEndReq(const uint8_t *data, uint16_t len)
{
    (void)len;
    uint8_t req_status = data[0];

    if (!g_ota_ctx.active)
    {
        uint8_t st = DW2_OTA_STATUS_NOT_ACTIVE;
        Dw2_Ota_SendResponse(DW2_RSP_OTA_END, &st, 1);
        return;
    }

    if (req_status == DW2_OTA_STATUS_SUCCESS)
    {
        /* Check size matches expected */
        if (g_ulMwOtaPrepareWriteSize != g_tMwOtaPrepareHeaderInfo.ulImageSize)
        {
            DW2_LOG("[OTA] Size mismatch: written=%u expected=%u\r\n",
                    g_ulMwOtaPrepareWriteSize, g_tMwOtaPrepareHeaderInfo.ulImageSize);
            uint8_t st = DW2_OTA_STATUS_INVALID_LEN;
            Dw2_Ota_SendResponse(DW2_RSP_OTA_END, &st, 1);
            MwOta_DataGiveUp();
            g_ota_ctx.active = false;
            return;
        }

        /* Calculate checksum directly from SPI Flash without heap allocation */
        uint32_t calculated_csum = Dw2_CalculateFlashChecksum(g_ulMwOtaPrepareImageAddr, g_tMwOtaPrepareHeaderInfo.ulImageSize);
        DW2_LOG("[OTA] Checksum verification: calc=0x%08X expected=0x%08X\r\n",
                calculated_csum, g_tMwOtaPrepareHeaderInfo.ulImageSum);

        if (calculated_csum == g_tMwOtaPrepareHeaderInfo.ulImageSum)
        {
            /* Compute header checksum over first 20 bytes (total size minus 4-byte checksum field) */
            uint32_t ulHeaderSum = 0;
            uint32_t ulSize = sizeof(T_MwOtaFlashHeader) - 4;
            uint8_t *pubData = (uint8_t *)&g_tMwOtaPrepareHeaderInfo;
            for (uint32_t i = 0; i < ulSize; i++)
            {
                ulHeaderSum += pubData[i];
            }
            g_tMwOtaPrepareHeaderInfo.ulHeaderSum = ulHeaderSum;

            /* Write OTA Header into flash at g_ulMwOtaPrepareHeaderAddr */
            Hal_Flash_AddrProgram(SPI_IDX_0, g_ulMwOtaPrepareHeaderAddr, 0, sizeof(T_MwOtaFlashHeader), (uint8_t *)&g_tMwOtaPrepareHeaderInfo);
            DW2_LOG("[OTA] Header written to 0x%08X (seq=%u, sum=0x%08X)! Upgrade SUCCESS! Rebooting...\r\n",
                    g_ulMwOtaPrepareHeaderAddr, g_tMwOtaPrepareHeaderInfo.uwSeqNum, ulHeaderSum);

            uint8_t st = DW2_OTA_STATUS_SUCCESS;
            Dw2_Ota_SendResponse(DW2_RSP_OTA_END, &st, 1);

            /* Give radio time to transmit notification packet */
            osDelay(1000);

            /* Switch APS clock to XTAL before reset to prevent glitch-free clock mux hang */
            extern uint32_t Hal_Sys_SwResetAll_patch(void);
            Hal_Sys_SwResetAll_patch();
            return;
        }
        else
        {
            DW2_LOG("[OTA] MwOta checksum mismatch!\r\n");
            uint8_t st = DW2_OTA_STATUS_CHECKSUM;
            Dw2_Ota_SendResponse(DW2_RSP_OTA_END, &st, 1);
            MwOta_DataGiveUp();
        }
    }
    else
    {
        DW2_LOG("[OTA] Client aborted upgrade\r\n");
        MwOta_DataGiveUp();
        uint8_t st = DW2_OTA_STATUS_SUCCESS;
        Dw2_Ota_SendResponse(DW2_RSP_OTA_END, &st, 1);
    }

    g_ota_ctx.active = false;
}

static void HandleTimeSyncReq(const uint8_t *data, uint16_t len)
{
    if (len >= 4)
    {
        uint32_t epoch = (uint32_t)(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
        Dw2_SetEpochSec(epoch);
        DW2_LOG("[OTA] Time synced to Epoch: %u\r\n", epoch);
        uint8_t st = 0;
        Dw2_Ota_SendResponse(DW2_RSP_TIME_SYNC, &st, 1);
    }
}

static void HandleLogDumpReq(const uint8_t *data, uint16_t len)
{
    (void)len;
    uint32_t start_idx = 0;
    if (data != NULL && len >= 4)
    {
        start_idx = (uint32_t)(data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
    }

    uint32_t total = Dw2_FlashLog_GetTotalCount();
    DW2_LOG("[OTA] Log dump requested starting at idx=%u (total logged=%u)\r\n", start_idx, total);

    /* Stream in blocks of up to 4 entries (64 bytes) */
    dw2_log_entry_t batch[4];
    uint32_t batch_count = 0;

    for (uint32_t i = 0; i < 4 && (start_idx + i) < DW2_FLASH_LOG_MAX_ENTRIES; i++)
    {
        if (Dw2_FlashLog_Read(start_idx + i, &batch[i]))
        {
            batch_count++;
        }
        else
        {
            break;
        }
    }

    if (batch_count > 0)
    {
        Dw2_Ota_SendResponse(DW2_RSP_LOG_DUMP, (uint8_t *)batch, (uint16_t)(batch_count * sizeof(dw2_log_entry_t)));
    }
    else
    {
        /* End of logs */
        uint8_t end_tag[2] = {0xFF, 0xFF};
        Dw2_Ota_SendResponse(DW2_RSP_LOG_DUMP, end_tag, sizeof(end_tag));
    }
}

static uint32_t Dw2_ConvertCtsToEpoch(const uint8_t *val)
{
    /* val: 10 bytes:
       0-1: year (uint16_t Little-Endian)
       2:   month (1-12)
       3:   day (1-31)
       4:   hours (0-23)
       5:   minutes (0-59)
       6:   seconds (0-59)
       7:   day_of_week
       8:   fractions_256
       9:   adjust_reason
    */
    uint16_t year  = (uint16_t)(val[0] | (val[1] << 8));
    uint8_t  month = val[2];
    uint8_t  day   = val[3];
    uint8_t  hour  = val[4];
    uint8_t  min   = val[5];
    uint8_t  sec   = val[6];

    if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31)
    {
        return 0;
    }

    /* Convert date to days since 1970 */
    static const uint16_t days_before_month[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint32_t days = 0;
    for (uint16_t y = 1970; y < year; y++)
    {
        days += ((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0)) ? 366 : 365;
    }

    days += days_before_month[month - 1];
    if (month > 2 && (((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))))
    {
        days += 1;
    }
    days += (day - 1);

    uint32_t epoch = days * 86400U + hour * 3600U + min * 60U + sec;
    return epoch;
}

void Dw2_Ota_HandleAccessWrite(LE_GATT_MSG_ACCESS_WRITE_IND_T *ind)
{
    UINT8 att_err = 0;

    if (g_p_cts_svc && ind->handle >= g_p_cts_svc->startHdl && ind->handle <= g_p_cts_svc->endHdl)
    {
        UINT16 cts_attr = ind->handle - g_p_cts_svc->startHdl;
        if (cts_attr == CTS_IDX_VAL && ind->len >= 7)
        {
            uint32_t epoch = Dw2_ConvertCtsToEpoch(ind->pVal);
            if (epoch > 0)
            {
                Dw2_SetEpochSec(epoch);
                DW2_LOG("[CTS] Phone synced time via Standard CTS: epoch=%u\r\n", epoch);
            }
        }
        LeGattAccessWriteRsp(ind->conn_hdl, ind->flag, ind->handle, att_err);
        return;
    }

    UINT16 attr_id = ind->handle - g_p_ota_svc->startHdl;

    switch (attr_id)
    {
        case OTA_IDX_DATA_IN_VAL:
        {
            if (ind->len >= (int)sizeof(dw2_ota_pkt_hdr_t))
            {
                dw2_ota_pkt_hdr_t *hdr = (dw2_ota_pkt_hdr_t *)ind->pVal;
                uint16_t payload_len = (uint16_t)(ind->len - sizeof(dw2_ota_pkt_hdr_t));

                switch (hdr->type)
                {
                    case DW2_REQ_OTA_VERSION:
                        HandleOtaVersionReq();
                        break;
                    case DW2_REQ_OTA_UPGRADE:
                        HandleOtaUpgradeReq(hdr->data, payload_len);
                        break;
                    case DW2_REQ_OTA_RAW:
                        HandleOtaRawReq(hdr->data, payload_len);
                        break;
                    case DW2_REQ_OTA_END:
                        HandleOtaEndReq(hdr->data, payload_len);
                        break;
                    case DW2_REQ_TIME_SYNC:
                        HandleTimeSyncReq(hdr->data, payload_len);
                        break;
                    case DW2_REQ_LOG_DUMP:
                        HandleLogDumpReq(hdr->data, payload_len);
                        break;
                    default:
                        DW2_LOG("[OTA] Unknown request type: 0x%04X\r\n", hdr->type);
                        break;
                }
            }
            break;
        }

        case OTA_IDX_DATA_OUT_CCCD:
        {
            if (ind->len == 2)
            {
                g_ota_data_out_cccd = *((UINT16 *)ind->pVal);
                LeGattChangeAttrVal(g_p_ota_svc, OTA_IDX_DATA_OUT_CCCD, 2, &g_ota_data_out_cccd);
                DW2_LOG("[OTA] CCCD updated to %u\r\n", g_ota_data_out_cccd);
            }
            else
            {
                att_err = LE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
            }
            break;
        }

        default:
            att_err = LE_ATT_ERR_WRITE_NOT_PERMITTED;
            break;
    }

    LeGattAccessWriteRsp(ind->conn_hdl, ind->flag, ind->handle, att_err);
}

void Dw2_Ota_HandleAccessRead(LE_GATT_MSG_ACCESS_READ_IND_T *ind)
{
    UINT8 att_err = 0;
    LeGattAccessReadRsp(ind->conn_hdl, ind->handle, att_err);
}
