/**
 * @file dw2_ble_receiver.hpp
 * @brief Header-only DW2 BLE passive receiver for ESP32 (Arduino / PlatformIO).
 *
 * Uses NimBLE-Arduino for ultra-lightweight passive BLE scanning concurrent
 * with Wi-Fi on ESP32 WROOM. Integrates directly into smart-home-heating.
 */

#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <functional>

#define DW2_BLE_COMPANY_ID          (0xFFFFU)
#define DW2_BLE_MAGIC_0             (0x44U) /* 'D' */
#define DW2_BLE_MAGIC_1             (0x57U) /* 'W' */
#define DW2_BLE_PROTO_VERSION       (0x01U)

#define DW2_FLAG_DOOR_OPEN          (1U << 0)
#define DW2_FLAG_HEARTBEAT          (1U << 1)
#define DW2_FLAG_BUTTON_TEST        (1U << 2)

struct __attribute__((packed)) Dw2AdvPayload
{
    uint16_t company_id;
    uint8_t  magic[2];
    uint8_t  version;
    uint8_t  flags;
    uint16_t event_counter;
    uint16_t battery_mv;
};

struct Dw2SensorEvent
{
    NimBLEAddress address;
    bool          is_open;
    bool          is_heartbeat;
    bool          is_button;
    uint16_t      event_counter;
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
    {
    }

    void begin()
    {
        NimBLEDevice::init("");
        NimBLEScan *scan = NimBLEDevice::getScan();
        scan->setScanCallbacks(this, false);
        scan->setActiveScan(false);             /* Passive scan: lowest power, doesn't interfere with Wi-Fi */
        scan->setInterval(160);                 /* 100 ms */
        scan->setWindow(80);                    /* 50 ms (50% duty cycle, coexists cleanly with Wi-Fi) */
        scan->setMaxResults(0);                 /* Continuous scan */
        scan->start(0, nullptr, false);
    }

    void onDiscovered(const NimBLEAdvertisedDevice *dev) override
    {
        processAdvertisedDevice(dev);
    }

    void onResult(const NimBLEAdvertisedDevice *dev) override
    {
        processAdvertisedDevice(dev);
    }

private:
    void processAdvertisedDevice(const NimBLEAdvertisedDevice *dev)
    {
        const std::string &mfg = dev->getManufacturerData();
        if (mfg.size() < sizeof(Dw2AdvPayload))
        {
            return;
        }

        const auto *p = reinterpret_cast<const Dw2AdvPayload *>(mfg.data());
        if (p->company_id != DW2_BLE_COMPANY_ID ||
            p->magic[0]   != DW2_BLE_MAGIC_0    ||
            p->magic[1]   != DW2_BLE_MAGIC_1    ||
            p->version    != DW2_BLE_PROTO_VERSION)
        {
            return;
        }

        /* Deduplicate packets within the same burst using the monotonic event counter */
        if (has_seen_event_ && p->event_counter == last_counter_)
        {
            return;
        }

        has_seen_event_ = true;
        last_counter_   = p->event_counter;

        Dw2SensorEvent ev;
        ev.address       = dev->getAddress();
        ev.is_open       = (p->flags & DW2_FLAG_DOOR_OPEN) != 0;
        ev.is_heartbeat  = (p->flags & DW2_FLAG_HEARTBEAT) != 0;
        ev.is_button     = (p->flags & DW2_FLAG_BUTTON_TEST) != 0;
        ev.event_counter = p->event_counter;
        ev.battery_mv    = p->battery_mv;
        ev.rssi          = dev->getRSSI();

        if (callback_)
        {
            callback_(ev);
        }
    }

    Dw2EventCallback callback_;
    uint16_t         last_counter_;
    bool             has_seen_event_;
};
