# Aidan du Bruyn (25040104)
# Connor Human (22752154)
# Ishmael Ngwasheng (24642542)
# Jay Macaskill (25198387)
# Keval Govind (24904407)
# Ruan van Tonder (25284607)
# Thabo Ngwabi (24742512)

# COS 214 (Software Modelling) TIVIDY
# Last Modified: 1 November 2026

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

# Definition tests use the real core and validator with structural item fixtures.
# Explicit object linkage keeps those fixtures separate from libtividy.a as real
# item implementations are added in later phases.
VALIDATION_TEST_SOURCES := tests/test_workflow_definition_validation.cpp \
						   src/engine/definition/WorkflowDefinition.cpp \
						   src/engine/definition/WorkflowDefinitionValidation.cpp \
						   src/engine/items/WorkItemSpec.cpp \
						   src/engine/core/Outcome.cpp \
						   src/engine/core/Types.cpp \
						   src/engine/policies/EscalationHandler.cpp
VALIDATION_TEST_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(VALIDATION_TEST_SOURCES))

# Isolated structural StageItem fixtures are confined to this builder test.
# Explicit objects prevent collisions with real item implementations in libtividy.a.
BUILDER_TEST_SOURCES := tests/test_workflow_builder.cpp \
						 src/engine/definition/WorkflowBuilder.cpp \
						 src/engine/definition/WorkflowDirector.cpp \
						 src/engine/definition/WorkflowDefinition.cpp \
						 src/engine/definition/WorkflowDefinitionValidation.cpp \
						 src/engine/items/WorkItem.cpp \
						 src/engine/items/WorkItemMemento.cpp \
						 src/engine/items/WorkItemIterator.cpp \
						 src/engine/items/WorkItemSpec.cpp \
						 src/engine/core/Types.cpp \
						 src/engine/core/Outcome.cpp \
						 src/engine/core/Participant.cpp \
						 src/engine/core/WorkflowData.cpp \
						 src/engine/policies/AssignmentStrategy.cpp \
						 src/engine/policies/RoutingStrategy.cpp \
						 src/engine/policies/EscalationHandler.cpp
BUILDER_TEST_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(BUILDER_TEST_SOURCES))

# PR #34 facade startup unit tests use explicit linkage to keep substitutes for
# unpublished runtime/library collaborators out of every other test binary.
FACADE_STARTUP_SOURCES := tests/test_facade_startup.cpp \
	src/engine/facade/Tividy.cpp src/engine/facade/IndustryPack.cpp \
	src/engine/definition/WorkflowBuilder.cpp src/engine/definition/WorkflowDirector.cpp \
	src/engine/definition/WorkflowDefinition.cpp src/engine/definition/WorkflowDefinitionValidation.cpp \
	src/engine/items/StageItem.cpp src/engine/items/WorkItem.cpp \
	src/engine/items/WorkItemMemento.cpp src/engine/items/WorkItemIterator.cpp \
	src/engine/items/WorkItemSpec.cpp src/engine/core/Clock.cpp \
	src/engine/core/Types.cpp src/engine/core/Outcome.cpp src/engine/core/Participant.cpp \
	src/engine/core/WorkflowData.cpp src/engine/events/Observer.cpp \
	src/engine/events/WorkflowEvent.cpp src/engine/connectors/ExternalConnector.cpp \
	src/engine/runtime/Observers.cpp src/engine/policies/AssignmentStrategy.cpp \
	src/engine/policies/RoutingStrategy.cpp src/engine/policies/EscalationHandler.cpp \
	src/ui/ClientProfile.cpp
FACADE_STARTUP_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(FACADE_STARTUP_SOURCES))

# State defaults use structural task fixtures while the state machine is assembled.
# Explicit linkage keeps these fixtures separate from the production archive.
STATE_DEFAULT_TEST_SOURCES := tests/test_work_item_state_defaults.cpp \
							  src/engine/items/WorkItemState.cpp \
							  src/engine/items/WorkItemSpec.cpp \
							  src/engine/core/Outcome.cpp \
							  src/engine/core/Types.cpp \
							  src/engine/core/Participant.cpp
STATE_DEFAULT_TEST_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(STATE_DEFAULT_TEST_SOURCES))

