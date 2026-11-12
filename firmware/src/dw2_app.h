/**
 * @file dw2_app.h
 * @brief Enhanced DW2 application header with time sync, adaptive burst, door ajar and flash logging.
 */

#ifndef __DW2_APP_H__
#define __DW2_APP_H__

#include "ble.h"
#include "ble_msg.h"

typedef enum
{
    DW2_BLE_STATE_INIT = 0,
    DW2_BLE_STATE_IDLE,
    DW2_BLE_STATE_CONNECTABLE_SYNC,
    DW2_BLE_STATE_ADVERTISING_FAST,
    DW2_BLE_STATE_ADVERTISING_SAFE
} dw2_ble_state_t;

enum
{
    DW2_MSG_INITIALIZING = 1,
    DW2_MSG_BURST_PHASE2,
    DW2_MSG_STOP_BURST,
    DW2_MSG_DOOR_DEBOUNCE,
    DW2_MSG_BUTTON_DEBOUNCE,
    DW2_MSG_DOOR_AJAR,
    DW2_MSG_HEARTBEAT,
    DW2_MSG_TIME_SYNC_TIMEOUT,
    DW2_MSG_TOP
};

void Dw2App_Init(void);

#endif /* __DW2_APP_H__ */
