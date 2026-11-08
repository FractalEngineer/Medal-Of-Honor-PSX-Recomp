cmake_minimum_required(VERSION 3.20)
set(PSXRECOMP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/psxrecomp" CACHE PATH
    "Path to psxrecomp")
if(NOT EXISTS "${PSXRECOMP_ROOT}/runtime/runtime.cmake")
    message(FATAL_ERROR
        "PSXRecomp runtime is missing: ${PSXRECOMP_ROOT}/runtime/runtime.cmake\n"
        "This project expects the psxrecomp framework checkout at ${PSXRECOMP_ROOT}.\n"
        "Run: git submodule update --init --recursive\n"
        "If psxrecomp/ is not a submodule, set -DPSXRECOMP_ROOT=<path-to-psxrecomp>.")
endif()

project(Medal_of_Honor_Recomp LANGUAGES C CXX)

# --- Opt-in product surfaces (OFF by default in the framework) -------------
# Scaffold prompts / flags fill these. Without recomp-ui, PSX_RECOMP_UI is OFF.
# The wizard (WIZARD_CMAKE_BLOCK / ENABLE_SETUP_WIZARD) is the first-run disc
# picker; releases are compiled from the committed generated/ C, so it never
# needs to generate or rebuild anything on a player's machine.
# recomp-ui submodule present (PSX_RECOMP_UI defaults ON).
# Full netplay UI -- enable after adding recomp-ui + testing:
# if(EXISTS "${PSXRECOMP_ROOT}/lib/recomp-net/CMakeLists.txt")
#     set(PSX_NETPLAY ON CACHE BOOL "..." FORCE)
# endif()
set(PSX_SETUP_WIZARD ON CACHE BOOL
    "Advertise first-run setup wizard + Generate & rebuild in recomp-ui" FORCE)
# Detailed per-instruction diagnostics are for dedicated profiling builds.
# Keep them opt-in for normal play; -DPSX_DEBUG_TOOLS=ON selects that build.
set(PSX_DEBUG_TOOLS OFF CACHE BOOL "Build detailed developer diagnostics")
include("${PSXRECOMP_ROOT}/runtime/runtime.cmake")

# This project's own mod catalog. The FRAMEWORK stages it -- together with its
# builtin psx.* packages -- into <exe-dir>/mods/bundled, and refuses to
# configure if a catalog exists here but is not declared. Never copy it by
# hand: five titles did, and framework 4cc04be3's rename of the staged
# directory broke all five silently (bead beads-eio.3.101).
set(PSX_PRELOADED_MODS "${CMAKE_CURRENT_SOURCE_DIR}/mods/preloaded")
if(NOT IS_DIRECTORY "${PSX_PRELOADED_MODS}")
    set(PSX_PRELOADED_MODS "NONE")
endif()

option(MOH_PGXP "Compile precision provenance hooks for VR projective rendering" ON)
set(MOH_PRECISION_OPTIONS)
if(MOH_PGXP)
    list(APPEND MOH_PRECISION_OPTIONS PGXP)
endif()
psxrecomp_add_game_runtime(psx-runtime
    ${MOH_PRECISION_OPTIONS}
    PRELOADED_MODS_DIR "${PSX_PRELOADED_MODS}"
    # ENABLE_NETPLAY_IF_PRESENT
    # NETPLAY_LOBBY_URL "ws://netplay.retcomm.net:8765"
    ENABLE_SETUP_WIZARD
    WINDOW_TITLE "Medal of Honor  Recompiled"
    DEFAULT_GAME_CONFIG_PATH "game.toml"
    MAX_PLAYERS 2
    LAUNCHER_BOXART "${CMAKE_CURRENT_SOURCE_DIR}/launcher_assets/img/boxart.tga"
    APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/assets/psxrecomp.ico"
    # Committed game C (psxrecomp_cli.py generate; git add generated). Release
    # CI configures with -DPSXRECOMP_REQUIRE_GAME_C=ON so a checkout without
    # it fails instead of building a setup host.
    GEN_MARKER "generated/SLUS_009.74_dispatch.c"
    GEN_FULL_GLOB "generated/SLUS_009.74_full_*.c"
    CODEGEN_SETUP_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/codegen_setup.c"
)

