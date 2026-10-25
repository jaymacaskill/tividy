# Aidan du Bruyn (25040104)
# Connor Human (22752154)
# Ishmael Ngwasheng (24642542)
# Jay Macaskill (25198387)
# Keval Govind (24904407)
# Ruan van Tonder (25284607)
# Thabo Ngwabi (24742512)

# COS 214 (Software Modelling) TIVIDY
# Last Modified: 12 October 2026

# Makefile (from interwebs)


CXX := g++
CXXFLAGS := -std=c++11 -Wall -Wextra -Iinclude -MMD -MP
SRC_DIR := src
BUILD_DIR := build
TARGET := tividy

# Finds every .cpp under src/, including subfolders
SOURCES := $(shell find $(SRC_DIR) -name '*.cpp')
OBJECTS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SOURCES))

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(OBJECTS:.o=.d)