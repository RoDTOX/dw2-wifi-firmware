/**
 * @file dw2_app.c
 * @brief Enhanced DW2 Pro firmware:
 * 1. Auto Time-Sync: accepts BLE handshake sync from ESP32, Home Assistant or phone
 * 2. Complete Flash Logger: logs Door Open/Close, Door Ajar alarms, Tamper/Button, Heartbeats, Low Battery
 * 3. Power-saving LED: blinks on door open/close only during the first 24 hours (install mode)
 * 4. 45s Door Ajar warning repeated every 10s on BLE
 * 5. Adaptive Burst: 300ms fast + 500ms safe
 */

#include <stdio.h>
#include <string.h>

#include "cmsis_os.h"
#include "hal_auxadc.h"
#include "hal_flash_patch.h"
#include "hal_pin.h"
#include "hal_pin_def.h"
#include "hal_spi.h"
#include "hal_tick.h"
#include "hal_vic.h"
#include "ps_public.h"

#include "ble_cm_if.h"
#include "ble_gap_if.h"
#include "ble_hci_if.h"
#include "ble_util.h"
#include "msg.h"

#include "dw2_app.h"
#include "dw2_config.h"
#include "dw2_protocol.h"

#define DW2_LOG printf

extern uint32_t g_ulHalAux_AverageCount;

typedef struct
{
    TASKPACK        task;
    dw2_ble_state_t state;
    uint32_t        event_counter;
    uint32_t        base_epoch_sec;
    uint32_t        boot_tick;
    uint32_t        base_tick;
    uint32_t        flash_log_offset;
    uint32_t        door_opened_epoch;
    uint32_t        last_toggle_tick;
    uint16_t        last_open_duration;
    dw2_nv_config_t config;
    uint8_t         door_open;
    uint8_t         time_synced;
    uint8_t         pending_burst;
    uint8_t         pending_flags;
    uint8_t         door_ajar_active;
    uint8_t         rapid_toggle_count;
} dw2_app_ctx_t;

static dw2_app_ctx_t g_app;

static uint8_t g_adv_data[3 + 2 + sizeof(dw2_adv_payload_t)];

static const uint8_t g_scan_rsp_data[] =
{
    0x04,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'D', 'W', '2'
};

static void Dw2_LedSet(uint8_t on)
{
    Hal_Vic_GpioOutput(DW2_LED_IO_PORT, on ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);
}

static void Dw2_LedBlink(uint8_t count, uint32_t on_ms, uint32_t off_ms)
{
    for (uint8_t i = 0; i < count; i++)
    {
        Dw2_LedSet(1U);
        osDelay(on_ms);
        Dw2_LedSet(0U);
        if (i + 1 < count)
        {
            osDelay(off_ms);
        }
    }
}

/* Check if we are still within the first 24h install window */
static uint8_t Dw2_IsLedFeedbackAllowed(void)
{
    uint32_t elapsed_ticks = Hal_Tick_Diff(g_app.boot_tick);
    uint32_t elapsed_sec = elapsed_ticks / (SystemCoreClockGet());
    return (elapsed_sec < DW2_LED_DOOR_ACTIVE_SEC) ? 1U : 0U;
}

static uint32_t Dw2_GetCurrentEpoch(void)
{
    uint32_t elapsed_ticks = Hal_Tick_Diff(g_app.base_tick);
    uint32_t elapsed_sec = elapsed_ticks / (SystemCoreClockGet());
    return g_app.base_epoch_sec + elapsed_sec;
}

void Dw2_ApplyExternalTimeSync(uint32_t epoch_sec)
{
    g_app.base_epoch_sec = epoch_sec;
    g_app.base_tick = Hal_Tick_Diff(0);
    g_app.time_synced = 1U;
    DW2_LOG("[DW2] Time Synced from BLE: Epoch=%u\r\n", epoch_sec);
    Dw2_LedBlink(2, DW2_LED_BLINK_LONG_MS, 150);
}

