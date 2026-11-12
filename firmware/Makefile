##### DW2-Wi-Fi Custom BLE-Only Firmware (OPL1000 A2) #####

SDK_ROOT = ../third_party/OPL1000A2-SDK/SDK
FOLDER   = Output-GCC
OUTPUT   = dw2_ble

LD_SCRIPT = $(SDK_ROOT)/APS/project/opl1000/link.ld
PATCH_LIB = $(SDK_ROOT)/APS/targets/opl1000/Output-GCC/opl1000_app_m3.a
SYMDEFS   = $(SDK_ROOT)/APS/targets/opl1000/object/symdefs_gcc.o

CROSS_COMPILE = arm-none-eabi-
CC      = $(CROSS_COMPILE)gcc
OBJCOPY = $(CROSS_COMPILE)objcopy
SIZE    = $(CROSS_COMPILE)size

DEFS  = -DGCC
DEFS += -DARMCM3
DEFS += -DOPL1000_APS
DEFS += -D__noSIMULATOR__
DEFS += -D__BLE__
DEFS += -D__LE_HOST_USE_CMD__
DEFS += -D__LWIP_TASK__
DEFS += -D__WPA_SUPPLICANT__
DEFS += -D__noHW_CRYPTO_ENGINE__
DEFS += -D__WIFI_MAC_TASK__
DEFS += -D__NL1000_An__
DEFS += -D__PMP_ENABLE__
DEFS += -D__PMP_REGION__
DEFS += -D__HEAP_REGION__
DEFS += -DnoLWIP_DYNAMIC_DEBUG_ENABLE
DEFS += -D__AT_CMD_TASK__
DEFS += -D__noRTL_SIMULATION__
DEFS += -D_ARMABI_PURE=

CFLAGS  = $(DEFS)
CFLAGS += -mthumb
CFLAGS += -mcpu=cortex-m3
CFLAGS += -mlittle-endian
CFLAGS += -std=c99
CFLAGS += -Os
CFLAGS += -Wall -Wextra
CFLAGS += -x none
CFLAGS += -include sys_common.h
CFLAGS += -nostartfiles
CFLAGS += -no-pie
CFLAGS += -fdata-sections
CFLAGS += -ffunction-sections
CFLAGS += -specs=nosys.specs
CFLAGS += -Xlinker "--just-symbols=$(SYMDEFS)"
CFLAGS += -Wno-implicit-function-declaration
CFLAGS += -Wno-int-conversion

LFLAGS  = -T $(LD_SCRIPT)
LFLAGS += -Wl,--gc-sections
LFLAGS += -Wl,--entry=__Patch_EntryPoint
LFLAGS += -Wl,-Map=$(FOLDER)/$(OUTPUT).map

