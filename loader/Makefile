ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/base_rules

LOADER_LOAD_ADDR ?= 0x40010000

BUILD_DIR = build
OUT_DIR = output

SRC_DIR = .

BDK_DIR = ../bdk

VPATH = $(dir $(SRC_DIR)/) $(dir $(BDK_DIR)/) $(BDK_DIR)/ $(dir $(wildcard $(BDK_DIR)/*/)) $(dir $(wildcard $(BDK_DIR)/*/*/))

TARGET = loader
PAYLOAD_NAME = $(TARGET)

OBJS = $(addprefix $(BUILD_DIR)/$(TARGET)/, \
    main.o)

INC_DIR = -I./$(BDK_DIR) -I./$(SRC_DIR)

WARNINGS := -Wall -Wno-array-bounds -Wno-stringop-overread -Wno-stringop-overflow
ARCH := -march=armv4t -mtune=arm7tdmi -mthumb -mthumb-interwork -Wstack-usage=1536
CFLAGS = $(ARCH) -Os -g -nostdlib -ffunction-sections -fdata-sections -std=gnu11  $(WARNINGS)
LDFLAGS = $(ARCH) -nostartfiles -lgcc -Wl,--nmagic,--gc-sections -Xlinker --defsym=LOADER_LOAD_ADDR=$(LOADER_LOAD_ADDR)

.PHONY: all

all: $(OUT_DIR)/$(PAYLOAD_NAME).bin
	$(eval PAYLOAD_SIZE = $(shell wc -c < $(OUT_DIR)/$(PAYLOAD_NAME).bin))
	@echo "Payload size is ${PAYLOAD_SIZE}"
	@echo "Load address is ${LOADER_LOAD_ADDR}"

$(OUT_DIR)/$(PAYLOAD_NAME).bin: $(BUILD_DIR)/$(TARGET)/$(TARGET).elf | $(OUT_DIR)
	@$(OBJCOPY) -S -O binary $< $@
	@echo Building $@ ...

$(BUILD_DIR)/$(TARGET)/$(TARGET).elf: $(OBJS)
	@$(CC) $(LDFLAGS) -T $(SRC_DIR)/link.ld $^ -o $@
	@echo Building $@ ...

$(BUILD_DIR)/$(TARGET)/%.o: %.c
	@$(CC) $(CFLAGS) $(INC_DIR) -c $< -o $@
	@echo Building $@ ...

$(BUILD_DIR)/$(TARGET)/%.o: %.s
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo Building $@ ...

$(BUILD_DIR)/$(TARGET)/%.o: %.S
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo Building $@ ...

$(OBJS): | $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET):
	@mkdir -p "$(BUILD_DIR)/$(TARGET)"

$(OUT_DIR):
	@mkdir -p "$(OUT_DIR)"

clean:
	rm -rf $(OUT_DIR)
	rm -rf $(BUILD_DIR)/$(TARGET)
