# -----------------------------------------------------------------------------
#  Football Management Project
#  Install layout and CPack packages for the game executable.
#  See docs/development/release.md.
#
#  Layout (must match RuntimePaths::assetRoot()):
#    Linux    bin/Player12, share/footballmanagement/assets
#    macOS    Player12.app/Contents/Resources/assets
#    Windows  Player12.exe and assets/ side by side
#  Only the "game" component is packaged, so install rules of fetched
#  dependencies never leak headers or static libraries into a package.
# -----------------------------------------------------------------------------

# FM_GAME_VERSION (top-level CMakeLists.txt) names the packages; bundle
# metadata takes the plain numeric project version.
set(FM_GAME_TARGET FootballManagement)
# Stable CMake target; public executable and package name follow the brand.
set(FM_GAME_NAME Player12)
set_target_properties(${FM_GAME_TARGET} PROPERTIES OUTPUT_NAME "${FM_GAME_NAME}")
set(FM_COMPONENT game)

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" FM_ARCH)
if(FM_ARCH MATCHES "^(amd64|x86_64|x64)$")
  set(FM_ARCH x86_64)
elseif(FM_ARCH MATCHES "^(arm64|aarch64)$")
  set(FM_ARCH arm64)
endif()
if(APPLE)
  set(FM_OS macos)
elseif(WIN32)
  set(FM_OS windows)
else()
  set(FM_OS linux)
endif()
set(FM_PACKAGE_PLATFORM "${FM_OS}-${FM_ARCH}" CACHE STRING
  "Platform suffix of package file names")