INCDIR  = -I ./src
INCDIR += -I $(SDK_ROOT)/APS/apps/le_app/cmd_app
INCDIR += -I $(SDK_ROOT)/APS/apps/le_app/mtc_app
INCDIR += -I $(SDK_ROOT)/APS/apps/le_app/pts_app
INCDIR += -I $(SDK_ROOT)/APS/apps/wifi_app
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_auxadc
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_dbg_uart
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_dma
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_i2c
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_patch
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_pwm
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_spi
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_system
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_tick
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_tmr
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_uart
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_vic
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/hal_wdt
INCDIR += -I $(SDK_ROOT)/APS/driver/chip/opl1000/securityipdriver
INCDIR += -I $(SDK_ROOT)/APS/driver/CMSIS/Device/opl1000/Include
INCDIR += -I $(SDK_ROOT)/APS/driver/CMSIS/Include
INCDIR += -I $(SDK_ROOT)/APS/FreeRtos/Source/include
INCDIR += -I $(SDK_ROOT)/APS/FreeRtos/Source/portable/GCC/ARM_CM3
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/at
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/ble_controller_layer/inc
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/cli
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/common/sys_api
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/common/sys_api/rom_if
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/common/sys_ctrl
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/controller_task
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/data_flow
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/diag_task
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/le_stack
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/msg
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/mw_fim
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/mw_ota
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/ps_task
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/wifi_controller_layer
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/wifi_controller_layer/rom_if
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/wifi_mac
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/wifi_mac/common
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/wifi_mac/crypto
INCDIR += -I $(SDK_ROOT)/APS/middleware/netlink/wifi_mac/utils
INCDIR += -I $(SDK_ROOT)/APS/project/common
INCDIR += -I $(SDK_ROOT)/APS/project/opl1000/boot_sequence
INCDIR += -I $(SDK_ROOT)/APS/project/opl1000/include
INCDIR += -I $(SDK_ROOT)/APS/project/opl1000/startup
INCDIR += -I $(SDK_ROOT)/APS_PATCH/apps/le_app/include
INCDIR += -I $(SDK_ROOT)/APS_PATCH/apps/le_app/cmd_app
INCDIR += -I $(SDK_ROOT)/APS_PATCH/apps/le_app/mtc_app
INCDIR += -I $(SDK_ROOT)/APS_PATCH/apps/le_app/pts_app
INCDIR += -I $(SDK_ROOT)/APS_PATCH/apps/wifi_app
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_auxadc
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_dbg_uart
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_dma
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_i2c
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_patch
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_pwm
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_spi
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_system
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_tick
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_tmr
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_uart
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_vic
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/hal_wdt
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/chip/opl1000/securityipdriver
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/CMSIS/Device/opl1000/Include
INCDIR += -I $(SDK_ROOT)/APS_PATCH/driver/CMSIS/Include
INCDIR += -I $(SDK_ROOT)/APS_PATCH/FreeRtos/Source/include
INCDIR += -I $(SDK_ROOT)/APS_PATCH/FreeRtos/Source/portable/Keil/ARM_CM3
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/at
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/ble_controller_layer/inc
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/cli
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/common/sys_api
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/common/sys_api/rom_if
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/common/sys_ctrl
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/controller_task
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/data_flow
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/diag_task
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/le_stack
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/le_stack/patch
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/msg
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/mw_fim
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/mw_ota
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/ps_task
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/wifi_controller_layer
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/wifi_controller_layer/rom_if
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/wifi_mac
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/wifi_mac/common
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/wifi_mac/crypto
INCDIR += -I $(SDK_ROOT)/APS_PATCH/middleware/netlink/wifi_mac/utils
INCDIR += -I $(SDK_ROOT)/APS_PATCH/project/common
INCDIR += -I $(SDK_ROOT)/APS_PATCH/project/opl1000/boot_sequence
INCDIR += -I $(SDK_ROOT)/APS_PATCH/project/opl1000/include
INCDIR += -I $(SDK_ROOT)/APS_PATCH/project/opl1000/startup
INCDIR += -I $(SDK_ROOT)/APS_PATCH/project/opl1000/lib

M_SRC  = ./src/main_patch.c
M_SRC += ./src/dw2_app.c
M_SRC += ./src/dw2_flash_log.c
M_SRC += ./src/dw2_ota.c

.PHONY: all clean

all:
	@$(shell if not exist "$(FOLDER)" mkdir "$(FOLDER)")
	$(file >$(FOLDER)/args.rsp,$(LFLAGS) $(CFLAGS) $(INCDIR) $(M_SRC) $(PATCH_LIB))
	$(CC) -o $(FOLDER)/$(OUTPUT).elf @$(FOLDER)/args.rsp
	$(OBJCOPY) -O binary $(FOLDER)/$(OUTPUT).elf $(FOLDER)/$(OUTPUT).bin
	$(SIZE) $(FOLDER)/$(OUTPUT).elf

clean:
	@if exist "$(FOLDER)" rmdir /s /q "$(FOLDER)"
