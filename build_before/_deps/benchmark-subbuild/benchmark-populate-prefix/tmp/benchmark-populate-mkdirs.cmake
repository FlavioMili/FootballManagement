# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file LICENSE.rst or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/Flavio/Programming/FootballManagement/build_before/_deps/benchmark-src")
  file(MAKE_DIRECTORY "/home/Flavio/Programming/FootballManagement/build_before/_deps/benchmark-src")
endif()
file(MAKE_DIRECTORY
  "/home/Flavio/Programming/FootballManagement/build_before/_deps/benchmark-build"
  "/home/Flavio/Programming/FootballManagement/build_before/_deps/benchmark-subbuild/benchmark-populate-prefix"
  "/home/Flavio/Programming/FootballManagement/build_before/_deps/benchmark-subbuild/benchmark-populate-prefix/tmp"
  "/home/Flavio/Programming/FootballManagement/build_before/_deps/benchmark-subbuild/benchmark-populate-prefix/src/benchmark-populate-stamp"
  "/home/Flavio/Programming/FootballManagement/build_before/_deps/benchmark-subbuild/benchmark-populate-prefix/src"
  "/home/Flavio/Programming/FootballManagement/build_before/_deps/benchmark-subbuild/benchmark-populate-prefix/src/benchmark-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/Flavio/Programming/FootballManagement/build_before/_deps/benchmark-subbuild/benchmark-populate-prefix/src/benchmark-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/Flavio/Programming/FootballManagement/build_before/_deps/benchmark-subbuild/benchmark-populate-prefix/src/benchmark-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
