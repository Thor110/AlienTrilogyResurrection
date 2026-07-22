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
    src/Formats/placeholder.cpp
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
)
target_include_directories(ALTFormats PUBLIC src)
target_link_libraries(ALTFormats PRIVATE nlohmann_json::nlohmann_json)

find_package(SDL3 CONFIG REQUIRED)

# ---------------------------------------------------------------------------
# ALTEngine - the port executable. Currently: MU/TH/UR 9000 boot sequence
# (locate game files), then apply patch routines. Engine/renderer proper
# comes later.
# ---------------------------------------------------------------------------
add_executable(ALTEngine
    src/main.cpp
    src/Bootstrap/DirectoryBrowser.cpp
    src/Bootstrap/GameLocator.cpp
    src/Bootstrap/Config.cpp
    src/Bootstrap/ImageDisplay.cpp
)
target_include_directories(ALTEngine PRIVATE src)
target_link_libraries(ALTEngine PRIVATE ALTFormats)
target_link_libraries(ALTEngine PRIVATE SDL3::SDL3)

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

# ---------------------------------------------------------------------------
# Third-party licenses. Every dependency we link against gets its license
# text committed under third_party_licenses/<name>/ and shipped alongside
# the binary - required by (at minimum) SDL3's zlib license and
# nlohmann-json's MIT license, and by anything else added later (FFmpeg
# included - note FFmpeg's own license is LGPL or GPL depending on which
# optional components are enabled at build time, which has real copyleft
# implications for how it can be linked; sort that out explicitly before
# picking a build config for it, don't default to whatever vcpkg installs).
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
install(FILES "${CMAKE_SOURCE_DIR}/third_party_licenses/SDL3/LICENSE.txt"
        DESTINATION bin/licenses/SDL3)
install(FILES "${CMAKE_SOURCE_DIR}/third_party_licenses/nlohmann-json/LICENSE.txt"
        DESTINATION bin/licenses/nlohmann-json)
