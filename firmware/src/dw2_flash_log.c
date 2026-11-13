/**
 * @file dw2_flash_log.c
 * @brief 128 KB SPI Flash circular ring buffer logger for Sonoff DW2 (8,192 entries).
 *
 * Implements high-durability circular flash logging at 0x000E0000 - 0x000FFFFF.
 * Automatically discovers write head on boot without re-erasing old history.
 */

#include <stdio.h>
#include <string.h>

#include "hal_flash.h"
#include "dw2_config.h"
#include "dw2_protocol.h"
#include "dw2_flash_log.h"

#define DW2_LOG printf

static uint32_t g_write_addr = DW2_FLASH_LOG_BASE_ADDR;
static uint32_t g_total_count = 0;
static uint32_t g_last_counter = 0;
static bool     g_initialized = false;

static uint8_t CalcEntryChecksum(const dw2_log_entry_t *entry)
{
    const uint8_t *raw = (const uint8_t *)entry;
    uint8_t csum = 0;
    for (size_t i = 0; i < sizeof(dw2_log_entry_t) - 1; i++)
    {
        csum ^= raw[i];
    }
    return csum;
}

void Dw2_FlashLog_Init(void)
{
    if (g_initialized)
    {
        return;
    }

    DW2_LOG("[LOG] Initializing 128 KB SPI Flash ring buffer at 0x%08X...\r\n", DW2_FLASH_LOG_BASE_ADDR);

    int active_sector = -1;
    uint32_t sec_headers[DW2_FLASH_LOG_SECTORS];

    /* Step 1: Scan sector headers (32 reads, ~2ms) */
    for (uint32_t s = 0; s < DW2_FLASH_LOG_SECTORS; s++)
    {
        uint32_t sec_addr = DW2_FLASH_LOG_BASE_ADDR + (s * DW2_FLASH_LOG_SECTOR_SIZE);
        sec_headers[s] = 0;
        Hal_Flash_AddrRead(SPI_IDX_0, sec_addr, 0, sizeof(uint32_t), (uint8_t *)&sec_headers[s]);
    }

    /* Check if flash is completely empty (all 0xFFFFFFFF) */
    bool all_empty = true;
    for (uint32_t s = 0; s < DW2_FLASH_LOG_SECTORS; s++)
    {
        if (sec_headers[s] != 0xFFFFFFFFU)
        {
            all_empty = false;
            break;
        }
    }

    if (all_empty)
    {
        /* Fresh flash: Erase first sector to guarantee clean state */
        DW2_LOG("[LOG] Flash buffer empty. Initializing head at sector 0\r\n");
        Hal_Flash_4KSectorAddrErase(SPI_IDX_0, DW2_FLASH_LOG_BASE_ADDR);
        g_write_addr = DW2_FLASH_LOG_BASE_ADDR;
        g_total_count = 0;
        g_initialized = true;
        return;
    }

    /* Find transition from written sector to erased sector */
    for (uint32_t s = 0; s < DW2_FLASH_LOG_SECTORS; s++)
    {
        uint32_t next_s = (s + 1) % DW2_FLASH_LOG_SECTORS;
        if (sec_headers[s] != 0xFFFFFFFFU && sec_headers[next_s] == 0xFFFFFFFFU)
        {
            active_sector = (int)s;
            break;
        }
    }

    /* If all sectors contain data (ring has wrapped), find rollover point via counter */
    if (active_sector < 0)
    {
        uint32_t min_counter = 0xFFFFFFFFU;
        for (uint32_t s = 0; s < DW2_FLASH_LOG_SECTORS; s++)
        {
            uint32_t sec_addr = DW2_FLASH_LOG_BASE_ADDR + (s * DW2_FLASH_LOG_SECTOR_SIZE);
            dw2_log_entry_t sample;
            Hal_Flash_AddrRead(SPI_IDX_0, sec_addr, 0, sizeof(dw2_log_entry_t), (uint8_t *)&sample);
            if (sample.event_counter < min_counter)
            {
                min_counter = sample.event_counter;
                active_sector = (s == 0) ? (DW2_FLASH_LOG_SECTORS - 1) : (s - 1);
            }
        }
    }

    if (active_sector < 0)
    {
        active_sector = 0;
    }

    /* Step 2: Linear scan within candidate sector (up to 256 entries) to find first empty slot */
    uint32_t sec_base = DW2_FLASH_LOG_BASE_ADDR + (active_sector * DW2_FLASH_LOG_SECTOR_SIZE);
    bool slot_found = false;

    for (uint32_t e = 0; e < (DW2_FLASH_LOG_SECTOR_SIZE / DW2_FLASH_LOG_ENTRY_SIZE); e++)
    {
        uint32_t entry_addr = sec_base + (e * DW2_FLASH_LOG_ENTRY_SIZE);
        uint32_t first_word = 0;
        Hal_Flash_AddrRead(SPI_IDX_0, entry_addr, 0, sizeof(uint32_t), (uint8_t *)&first_word);

        if (first_word == 0xFFFFFFFFU)
        {
            g_write_addr = entry_addr;
            slot_found = true;
            break;
        }
        else
        {
            g_total_count++;
        }
    }

    /* If candidate sector was 100% full, head is at start of next sector */
    if (!slot_found)
    {
        uint32_t next_sec = (active_sector + 1) % DW2_FLASH_LOG_SECTORS;
        g_write_addr = DW2_FLASH_LOG_BASE_ADDR + (next_sec * DW2_FLASH_LOG_SECTOR_SIZE);
    }

    /* Recover last event counter from previous entry to ensure monotonic sequence */
    uint32_t last_entry_addr = 0xFFFFFFFFU;
    if (slot_found && g_write_addr > sec_base)
    {
        last_entry_addr = g_write_addr - DW2_FLASH_LOG_ENTRY_SIZE;
    }
    else if (slot_found && g_write_addr == sec_base && active_sector > 0)
    {
        last_entry_addr = DW2_FLASH_LOG_BASE_ADDR + (active_sector * DW2_FLASH_LOG_SECTOR_SIZE) - DW2_FLASH_LOG_ENTRY_SIZE;
    }
    else if (!slot_found)
    {
        last_entry_addr = sec_base + DW2_FLASH_LOG_SECTOR_SIZE - DW2_FLASH_LOG_ENTRY_SIZE;
    }

    if (last_entry_addr != 0xFFFFFFFFU)
    {
        dw2_log_entry_t last_ent;
        Hal_Flash_AddrRead(SPI_IDX_0, last_entry_addr, 0, sizeof(dw2_log_entry_t), (uint8_t *)&last_ent);
        if (CalcEntryChecksum(&last_ent) == last_ent.checksum && last_ent.event_counter != 0xFFFFFFFFU)
        {
            g_last_counter = last_ent.event_counter;
        }
    }

    DW2_LOG("[LOG] Log head located at 0x%08X (Sector %u), recovered entries=%u, last_counter=%u\r\n",
            g_write_addr, (g_write_addr - DW2_FLASH_LOG_BASE_ADDR) / DW2_FLASH_LOG_SECTOR_SIZE, g_total_count, g_last_counter);

    g_initialized = true;
}

