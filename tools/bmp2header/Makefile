BUILD_DIR = build
OUT_DIR = output
TARGET = bmp2header

CC ?= cl

OBJS = $(addprefix $(BUILD_DIR)/$(TARGET)/, \
    main.obj )

all: | $(OUT_DIR) $(OUT_DIR)/$(TARGET).exe

clean:
	rm -rf $(BUILD_DIR)
	rm -rf $(OUT_DIR)

$(OUT_DIR)/$(TARGET).exe: $(OBJS)
	@MSYS_NO_PATHCONV=1 cl /Fe:$@ $^ &>/dev/null
	@echo Building $@ ...

$(BUILD_DIR)/$(TARGET)/%.obj: %.cpp
	@MSYS_NO_PATHCONV=1 cl /std:c++20 /EHsc /O2 /Fo:$@ /c $< &>/dev/null
	@echo Building $@ ...

$(OBJS): | $(BUILD_DIR)/$(TARGET)

$(BUILD_DIR)/$(TARGET):
	@mkdir -p "$(BUILD_DIR)/$(TARGET)"

$(OUT_DIR):
	@mkdir -p "$(OUT_DIR)"