if(APPLE)
  set(FM_BUNDLE "${FM_GAME_NAME}.app")
  set(FM_DATA_DESTINATION "${FM_BUNDLE}/Contents/Resources")
  set(FM_LICENSE_DESTINATION licenses)
  set_target_properties(${FM_GAME_TARGET} PROPERTIES
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_INFO_PLIST "${PROJECT_SOURCE_DIR}/packaging/macos/Info.plist.in"
    MACOSX_BUNDLE_BUNDLE_NAME "${FM_GAME_NAME}"
    MACOSX_BUNDLE_GUI_IDENTIFIER "io.github.flaviomili.footballmanagement"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2025 - 2026 Flavio Milinanni")
  install(TARGETS ${FM_GAME_TARGET}
    BUNDLE DESTINATION . COMPONENT ${FM_COMPONENT})
elseif(WIN32)
  set(FM_DATA_DESTINATION .)
  set(FM_LICENSE_DESTINATION licenses)
  if(MSVC)
    # GUI subsystem (no console window) while keeping the standard main().
    set_target_properties(${FM_GAME_TARGET} PROPERTIES WIN32_EXECUTABLE ON)
    target_link_options(${FM_GAME_TARGET} PRIVATE /ENTRY:mainCRTStartup)
  endif()
  # UTF-8 as the process code page: narrow paths (SDL, SQLite, std::filesystem)
  # then work in user folders with non-ASCII names.
  target_sources(${FM_GAME_TARGET} PRIVATE
    "${PROJECT_SOURCE_DIR}/packaging/windows/footballmanagement.manifest")
  install(TARGETS ${FM_GAME_TARGET}
    RUNTIME DESTINATION . COMPONENT ${FM_COMPONENT})
  install(FILES packaging/windows/README-WINDOWS.txt
    DESTINATION . RENAME README.txt COMPONENT ${FM_COMPONENT})
else()
  # Fixed (not GNUInstallDirs) because the runtime lookup expects exactly
  # <exe>/../share/footballmanagement.
  set(FM_DATA_DESTINATION share/footballmanagement)
  set(FM_LICENSE_DESTINATION share/doc/footballmanagement)
  install(TARGETS ${FM_GAME_TARGET}
    RUNTIME DESTINATION bin COMPONENT ${FM_COMPONENT})
  # The desktop entry carries the version for AppImage tooling.
  configure_file(packaging/linux/footballmanagement.desktop.in
    "${PROJECT_BINARY_DIR}/packaging/footballmanagement.desktop" @ONLY)
  install(FILES "${PROJECT_BINARY_DIR}/packaging/footballmanagement.desktop"
    DESTINATION share/applications COMPONENT ${FM_COMPONENT})
  install(FILES packaging/linux/footballmanagement.svg
    DESTINATION share/icons/hicolor/scalable/apps COMPONENT ${FM_COMPONENT})
endif()

# Read-only game data. Developer-local files (ignored by git) stay out.
install(DIRECTORY assets
  DESTINATION "${FM_DATA_DESTINATION}"
  COMPONENT ${FM_COMPONENT}
  PATTERN "imgui.ini" EXCLUDE
  PATTERN "config/settings.json" EXCLUDE
  PATTERN ".DS_Store" EXCLUDE)

# -----------------------------------------------------------------------------
# Licenses: the game's own plus every third-party component linked in.
# -----------------------------------------------------------------------------
install(FILES LICENSE README.md
  DESTINATION "${FM_LICENSE_DESTINATION}" COMPONENT ${FM_COMPONENT})
install(FILES
    packaging/licenses/Roboto-LICENSE.txt
    packaging/licenses/SQLite-NOTICE.txt
  DESTINATION "${FM_LICENSE_DESTINATION}/third_party"
  COMPONENT ${FM_COMPONENT})

function(fm_install_third_party_license name file)
  if(EXISTS "${file}")
    get_filename_component(filename "${file}" NAME)
    install(FILES "${file}"
      DESTINATION "${FM_LICENSE_DESTINATION}/third_party"
      RENAME "${name}-${filename}"
      COMPONENT ${FM_COMPONENT})
  endif()
endfunction()

fm_install_third_party_license(fmt "${fmt_SOURCE_DIR}/LICENSE.rst")
fm_install_third_party_license(fmt "${fmt_SOURCE_DIR}/LICENSE")
fm_install_third_party_license(spdlog "${spdlog_SOURCE_DIR}/LICENSE")
fm_install_third_party_license(nlohmann_json "${json_SOURCE_DIR}/LICENSE.MIT")
fm_install_third_party_license(imgui "${imgui_SOURCE_DIR}/LICENSE.txt")
if(NOT FM_USE_SYSTEM_SDL)
  fm_install_third_party_license(SDL3 "${sdl3_SOURCE_DIR}/LICENSE.txt")
  fm_install_third_party_license(SDL3_ttf "${sdl3_ttf_SOURCE_DIR}/LICENSE.txt")
  if(SDLTTF_VENDORED)
    set(FM_TTF_EXTERNAL "${sdl3_ttf_SOURCE_DIR}/external")
    fm_install_third_party_license(freetype "${FM_TTF_EXTERNAL}/freetype/LICENSE.TXT")
    fm_install_third_party_license(freetype "${FM_TTF_EXTERNAL}/freetype/docs/FTL.TXT")
    fm_install_third_party_license(harfbuzz "${FM_TTF_EXTERNAL}/harfbuzz/COPYING")
    fm_install_third_party_license(plutosvg "${FM_TTF_EXTERNAL}/plutosvg/LICENSE")
    fm_install_third_party_license(plutovg "${FM_TTF_EXTERNAL}/plutovg/LICENSE")
  endif()
endif()

if(APPLE)
  # Signing must come last: the ad-hoc signature seals the bundle contents
  # installed above (arm64 refuses to run unsigned code).
  install(CODE "
    execute_process(
      COMMAND codesign --force --deep --sign -
        \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${FM_BUNDLE}\"
      RESULT_VARIABLE fm_codesign_result)
    if(NOT fm_codesign_result EQUAL 0)
      message(FATAL_ERROR \"codesign failed for ${FM_BUNDLE}\")
    endif()"
    COMPONENT ${FM_COMPONENT})
endif()

# -----------------------------------------------------------------------------
# CPack: TGZ on Linux, ZIP on macOS and Windows.
# -----------------------------------------------------------------------------
set(CPACK_PACKAGE_NAME "${FM_GAME_NAME}")
set(CPACK_PACKAGE_VENDOR "Flavio Milinanni")
set(CPACK_PACKAGE_VERSION "${FM_GAME_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Football club management game")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/FlavioMili/FootballManagement")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_FILE_NAME
  "${CPACK_PACKAGE_NAME}-${FM_GAME_VERSION}-${FM_PACKAGE_PLATFORM}")
set(CPACK_INSTALL_CMAKE_PROJECTS
  "${CMAKE_BINARY_DIR};${PROJECT_NAME};${FM_COMPONENT};/")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_VERBATIM_VARIABLES ON)
set(CPACK_THREADS 0)
set(CPACK_SOURCE_GENERATOR "")
if(APPLE OR WIN32)
  set(CPACK_GENERATOR ZIP)
else()
  set(CPACK_GENERATOR TGZ)
  set(CPACK_STRIP_FILES ON)
endif()
include(CPack)
