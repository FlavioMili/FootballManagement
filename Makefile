# Compiler and flags
CXX := g++
CXXFLAGS := -Wall -Wextra -std=c++20 -I./include -I./src -I./src/model -I./src/controller -I./src/view/cli
MAKEFLAGS += -j$(shell nproc)

# Directories
SRC_DIR := src
BUILD_DIR := build
TARGET := football_management

# Sources and objects
SRCS := $(shell find $(SRC_DIR) -name '*.cpp')
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lsqlite3

# Compile .cpp -> .o into build/
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -I./include -c $< -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)
