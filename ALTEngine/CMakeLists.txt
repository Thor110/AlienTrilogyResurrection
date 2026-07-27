cmake_minimum_required(VERSION 3.20)
project(ALTEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# ALTFormats - pure data layer (BND/B16/palette/model/level parsing, plus
# binary patching). No windowing, no rendering, no platform-specific calls
# beyond filesystem.
# ---------------------------------------------------------------------------
find_package(nlohmann_json CONFIG REQUIRED)

add_library(ALTFormats STATIC
    src/Formats/ModelIndices.h
    src/Formats/BinaryUtility.cpp
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
    src/Formats/RenderMesh.cpp
    src/Formats/LevelLoader.cpp
    src/Formats/BinaryReadLE.h
    src/Formats/MissionText.cpp
    src/Formats/Obj3DTexture.cpp
    src/Formats/LevelTransparency.cpp
    src/Formats/SpriteFrameDecompressor.cpp
    src/Formats/SpriteFrameDimensions.cpp
    src/Formats/SpriteFrameLoader.cpp
)
target_include_directories(ALTFormats PUBLIC src)
target_link_libraries(ALTFormats PRIVATE nlohmann_json::nlohmann_json)

find_package(SDL3 CONFIG REQUIRED)

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
target_link_libraries(ALTEngine PRIVATE ALTFormats)
target_link_libraries(ALTEngine PRIVATE SDL3::SDL3)
target_link_libraries(ALTEngine PRIVATE PkgConfig::FFMPEG)

if (WIN32 AND DXC_EXECUTABLE)
    add_dependencies(ALTEngine CompileModelShaders)
endif()

set_target_properties(ALTEngine PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:ALTEngine>"
)

# Many installs (Program Files, Program Files (x86), Collection Chamber's
# defaults among them) are UAC-protected: reads succeed but writes fail
# silently without elevation. Rather than relying on VS/the user happening
# to run elevated, ask Windows for it directly.
if (MSVC)
    target_link_options(ALTEngine PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
endif()

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