static void Dw2_LoadConfig(void)
{
    Hal_Flash_AddrRead(SPI_IDX_0, DW2_FLASH_CONFIG_ADDR, 0, sizeof(dw2_nv_config_t), (uint8_t *)&g_app.config);
    if (g_app.config.magic != 0x44574346U) /* "DWCF" */
    {
        DW2_LOG("[DW2] No valid config in Flash -> defaults loaded\r\n");
        memset(&g_app.config, 0, sizeof(g_app.config));
        g_app.config.magic = 0x44574346U;
        g_app.config.version = 1U;
        g_app.config.wifi_fallback_en = 0U; /* Default BLE only */
        g_app.config.led_mode = 1U;         /* 24h install mode */
        g_app.config.ble_ack_timeout_ms = 1000U;
        g_app.config.door_ajar_sec = 45U;
        strcpy(g_app.config.mqtt_topic, "home/frontdoor");
    }
    else
    {
        DW2_LOG("[DW2] Config loaded: fallback=%u ajar=%us topic=%s\r\n",
                g_app.config.wifi_fallback_en, g_app.config.door_ajar_sec, g_app.config.mqtt_topic);
    }
}

void Dw2_SaveConfig(const dw2_nv_config_t *new_cfg)
{
    memcpy(&g_app.config, new_cfg, sizeof(dw2_nv_config_t));
    g_app.config.magic = 0x44574346U;
    g_app.config.version = 1U;
    Hal_Flash_4KSectorAddrErase(SPI_IDX_0, DW2_FLASH_CONFIG_ADDR);
    Hal_Flash_AddrProgram(SPI_IDX_0, DW2_FLASH_CONFIG_ADDR, 0, sizeof(dw2_nv_config_t), (uint8_t *)&g_app.config);
    DW2_LOG("[DW2] Config saved to Flash!\r\n");
}

static void Dw2_LogEventToFlash(uint32_t epoch_sec, uint32_t counter, uint16_t vbat_mv, uint16_t open_duration_sec, uint8_t flags)
{
    dw2_log_entry_t entry;
    entry.epoch_sec         = epoch_sec;
    entry.event_counter     = counter;
    entry.battery_mv        = vbat_mv;
    entry.open_duration_sec = open_duration_sec;
    entry.flags             = flags;
    entry.reserved[0]       = 0;
    entry.reserved[1]       = 0;

    uint8_t *raw = (uint8_t *)&entry;
    uint8_t csum = 0;
    for (size_t i = 0; i < sizeof(entry) - 1; i++)
    {
        csum ^= raw[i];
    }
    entry.checksum = csum;

    uint32_t write_addr = DW2_FLASH_LOG_BASE_ADDR + g_app.flash_log_offset;
    if ((g_app.flash_log_offset % 4096) == 0)
    {
        Hal_Flash_4KSectorAddrErase(SPI_IDX_0, write_addr);
    }

    Hal_Flash_AddrProgram(SPI_IDX_0, write_addr, 0, sizeof(entry), (uint8_t *)&entry);
    g_app.flash_log_offset += sizeof(entry);
    if (g_app.flash_log_offset >= DW2_FLASH_LOG_SIZE)
    {
        g_app.flash_log_offset = 0;
    }
}

static uint16_t Dw2_ReadBatteryMv(void)
{
    float vbat = 0.0f;
    g_ulHalAux_AverageCount = DW2_VBAT_AVG_COUNT;
    Hal_Pin_ConfigSet(DW2_BATTERY_IO_PORT, PIN_TYPE_GPIO_OUTPUT_HIGH, PIN_DRIVING_FLOAT);
    osDelay(3);
    Hal_Aux_IoVoltageGet(DW2_BATTERY_IO_PORT, &vbat);
    Hal_Pin_ConfigSet(DW2_BATTERY_IO_PORT, PIN_TYPE_NONE, PIN_DRIVING_FLOAT);

    if (vbat < 0.0f) return 0U;
    return (uint16_t)(vbat * 1000.0f + 0.5f);
}

static void Dw2_ArmGpioInt(E_GpioIdx_t pin, E_GpioLevel_t current_level, T_Gpio_CallBack cb)
{
    Hal_Vic_GpioCallBackFuncSet(pin, cb);
    Hal_Vic_GpioDirection(pin, GPIO_INPUT);
    Hal_Vic_GpioIntTypeSel(pin, INT_TYPE_LEVEL);
    Hal_Vic_GpioIntInv(pin, (current_level == GPIO_LEVEL_LOW) ? 0U : 1U);
    Hal_Vic_GpioIntMask(pin, 0U);
    Hal_Vic_GpioIntEn(pin, 1U);
}

