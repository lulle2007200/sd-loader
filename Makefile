.PHONY: all

LOADER_LOAD_ADDR = 0x40010000

OUT_DIR = output

PAYLOAD_NAME = sdloader

LOADER_DIR = loader
SDLOADER_DIR = sdloader


LOADER = "$(LOADER_DIR)/output/$(LOADER_DIR).bin"
SDLOADER = "$(SDLOADER_DIR)/output/$(SDLOADER_DIR).bin"

PAYLOAD1 = "$(OUT_DIR)/payload.bin"

all: $(OUT_DIR)/$(PAYLOAD_NAME).bin $(OUT_DIR)/$(PAYLOAD_NAME).enc

clean:
	rm -rf $(OUT_DIR)
	$(MAKE) -C $(LOADER_DIR) clean
	$(MAKE) -C $(SDLOADER_DIR) clean

$(OUT_DIR)/$(PAYLOAD_NAME).enc: $(OUT_DIR) $(SDLOADER) $(LOADER)
	@python check.py make --version 9 --loader $(LOADER) --load_addr $(LOADER_LOAD_ADDR) $(SDLOADER) $@

$(OUT_DIR)/$(PAYLOAD_NAME).bin : | $(OUT_DIR) $(SDLOADER)
	@cp $(SDLOADER) $@
	@cp $(SDLOADER) $(PAYLOAD1)

$(LOADER):
	$(MAKE) -C $(LOADER_DIR) LOADER_LOAD_ADDR=$(LOADER_LOAD_ADDR)

$(SDLOADER):
	$(MAKE) -C $(SDLOADER_DIR)

$(OUT_DIR):
	@mkdir -p "$(OUT_DIR)"