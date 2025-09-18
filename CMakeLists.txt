cmake_minimum_required(VERSION 3.12)
project(FootballManagement LANGUAGES CXX)

# -----------------------------
# C++ Standard
# -----------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# -----------------------------
# Build types
# -----------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# -----------------------------
# Sources
# -----------------------------
file(GLOB_RECURSE CORE_SRC
  src/*.cpp
)

# -----------------------------
# Modules
# -----------------------------
set(MODEL_DIR "${PROJECT_SOURCE_DIR}/src/model")
set(CONTROLLER_DIR "${PROJECT_SOURCE_DIR}/src/controller")
set(DATABASE_DIR "${PROJECT_SOURCE_DIR}/src/database")
set(VIEW_GUI_DIR "${PROJECT_SOURCE_DIR}/src/gui")
set(VIEW_GUI_SCENES_DIR "${VIEW_GUI_DIR}/scenes")

# -----------------------------
# Executable
# -----------------------------
add_executable(${PROJECT_NAME} ${CORE_SRC})

# -----------------------------
# Target-specific include directories
# -----------------------------
target_include_directories(${PROJECT_NAME} PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
  ${MODEL_DIR}
  ${CONTROLLER_DIR}
  ${DATABASE_DIR}
  ${VIEW_GUI_DIR}
  ${VIEW_GUI_SCENES_DIR}
)

# -----------------------------
# Compiler warnings
# -----------------------------
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -pedantic)

# -----------------------------
# Dependencies
# -----------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)

find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(SQLite3 REQUIRED)

target_include_directories(${PROJECT_NAME} PRIVATE ${SDL3_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE
  ${SDL3_LIBRARIES}
  SQLite::SQLite3
  spdlog::spdlog
  fmt::fmt
  SDL3_ttf
)

set_target_properties(${PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}"
)

# -----------------------------
# Build type flags
# -----------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
  target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall -Wextra -Wpedantic
    -Wconversion -Wsign-conversion -Wshadow -Wold-style-cast -Wcast-align -Wformat=2
    -Og -g3 -fsanitize=address,undefined
    -fno-omit-frame-pointer -fstack-protector-strong -fno-optimize-sibling-calls
  )
  target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)
  target_compile_options(${PROJECT_NAME} PRIVATE -O2 -march=native -flto)
  target_link_options(${PROJECT_NAME} PRIVATE -flto)
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  target_compile_options(${PROJECT_NAME} PRIVATE -O2 -g -march=native)
endif()