static void Dw2_DoorGpioCb(E_GpioIdx_t gpio_idx)
{
    (void)gpio_idx;
    Hal_Vic_GpioIntEn(DW2_MAGNETIC_IO_PORT, 0U);
    LeSendMessageAfter(&g_app.task, DW2_MSG_DOOR_DEBOUNCE, NULL, DW2_DOOR_DEBOUNCE_MS);
}

static void Dw2_ButtonGpioCb(E_GpioIdx_t gpio_idx)
{
    (void)gpio_idx;
    Hal_Vic_GpioIntEn(DW2_BUTTON_IO_PORT, 0U);
    LeSendMessageAfter(&g_app.task, DW2_MSG_BUTTON_DEBOUNCE, NULL, DW2_BUTTON_DEBOUNCE_MS);
}

static void Dw2_BuildAdvPacket(uint8_t flags, uint16_t battery_mv, uint32_t epoch_sec, uint16_t open_duration_sec)
{
    dw2_adv_payload_t payload;

    payload.company_id        = DW2_BLE_COMPANY_ID;
    payload.magic[0]          = DW2_BLE_MAGIC_0;
    payload.magic[1]          = DW2_BLE_MAGIC_1;
    payload.version           = DW2_BLE_PROTO_VERSION;
    payload.flags             = flags;
    payload.event_counter     = ++g_app.event_counter;
    payload.epoch_sec         = epoch_sec;
    payload.battery_mv        = battery_mv;
    payload.open_duration_sec = open_duration_sec;
    payload.bthome_id         = 0x1AU;
    payload.bthome_val        = (flags & DW2_FLAG_DOOR_OPEN) ? 1U : 0U;

    g_adv_data[0] = 0x02U;
    g_adv_data[1] = GAP_ADTYPE_FLAGS;
    g_adv_data[2] = GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED;
    g_adv_data[3] = (uint8_t)(1U + sizeof(dw2_adv_payload_t));
    g_adv_data[4] = GAP_ADTYPE_MANUFACTURER_SPECIFIC;
    memcpy(&g_adv_data[5], &payload, sizeof(payload));
}

static void Dw2_SetAdvParams(uint16_t min_int, uint16_t max_int, uint8_t adv_type)
{
    LE_GAP_ADVERTISING_PARAM_T para;
    para.interval_min   = min_int;
    para.interval_max   = max_int;
    para.type           = adv_type;
    para.own_addr_type  = LE_HCI_OWN_ADDR_PUBLIC;
    para.peer_addr_type = LE_HCI_ADV_PEER_ADDR_PUBLIC;
    memset(para.peer_addr, 0, sizeof(para.peer_addr));
    para.channel_map    = 0x07U;
    para.filter_policy  = LE_HCI_ADV_FILT_NONE;
    LeGapSetAdvParameter(&para);
}

static void Dw2_TriggerAdaptiveBurst(uint8_t extra_flags)
{
    uint8_t flags = (g_app.door_open ? DW2_FLAG_DOOR_OPEN : 0U) | extra_flags;
    if (g_app.time_synced)
    {
        flags |= (DW2_FLAG_TIME_SYNCED | DW2_FLAG_RTC_BEACON);
    }
    if (g_app.config.wifi_fallback_en)
    {
        flags |= DW2_FLAG_WIFI_FALLBACK;
    }

    uint16_t vbat_mv = Dw2_ReadBatteryMv();
    if (vbat_mv > 0 && vbat_mv < DW2_VBAT_LOW_THRESHOLD_MV)
    {
        flags |= DW2_FLAG_LOW_BATTERY;
    }

    uint32_t epoch_sec = Dw2_GetCurrentEpoch();

    /* Log event to Flash Black-Box (Door events, Alarms, Heartbeats, Low Battery) */
    Dw2_LogEventToFlash(epoch_sec, g_app.event_counter + 1, vbat_mv, g_app.last_open_duration, flags);

    if (g_app.state == DW2_BLE_STATE_ADVERTISING_FAST || g_app.state == DW2_BLE_STATE_ADVERTISING_SAFE)
    {
        g_app.pending_burst = 1U;
        g_app.pending_flags = flags;
        LeGapAdvertisingEnable(FALSE);
        return;
    }

    Dw2_BuildAdvPacket(flags, vbat_mv, epoch_sec, g_app.last_open_duration);
    DW2_LOG("[DW2] Burst: open=%u cnt=%u epoch=%u vbat=%umV dur=%us flags=0x%02X\r\n",
            g_app.door_open, g_app.event_counter, epoch_sec, vbat_mv, g_app.last_open_duration, flags);

    Dw2_SetAdvParams(DW2_BLE_ADV_FAST_MIN, DW2_BLE_ADV_FAST_MAX, LE_HCI_ADV_TYPE_ADV_NONCONN_IND);
    LeGapSetAdvData(sizeof(g_adv_data), g_adv_data);
}

