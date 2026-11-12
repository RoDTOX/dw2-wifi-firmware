/**
 * @file main.cpp
 * @brief Example ESP32 WROOM firmware integrating DW2 BLE door sensor into smart-home-heating.
 */

#include <Arduino.h>
#include "dw2_ble_receiver.hpp"

/* MAC address filter: matches your flashed DW2 sensor */
static const char *TARGET_DW2_MAC = "88:4a:18:ea:cd:a0";

static void onDoorEvent(const Dw2SensorEvent &ev)
{
    /* Optional: filter by MAC if multiple sensors are deployed */
    if (strcasecmp(ev.address.toString().c_str(), TARGET_DW2_MAC) != 0)
    {
        return;
    }

    Serial.printf("[DW2] MAC: %s | RSSI: %d dBm | Event #%u | Vbat: %u mV\n",
                  ev.address.toString().c_str(),
                  ev.rssi,
                  ev.event_counter,
                  ev.battery_mv);

    if (ev.is_heartbeat)
    {
        Serial.println("  -> Periodic Heartbeat (sensor is alive)");
    }
    else if (ev.is_open)
    {
        Serial.println("  -> [ACTION] DOOR/WINDOW OPENED: Turning OFF heating in this zone!");
        /* smart-home-heating integration:
         * zoneManager.setWindowOpen(ZONE_LIVING_ROOM, true);
         */
    }
    else
    {
        Serial.println("  -> [ACTION] DOOR/WINDOW CLOSED: Resuming normal heating schedule.");
        /* smart-home-heating integration:
         * zoneManager.setWindowOpen(ZONE_LIVING_ROOM, false);
         */
    }
}

static Dw2BleReceiver receiver(onDoorEvent);

void setup()
{
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== ESP32 Smart-Heating DW2 BLE Receiver ===");

    /* Start passive scanning; NimBLE coexists with Wi-Fi / MQTT */
    receiver.begin();
    Serial.println("Listening for DW2 BLE advertisements...");
}

void loop()
{
    /* FreeRTOS loop: WiFi / MQTT / PID heating logic runs here without blocking */
    delay(1000);
}