# Task-work tests isolate pending task/context/state collaborators from the archive.
TASK_WORK_TEST_SOURCES := tests/test_task_item_work.cpp \
						  src/engine/items/TaskItemWork.cpp \
						  src/engine/items/WorkItemState.cpp \
						  src/engine/items/WorkItemSpec.cpp \
						  src/engine/core/Types.cpp \
						  src/engine/core/Outcome.cpp \
						  src/engine/core/Clock.cpp \
						  src/engine/core/Participant.cpp \
						  src/engine/core/WorkflowData.cpp \
						  src/engine/events/WorkflowEvent.cpp \
						  src/engine/events/Observer.cpp \
						  src/engine/connectors/ExternalConnector.cpp
TASK_WORK_TEST_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(TASK_WORK_TEST_SOURCES))


# Snapshot tests use the real snapshot methods with isolated runtime/item fixtures.
# Explicit linkage avoids duplicate definitions when the runtime pieces are merged.
SNAPSHOT_TEST_SOURCES := tests/test_workflow_instance_snapshot.cpp \
						 src/engine/runtime/WorkflowInstanceSnapshot.cpp \
						 src/engine/items/WorkItemIterator.cpp \
						 src/engine/items/WorkItemSpec.cpp \
						 src/engine/core/Types.cpp \
						 src/engine/core/Outcome.cpp \
						 src/engine/core/Clock.cpp \
						 src/engine/core/Participant.cpp \
						 src/engine/core/WorkflowData.cpp \
						 src/engine/events/Observer.cpp \
						 src/engine/events/WorkflowEvent.cpp \
						 src/engine/connectors/ExternalConnector.cpp \
						 src/engine/commands/Command.cpp \
						 src/engine/runtime/Observers.cpp \
						 src/engine/policies/AssignmentStrategy.cpp
SNAPSHOT_TEST_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(SNAPSHOT_TEST_SOURCES))


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

$(BUILD_DIR)/tests/test_workflow_definition_validation: $(VALIDATION_TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(VALIDATION_TEST_OBJECTS) $(GTEST_LIBS)

$(BUILD_DIR)/tests/test_workflow_builder: $(BUILDER_TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(BUILDER_TEST_OBJECTS) $(GTEST_LIBS)

$(BUILD_DIR)/tests/test_facade_startup: $(FACADE_STARTUP_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(FACADE_STARTUP_OBJECTS) $(GTEST_LIBS)

$(BUILD_DIR)/tests/test_work_item_state_defaults: $(STATE_DEFAULT_TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(STATE_DEFAULT_TEST_OBJECTS) $(GTEST_LIBS)

$(BUILD_DIR)/tests/test_task_item_work: $(TASK_WORK_TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(TASK_WORK_TEST_OBJECTS) $(GTEST_LIBS)


$(BUILD_DIR)/tests/test_workflow_instance_snapshot: $(SNAPSHOT_TEST_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(SNAPSHOT_TEST_OBJECTS) $(GTEST_LIBS)


# Facade run-time operations (TividyActions.cpp): explicit linkage, substitutes only for runtime classes not yet in the tree.
FACADE_ACTIONS_SOURCES := tests/test_facade_actions.cpp \
	src/engine/facade/Tividy.cpp src/engine/facade/TividyActions.cpp src/engine/facade/IndustryPack.cpp \
	src/engine/commands/ActionHistory.cpp src/engine/commands/Command.cpp \
	src/engine/definition/WorkflowBuilder.cpp src/engine/definition/WorkflowDirector.cpp \
	src/engine/definition/WorkflowDefinition.cpp src/engine/definition/WorkflowDefinitionValidation.cpp \
	src/engine/items/StageItem.cpp src/engine/items/WorkItem.cpp \
	src/engine/items/WorkItemMemento.cpp src/engine/items/WorkItemIterator.cpp \
	src/engine/items/WorkItemSpec.cpp src/engine/core/Clock.cpp \
	src/engine/core/Types.cpp src/engine/core/Outcome.cpp src/engine/core/Participant.cpp \
	src/engine/core/WorkflowData.cpp src/engine/events/Observer.cpp \
	src/engine/events/WorkflowEvent.cpp src/engine/connectors/ExternalConnector.cpp \
	src/engine/runtime/Observers.cpp src/engine/policies/AssignmentStrategy.cpp \
	src/engine/policies/RoutingStrategy.cpp src/engine/policies/EscalationHandler.cpp \
	src/ui/ClientProfile.cpp
FACADE_ACTIONS_OBJECTS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(FACADE_ACTIONS_SOURCES))

$(BUILD_DIR)/tests/test_facade_actions: $(FACADE_ACTIONS_OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -o $@ $(FACADE_ACTIONS_OBJECTS) $(GTEST_LIBS)

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