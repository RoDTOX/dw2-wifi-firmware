/**
 * @file dw2_app.c
 * @brief BLE-only Sonoff DW2-Wi-Fi application:
 * 1. Monotonic Event Counter (32-bit)
 * 2. Real-time Door Open Duration tracker (`open_duration_sec`) emitted on Close
 * 3. Door Ajar alert if door left open > 45s (repeated every 15s)
 * 4. Distinct LED feedback (1 blink Open, 2 blinks Close, 3 Boot, 5 Secret Knock)
 * 5. Secret Magnetic Knock gesture (5 rapid flicks < 1200ms)
 * 6. RAM Blackbox Event Log (64 entries)
 * 7. BTHome V2 Door object (0x1A) + rich manufacturer telemetry in every packet
 * 8. Ultra-low-power smart sleep (~30 uA, ~2-year battery life)
 */

#include <stdio.h>
#include <string.h>

#include "cmsis_os.h"
#include "hal_auxadc.h"
#include "hal_pin.h"
#include "hal_pin_def.h"
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
    uint16_t        open_duration_sec;
    uint16_t        last_open_duration;
    uint8_t         door_open;
    uint8_t         pending_burst;
    uint8_t         pending_flags;
    uint8_t         knock_count;
} dw2_app_ctx_t;

static dw2_app_ctx_t g_app;

/* RAM Blackbox Circular Ring Buffer */
static dw2_log_entry_t g_ram_log[DW2_RAM_LOG_CAPACITY];
static uint16_t g_ram_log_head = 0;
static uint16_t g_ram_log_count = 0;

/* Advertising payload: Flags (3B) + Manufacturer Specific Data (2B header + 20B payload) = 25B */
static uint8_t g_adv_data[3 + 2 + sizeof(dw2_adv_payload_t)];

static const uint8_t g_scan_rsp_data[] =
{
    0x04,
    GAP_ADTYPE_LOCAL_NAME_COMPLETE,
    'D', 'W', '2'
};

/**
 * @brief Arm a level-triggered GPIO interrupt to fire when pin leaves current_level.
 */
static void Dw2_ArmGpioInt(E_GpioIdx_t pin, E_GpioLevel_t current_level, T_Gpio_CallBack cb)
{
    Hal_Vic_GpioCallBackFuncSet(pin, cb);
    Hal_Vic_GpioDirection(pin, GPIO_INPUT);
    Hal_Vic_GpioIntTypeSel(pin, INT_TYPE_LEVEL);
    Hal_Vic_GpioIntInv(pin, (current_level == GPIO_LEVEL_LOW) ? 0U : 1U);
    Hal_Vic_GpioIntMask(pin, 0U);
    Hal_Vic_GpioIntEn(pin, 1U);
}

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
        if (i + 1 < count && off_ms > 0)
        {
            osDelay(off_ms);
        }
    }
}

/**
 * @brief Measure battery voltage in millivolts via AuxADC on DW2_BATTERY_IO_PORT.
 */
static uint16_t Dw2_ReadBatteryMv(void)
{
    float vbat = 0.0f;

    g_ulHalAux_AverageCount = DW2_VBAT_AVG_COUNT;
    Hal_Pin_ConfigSet(DW2_BATTERY_IO_PORT, PIN_TYPE_GPIO_OUTPUT_HIGH, PIN_DRIVING_FLOAT);
    osDelay(3);
    Hal_Aux_IoVoltageGet(DW2_BATTERY_IO_PORT, &vbat);
    Hal_Pin_ConfigSet(DW2_BATTERY_IO_PORT, PIN_TYPE_NONE, PIN_DRIVING_FLOAT);

    if (vbat < 0.0f)
    {
        return 0U;
    }
    return (uint16_t)(vbat * 1000.0f + 0.5f);
}

/**
 * @brief Log event safely to RAM circular ring buffer.
 */
