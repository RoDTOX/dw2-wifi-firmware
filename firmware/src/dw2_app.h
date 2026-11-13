/**
 * @file dw2_app.h
 * @brief Enhanced DW2 application header with door timer, knock gesture, 128KB flash logging & BLE OTA.
 */

#ifndef __DW2_APP_H__
#define __DW2_APP_H__

#include <stdint.h>
#include "ble.h"
#include "ble_msg.h"

typedef enum
{
    DW2_BLE_STATE_INIT = 0,
    DW2_BLE_STATE_IDLE,
    DW2_BLE_STATE_ADVERTISING,
    DW2_BLE_STATE_CONNECTABLE,
    DW2_BLE_STATE_CONNECTED
} dw2_ble_state_t;

enum
{
    DW2_MSG_INITIALIZING = 1,
    DW2_MSG_STOP_BURST,
    DW2_MSG_DOOR_DEBOUNCE,
    DW2_MSG_BUTTON_DEBOUNCE,
    DW2_MSG_DOOR_TIMER,
    DW2_MSG_KNOCK_TIMEOUT,
    DW2_MSG_HEARTBEAT,
    DW2_MSG_CONNECTABLE_TIMEOUT,
    DW2_MSG_BUTTON_CUE_REBOOT,
    DW2_MSG_BUTTON_CUE_FACTORY,
    DW2_MSG_TOP
};

void Dw2App_Init(void);
void Dw2_SetEpochSec(uint32_t epoch);

#endif /* __DW2_APP_H__ */
