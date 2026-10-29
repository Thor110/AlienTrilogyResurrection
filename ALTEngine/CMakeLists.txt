cmake_minimum_required(VERSION 3.20)

# The release version lives in one place, VERSION, next to this file:
# tools/make_release.bat names the release folder from it, and the engine
# reports it (ALTENGINE_VERSION). Change it there and nowhere else.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" ALTENGINE_VERSION LIMIT_COUNT 1)
string(STRIP "${ALTENGINE_VERSION}" ALTENGINE_VERSION)
project(ALTEngine VERSION ${ALTENGINE_VERSION} LANGUAGES CXX)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings for this project's own targets only (alt_warnings below), so the
# libraries fetched when not installed (SDL3, nlohmann-json) build with their
# own settings.
add_library(alt_warnings INTERFACE)
if (MSVC)
    target_compile_options(alt_warnings INTERFACE /W4 /permissive-)
else()
    target_compile_options(alt_warnings INTERFACE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# ALTFormats - pure data layer (BND/B16/palette/model/level parsing, plus
# binary patching). No windowing, no rendering, no platform-specific calls
# beyond filesystem.
# ---------------------------------------------------------------------------
# nlohmann-json: an installed one (vcpkg on Windows, nlohmann-json3-dev on
# Linux, brew's nlohmann-json on macOS), or else the release fetched at
# configure time. It is header-only, so the fetch is quick.
find_package(nlohmann_json CONFIG QUIET)
if (NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann-json not found: fetching 3.11.3")
    include(FetchContent)
    FetchContent_Declare(nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d)
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(ALTFormats STATIC
    src/Formats/ModelIndices.h
    src/Formats/BinaryUtility.cpp
    src/Formats/FaceUvPatches.cpp
    src/Formats/PatchLoader.cpp
    src/Formats/PatchRunner.cpp
    src/Formats/BndParser.cpp
    src/Formats/PaletteFile.cpp
    src/Formats/RawImageRenderer.cpp
    src/Formats/SplitFrameImage.cpp
    src/Formats/SplashImageLoader.cpp
    src/Formats/BxParser.cpp
    src/Formats/BndTextureLoader.cpp
    src/Formats/DiscManifest.cpp
    src/Formats/Installer.cpp
    src/Formats/CddaRipper.cpp
    src/Formats/OverrideImage.cpp
    src/Formats/ModelLoader.cpp
    src/Formats/LevelList.cpp
    src/Formats/ObjLoader.cpp
    src/Formats/RenderMesh.cpp
    src/Formats/UvSeamScan.cpp
    src/Formats/LevelLoader.cpp
    src/Formats/BinaryReadLE.h
    src/Formats/MissionText.cpp
    src/Formats/Obj3DTexture.cpp
    src/Formats/SpriteFrameDecompressor.cpp
    src/Formats/SpriteFrameDimensions.cpp
    src/Formats/SpriteFrameLoader.cpp
)
target_include_directories(ALTFormats PUBLIC src)
target_link_libraries(ALTFormats PRIVATE nlohmann_json::nlohmann_json alt_warnings)

# ---------------------------------------------------------------------------
# ALTGame - gameplay rules (triggers, doors, ...) pulled out of
# GameplayScreen so they can be tested. Same rule as ALTFormats: no SDL, no
# rendering. The screen owns presentation and calls into this.
# ---------------------------------------------------------------------------
add_library(ALTGame STATIC
    src/Game/Blast.cpp
    src/Game/Doors.cpp
    src/Game/PathGraph.cpp
    src/Game/ProjectileCollision.cpp
    src/Game/CreatureSight.cpp
    src/Game/Lifts.cpp
    src/Game/Progression.cpp
    src/Game/SaveGame.cpp
    src/Game/Triggers.cpp
)
target_include_directories(ALTGame PUBLIC src)
target_link_libraries(ALTGame PUBLIC ALTFormats)
target_link_libraries(ALTGame PRIVATE nlohmann_json::nlohmann_json alt_warnings)

# ---------------------------------------------------------------------------
# ALTLevelScan - a command-line check of every level's texture orientation
# (src/Tools/LevelScanMain.cpp). SDL-free, like the tests.
# ---------------------------------------------------------------------------
option(ALTENGINE_BUILD_TOOLS "Build the ALTLevelScan command-line tool" ON)
if (ALTENGINE_BUILD_TOOLS)
    add_executable(ALTLevelScan src/Tools/LevelScanMain.cpp)
    target_link_libraries(ALTLevelScan PRIVATE ALTFormats alt_warnings nlohmann_json::nlohmann_json)
endif()

# ---------------------------------------------------------------------------
# Tests. SDL-free, so they build and run anywhere ALTFormats does:
#   ctest --test-dir <build dir>      or run ALTEngineTests directly
# Level fixtures come from the demo disc copy under Notes/; a test whose
# fixture is missing is reported as skipped, not failed.
# ---------------------------------------------------------------------------
option(ALTENGINE_BUILD_TESTS "Build the ALTEngineTests unit test executable" ON)
if (ALTENGINE_BUILD_TESTS)
    enable_testing()
    add_executable(ALTEngineTests
        tests/TestMain.cpp
        tests/LevelLoaderTests.cpp
        tests/TriggerTests.cpp
        tests/DoorTests.cpp
        tests/InventoryTests.cpp
        tests/EnemyTests.cpp
        tests/SaveGameTests.cpp
        tests/ProgressionTests.cpp
        tests/BlastTests.cpp
        tests/PathGraphTests.cpp
        tests/ProjectileCollisionTests.cpp
        tests/CreatureSightTests.cpp
        tests/UvSeamScanTests.cpp
        tests/EnemySpriteSetTests.cpp
        tests/CreatureAITests.cpp
        tests/LiftTests.cpp
        tests/CreatureTaskTests.cpp
        tests/PlayerMoverTests.cpp
        tests/CddaTests.cpp
        tests/LevelSnapshotTests.cpp
        tests/RenderMeshTests.cpp
    )
    target_include_directories(ALTEngineTests PRIVATE src tests)
    target_link_libraries(ALTEngineTests PRIVATE ALTGame ALTFormats alt_warnings nlohmann_json::nlohmann_json)
    target_compile_definitions(ALTEngineTests PRIVATE
        ALT_TEST_DEMO_ROOT="${CMAKE_SOURCE_DIR}/../Notes/repack-disc-comparison/DEMO"
        ALT_TEST_DATA_ROOT="${CMAKE_SOURCE_DIR}/data")
    add_test(NAME ALTEngineTests COMMAND ALTEngineTests)
endif()

# SDL3: an installed one (vcpkg on Windows, brew's sdl3 on macOS), or else
# SDL built from source as a static library the first time you configure
# (Ubuntu 24.04 has no SDL3 package). On Linux that needs X11/Wayland and
# audio development packages - see .github/workflows/build.yml.
find_package(SDL3 CONFIG QUIET)
if (NOT SDL3_FOUND)
    message(STATUS "SDL3 not found: fetching SDL 3.2.30 (first configure only, takes a few minutes)")
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.30
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

# FFmpeg (vcpkg's ffmpeg port ships pkg-config .pc files, not a CMake
# package config like SDL3/nlohmann-json do - pkg-config is the standard
# portable way to consume it either way, vcpkg or system-installed).
#
# LICENSING: only request vcpkg's *default* ffmpeg features. The "gpl"
# and "nonfree" features are opt-in and OFF by default specifically
# because enabling them changes FFmpeg's own license from LGPL v2.1+ to
# GPL v2+ (or worse, for nonfree) - see third_party_licenses/ffmpeg/.
# Don't add gpl-gated features (x264/x265/etc) without revisiting this.
#
# Also: prefer FFmpeg built/linked as shared libraries (vcpkg's default
# x64-windows triplet already does this) rather than static - LGPL v2.1
# permits static linking, but only if the user can still relink against a
# modified FFmpeg, which is a real obligation to satisfy manually. Dynamic
# linking (ship the .dll alongside the .exe) sidesteps that requirement
# entirely and is simpler to stay compliant with. Don't switch to a
# static triplet for this dependency without addressing that.
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
    libswresample
)

# ---------------------------------------------------------------------------
# Model renderer shaders (data/shaders/*.hlsl -> DXIL, via dxc). Canonical
# source is HLSL; dxc ships with the Windows SDK (already required for
# windows.h/CD-ROM IOCTLs elsewhere in this project), so no extra
# toolchain install should be needed on a normal VS2022 setup.
#
# NOTE: this compilation step itself is untested - no dxc available in
# the environment these shaders were originally authored/verified in.
# The shaders were instead verified via an equivalent GLSL pair compiled
# to SPIR-V (same SDL_GPU pipeline C++ code, different shader compiler) -
# see ModelRenderer.cpp's comments. If dxc rejects this HLSL, that's a
# real gap to fix, not a build-config issue.
# ---------------------------------------------------------------------------
if (WIN32)
    find_program(DXC_EXECUTABLE dxc)
    if (NOT DXC_EXECUTABLE)
        file(GLOB _dxc_candidates "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe")
        if (_dxc_candidates)
            list(SORT _dxc_candidates)
            list(REVERSE _dxc_candidates) # newest SDK version first
            list(GET _dxc_candidates 0 DXC_EXECUTABLE)
        endif()
    endif()

    if (NOT DXC_EXECUTABLE)
        message(WARNING "dxc.exe (DirectXShaderCompiler) not found - model rendering shaders won't be compiled. "
                         "Install the Windows SDK (ships with VS2022's Desktop C++ workload) or add dxc to PATH.")
    else()
        message(STATUS "Using dxc: ${DXC_EXECUTABLE}")
        set(SHADER_DIR "${CMAKE_SOURCE_DIR}/data/shaders")

        add_custom_command(
            OUTPUT "${SHADER_DIR}/model.vert.dxil"
            COMMAND ${DXC_EXECUTABLE} -T vs_6_0 -E main -Fo "${SHADER_DIR}/model.vert.dxil" "${SHADER_DIR}/model.vert.hlsl"
            DEPENDS "${SHADER_DIR}/model.vert.hlsl"
            COMMENT "Compiling model.vert.hlsl -> DXIL"
        )
        add_custom_command(
            OUTPUT "${SHADER_DIR}/model.frag.dxil"
            COMMAND ${DXC_EXECUTABLE} -T ps_6_0 -E main -Fo "${SHADER_DIR}/model.frag.dxil" "${SHADER_DIR}/model.frag.hlsl"
            DEPENDS "${SHADER_DIR}/model.frag.hlsl"
            COMMENT "Compiling model.frag.hlsl -> DXIL"
        )
        add_custom_target(CompileModelShaders ALL
            DEPENDS "${SHADER_DIR}/model.vert.dxil" "${SHADER_DIR}/model.frag.dxil")
    endif()
endif()

# ---------------------------------------------------------------------------
# The same shaders for Vulkan and Metal: data/shaders/*.glsl (the HLSL's GLSL
# twins) -> SPIR-V (glslangValidator) -> MSL (spirv-cross). The outputs are
# committed next to the DXIL, so a build without these tools still has them;
# with the tools (apt glslang-tools spirv-cross, brew glslang spirv-cross)
# they are rebuilt whenever a .glsl changes.
# ---------------------------------------------------------------------------
find_program(GLSLANG_VALIDATOR glslangValidator)
find_program(SPIRV_CROSS spirv-cross)
if (GLSLANG_VALIDATOR AND SPIRV_CROSS)
    set(SHADER_DIR "${CMAKE_SOURCE_DIR}/data/shaders")
    set(_alt_portable_shaders "")
    foreach (_stage vert frag)
        add_custom_command(
            OUTPUT "${SHADER_DIR}/model.${_stage}.spv" "${SHADER_DIR}/model.${_stage}.msl"
            COMMAND ${GLSLANG_VALIDATOR} -V -S ${_stage} -o "${SHADER_DIR}/model.${_stage}.spv" "${SHADER_DIR}/model.${_stage}.glsl"
            COMMAND ${SPIRV_CROSS} "${SHADER_DIR}/model.${_stage}.spv" --msl --msl-version 20000 --output "${SHADER_DIR}/model.${_stage}.msl"
            DEPENDS "${SHADER_DIR}/model.${_stage}.glsl"
            COMMENT "Compiling model.${_stage}.glsl -> SPIR-V and MSL"
        )
        list(APPEND _alt_portable_shaders "${SHADER_DIR}/model.${_stage}.spv" "${SHADER_DIR}/model.${_stage}.msl")
    endforeach()
    add_custom_target(CompilePortableShaders ALL DEPENDS ${_alt_portable_shaders})
else()
    message(STATUS "glslangValidator or spirv-cross not found: using the committed SPIR-V and MSL shaders")
endif()

# ---------------------------------------------------------------------------
# Executable icon. TRILOGY.ICO used to be copied from the disc during
# install and picked up via GameLocator/DiscLocator's own marker-file
# checks (see those files) - the disc's own copy is low quality (8-bit).
# Edward has a higher-quality one (made from the egg sprites) in his
# ALTViewer toolkit, kept as a sibling folder next to ALTEngine rather
# than inside this repo. Embedded directly into the executable at build
# time instead of shipped as a loose file (Edward, 2026).
# ---------------------------------------------------------------------------
set(ALTENGINE_ICON_RC "")
if (WIN32)
    set(ALTENGINE_ICON_PATH "${CMAKE_SOURCE_DIR}/../ALTViewer/TRILOGY.ICO")
    if (EXISTS "${ALTENGINE_ICON_PATH}")
        configure_file("${CMAKE_SOURCE_DIR}/src/AppIcon.rc.in" "${CMAKE_BINARY_DIR}/AppIcon.rc" @ONLY)
        set(ALTENGINE_ICON_RC "${CMAKE_BINARY_DIR}/AppIcon.rc")
        message(STATUS "Using icon: ${ALTENGINE_ICON_PATH}")
    else()
        message(WARNING "TRILOGY.ICO not found at ${ALTENGINE_ICON_PATH} - executable will have no icon. "
                         "Expected as a sibling ALTViewer folder next to ALTEngine.")
    endif()
endif()

# ---------------------------------------------------------------------------
# Application manifest - requests asInvoker execution level so Windows
# doesn't apply its "installer detection heuristics" to an unmanifested
# executable (silent UAC elevation prompts / file-write virtualization
# on every launch, not just the first). Edward, 2026: same asInvoker fix
# already used for WoWRevived's UAC virtualization issue. Unlike the
# icon block above, this is unconditional on Windows - both files live
# together in src/ (app.manifest is a fixed, versioned part of the
# repo, not sourced from an external toolkit path), so there's nothing
# to conditionally check for existing.
# ---------------------------------------------------------------------------
set(ALTENGINE_MANIFEST_RC "")
if (WIN32)
    set(ALTENGINE_MANIFEST_RC "${CMAKE_SOURCE_DIR}/src/AppManifest.rc")
endif()

# ---------------------------------------------------------------------------
# ALTEngine - the port executable. Currently: MU/TH/UR 9000 boot sequence
# (locate game files), patch routines, LEGAL splash, AVI intro playback.
# Engine/renderer proper comes later.
# ---------------------------------------------------------------------------
add_executable(ALTEngine
    ${ALTENGINE_ICON_RC}
    ${ALTENGINE_MANIFEST_RC}
    src/main.cpp
    src/Bootstrap/DirectoryBrowser.cpp
    src/Bootstrap/GameLocator.cpp
    src/Bootstrap/Config.cpp
    src/Bootstrap/ImageDisplay.cpp
    src/Bootstrap/AppWindow.cpp
    src/Bootstrap/DiscLocator.cpp
    src/Bootstrap/InstallPipeline.cpp
    src/Video/VideoPlayer.cpp
    src/Video/OverrideVideo.cpp
    src/Audio/SfxPlayer.cpp
    src/Audio/MusicPlayer.cpp
    src/Menu/MenuTree.cpp
    src/Menu/MenuNavigation.cpp
    src/Menu/MenuController.cpp
    src/Screens/MissionBriefingScreen.cpp
    src/Screens/PauseMenuTree.cpp
    src/Screens/PauseMenuScreen.cpp
    src/Screens/GameplayScreen.cpp
    src/Screens/MenuBackground.cpp
    src/Screens/SaveSlotScreen.cpp
    src/Screens/EndLevelPromptScreen.cpp
    src/Screens/MissionAssessmentScreen.cpp
    src/Screens/TextFieldEditor.cpp
    src/Screens/MultiplayerScreens.cpp
    src/Renderer/Mat4.h
    src/Renderer/ModelRenderer.cpp
    src/Renderer/ModelPreview.cpp
    # These three draw with SDL_Renderer, so they belong to the executable
    # rather than ALTFormats - the data layer does not link SDL, and only
    # built before because vcpkg puts SDL on every target's include path.
    src/Renderer/WeaponView.cpp
    src/Renderer/HudRenderer.cpp
    src/Renderer/Minimap.cpp
)

# MSVC's linker auto-generates and embeds its own default manifest
# (also at resource ID 1) unless told not to - collides with our own
# AppManifest.rc, which embeds app.manifest at that same ID, causing
# "duplicate resource. type:MANIFEST, name:1" (CVT1100) and a link
# failure (LNK1123) (Edward, 2026). MinGW/windres builds don't have
# this auto-generation behaviour, so this is MSVC-specific.
if (MSVC)
    target_link_options(ALTEngine PRIVATE "/MANIFEST:NO")
endif()

target_include_directories(ALTEngine PRIVATE src)
target_compile_definitions(ALTEngine PRIVATE ALTENGINE_VERSION="${ALTENGINE_VERSION}")
target_link_libraries(ALTEngine PRIVATE ALTGame ALTFormats)
target_link_libraries(ALTEngine PRIVATE SDL3::SDL3)
target_link_libraries(ALTEngine PRIVATE PkgConfig::FFMPEG)
target_link_libraries(ALTEngine PRIVATE alt_warnings)
target_link_libraries(ALTEngine PRIVATE nlohmann_json::nlohmann_json)

if (WIN32 AND DXC_EXECUTABLE)
    add_dependencies(ALTEngine CompileModelShaders)
endif()
if (TARGET CompilePortableShaders)
    add_dependencies(ALTEngine CompilePortableShaders)
endif()

set_target_properties(ALTEngine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:ALTEngine>"
)

# No elevation request here. The embedded app.manifest (AppManifest.rc)
# asks for asInvoker, and /MANIFEST:NO above means a linker-level
# /MANIFESTUAC would be ignored anyway - so the manifest is the one place
# the execution level is decided.

# Ship data/ (Patches.json etc.) next to the built executable, so it can be
# found at runtime relative to the exe regardless of build type/generator.
add_custom_command(TARGET ALTEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:ALTEngine>/data"
)

# LANGUAGE/ (localisation packs) lives in the source tree as the source
# of truth, deployed to GameData/CD/LANGUAGE at build time - alongside
# where the original disc's own language-specific files end up (see
# Bootstrap/Strings.h's SetLanguagePackDirectory). GameData sits next to
# the executable, matching InstallFromDisc's own destination (Edward,
# 2026).
add_custom_command(TARGET ALTEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/LANGUAGE"
        "$<TARGET_FILE_DIR:ALTEngine>/GameData/CD/LANGUAGE"
)

# OVERRIDE/ holds the override guide and the light manifest reference. Same
# treatment as LANGUAGE: source tree is the source of truth, deployed to
# GameData/CD/OVERRIDE so every build and every install carries the guide in
# the folder it describes, and any example overrides sit where the engine
# already looks for them (Edward, 2026).
add_custom_command(TARGET ALTEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/OVERRIDE"
        "$<TARGET_FILE_DIR:ALTEngine>/GameData/CD/OVERRIDE"
)

# CUSTOM/ holds artwork this port adds that is not in the original game - the
# minimap border, and whatever follows. Deployed INTO the override tree at
# GameData/CD/OVERRIDE/CUSTOM, at the same time as LANGUAGE and OVERRIDE, so a
# fresh build has it without a manual copy (Edward, 2026).
add_custom_command(TARGET ALTEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/CUSTOM"
        "$<TARGET_FILE_DIR:ALTEngine>/GameData/CD/OVERRIDE/CUSTOM"
)

# ---------------------------------------------------------------------------
# Third-party licenses. Every dependency we link against gets its license
# text committed under third_party_licenses/<name>/ and shipped alongside
# the binary - required by SDL3's zlib license, nlohmann-json's MIT
# license, and FFmpeg's LGPL v2.1+ (see the licensing note above the
# pkg_check_modules call - default features only, dynamic linking).
#
# POST_BUILD covers ordinary F5 dev builds (VS2022 users mostly won't run
# `cmake --install` day to day); install() covers actual packaging.
# ---------------------------------------------------------------------------
add_custom_command(TARGET ALTEngine POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/third_party_licenses"
        "$<TARGET_FILE_DIR:ALTEngine>/licenses"
)

install(TARGETS ALTEngine RUNTIME DESTINATION bin)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/data/" DESTINATION bin/data)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/OVERRIDE/" DESTINATION bin/GameData/CD/OVERRIDE)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/CUSTOM/" DESTINATION bin/GameData/CD/OVERRIDE/CUSTOM)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/LANGUAGE/" DESTINATION bin/GameData/CD/LANGUAGE)
install(FILES "${CMAKE_SOURCE_DIR}/third_party_licenses/SDL3/LICENSE.txt"
        DESTINATION bin/licenses/SDL3)
install(FILES "${CMAKE_SOURCE_DIR}/third_party_licenses/nlohmann-json/LICENSE.txt"
        DESTINATION bin/licenses/nlohmann-json)
install(FILES "${CMAKE_SOURCE_DIR}/third_party_licenses/ffmpeg/LICENSE.txt"
              "${CMAKE_SOURCE_DIR}/third_party_licenses/ffmpeg/LICENSE.md"
        DESTINATION bin/licenses/ffmpeg)
install(FILES "${CMAKE_SOURCE_DIR}/third_party_licenses/stb/LICENSE.txt"
        DESTINATION bin/licenses/stb)
