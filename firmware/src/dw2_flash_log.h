/**
 * @file dw2_flash_log.h
 * @brief 128 KB SPI Flash circular ring buffer logger for Sonoff DW2 (8,192 entries).
 */

#ifndef __DW2_FLASH_LOG_H__
#define __DW2_FLASH_LOG_H__

#include <stdint.h>
#include <stdbool.h>
#include "dw2_protocol.h"

void Dw2_FlashLog_Init(void);
bool Dw2_FlashLog_Write(uint32_t epoch_sec, uint32_t counter, uint16_t vbat_mv, uint16_t open_duration_sec, uint8_t flags);
bool Dw2_FlashLog_Read(uint32_t entry_idx, dw2_log_entry_t *out_entry);
bool Dw2_FlashLog_Erase(void);
uint32_t Dw2_FlashLog_GetTotalCount(void);
uint32_t Dw2_FlashLog_GetLastCounter(void);
uint32_t Dw2_FlashLog_GetWriteAddr(void);

#endif /* __DW2_FLASH_LOG_H__ */