static void Dw2_CmMsgHandler(TASK task, MESSAGEID id, MESSAGE message)
{
    (void)task;

    switch (id)
    {
        case LE_CM_MSG_INIT_COMPLETE_CFM:
        {
            DW2_LOG("[DW2] BLE Init OK\r\n");
            LeSetScanRspData(sizeof(g_scan_rsp_data), (UINT8 *)g_scan_rsp_data);

            /* Boot visual feedback: 3 short blinks */
            Dw2_LedBlink(3, DW2_LED_BLINK_SHORT_MS, 60);

            /* Start 5-minute Connectable Time-Sync Window */
            g_app.state = DW2_BLE_STATE_CONNECTABLE_SYNC;
            Dw2_SetAdvParams(0x00A0, 0x00A0, LE_HCI_ADV_TYPE_ADV_IND);
            Dw2_BuildAdvPacket(0U, Dw2_ReadBatteryMv(), 0U, 0U);
            LeGapSetAdvData(sizeof(g_adv_data), g_adv_data);

            LeSendMessageAfter(&g_app.task, DW2_MSG_TIME_SYNC_TIMEOUT, NULL, DW2_TIME_SYNC_WINDOW_MS);

            E_GpioLevel_t door_lvl = Hal_Vic_GpioInput(DW2_MAGNETIC_IO_PORT);
            E_GpioLevel_t btn_lvl  = Hal_Vic_GpioInput(DW2_BUTTON_IO_PORT);
            g_app.door_open = (door_lvl != DW2_DOOR_CLOSED_LEVEL) ? 1U : 0U;
            Dw2_ArmGpioInt(DW2_MAGNETIC_IO_PORT, door_lvl, Dw2_DoorGpioCb);
            Dw2_ArmGpioInt(DW2_BUTTON_IO_PORT, btn_lvl, Dw2_ButtonGpioCb);
            break;
        }

        case LE_CM_MSG_SET_ADVERTISING_PARAMS_CFM:
            LeGapAdvertisingEnable(TRUE);
            break;

        case LE_CM_MSG_SET_ADVERTISING_DATA_CFM:
            LeGapAdvertisingEnable(TRUE);
            break;

        case LE_CM_MSG_ENTER_ADVERTISING_CFM:
        {
            LE_CM_MSG_ENTER_ADVERTISING_CFM_T *cfm = (LE_CM_MSG_ENTER_ADVERTISING_CFM_T *)message;
            if (cfm->status == SYS_ERR_SUCCESS)
            {
                if (g_app.state != DW2_BLE_STATE_CONNECTABLE_SYNC)
                {
                    g_app.state = DW2_BLE_STATE_ADVERTISING_FAST;
                    LeSendMessageAfter(&g_app.task, DW2_MSG_BURST_PHASE2, NULL, DW2_ADV_BURST_FAST_MS);
                }
            }
            break;
        }

        case LE_CM_MSG_EXIT_ADVERTISING_CFM:
        {
            if (g_app.state == DW2_BLE_STATE_ADVERTISING_FAST)
            {
                g_app.state = DW2_BLE_STATE_ADVERTISING_SAFE;
                Dw2_SetAdvParams(DW2_BLE_ADV_SAFE_MIN, DW2_BLE_ADV_SAFE_MAX, LE_HCI_ADV_TYPE_ADV_NONCONN_IND);
                LeSendMessageAfter(&g_app.task, DW2_MSG_STOP_BURST, NULL, DW2_ADV_BURST_TOTAL_MS - DW2_ADV_BURST_FAST_MS);
            }
            else
            {
                g_app.state = DW2_BLE_STATE_IDLE;
                if (g_app.pending_burst)
                {
                    g_app.pending_burst = 0U;
                    Dw2_TriggerAdaptiveBurst(g_app.pending_flags);
                }
            }
            break;
        }

        default:
            break;
    }
}

