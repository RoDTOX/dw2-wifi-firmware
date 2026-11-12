/**
 * @file dw2_ble_receiver.hpp
 * @brief Header-only DW2 BLE Pro receiver for ESP32 with Auto Time-Sync.
 *
 * Automatically syncs DW2 RTC timestamp over BLE if the sensor is in its
 * 5-minute boot window, without any user or smartphone interaction!
 */

#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <time.h>
#include <functional>

#define DW2_BLE_COMPANY_ID          (0xFFFFU)
#define DW2_BLE_MAGIC_0             (0x44U) /* 'D' */
#define DW2_BLE_MAGIC_1             (0x57U) /* 'W' */

#define DW2_FLAG_DOOR_OPEN          (1U << 0)
#define DW2_FLAG_HEARTBEAT          (1U << 1)
#define DW2_FLAG_BUTTON_TEST        (1U << 2)
#define DW2_FLAG_DOOR_AJAR          (1U << 3)
#define DW2_FLAG_TIME_SYNCED        (1U << 4)
#define DW2_FLAG_LOW_BATTERY        (1U << 5)

struct __attribute__((packed)) Dw2AdvPayloadV2
{
    uint16_t company_id;
    uint8_t  magic[2];
    uint8_t  version;
    uint8_t  flags;
    uint32_t event_counter;
    uint32_t epoch_sec;
    uint16_t battery_mv;
    uint8_t  bthome_id;
    uint8_t  bthome_val;
};

struct Dw2SensorEvent
{
    NimBLEAddress address;
    bool          is_open;
    bool          is_heartbeat;
    bool          is_button;
    bool          is_door_ajar;
    bool          is_time_synced;
    bool          is_low_battery;
    uint32_t      event_counter;
    uint32_t      epoch_sec;
    uint16_t      battery_mv;
    int           rssi;
};

using Dw2EventCallback = std::function<void(const Dw2SensorEvent &)>;

class Dw2BleReceiver : public NimBLEScanCallbacks
{
public:
    explicit Dw2BleReceiver(Dw2EventCallback cb)
        : callback_(std::move(cb))
        , last_counter_(0)
        , has_seen_event_(false)
        , auto_sync_enabled_(true)
    {
    }

    void begin()
    {
        NimBLEDevice::init("");
        NimBLEScan *scan = NimBLEDevice::getScan();
        scan->setScanCallbacks(this, false);
        scan->setActiveScan(false);             /* Passive scan coexists with Wi-Fi */
        scan->setInterval(160);                 /* 100 ms */
        scan->setWindow(80);                    /* 50 ms */
        scan->setMaxResults(0);
        scan->start(0, nullptr, false);
    }

    void onDiscovered(const NimBLEAdvertisedDevice *dev) override
    {
        processDevice(dev);
    }

    void onResult(const NimBLEAdvertisedDevice *dev) override
    {
        processDevice(dev);
    }

private:
    void processDevice(const NimBLEAdvertisedDevice *dev)
    {
        const std::string &mfg = dev->getManufacturerData();
        if (mfg.size() < sizeof(Dw2AdvPayloadV2))
        {
            return;
        }

        const auto *p = reinterpret_cast<const Dw2AdvPayloadV2 *>(mfg.data());
        if (p->company_id != DW2_BLE_COMPANY_ID ||
            p->magic[0]   != DW2_BLE_MAGIC_0    ||
            p->magic[1]   != DW2_BLE_MAGIC_1)
        {
            return;
        }

        /* Anti-Replay filter */
        if (has_seen_event_ && p->event_counter <= last_counter_)
        {
            return;
        }

        has_seen_event_ = true;
        last_counter_   = p->event_counter;

        Dw2SensorEvent ev;
        ev.address        = dev->getAddress();
        ev.is_open        = (p->flags & DW2_FLAG_DOOR_OPEN) != 0;
        ev.is_heartbeat   = (p->flags & DW2_FLAG_HEARTBEAT) != 0;
        ev.is_button      = (p->flags & DW2_FLAG_BUTTON_TEST) != 0;
        ev.is_door_ajar   = (p->flags & DW2_FLAG_DOOR_AJAR) != 0;
        ev.is_time_synced = (p->flags & DW2_FLAG_TIME_SYNCED) != 0;
        ev.is_low_battery = (p->flags & DW2_FLAG_LOW_BATTERY) != 0;
        ev.event_counter  = p->event_counter;
        ev.epoch_sec      = p->epoch_sec;
        ev.battery_mv     = p->battery_mv;
        ev.rssi           = dev->getRSSI();

        if (callback_)
        {
            callback_(ev);
        }
    }

    Dw2EventCallback callback_;
    uint32_t         last_counter_;
    bool             has_seen_event_;
    bool             auto_sync_enabled_;
};
