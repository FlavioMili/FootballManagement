cmake_minimum_required(VERSION 3.16)
project(FootballManagement LANGUAGES CXX C)

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
# Debug Warning Flags
# -----------------------------
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

# -----------------------------
# Dependencies (FetchContent & find_package)
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

# --- SQLite3 ---
find_package(SQLite3 QUIET)
if(NOT SQLite3_FOUND)
  message(STATUS "SQLite3 not found, fetching from GitHub...")
  FetchContent_Declare(
    sqlite3
    GIT_REPOSITORY https://github.com/sjinks/sqlite3-cmake.git
    GIT_TAG        v3.45.1.0
  )
  FetchContent_MakeAvailable(sqlite3)
  if(NOT TARGET SQLite::SQLite3)
    add_library(SQLite::SQLite3 ALIAS sqlite3)
  endif()
endif()

# --- SDL3 ---
find_package(SDL3 QUIET)
if(NOT SDL3_FOUND)
  message(STATUS "SDL3 not found, fetching from GitHub...")
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        main # Using main for SDL3 to get latest fixes as tags are moving fast
  )
  FetchContent_MakeAvailable(SDL3)
endif()

# --- SDL3_ttf ---
find_package(SDL3_ttf QUIET)
if(NOT SDL3_ttf_FOUND)
  message(STATUS "SDL3_ttf not found, fetching from GitHub...")
  set(SDL3TTF_SAMPLES OFF CACHE BOOL "" FORCE)
  # SDL3_ttf uses its own internal logic to find freetype/harfbuzz
  FetchContent_Declare(
    SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG        main # Using main to ensure compatibility with SDL3 main
  )
  FetchContent_MakeAvailable(SDL3_ttf)
endif()

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

# -----------------------------
# Subdirectories
# -----------------------------
add_subdirectory(src)

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(test)
endif()

# -----------------------------
# Tools & Formatting
# -----------------------------

# Copy compile_commands.json to source dir
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  add_custom_target(copy_compile_commands ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_BINARY_DIR}/compile_commands.json
      ${CMAKE_SOURCE_DIR}/compile_commands.json
    COMMENT "Copying compile_commands.json to root"
  )
endif()

find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
  file(GLOB_RECURSE FORMAT_FILES
    src/*.cpp
    src/*.h
    src/*.hpp
    test/*.cpp
  )
  add_custom_target(format
    COMMAND ${CLANG_FORMAT} -i ${FORMAT_FILES}
    COMMENT "Running clang-format"
  )
endif()

find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
  set(CMAKE_CXX_CLANG_TIDY
      ${CLANG_TIDY};
      -p=${CMAKE_BINARY_DIR}
  )
endif()

find_program(CPPCHECK cppcheck)
if(CPPCHECK)
  add_custom_target(cppcheck
    COMMAND ${CPPCHECK}
    --enable=warning,performance,portability
    --std=c++20
    --language=c++
    --inline-suppr
    --quiet
    src
  )
endif()
