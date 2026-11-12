/**
 * @file main.cpp
 * @brief ESP32 receiver integration with smart-home-heating and home-security logic.
 */

#include <Arduino.h>
#include <time.h>
#include "dw2_ble_receiver.hpp"

static const char *TARGET_DW2_MAC = "88:4a:18:ea:cd:a0";

static void onDoorEvent(const Dw2SensorEvent &ev)
{
    if (strcasecmp(ev.address.toString().c_str(), TARGET_DW2_MAC) != 0)
    {
        return;
    }

    char time_str[32] = "N/A";
    if (ev.is_time_synced && ev.epoch_sec > 0)
    {
        time_t t = (time_t)ev.epoch_sec;
        struct tm *tm_info = localtime(&t);
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    }

    Serial.printf("\n[DW2 EVENT] #%u | Time: %s | RSSI: %d dBm | Battery: %u mV\n",
                  ev.event_counter,
                  time_str,
                  ev.rssi,
                  ev.battery_mv);

    if (ev.is_low_battery)
    {
        Serial.println("  [ALERT] LOW BATTERY! Replace 2xAAA cells soon.");
    }

    if (ev.is_door_ajar)
    {
        Serial.println("  [SECURITY ALARM] DOOR AJAR! Front door has been left open for > 45 seconds!");
        /* Trigger buzzer / push notification */
    }
    else if (ev.is_heartbeat)
    {
        Serial.println("  [STATUS] Periodic Heartbeat: Sensor is alive and operational.");
    }
    else if (ev.is_button)
    {
        Serial.println("  [TAMPER / TEST] S1 button pressed on sensor.");
    }
    else if (ev.is_open)
    {
        Serial.println("  [HEATING] FRONT DOOR OPENED -> Turning OFF heating in this zone.");
    }
    else
    {
        Serial.println("  [HEATING] FRONT DOOR CLOSED -> Resuming heating schedule.");
    }
}

static Dw2BleReceiver receiver(onDoorEvent);

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== ESP32 Home Security & Smart-Heating Receiver ===");

    receiver.begin();
    Serial.println("Passive BLE scanning active. Listening for DW2 events...");
}

void loop()
{
    delay(1000);
}
