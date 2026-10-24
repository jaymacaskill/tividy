# Aidan du Bruyn (25040104)
# Connor Human (22752154)
# Ishmael Ngwasheng (24642542)
# Jay Macaskill (25198387)
# Keval Govind (24904407)
# Ruan van Tonder (25284607)
# Thabo Ngwabi (24742512)

# COS 214 (Software Modelling) TIVIDY
# Last Modified: 24 October 2026

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

GTEST_LIBS := -lgtest -lgtest_main -pthread

# --- Valgrind ---
VALGRIND       := valgrind
VALGRIND_FLAGS := --leak-check=full \
                  --show-leak-kinds=definite,indirect \
                  --errors-for-leak-kinds=definite,indirect \
                  --track-origins=yes \
                  --error-exitcode=1 \
                  --num-callers=30

# --- Docker ---
DOCKER_IMAGE := tividy-dev
DOCKER_RUN   := docker run --rm -v "$(CURDIR)":/app -u "$$(id -u):$$(id -g)" $(DOCKER_IMAGE)

.PHONY: all clean test run doxygen help \
        docker-build docker-make docker-test docker-run docker-clean

all: $(TARGET)

$(LIB): $(ENGINE_OBJECTS)
	@mkdir -p $(dir $@)
	ar rcs $@ $^

ifneq ($(strip $(APP_SOURCES)),)
$(TARGET): $(APP_OBJECTS) $(LIB)
	$(CXX) $(CXXFLAGS) -o $@ $(APP_OBJECTS) $(LIB)
else
$(TARGET):
	@echo "No sources in app/ — add app/main.cpp to build $(TARGET)."
endif

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/%: $(BUILD_DIR)/tests/%.o $(LIB)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LIB) $(GTEST_LIBS)

test: $(TEST_BINARIES)
	@if [ -z "$(TEST_BINARIES)" ]; then \
		echo "No tests found in tests/."; \
	else \
		failed=0; passed=0; \
		for t in $(TEST_BINARIES); do \
			echo ""; echo "--- $$t ---"; \
			if $$t; then passed=$$((passed + 1)); \
			else failed=$$((failed + 1)); fi; \
		done; \
		echo ""; \
		if [ $$failed -eq 0 ]; then \
			printf '\033[32mPASS\033[0m: %d test file(s) passed\n' $$passed; \
		else \
			printf '\033[31mFAIL\033[0m: %d passed, %d failed\n' $$passed $$failed; \
			exit 1; \
		fi; \
	fi

valgrind: $(TEST_BINARIES)
	@if [ -z "$(TEST_BINARIES)" ]; then \
		echo "No tests to check."; \
	else \
		failed=0; passed=0; \
		for t in $(TEST_BINARIES); do \
			echo ""; echo "--- valgrind $$t ---"; \
			if $(VALGRIND) $(VALGRIND_FLAGS) $$t; then passed=$$((passed + 1)); \
			else failed=$$((failed + 1)); fi; \
		done; \
		echo ""; \
		if [ $$failed -eq 0 ]; then \
			printf '\033[32mPASS\033[0m: no memory errors in %d test file(s)\n' $$passed; \
		else \
			printf '\033[31mFAIL\033[0m: memory errors in %d test file(s)\n' $$failed; \
			exit 1; \
		fi; \
	fi

valgrind-run: $(TARGET)
	$(VALGRIND) $(VALGRIND_FLAGS) ./$(TARGET)

run: $(TARGET)
	./$(TARGET)

doxygen:
	@command -v doxygen >/dev/null 2>&1 || { echo "doxygen not installed"; exit 1; }
	doxygen Doxyfile

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# --- Docker targets ---
docker-build:
	docker build -t $(DOCKER_IMAGE) .

docker-make: docker-build
	$(DOCKER_RUN) make

docker-test: docker-build
	$(DOCKER_RUN) make test

docker-run: docker-build
	$(DOCKER_RUN) -it make run

docker-valgrind: docker-build
	$(DOCKER_RUN) make valgrind

docker-valgrind-run: docker-build
	$(DOCKER_RUN) -it make valgrind-run

docker-clean:
	-docker rmi $(DOCKER_IMAGE)
	rm -rf $(BUILD_DIR)

help:
	@echo "Local targets (require system C++11 toolchain + GoogleTest):"
	@echo "  make              Build $(TARGET)"
	@echo "  make run          Build and run"
	@echo "  make test         Build and run all tests"
	@echo "  make doxygen      Generate HTML docs"
	@echo "  make clean        Remove build artifacts"
	@echo "  make valgrind            Run test suite under valgrind"
	@echo "  make valgrind-run        Run the TUI under valgrind"
	@echo ""
	@echo "Docker targets (require only Docker and make):"
	@echo "  make docker-build Build the dev image"
	@echo "  make docker-make  Run 'make' inside the container"
	@echo "  make docker-test  Run 'make test' inside the container"
	@echo "  make docker-run   Run the TUI inside the container"
	@echo "  make docker-clean Remove the dev image and build artifacts"
	@echo "  make docker-valgrind     Same as 'valgrind', inside Docker"
	@echo "  make docker-valgrind-run Same as 'valgrind-run', inside Docker"

-include $(ENGINE_OBJECTS:.o=.d) $(APP_OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d)