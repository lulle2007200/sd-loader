.PHONY: all

LOADER_LOAD_ADDR = 0x40010000

OUT_DIR = output
BUILD_DIR = build

LOADER_DIR = loader
SDLOADER_DIR = sdloader

PAYLOAD_NAME = sdloader

BCT_SIG = bct_sig
BCT_BL_ENTRY = bct_bl_entry

BCT_HEADERS = $(addprefix $(OUT_DIR)/, \
	$(BCT_SIG).h $(BCT_BL_ENTRY).h)

BCT_BINS = $(patsubst $(OUT_DIR)/%.h, $(BUILD_DIR)/%.bin, $(BCT_HEADERS))

LOADER = "$(LOADER_DIR)/output/$(LOADER_DIR).bin"
SDLOADER = "$(SDLOADER_DIR)/output/$(SDLOADER_DIR).bin"

PAYLOAD1 = "$(OUT_DIR)/payload.bin"

BIN2HDR_DIR = tools/bin2header
BMP2HDR_DIR = tools/bmp2header
TOOLS = $(BMP2HDR_DIR) $(BIN2HDR_DIR)

BIN2HDR = $(BIN2HDR_DIR)/output/bin2header.exe

.PHONY: all tools $(TOOLS) bctbin

all: $(OUT_DIR)/$(PAYLOAD_NAME).bin $(OUT_DIR)/$(PAYLOAD_NAME).enc $(OUT_DIR)/$(PAYLOAD_NAME).h $(BCT_HEADERS)

tools: $(TOOLS)

clean: $(TOOLS) $(LOADER) $(SDLOADER)
	rm -rf $(OUT_DIR)

$(TOOLS):
	@echo building $@
	@$(MAKE) --no-print-directory -C $@ $(MAKECMDGOALS)

$(OUT_DIR)/$(PAYLOAD_NAME).h: $(OUT_DIR)/$(PAYLOAD_NAME).enc $(OUT_DIR) $(TOOLS)
	@echo building $@
	@echo $(BIN2HDR) $< $@

	@$(BIN2HDR) $< $@

$(OUT_DIR)/$(PAYLOAD_NAME).enc: $(OUT_DIR) $(SDLOADER) $(LOADER) $(TOOLS)
	@echo building $@
	@python check.py make --version 9 --loader $(LOADER) --load_addr $(LOADER_LOAD_ADDR) $(SDLOADER) $@

$(OUT_DIR)/$(PAYLOAD_NAME).bin : | $(OUT_DIR) $(SDLOADER)
	@echo building $@
	@cp $(SDLOADER) $@

$(LOADER):
	@$(MAKE) --no-print-directory -C $(LOADER_DIR) $(MAKECMDGOALS) LOADER_LOAD_ADDR=$(LOADER_LOAD_ADDR)

$(SDLOADER): $(TOOLS)
	@$(MAKE) --no-print-directory -C $(SDLOADER_DIR) $(MAKECMDGOALS)


$(BCT_HEADERS): $(OUT_DIR)/%.h: $(BUILD_DIR)/%.bin
	@echo Building $@ ...
	@$(BIN2HDR) $< $@

$(BCT_BINS) : bctbin

bctbin : $(OUT_DIR)/$(PAYLOAD_NAME).enc $(LOADER) $(BUILD_DIR)
	@echo building $@
	@python check.py make_erista_bct  --sig_out_path $(BUILD_DIR)/$(BCT_SIG).bin --bl_entry_out_path $(BUILD_DIR)/$(BCT_BL_ENTRY).bin $(OUT_DIR)/$(PAYLOAD_NAME).enc $(LOADER)

$(OUT_DIR):
	@mkdir -p "$(OUT_DIR)"

$(BUILD_DIR):
	@mkdir -p "$(BUILD_DIR)"