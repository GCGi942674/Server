BUILD_DIR := build

.PHONY: all build rebuild clean

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && make -j

rebuild:
	@rm -rf $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && make -j

clean:
	@rm -rf $(BUILD_DIR)