bool Dw2_FlashLog_Write(uint32_t epoch_sec, uint32_t counter, uint16_t vbat_mv, uint16_t open_duration_sec, uint8_t flags)
{
    if (!g_initialized)
    {
        Dw2_FlashLog_Init();
    }

    g_last_counter = counter;

    /* If at sector boundary, erase the sector before writing */
    if ((g_write_addr % DW2_FLASH_LOG_SECTOR_SIZE) == 0)
    {
        DW2_LOG("[LOG] Erasing 4KB sector at 0x%08X before write\r\n", g_write_addr);
        Hal_Flash_4KSectorAddrErase(SPI_IDX_0, g_write_addr);
    }

    dw2_log_entry_t entry;
    entry.epoch_sec         = epoch_sec;
    entry.event_counter     = counter;
    entry.battery_mv        = vbat_mv;
    entry.open_duration_sec = open_duration_sec;
    entry.flags             = flags;
    entry.reserved[0]       = 0;
    entry.reserved[1]       = 0;
    entry.checksum          = CalcEntryChecksum(&entry);

    uint32_t rc = Hal_Flash_AddrProgram(SPI_IDX_0, g_write_addr, 0, sizeof(entry), (uint8_t *)&entry);
    if (rc != 0)
    {
        DW2_LOG("[LOG] Flash write failed at 0x%08X (rc=%u)\r\n", g_write_addr, rc);
        return false;
    }

    /* Advance circular write pointer */
    uint32_t next_offset = (g_write_addr - DW2_FLASH_LOG_BASE_ADDR + sizeof(entry)) % DW2_FLASH_LOG_SIZE;
    g_write_addr = DW2_FLASH_LOG_BASE_ADDR + next_offset;
    g_total_count++;

    return true;
}

bool Dw2_FlashLog_Read(uint32_t entry_idx, dw2_log_entry_t *out_entry)
{
    if (!out_entry || entry_idx >= DW2_FLASH_LOG_MAX_ENTRIES)
    {
        return false;
    }

    uint32_t addr = DW2_FLASH_LOG_BASE_ADDR + (entry_idx * DW2_FLASH_LOG_ENTRY_SIZE);
    uint32_t rc = Hal_Flash_AddrRead(SPI_IDX_0, addr, 0, sizeof(dw2_log_entry_t), (uint8_t *)out_entry);
    if (rc != 0)
    {
        return false;
    }

    /* Check if slot is empty */
    if (out_entry->epoch_sec == 0xFFFFFFFFU && out_entry->event_counter == 0xFFFFFFFFU)
    {
        return false;
    }

    /* Validate checksum */
    if (CalcEntryChecksum(out_entry) != out_entry->checksum)
    {
        return false;
    }

    return true;
}

uint32_t Dw2_FlashLog_GetTotalCount(void)
{
    return g_total_count;
}

uint32_t Dw2_FlashLog_GetLastCounter(void)
{
    return g_last_counter;
}

uint32_t Dw2_FlashLog_GetWriteAddr(void)
{
    return g_write_addr;
}
