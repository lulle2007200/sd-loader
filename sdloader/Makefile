ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_rules


################################################################################

# by default, we want to be as low as possible in iram to have a large continuous buffer above payload
# 0x40000000 - 0x40003000 reserved for BIT/BCT
IPL_LOAD_ADDR ?= 0x40003000
MAX_PAYLOAD_SIZE := 65536

################################################################################

BUILD_DIR = build
OUT_DIR = output

SRC_DIR = .

BDK_DIR = ../bdk
GFX_DIR = gfx
STORAGE_DIR = ./storage

VPATH = $(dir $(STORAGE_DIR)/) $(dir $(GFX_DIR)/) $(dir $(SRC_DIR)/) $(dir $(BDK_DIR)/) $(BDK_DIR)/ $(dir $(wildcard $(BDK_DIR)/*/)) $(dir $(wildcard $(BDK_DIR)/*/*/))

TARGET = sdloader
PAYLOAD_NAME = $(TARGET)

OBJS = $(addprefix $(BUILD_DIR)/$(TARGET)/, \
    main.o irq.o \
	heap.o mc.o bpmp.o clock.o fuse.o se.o hw_init.o gpio.o pinmux.o i2c.o util.o btn.o \
    max7762x.o bq24193.o max77620-rtc.o \
    sdmmc.o sd.o sdmmc_driver.o \
	sprintf.o \
	di.o gfx.o tui.o emmc.o timer.o \
	diskio.o ff.o ffsystem.o ffunicode.o max17050.o bq24193.o \
	usb_gadget_ums.o usb_descriptors.o xusbd.o ums.o modchip.o \
	loader.o modchip.o modchip_toolbox.o files.o )

# startup code must be compiled with lto disabled
OBJS_NO_LTO_C = $(addprefix $(BUILD_DIR)/$(TARGET)/, \
	startup.o loader.o)

OBJS_NO_LTO_S = $(addprefix $(BUILD_DIR)/$(TARGET)/, \
	start.o exception_handlers.o)

GFX_INC = '"../sdloader/$(GFX_DIR)/gfx.h"'
INC_DIR = -I./$(BDK_DIR) -I./$(SRC_DIR) -I./$(GFX_DIR)

CUSTOMDEFINES += -DGFX_INC=$(GFX_INC) -DMAX_PAYLOAD_SIZE=$(MAX_PAYLOAD_SIZE) -DBDK_MC_ENABLE_AHB_REDIRECT # BDK_MC_ENABLE_AHB_REDIRECT for iram access

WARNINGS := -Wno-main-return-type -Wno-main -Wall -Wno-array-bounds -Wno-stringop-overread -Wno-stringop-overflow
ARCH := -march=armv4t -mtune=arm7tdmi -mthumb-interwork -mthumb -Wstack-usage=1536
LTO_FLAGS = -flto
CFLAGS = $(ARCH) -Os -g -gdwarf-4 -nostdlib -ffunction-sections -fdata-sections -fomit-frame-pointer -std=gnu11 $(WARNINGS) $(CUSTOMDEFINES)
LDFLAGS = $(ARCH) $(LTO_FLAGS) -nostartfiles -lgcc -Wl,--nmagic,--gc-sections -Xlinker --defsym=IPL_LOAD_ADDR=$(IPL_LOAD_ADDR)

.PHONY: all

all: $(OUT_DIR)/$(PAYLOAD_NAME).bin
	$(eval PAYLOAD_SIZE = $(shell wc -c < $(OUT_DIR)/$(PAYLOAD_NAME).bin))
	$(eval PAYLOAD_TOTAL_SIZE = $(shell nm $(BUILD_DIR)/$(TARGET)/$(TARGET).elf | awk '/__payload_size/ {print $$1}'))
	$(eval PAYLOAD_TOTAL_SIZE := $(shell printf "%d" 0x$(PAYLOAD_TOTAL_SIZE)))
	@echo Done
	@echo "Payload size is ${PAYLOAD_SIZE}"
	@echo "Payload (incl. bss) size is ${PAYLOAD_TOTAL_SIZE}"
	@echo "Load address is ${IPL_LOAD_ADDR}"
	@if [ ${PAYLOAD_TOTAL_SIZE} -gt ${MAX_PAYLOAD_SIZE} ]; then echo "\033[0;31m ERROR: Payload size is ${PAYLOAD_SIZE} bytes. Maximum allowed is ${MAX_PAYLOAD_SIZE} bytes \033[0m"; fi

$(OUT_DIR)/$(PAYLOAD_NAME).bin: $(BUILD_DIR)/$(TARGET)/$(TARGET).elf | $(OUT_DIR)
	@$(OBJCOPY) -S -O binary $< $@
	@echo Building $@ ...

$(BUILD_DIR)/$(TARGET)/$(TARGET).elf: $(OBJS) $(OBJS_NO_LTO_C) $(OBJS_NO_LTO_S)
	@$(CC) $(LDFLAGS) -T $(SRC_DIR)/link.ld $^ -o $@
	@echo Building $@ ...

#----------------------------------------------

$(BUILD_DIR)/$(TARGET)/%.o: %.c
	@$(CC) $(CFLAGS) $(LTO_FLAGS) $(INC_DIR) -c $< -o $@
	@echo Building $@ ...

$(BUILD_DIR)/$(TARGET)/%.o: %.s
	@$(CC) $(CFLAGS) $(LTO_FLAGS) -c $< -o $@
	@echo Building $@ ...

$(BUILD_DIR)/$(TARGET)/%.o: %.S
	@$(CC) $(CFLAGS) $(LTO_FLAGS) -c $< -o $@
	@echo Building $@ ...

#----------------------------------------------

$(OBJS_NO_LTO_C): $(BUILD_DIR)/$(TARGET)/%.o: %.c
	@$(CC) $(CFLAGS) -fno-lto $(INC_DIR) -c $< -o $@
	@echo Building $@ ...

$(OBJS_NO_LTO_S): $(BUILD_DIR)/$(TARGET)/%.o: %.S
	@$(CC) $(CFLAGS) -fno-lto -c $< -o $@
	@echo Building $@ ...

#----------------------------------------------


$(OBJS): | $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET):
	@mkdir -p "$(BUILD_DIR)/$(TARGET)"

$(OUT_DIR):
	@mkdir -p "$(OUT_DIR)"

clean:
	rm -rf $(OUT_DIR)
	rm -rf $(BUILD_DIR)/$(TARGET)
