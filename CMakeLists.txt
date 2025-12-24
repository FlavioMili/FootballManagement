cmake_minimum_required(VERSION 3.14)
project(FootballManagement LANGUAGES CXX)

# -----------------------------
# C++ Standard
# -----------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------
# Build types
# -----------------------------
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# -----------------------------
# Dependencies (FetchContent)
# -----------------------------
include(FetchContent)

# fmt
FetchContent_Declare(
  fmt
  GIT_REPOSITORY https://github.com/fmtlib/fmt.git
  GIT_TAG        10.1.1 
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(fmt)

# spdlog
FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG        v1.12.0
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)

# nlohmann_json
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(json)

# GoogleTest (for tests)
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
  )
  FetchContent_MakeAvailable(googletest)
endif()

# SDL3 (System)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL3 REQUIRED sdl3)

# SQLite3 (System)
find_package(SQLite3 REQUIRED)

# -----------------------------
# Sources & Library
# -----------------------------
file(GLOB_RECURSE ALL_SRC src/*.cpp)
list(FILTER ALL_SRC EXCLUDE REGEX "src/main.cpp$")

add_library(core_lib ${ALL_SRC})

# -----------------------------
# Include Directories
# -----------------------------
set(MODEL_DIR "${PROJECT_SOURCE_DIR}/src/model")
set(CONTROLLER_DIR "${PROJECT_SOURCE_DIR}/src/controller")
set(DATABASE_DIR "${PROJECT_SOURCE_DIR}/src/database")
set(VIEW_GUI_DIR "${PROJECT_SOURCE_DIR}/src/gui")
set(VIEW_GUI_SCENES_DIR "${VIEW_GUI_DIR}/scenes")

target_include_directories(core_lib PUBLIC
  ${PROJECT_SOURCE_DIR}/src
  ${MODEL_DIR}
  ${CONTROLLER_DIR}
  ${DATABASE_DIR}
  ${VIEW_GUI_DIR}
  ${VIEW_GUI_SCENES_DIR}
  ${SDL3_INCLUDE_DIRS}
)

target_link_directories(core_lib PUBLIC ${SDL3_LIBRARY_DIRS})

target_link_libraries(core_lib PUBLIC
  fmt::fmt
  spdlog::spdlog
  nlohmann_json::nlohmann_json
  SQLite::SQLite3
  ${SDL3_LIBRARIES}
  SDL3_ttf
)

# Basic warnings for all builds
target_compile_options(core_lib PRIVATE -Wall -Wextra -pedantic)

# -----------------------------
# Executable
# -----------------------------
add_executable(${PROJECT_NAME} src/main.cpp)
target_link_libraries(${PROJECT_NAME} PRIVATE core_lib)

# -----------------------------
# Build type flags
# -----------------------------
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  
  # Extensive warning flags for Debug build
  set(DEBUG_WARNINGS
    -Wshadow
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Wunused
    -Woverloaded-virtual
    -Wconversion
    -Wsign-conversion
    -Wmisleading-indentation
    -Wduplicated-cond
    -Wduplicated-branches
    -Wlogical-op
    -Wnull-dereference
    -Wuseless-cast
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
  )

  target_compile_definitions(core_lib PRIVATE DEBUG)
  target_compile_options(core_lib PRIVATE 
    -Og -g3 
    -fsanitize=address,undefined
    ${DEBUG_WARNINGS}
  )
  target_link_options(core_lib PRIVATE -fsanitize=address,undefined)
  
  target_compile_definitions(${PROJECT_NAME} PRIVATE DEBUG)
  target_compile_options(${PROJECT_NAME} PRIVATE 
    -Og -g3 
    -fsanitize=address,undefined
    ${DEBUG_WARNINGS}
  )
  target_link_options(${PROJECT_NAME} PRIVATE -fsanitize=address,undefined)

elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_definitions(core_lib PRIVATE NDEBUG)
  target_compile_options(core_lib PRIVATE -O2 -march=native -flto=auto)
  target_link_options(core_lib PRIVATE -flto=auto)

  target_compile_definitions(${PROJECT_NAME} PRIVATE NDEBUG)
  target_compile_options(${PROJECT_NAME} PRIVATE -O2 -march=native -flto=auto)
  target_link_options(${PROJECT_NAME} PRIVATE -flto=auto)

elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
  target_compile_options(core_lib PRIVATE -O2 -g -march=native)
  target_compile_options(${PROJECT_NAME} PRIVATE -O2 -g -march=native)
endif()

# -----------------------------
# Tests
# -----------------------------
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()