static void Dw2_LogEventToRam(uint32_t epoch_sec, uint32_t counter, uint16_t vbat_mv, uint16_t open_duration_sec, uint8_t flags)
{
    dw2_log_entry_t *entry = &g_ram_log[g_ram_log_head];
    entry->epoch_sec         = epoch_sec;
    entry->event_counter     = counter;
    entry->battery_mv        = vbat_mv;
    entry->open_duration_sec = open_duration_sec;
    entry->flags             = flags;
    entry->reserved[0]       = 0;
    entry->reserved[1]       = 0;

    uint8_t *raw = (uint8_t *)entry;
    uint8_t csum = 0;
    for (size_t i = 0; i < sizeof(dw2_log_entry_t) - 1; i++)
    {
        csum ^= raw[i];
    }
    entry->checksum = csum;

    g_ram_log_head = (g_ram_log_head + 1) % DW2_RAM_LOG_CAPACITY;
    if (g_ram_log_count < DW2_RAM_LOG_CAPACITY)
    {
        g_ram_log_count++;
    }
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

static void Dw2_BuildAdvPacket(uint8_t flags, uint16_t battery_mv)
{
    dw2_adv_payload_t payload;

    payload.company_id        = DW2_BLE_COMPANY_ID;
    payload.magic[0]          = DW2_BLE_MAGIC_0;
    payload.magic[1]          = DW2_BLE_MAGIC_1;
    payload.version           = DW2_BLE_PROTO_VERSION;
    payload.flags             = flags;
    payload.event_counter     = ++g_app.event_counter;
    payload.epoch_sec         = g_app.base_epoch_sec;
    payload.battery_mv        = battery_mv;
    payload.open_duration_sec = (flags & DW2_FLAG_DOOR_OPEN) ? 0U : g_app.last_open_duration;
    payload.bthome_id         = 0x1AU;
    payload.bthome_val        = (flags & DW2_FLAG_DOOR_OPEN) ? 1U : 0U;

    g_adv_data[0] = 0x02U;
    g_adv_data[1] = GAP_ADTYPE_FLAGS;
    g_adv_data[2] = GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED;
    g_adv_data[3] = (uint8_t)(1U + sizeof(dw2_adv_payload_t));
    g_adv_data[4] = GAP_ADTYPE_MANUFACTURER_SPECIFIC;
    memcpy(&g_adv_data[5], &payload, sizeof(payload));

    Dw2_LogEventToRam(g_app.base_epoch_sec, g_app.event_counter, battery_mv, payload.open_duration_sec, flags);
}

static void Dw2_SetAdvParams(void)
{
    LE_GAP_ADVERTISING_PARAM_T para;

    para.interval_min  = DW2_BLE_ADV_INTERVAL_MIN;
    para.interval_max  = DW2_BLE_ADV_INTERVAL_MAX;
    para.type          = LE_HCI_ADV_TYPE_ADV_NONCONN_IND;
    para.own_addr_type = LE_HCI_OWN_ADDR_PUBLIC;
    para.peer_addr_type = LE_HCI_ADV_PEER_ADDR_PUBLIC;
    memset(para.peer_addr, 0, sizeof(para.peer_addr));
    para.channel_map   = 0x07U;
    para.filter_policy = LE_HCI_ADV_FILT_NONE;

    LeGapSetAdvParameter(&para);
}

static void Dw2_TriggerBurst(uint8_t extra_flags)
{
    uint8_t flags = (g_app.door_open ? DW2_FLAG_DOOR_OPEN : 0U) | extra_flags;

    if (g_app.state == DW2_BLE_STATE_ADVERTISING)
    {
        g_app.pending_burst = 1U;
        g_app.pending_flags = flags;
        LeGapAdvertisingEnable(FALSE);
        return;
    }

    uint16_t vbat_mv = Dw2_ReadBatteryMv();
    if (vbat_mv < DW2_VBAT_LOW_THRESHOLD_MV && vbat_mv > 0)
    {
        flags |= DW2_FLAG_LOW_BATTERY;
    }

    Dw2_BuildAdvPacket(flags, vbat_mv);
    DW2_LOG("[DW2] Burst: open=%u flags=0x%02X cnt=%u dur=%us vbat=%umV\r\n",
            g_app.door_open, flags, g_app.event_counter, g_app.last_open_duration, vbat_mv);

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
            Dw2_SetAdvParams();
            g_app.state = DW2_BLE_STATE_IDLE;
            break;
        }

        case LE_CM_MSG_SET_ADVERTISING_PARAMS_CFM:
        {
            E_GpioLevel_t door_lvl = Hal_Vic_GpioInput(DW2_MAGNETIC_IO_PORT);
            E_GpioLevel_t btn_lvl  = Hal_Vic_GpioInput(DW2_BUTTON_IO_PORT);

            g_app.door_open = (door_lvl != DW2_DOOR_CLOSED_LEVEL) ? 1U : 0U;
            Dw2_ArmGpioInt(DW2_MAGNETIC_IO_PORT, door_lvl, Dw2_DoorGpioCb);
            Dw2_ArmGpioInt(DW2_BUTTON_IO_PORT, btn_lvl, Dw2_ButtonGpioCb);

            ps_smart_sleep(DW2_SMART_SLEEP_EN);
            LeSendMessageAfter(&g_app.task, DW2_MSG_HEARTBEAT, NULL, DW2_HEARTBEAT_PERIOD_MS);

            Dw2_LedBlink(3, DW2_LED_BLINK_SHORT_MS, 100);
            Dw2_TriggerBurst(DW2_FLAG_HEARTBEAT);
            break;
        }

        case LE_CM_MSG_SET_ADVERTISING_DATA_CFM:
        {
            LE_ERR_STATE rc = LeGapAdvertisingEnable(TRUE);
            if (rc != SYS_ERR_SUCCESS)
            {
                DW2_LOG("[DW2] AdvEnable(TRUE) err=0x%X\r\n", rc);
            }
            break;
        }

        case LE_CM_MSG_ENTER_ADVERTISING_CFM:
        {
            LE_CM_MSG_ENTER_ADVERTISING_CFM_T *cfm = (LE_CM_MSG_ENTER_ADVERTISING_CFM_T *)message;
            if (cfm->status == SYS_ERR_SUCCESS)
            {
                g_app.state = DW2_BLE_STATE_ADVERTISING;
                LeSendMessageAfter(&g_app.task, DW2_MSG_STOP_BURST, NULL, DW2_BLE_BURST_DURATION_MS);
            }
            break;
        }

        case LE_CM_MSG_EXIT_ADVERTISING_CFM:
        {
            g_app.state = DW2_BLE_STATE_IDLE;
            if (g_app.pending_burst)
            {
                uint8_t flags = g_app.pending_flags;
                uint16_t vbat_mv = Dw2_ReadBatteryMv();
                g_app.pending_burst = 0U;
                Dw2_BuildAdvPacket(flags, vbat_mv);
                LeGapSetAdvData(sizeof(g_adv_data), g_adv_data);
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

        case DW2_MSG_STOP_BURST:
            if (g_app.state == DW2_BLE_STATE_ADVERTISING)
            {
                LeGapAdvertisingEnable(FALSE);
            }
            break;

        case DW2_MSG_DOOR_TIMER:
        {
            if (g_app.door_open)
            {
                g_app.open_duration_sec++;
                if (g_app.open_duration_sec == DW2_DOOR_AJAR_TIMEOUT_SEC)
                {
                    Dw2_LedBlink(2, 80, 80);
                    Dw2_TriggerBurst(DW2_FLAG_DOOR_AJAR);
                }
                else if (g_app.open_duration_sec > DW2_DOOR_AJAR_TIMEOUT_SEC &&
                        ((g_app.open_duration_sec - DW2_DOOR_AJAR_TIMEOUT_SEC) % DW2_DOOR_AJAR_REPEAT_SEC == 0))
                {
                    Dw2_TriggerBurst(DW2_FLAG_DOOR_AJAR);
                }
                LeSendMessageAfter(&g_app.task, DW2_MSG_DOOR_TIMER, NULL, 1000U);
            }
            break;
        }

        case DW2_MSG_KNOCK_TIMEOUT:
            g_app.knock_count = 0;
            break;

        case DW2_MSG_DOOR_DEBOUNCE:
        {
            E_GpioLevel_t lvl = Hal_Vic_GpioInput(DW2_MAGNETIC_IO_PORT);
            uint8_t is_open = (lvl != DW2_DOOR_CLOSED_LEVEL) ? 1U : 0U;

            /* Secret Magnetic Knock gesture detection */
            g_app.knock_count++;
            LeSendMessageAfter(&g_app.task, DW2_MSG_KNOCK_TIMEOUT, NULL, DW2_MAGNET_KNOCK_MAX_GAP_MS);
            if (g_app.knock_count >= DW2_MAGNET_KNOCK_THRESHOLD)
            {
                g_app.knock_count = 0;
                Dw2_LedBlink(5, 40, 40);
                DW2_LOG("[DW2] Secret Magnetic Knock detected!\r\n");
            }

            Dw2_ArmGpioInt(DW2_MAGNETIC_IO_PORT, lvl, Dw2_DoorGpioCb);

            if (is_open != g_app.door_open)
            {
                g_app.door_open = is_open;
                if (is_open)
                {
                    /* Door opened: 1 short blink */
                    g_app.open_duration_sec = 0;
                    Dw2_LedBlink(1, DW2_LED_BLINK_SHORT_MS, 0);
                    LeSendMessageAfter(&g_app.task, DW2_MSG_DOOR_TIMER, NULL, 1000U);
                    Dw2_TriggerBurst(0U);
                }
                else
                {
                    /* Door closed: 2 short blinks */
                    g_app.last_open_duration = g_app.open_duration_sec;
                    Dw2_LedBlink(2, DW2_LED_BLINK_SHORT_MS, 60);
                    Dw2_TriggerBurst(0U);
                }
            }
            break;
        }

        case DW2_MSG_BUTTON_DEBOUNCE:
        {
            E_GpioLevel_t lvl = Hal_Vic_GpioInput(DW2_BUTTON_IO_PORT);
            Dw2_ArmGpioInt(DW2_BUTTON_IO_PORT, lvl, Dw2_ButtonGpioCb);

            if (lvl == DW2_BUTTON_PRESSED_LEVEL)
            {
                Dw2_LedBlink(2, DW2_LED_BLINK_SHORT_MS, 60);
                Dw2_TriggerBurst(DW2_FLAG_BUTTON_TEST);
            }
            break;
        }

        case DW2_MSG_HEARTBEAT:
            LeSendMessageAfter(&g_app.task, DW2_MSG_HEARTBEAT, NULL, DW2_HEARTBEAT_PERIOD_MS);
            Dw2_TriggerBurst(DW2_FLAG_HEARTBEAT);
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

    LeHostCreateTask(&g_app.task, Dw2_TaskHandler);
    LeSendMessage(&g_app.task, DW2_MSG_INITIALIZING, NULL);
}