static void Dw2_AppMsgHandler(TASK task, MESSAGEID id, MESSAGE message)
{
    (void)task;
    (void)message;

    switch (id)
    {
        case DW2_MSG_INITIALIZING:
            LeCmInit(&g_app.task);
            break;

        case DW2_MSG_BURST_PHASE2:
            if (g_app.state == DW2_BLE_STATE_ADVERTISING_FAST)
            {
                LeGapAdvertisingEnable(FALSE);
            }
            break;

        case DW2_MSG_STOP_BURST:
            if (g_app.state == DW2_BLE_STATE_ADVERTISING_SAFE)
            {
                LeGapAdvertisingEnable(FALSE);
                g_app.state = DW2_BLE_STATE_IDLE;
                ps_smart_sleep(1);
            }
            break;

        case DW2_MSG_TIME_SYNC_TIMEOUT:
            if (g_app.state == DW2_BLE_STATE_CONNECTABLE_SYNC)
            {
                DW2_LOG("[DW2] 5min Time-Sync window closed -> entering normal low-power sleep\r\n");
                LeGapAdvertisingEnable(FALSE);
                g_app.state = DW2_BLE_STATE_IDLE;
                ps_smart_sleep(1);
                LeSendMessageAfter(&g_app.task, DW2_MSG_HEARTBEAT, NULL, DW2_HEARTBEAT_PERIOD_MS);
            }
            break;

        case DW2_MSG_DOOR_DEBOUNCE:
        {
            E_GpioLevel_t lvl = Hal_Vic_GpioInput(DW2_MAGNETIC_IO_PORT);
            uint8_t is_open = (lvl != DW2_DOOR_CLOSED_LEVEL) ? 1U : 0U;
            Dw2_ArmGpioInt(DW2_MAGNETIC_IO_PORT, lvl, Dw2_DoorGpioCb);

            if (is_open != g_app.door_open)
            {
                g_app.door_open = is_open;
                g_app.door_ajar_active = 0U;

                /* Magnetic Morse / Secret Knock detector: 5 rapid transitions within < 1200ms */
                uint32_t current_tick = Hal_Tick_Diff(0);
                uint32_t gap_ticks = Hal_Tick_Diff(g_app.last_toggle_tick);
                uint32_t gap_ms = (uint32_t)((uint64_t)gap_ticks * 1000ULL / SystemCoreClockGet());
                g_app.last_toggle_tick = current_tick;

                if (gap_ms <= DW2_MAGNET_KNOCK_MAX_GAP_MS)
                {
                    g_app.rapid_toggle_count++;
                }
                else
                {
                    g_app.rapid_toggle_count = 1U;
                }

                if (g_app.rapid_toggle_count >= DW2_MAGNET_KNOCK_THRESHOLD)
                {
                    g_app.rapid_toggle_count = 0U;
                    DW2_LOG("[DW2] SECRET MAGNETIC KNOCK! Entering OTA & Config Mode!\r\n");

                    /* Visual confirmation: 5 rapid blinks */
                    Dw2_LedBlink(5, DW2_LED_BLINK_SHORT_MS, 50);

                    /* Open 3-minute Connectable OTA & Config window */
                    g_app.state = DW2_BLE_STATE_CONNECTABLE_SYNC;
                    Dw2_SetAdvParams(0x00A0, 0x00A0, LE_HCI_ADV_TYPE_ADV_IND);
                    Dw2_BuildAdvPacket(DW2_FLAG_BUTTON_TEST, Dw2_ReadBatteryMv(), Dw2_GetCurrentEpoch(), 0U);
                    LeGapSetAdvData(sizeof(g_adv_data), g_adv_data);
                    LeSendMessageAfter(&g_app.task, DW2_MSG_TIME_SYNC_TIMEOUT, NULL, 180000U);
                    return;
                }

                uint32_t now_epoch = Dw2_GetCurrentEpoch();
                if (is_open)
                {
                    g_app.door_opened_epoch = now_epoch;
                    g_app.last_open_duration = 0U;
                }
                else
                {
                    if (g_app.door_opened_epoch > 0 && now_epoch >= g_app.door_opened_epoch)
                    {
                        g_app.last_open_duration = (uint16_t)(now_epoch - g_app.door_opened_epoch);
                    }
                    else
                    {
                        g_app.last_open_duration = 0U;
                    }
                    g_app.door_opened_epoch = 0U;
                }

                /* Power-saving LED: only blink on door events during the first 24h */
                if (Dw2_IsLedFeedbackAllowed())
                {
                    if (is_open)
                    {
                        Dw2_LedBlink(1, DW2_LED_BLINK_SHORT_MS, 0);
                    }
                    else
                    {
                        Dw2_LedBlink(2, DW2_LED_BLINK_SHORT_MS, 80);
                    }
                }

                if (is_open)
                {
                    /* Arm Door Ajar timer (default 45s or user-configured) */
                    uint32_t ajar_timeout_ms = (g_app.config.door_ajar_sec > 0) ?
                        ((uint32_t)g_app.config.door_ajar_sec * 1000U) : DW2_DOOR_AJAR_TIMEOUT_MS;
                    LeSendMessageAfter(&g_app.task, DW2_MSG_DOOR_AJAR, NULL, ajar_timeout_ms);
                }

                Dw2_TriggerAdaptiveBurst(0U);
            }
            break;
        }

        case DW2_MSG_DOOR_AJAR:
        {
            if (g_app.door_open)
            {
                DW2_LOG("[DW2] WARNING: Door Ajar > 45s\r\n");
                /* Door ajar warning always blinks (security priority) */
                Dw2_LedBlink(2, DW2_LED_BLINK_SHORT_MS, 60);
                Dw2_TriggerAdaptiveBurst(DW2_FLAG_DOOR_AJAR);
                /* Repeat alert every 10s until door is closed */
                LeSendMessageAfter(&g_app.task, DW2_MSG_DOOR_AJAR, NULL, DW2_DOOR_AJAR_REPEAT_MS);
            }
            break;
        }

        case DW2_MSG_BUTTON_DEBOUNCE:
        {
            E_GpioLevel_t lvl = Hal_Vic_GpioInput(DW2_BUTTON_IO_PORT);
            Dw2_ArmGpioInt(DW2_BUTTON_IO_PORT, lvl, Dw2_ButtonGpioCb);

            if (lvl == DW2_BUTTON_PRESSED_LEVEL)
            {
                /* Button press: 1 long blink + test/tamper burst */
                Dw2_LedBlink(1, DW2_LED_BLINK_LONG_MS, 0);
                Dw2_TriggerAdaptiveBurst(DW2_FLAG_BUTTON_TEST);

                /* Re-open 3-minute Connectable Time-Sync & Config window */
                g_app.state = DW2_BLE_STATE_CONNECTABLE_SYNC;
                Dw2_SetAdvParams(0x00A0, 0x00A0, LE_HCI_ADV_TYPE_ADV_IND);
                Dw2_BuildAdvPacket(DW2_FLAG_BUTTON_TEST, Dw2_ReadBatteryMv(), Dw2_GetCurrentEpoch(), 0U);
                LeGapSetAdvData(sizeof(g_adv_data), g_adv_data);
                LeSendMessageAfter(&g_app.task, DW2_MSG_TIME_SYNC_TIMEOUT, NULL, 180000U);
            }
            break;
        }

        case DW2_MSG_HEARTBEAT:
            LeSendMessageAfter(&g_app.task, DW2_MSG_HEARTBEAT, NULL, DW2_HEARTBEAT_PERIOD_MS);
            Dw2_TriggerAdaptiveBurst(DW2_FLAG_HEARTBEAT);
            break;

        default:
            break;
    }
}

static void Dw2_TaskHandler(TASK task, MESSAGEID id, MESSAGE message)
{
    if ((id >= LE_CM_MSG_BASE) && (id < LE_CM_MSG_TOP))
    {
        Dw2_CmMsgHandler(task, id, message);
    }
    else if ((id >= DW2_MSG_INITIALIZING) && (id < DW2_MSG_TOP))
    {
        Dw2_AppMsgHandler(task, id, message);
    }
}

void Dw2App_Init(void)
{
    memset(&g_app, 0, sizeof(g_app));
    g_app.state = DW2_BLE_STATE_INIT;
    g_app.base_epoch_sec = 0;
    g_app.boot_tick = Hal_Tick_Diff(0);
    g_app.base_tick = g_app.boot_tick;
    g_app.flash_log_offset = 0;

    Dw2_LoadConfig();

    LeHostCreateTask(&g_app.task, Dw2_TaskHandler);
    LeSendMessage(&g_app.task, DW2_MSG_INITIALIZING, NULL);
}
