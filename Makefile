# Aidan du Bruyn (25040104)
# Connor Human (22752154)
# Ishmael Ngwasheng (24642542)
# Jay Macaskill (25198387)
# Keval Govind (24904407)
# Ruan van Tonder (25284607)
# Thabo Ngwabi (24742512)

# COS 214 (Software Modelling) TIVIDY
# Last Modified: 22 October 2026

# Makefile (from interwebs)

CXX      := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -g -Iinclude -MMD -MP

BUILD_DIR := build
TARGET    := tividy
LIB       := $(BUILD_DIR)/libtividy.a

# --- Source discovery ---
ENGINE_SOURCES := $(shell find src -name '*.cpp' 2>/dev/null)
APP_SOURCES    := $(shell find app -name '*.cpp' 2>/dev/null)
TEST_SOURCES   := $(shell find tests -name '*.cpp' 2>/dev/null)

ENGINE_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(ENGINE_SOURCES))
APP_OBJECTS    := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(APP_SOURCES))
TEST_OBJECTS   := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(TEST_SOURCES))

TEST_BINARIES  := $(patsubst %.cpp,$(BUILD_DIR)/%,$(TEST_SOURCES))

# --- GoogleTest ---
GTEST_LIBS := -lgtest -lgtest_main -pthread

.PHONY: all clean test run doxygen help

all: $(TARGET)

# --- Engine static library ---
$(LIB): $(ENGINE_OBJECTS)
	@mkdir -p $(dir $@)
	ar rcs $@ $^

# --- Main executable ---
ifneq ($(strip $(APP_SOURCES)),)
$(TARGET): $(APP_OBJECTS) $(LIB)
	$(CXX) $(CXXFLAGS) -o $@ $(APP_OBJECTS) $(LIB)
else
$(TARGET):
	@echo "No sources in app/ — add app/main.cpp to build $(TARGET)."
endif

# --- Generic compile rule ---
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# --- Test binaries ---
$(BUILD_DIR)/tests/%: $(BUILD_DIR)/tests/%.o $(LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIB) $(GTEST_LIBS)

test: $(TEST_BINARIES)
	@if [ -z "$(TEST_BINARIES)" ]; then \
		echo "No tests found in tests/."; \
	else \
		for t in $(TEST_BINARIES); do \
			echo ""; echo "--- $$t ---"; \
			$$t || exit 1; \
		done; \
		echo ""; echo "All tests passed."; \
	fi

run: $(TARGET)
	./$(TARGET)

doxygen:
	@command -v doxygen >/dev/null 2>&1 || { echo "doxygen not installed"; exit 1; }
	doxygen Doxyfile

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

help:
	@echo "Targets:"
	@echo "  make         Build $(TARGET)"
	@echo "  make run     Build and run"
	@echo "  make test    Build and run all tests (requires GoogleTest)"
	@echo "  make doxygen Generate HTML docs from Doxyfile"
	@echo "  make clean   Remove build artifacts"

-include $(ENGINE_OBJECTS:.o=.d) $(APP_OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)