# Game-owned trusted plugin: entity-transform probe + stereo injection point.
# Registered via PSX_MOD_CONSTRUCTOR; activated by the moh.vr.stereo package.
target_sources(psx-runtime PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/vr/psx_vr_stereo.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vr/moh_vr_input.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vr/moh_vr_hud.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vr/moh_vr_aim.c")

# Opt-in mouse and keyboard plugin and its game-specific input adapter.
target_sources(psx-runtime PRIVATE
    controls/mouse_keyboard/moh_mkb.c
    controls/mouse_keyboard/moh_mkb_input.c
    controls/mouse_keyboard/moh_mkb_guest.c
    controls/mouse_keyboard/moh_mkb_plugin.c)

if(BUILD_TESTING)
    enable_testing()
    find_package(Python3 3.11 COMPONENTS Interpreter REQUIRED)
    add_test(NAME moh_mkb_manifest_test COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/controls/mouse_keyboard/test_manifest.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/mods/preloaded/packages/moh.input.mouse-keyboard/0.1.0/manifest.toml")
    add_executable(moh_mkb_input_test
        controls/mouse_keyboard/test_moh_mkb.c
        controls/mouse_keyboard/moh_mkb_input.c)
    target_include_directories(moh_mkb_input_test PRIVATE "${PSXRECOMP_ROOT}/runtime/include")
    if(NOT WIN32)
        target_link_libraries(moh_mkb_input_test PRIVATE m)
    endif()
    add_test(NAME moh_mkb_input_test COMMAND moh_mkb_input_test)

    add_executable(moh_mkb_controls_test
        controls/mouse_keyboard/test_moh_mkb_controls.c
        controls/mouse_keyboard/moh_mkb.c)
    target_include_directories(moh_mkb_controls_test PRIVATE "${PSXRECOMP_ROOT}/runtime/include")
    if(NOT WIN32)
        target_link_libraries(moh_mkb_controls_test PRIVATE m)
    endif()
    add_test(NAME moh_mkb_controls_test COMMAND moh_mkb_controls_test)

    add_executable(moh_mkb_guest_test
        controls/mouse_keyboard/test_moh_mkb_guest.c
        controls/mouse_keyboard/moh_mkb_guest.c
        controls/mouse_keyboard/moh_mkb_input.c)
    target_include_directories(moh_mkb_guest_test PRIVATE "${PSXRECOMP_ROOT}/runtime/include")
    if(NOT WIN32)
        target_link_libraries(moh_mkb_guest_test PRIVATE m)
    endif()
    add_test(NAME moh_mkb_guest_test COMMAND moh_mkb_guest_test)

    add_executable(moh_mkb_plugin_test
        controls/mouse_keyboard/test_moh_mkb_plugin.c
        controls/mouse_keyboard/moh_mkb.c
        controls/mouse_keyboard/moh_mkb_input.c)
    target_include_directories(moh_mkb_plugin_test PRIVATE "${PSXRECOMP_ROOT}/runtime/include")
    target_link_libraries(moh_mkb_plugin_test PRIVATE ${PSX_SDL_LIBRARIES})
    if(PSX_SDL3)
        target_compile_definitions(moh_mkb_plugin_test PRIVATE PSX_SDL3)
    endif()
    if(NOT WIN32)
        target_link_libraries(moh_mkb_plugin_test PRIVATE m)
    endif()
    add_test(NAME moh_mkb_plugin_test COMMAND moh_mkb_plugin_test)

    add_executable(moh_mod_binding_options_test
        "${PSXRECOMP_ROOT}/runtime/tests/test_mod_binding_options.cpp"
        "${PSXRECOMP_ROOT}/runtime/src/mod_packages.cpp"
        "${PSXRECOMP_ROOT}/runtime/src/mod_media.cpp"
        "${PSXRECOMP_ROOT}/runtime/src/iso_reader.cpp"
        "${PSXRECOMP_ROOT}/runtime/src/cue_sheet.cpp"
        "${PSXRECOMP_ROOT}/runtime/src/crc32.c"
        "${PSXRECOMP_ROOT}/runtime/src/psx_sha256.c")
    target_include_directories(moh_mod_binding_options_test PRIVATE
        "${PSXRECOMP_ROOT}/runtime/include" "${PSXRECOMP_ROOT}/recompiler/include"
        "${PSXRECOMP_ROOT}/recompiler/lib/toml11")
    target_link_libraries(moh_mod_binding_options_test PRIVATE chdr-static)
    set_target_properties(moh_mod_binding_options_test PROPERTIES
        CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)
    add_test(NAME moh_mod_binding_options_test COMMAND moh_mod_binding_options_test)
endif()

# Current psxrecomp embeds its UTF-8 manifest as an object library. recomp-ui
# cannot detect that when adding its older fallback manifest, so keep the
# framework manifest and drop the duplicate resource source.
if(WIN32 AND NOT MSVC)
    set(_recomp_ui_manifest
        "${CMAKE_CURRENT_BINARY_DIR}/psx-runtime_recomp_utf8_manifest.rc")
    get_target_property(_psx_runtime_sources psx-runtime SOURCES)
    if(_recomp_ui_manifest IN_LIST _psx_runtime_sources)
        list(REMOVE_ITEM _psx_runtime_sources "${_recomp_ui_manifest}")
        set_target_properties(psx-runtime PROPERTIES
            SOURCES "${_psx_runtime_sources}")
    endif()
endif()

# Stage runtime configs next to the exe for direct build/ launches. Only copy
# files that exist — never fail POST_BUILD when game_options.toml is absent.
foreach(_PSX_RUNTIME_CONFIG game.toml game_options.toml)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${_PSX_RUNTIME_CONFIG}")
        add_custom_command(TARGET psx-runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/${_PSX_RUNTIME_CONFIG}"
                "$<TARGET_FILE_DIR:psx-runtime>/${_PSX_RUNTIME_CONFIG}"
            COMMENT "Staging ${_PSX_RUNTIME_CONFIG}"
            VERBATIM)
    endif()
endforeach()
