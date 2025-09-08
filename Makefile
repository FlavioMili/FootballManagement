# Compiler
CXX := g++
MAKEFLAGS += -j$(shell nproc)

# Directories
SRC_DIR := src
BUILD_DIR := build
TARGET := football_management

# Sources
SRCS := $(shell find $(SRC_DIR) -name '*.cpp')

# Build type (debug / release / relwithdebinfo)
BUILD_TYPE ?= release

# Common include paths & flags
INCLUDES := -I./include -I./src -I./src/model -I./src/controller -I./src/view/cli -I./src/database -I./src/view/gui
BASE_FLAGS := -Wall -Wextra -pedantic -std=c++20 $(INCLUDES) $(shell pkg-config --cflags sdl3)

# Flags per build type
ifeq ($(BUILD_TYPE),debug)
    CXXFLAGS := $(BASE_FLAGS)
    CXXFLAGS += -Og -g3 -fsanitize=address,undefined -fno-omit-frame-pointer
    LDFLAGS  := -fsanitize=address,undefined
    OBJ_DIR  := $(BUILD_DIR)/debug
else ifeq ($(BUILD_TYPE),release)
    CXXFLAGS := $(BASE_FLAGS)
    CXXFLAGS += -O3 -march=native -DNDEBUG -flto
    LDFLAGS  := -flto
    OBJ_DIR  := $(BUILD_DIR)/release
else ifeq ($(BUILD_TYPE),relwithdebinfo)
    CXXFLAGS := $(BASE_FLAGS)
    CXXFLAGS += -O2 -g -march=native
    LDFLAGS  :=
    OBJ_DIR  := $(BUILD_DIR)/relwithdebinfo
else
    $(error Unknown BUILD_TYPE '$(BUILD_TYPE)'. Use 'debug', 'release', or 'relwithdebinfo')
endif

# Objects
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRCS))

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lsqlite3 $(shell pkg-config --libs sdl3) -lSDL3_ttf -lspdlog -lfmt


# Compile
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Shortcuts
debug:
	$(MAKE) BUILD_TYPE=debug

release:
	$(MAKE) BUILD_TYPE=release

relwithdebinfo:
	$(MAKE) BUILD_TYPE=relwithdebinfo
