# Alien Trilogy Resurrection

This project aims to create a modding toolkit and possibly more for Alien Trilogy.

- 1 : Install the game from : https://collectionchamber.blogspot.com/2017/05/alien-trilogy.html or from an original media source.
- 2 : Download and install the latest version of the toolkit from [the releases page](https://github.com/Thor110/AlienTrilogyResurrection/releases) then place it in the game directory. ( Alongside "Run.exe" or "TRILOGY.EXE" )
- 3 : Optional : Install any patches using the patch button in the tool, the details of all patches made to original game files are in the "Patch" folder of this repository.

# Notes

This optional step has been removed as it needs revising sometime in the future, so ignore it for now.

- # : Optional : Use the cleanup script and files provided in "Notes\repack-disc-comparison\CLEANUPSCRIPT" to delete 83.35MBs of unused files from the game. ( documentation and details on these files and more can be found in "Notes\repack-disc-comparison\readme.txt" )

# To Do List

- Engine Reimplementation
	- Gameplay Mechanics
	- Multiplayer
- Text Editor
	- UI text viewing .BIN files plaintext.
	- Editing and saving functionality.
- Model Viewer
	- M036 is an unused model and does not have a corresponding texture.
	- M039 is an unused model which is the same as the #41 the egg-husk.
	- M040 is the hibernation pod cover which does not have a texture, I believe this is likely coloured in code with a single colour.
- Graphics Viewer
	- Compressed images can not be replaced yet, until I implement a recompression algorithm that matches the original exactly.
- Level Viewer
	- Not really a level "viewer" per-se, currently it is just a testing tool for parsing level data, it can also export the level geometry as OBJ files.
	- May or may not extend it to level viewer and editor functionality one day.
	- Need to implement an export feature for the location of level objects, enemies, crates, pickups, switches, doors etc
- Patches
	- The texture scan has more candidates on L263LEV, L351LEV and L371LEV still to be reviewed.
	- The UVs for the Queens Egg Sack from the final level are now fixed (see L391LEV Fix 1).

# Road Map

The road map for this project.

- [❌ 0 : Engine Reimplementation](#engine-reimplementation) ( 25% Complete )
	- Original CD detection and selective asset extraction
	- Automated Redbook CD-DA audio ripping and pre-bundled playlist generation
	- Native image and video override architecture
	- Upgraded cutscene video playback pipeline (BSRGAN upscaled via local NCNN, batch-processed through FFmpeg)
	- Integrated main menu audio, functional layout drafts, options configs, resolution detection  and selection.
	- Graphics quality options smoothed and original rendering styles. (WIP)
	- Resolution scaling.
	- Modular language localization system.
- [❌ 1 : Text Editor](#text-editor) ( 50% Complete )
	- View text from the games missions and user interface.
	- Editing and saving functions not implemented yet.
	- Effectively a cancelled feature, the text is stored in .txt and .bin files as plain text. If you want to edit those, you don't need a toolkit for it.
- [✅ 2 : Model Viewer](#model-viewer) ( 99% Complete )
	- Can extract models from the three known model files.
	- Does not currently extract the associated textures alongside them.
- [✅ 3 : Graphics Viewer](#graphics-viewer) ( 99% Complete )
	- Palette detection implemented.
	- Toggle palette export or transparency for viewing and exporting.
	- View, export and replace textures, replacing compressed images is not supported yet. ( .B16 files, weapons and enemies. )
	- Automatic backup of the original file by default.
- [✅ 4 : Palette Editor](#palette-editor) ( 100% Complete )
	- Palette editor and image preview.
	- View, export, import and edit palettes for all types of images. ( Embedded, External & Compressed )
	- Visual feedback showing unused colours across all sections and frames, except for embedded palettes where each section has its own colour palette.
	- Automatic backup of the original file by default.
- [✅ 5 : Sound Effects Viewer](#sound-effects-viewer) ( 100% Complete )
	- .RAW audio files can be played, replaced and converted to and from .WAV files.
	- Waveform preview for selected sound files.
	- Link to the music directory, only available with the repack.
	- Automatic backup of the original file by default.
- [❌ 6 : Level Viewer](#level-viewer) ( 99% Complete )
	- Map files detected and listed.
	- Basic details parsed and listed.
	- Export level geometry as OBJ files.
	- Export levels with debug and unknown byte flags displayed visually.
	- Export door and lift geometry as OBJ files.
	- Does not currently extract the associated textures alongside them.
	- Provides a list based interface for looking through the parsed objects within each level.
	- Has a preview of the levels map generated using the collision blocks parsed for the level.
- [❌ 7 : Patches](#patches) ( ??% Complete )
	- [Patched an issue on L906LEV where a vertex had incorrect coordinates.](#L906LEV-Multiplayer-Map-7-Fix-1)
	- [Patched an issue on L906LEV where four faces were lacking the double sided transparency flag.](#L906LEV-Multiplayer-Map-7-Fix-2)
	- [Patched an issue on L906LEV where one railing was lacking the double sided transparency flag.](#L906LEV-Multiplayer-Map-7-Fix-3)
	- [Patched an issue on L906LEV where one railing was lacking the double sided transparency flag.](#L906LEV-Multiplayer-Map-7-Fix-4)
	- [Patched an issue on L906LEV where 8 different bridge sections had incorrect textures and UV coordinates.](#L906LEV-Multiplayer-Map-7-Fix-5)
	- [Patched an issue on L906LEV where doors had incorrectly flipped textures.](#L906LEV-Multiplayer-Map-7-Fix-6)
	- [Patched an issue on L905LEV where 2 textures were flipped when they shouldn't have been.](#L905LEV-Multiplayer-Map-6-Fix-1)
	- [Patched an issue on L905LEV where lots of different textures were flipped when they shouldn't have been.](#L905LEV-Multiplayer-Map-6-Fix-2)
	- [Patched an issue on L903LEV where some crates had incorrect textures.](#L903LEV-Multiplayer-Map-4-Fix-1)
	- [Patched an issue on L901LEV where player storage was located within the level boundaries.](#L901LEV-Multiplayer-Map-2-Fix-1)
	- [Patched an issue on L900LEV where four crates had textures which were sideways.](#L900LEV-Multiplayer-Map-1-Fix-1)
	- [Patched an issue on L900LEV where multiple texture flags were incorrectly set.](#L900LEV-Multiplayer-Map-1-Fix-2)
	- [Patched an issue on L900LEV where a vertex in the first lift had the wrong Z coordinate.](#L900LEV-Multiplayer-Map-1-Fix-3)
	- [Patched an issue on L900LEV where a face was assigned the wrong texture index.](#L900LEV-Multiplayer-Map-1-Fix-4)
	- [Patched an issue on L900LEV where the starting lift panel had its texture flipped.](#L900LEV-Multiplayer-Map-1-Fix-5)
	- [Patched an issue on L111LEV where four crates had textures which were sideways.](#L111LEV-Fix-1)
	- [Patched an issue on L111LEV where multiple texture flags were incorrectly set.](#L111LEV-Fix-2)
	- [Patched an issue on L111LEV where a vertex in the first lift had the wrong Z coordinate.](#L111LEV-Fix-3)
	- [Patched an issue on L111LEV where a face was assigned the wrong texture index.](#L111LEV-Fix-4)
	- [Patched an issue on L111LEV where the starting lift panel had its texture flipped.](#L111LEV-Fix-5)
- [Patched an issue on L112LEV where a corner under the hanging sign was covered by two sets of faces.](#L112LEV-Fix-1)
	- [Patched an issue on L141LEV where some crates had incorrect textures.](#L141LEV-Fix-1)
	- [Patched an issue on L161LEV where one face had an incorrect texture.](#L161LEV-Fix-1)
	- [Patched an issue on L161LEV where one face on a door had an incorrect texture.](#L161LEV-Fix-2)
	- [Patched an issue on L162LEV where one face on the starting door had an incorrect texture.](#L162LEV-Fix-1)
	- [Patched an issue on L371LEV where a secret area was inaccessible.](#L371LEV-Fix-1)
	- [Patched an issue on L371LEV where two faces had incorrect textures.](#L371LEV-Fix-2)
	- [Patched an issue on L371LEV where one of the textures unused tiles showed up at the edges of some faces.](#L371LEV-Fix-3)
	- [Patched an issue on L371LEV where the corner triangles of 14 wall recesses had their textures turned (28 faces).](#L371LEV-Fix-8)
	- [Plus the texture scan fixes, the walk-through pillars on L903LEV and the duplicate door and lift model removal: see the full list.](#patches)
- [✅ 8 : Easter Eggs](#easter-eggs) ( ??% Complete )
	- [Grade 33 Steel](#easter-egg-1)
	- [End Times](#easter-egg-2)
	- [Cheats](#easter-egg-3)

And possibly more to come.

Discord : https://discord.gg/Mk2YUuPmdU

## Documentation

Special thanks to Bobblen147 who created this repository : https://github.com/Bobblen147/atril_geom_extract

They also pointed me to the partial file format documentation and the texture decompression source code from Lex Safanov, the links to which are also in their repository on the great preserver archive.org which will save me endless amounts of time manually decoding the filetypes.

Also a big thanks to Lex Safanov for posting their source code for decompression of .B16 files, I used this as reference when reimplementing it for this project.

A thanks to bambamalicious for discovering the fix for the inaccessible secret on L371LEV. It sets the lift's trigger threshold to 1, which turns out to be the value the level's own scripts were built for: the lift's only other trigger is a switch at the bottom, inside the secret, so with the original value the lift could never be taken down (see Patch/L371LEV.txt).

And thanks to Kaiser for information regarding animated textures and some other previously unknown bytes.

Thanks to onelastpilot for the suggestion to add a timer to the levels for recording best times, as well as requesting inverted mouse look.

Thanks to CybeRingu for the suggestion to add co-operative to the multiplayer mode.

## Engine Reimplementation

Engine reimplementation is currently a work in progress. What changed in each version: [PATCH-NOTES.md](PATCH-NOTES.md).

MU/TH/UR Computer System Interface

This is used to locate the disc or game installation, copy the necessary files, rip the music tracks, patch game files and is only shown on the first launch of the game.

<div align="center">
  <img src="Images/ALTEngine/muthur.png" alt="MU/TH/UR Computer System">
</div>

Legal Screen - Upscaled

<div align="center">
  <img src="Images/ALTEngine/legal.png" alt="Legal Screen">
</div>

Probe Logo Video - Upscaled

<div align="center">
  <img src="Images/ALTEngine/probe.png" alt="Probe Logo Video">
</div>

Main Menu - Upscaled Background

<div align="center">
  <img src="Images/ALTEngine/main-menu.png" alt="Main Menu">
</div>

Options Menu - Upscaled Background

<div align="center">
  <img src="Images/ALTEngine/options-menu.png" alt="Options Menu">
</div>

Multiplayer Menu - Upscaled Background

<div align="center">
  <img src="Images/ALTEngine/multiplayer-menu.png" alt="Multiplayer Menu">
</div>

Pause Menu

<div align="center">
  <img src="Images/ALTEngine/pause-menu.png" alt="Pause Menu">
</div>

Briefing Menu - Upscaled Background

<div align="center">
  <img src="Images/ALTEngine/briefing-menu.png" alt="Briefing Menu">
</div>

Resolution Scaling

<div align="center">
  <img src="Images/ALTEngine/resolution-scaling.png" alt="Resolution Scaling">
</div>

Localization System - Modular Language Packs

<div align="center">
  <img src="Images/ALTEngine/localisation-system.png" alt="Localization System">
</div>

First Level

<div align="center">
  <img src="Images/ALTEngine/first-level.png" alt="First Level">
</div>

### Building the engine

The engine builds on Windows, Linux and macOS; every push is built and tested on all three by `.github/workflows/build.yml`, which keeps each build as a downloadable artifact.

Windows (Visual Studio 2022 with vcpkg, `VCPKG_ROOT` set):

```
cd ALTEngine
cmake --preset windows-release
cmake --build out/build/windows-release
```

To gather a release from that build, run `tools\make_release.bat` from the `ALTEngine` folder (`/zip` also packs it). It copies only what the game needs into `out\build\ALTEngine-v<version>`: the executable and its DLLs, the licences, `data`, and the language, override and custom folders. The version number is in `ALTEngine\VERSION`.

Linux (Ubuntu 24.04 or similar; SDL3 is built from source the first time, as Ubuntu has no package for it):

```
sudo apt-get install ninja-build pkg-config nlohmann-json3-dev glslang-tools spirv-cross \
  libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev \
  libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev libxss-dev \
  libxtst-dev libxkbcommon-dev libwayland-dev wayland-protocols libegl1-mesa-dev \
  libgl1-mesa-dev libasound2-dev libpulse-dev libudev-dev libdbus-1-dev
cd ALTEngine
cmake -S . -B out/build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/release
```

macOS (Homebrew):

```
brew install ninja pkg-config sdl3 ffmpeg nlohmann-json glslang spirv-cross
cd ALTEngine
cmake -S . -B out/build/release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build out/build/release
```

The tests run with `ctest --test-dir <build folder>`. A mounted disc under `/Volumes`, `/media` or `/mnt` is found automatically, and the music is ripped from it on all three systems: on Linux straight from the drive, on macOS from the audio tracks macOS mounts itself (or the drive if those aren't there).

The build also makes `ALTLevelScan`, which checks every level for faces whose texture is turned or mirrored against the faces next to it. Run it from the `ALTEngine` folder as `ALTLevelScan <CD folder>` (the folder holding `SECT11`, `SECT12` and so on). It writes `LevelUvScan.json`, which lists the suspect faces, strongest first, with a `faceUvRotations` entry for each in Patches.json's form. Check each one in the viewer before copying it across. `--level 391` scans one level; run with no arguments, it prints its options.

### Multiplayer: the port to open

> **IMPORTANT: the player hosting a game must allow UDP port 27960 in.**
>
> - **Same network (LAN):** allow ALTEngine through the firewall when Windows (or your system) asks the first time you host. Without that, nobody can see or join the game.
> - **Over the internet:** the host must also forward **UDP port 27960** on their router to the hosting computer, and the others join by typing the host's public address on the ADDRESS line of Join Multiplayer Game (for example `203.0.113.5`, or `203.0.113.5:27960`). There is no NAT help yet, so without the forward the join times out.
> - **It is UDP, not TCP.** Forwarding TCP 27960 alone does nothing.
> - Only the host needs the port open; players joining need nothing opened.

Four players, as in the original, or up to eight: the host sets the player limit (4-8) on the Start Multiplayer Game screen. The host can also add bots (none, or 3 up to the limit less one, so there is always room for the host) and set their skill (1-5); a player joining a full game takes a bot's place. Multiplayer is the original's ten maps, L900-L909; "Start at level" 1-10 picks one. Each player picks a name and a look (Synthetic, Soldier, Guard or Handler) in Multiplayer Options, and F2-F9 send the chat messages set there. Every player needs the same version of ALTEngine: an older or newer one is turned away.

The host chooses the **Game Mode** below Name Of Game:

- **Deathmatch** (the default) is the original's: the ten maps above, players against each other, with bots if wanted.
- **Co-operative** is ALTEngine's own: the players go through the story missions together, starting on any of the 35 ("Start at mission"). There is no friendly fire and no bots (those rows are greyed out), with the same player limit as deathmatch. A player who reaches the end of a level waits there, out of sight and out of harm, until every player in the game has reached it; then the next mission loads for everyone. Dying puts you back at the level's start. The level is shared: the host runs the aliens, which go after whichever player is nearest, and every player sees and fights the same ones; doors, lifts, switches, lights, pickups and destroyed crates are the same for everyone, and a player joining late catches up with all of it. The motion tracker works in co-operative games, pulse and all. Grenades and seismic charges are seen by everyone. Each mission keeps its briefing and its assessment (marked CO-OPERATIVE); a player coming into a level the others are already playing sees a short syncing screen, in the briefing's style, while the level catches up.

**The host's rules.** Every Modern option that changes how the game plays (Keep Items, Player Jumping, No Hit Immunity, Free Look, Render Distance, Live Minimap, Faster Xenomorphs, Speed Compensation, Pause Weapon Select) and the weapon pacing are the host's choice, for every player in the game. Enable Cheats, Save Anywhere and the testing aids that change play (Disable Enemy Spawn, Ghost Mode, Disable Resupply Timer) are off in a network game. Each player's own settings are untouched and come back afterwards.

## Alien Trilogy Viewer

The main program window.

<div align="center">
  <img src="Images/altviewer.png" alt="Alien Trilogy Viewer">
</div>

## Text Editor

Edit text in the game, intended for localisation efforts.

<div align="center">
  <img src="Images/texteditor.png" alt="Text Editor">
</div>

## Model Viewer

Extract models from the games files.

<div align="center">
  <img src="Images/modelviewer.png" alt="Model Viewer">
</div>

A showcase of some of the models from the games files.

<div align="center">
  <img src="Images/showcase.png" alt="Model Showcase">
</div>

## Graphics Viewer

View, extract and replace textures from the game.

<div align="center">
  <img src="Images/graphicsviewer.png" alt="Graphics Viewer">
</div>

View, extract and replace animation frames in the game. ( Note : Replacing animation frames is not supported yet )

<div align="center">
  <img src="Images/compressedimages.png" alt="Compressed Images">
</div>

Replace textures. ( Example : Barrel texture used for the Crate )

<div align="center">
  <img src="Images/texturereplacement.png" alt="Replace Textures">
</div>

## Palette Editor

Preview, edit, save, import and export palettes while previewing the image it belongs to.

<div align="center">
  <img src="Images/paletteeditor.png" alt="Palette Editor">
</div>

This image shows a compressed file palette that has been replaced.

<div align="center">
  <img src="Images/compressedpalettereplacement.png" alt="Compressed File Palette Replacement">
</div>

This image shows an embedded palette that has been replaced.

<div align="center">
  <img src="Images/embeddedpalettereplacement.png" alt="Embedded Palette Replacement">
</div>

## Sound Effects Viewer

Listen to, extract, replace or restore audio files from backups.

<div align="center">
  <img src="Images/soundeffects.png" alt="Sound Effects Viewer">
</div>

## Level Viewer

View level data and export level models as OBJ files.

<div align="center">
  <img src="Images/levelviewer.png" alt="Level Viewer">
</div>

Preview of L111LEV exported as an OBJ file.

<div align="center">
  <img src="Images/levelexported.png" alt="L111LEV Exported">
</div>

A view of L112LEV from within Blender.

<div align="center">
  <img src="Images/blender.png" alt="L112LEV Blender Preview">
</div>

A preview of the door D000 from L111LEV.

<div align="center">
  <img src="Images/doorobject.png" alt="L111LEV Door Preview">
</div>

A preview of the lift L000 from L232LEV.

<div align="center">
  <img src="Images/liftobject.png" alt="L232LEV Lift Preview">
</div>

## Patches

I am also looking at making patches for any issues I find in the original game.

Fixes marked "texture scan" were found by ALTLevelScan, which looks for faces whose texture does not run on from the faces around them. Each one was rendered before and after and kept only after review; the scan number (for example 321-1) identifies each one for screenshots. The exact bytes for every fix are listed in the Patch folder, and both ALTEngine and ALTViewer apply them.

- [Patched an issue on L909LEV where one face had its texture rotated (texture scan 909-14).](#L909LEV-Multiplayer-Map-10-Fix-1)
- [Patched an issue on L909LEV where one face had its texture rotated (texture scan 909-15).](#L909LEV-Multiplayer-Map-10-Fix-2)
- [Patched an issue on L909LEV where one face had its texture flipped (texture scan 909-19).](#L909LEV-Multiplayer-Map-10-Fix-3)
- [Patched an issue on L909LEV where one face had its texture rotated (texture scan 909-20).](#L909LEV-Multiplayer-Map-10-Fix-4)
- [Patched an issue on L909LEV where one face had its texture flipped and rotated (texture scan 909-23).](#L909LEV-Multiplayer-Map-10-Fix-5)
- [Patched an issue on L909LEV where the start room could not be left (temporary, for testing).](#L909LEV-Multiplayer-Map-10-Fix-6)
- [Patched an issue on L908LEV where one face had its texture rotated (texture scan 908-1).](#L908LEV-Multiplayer-Map-9-Fix-1)
- [Patched an issue on L908LEV where one face had its texture rotated (texture scan 908-12).](#L908LEV-Multiplayer-Map-9-Fix-2)
- [Patched an issue on L906LEV where a vertex had incorrect coordinates.](#L906LEV-Multiplayer-Map-7-Fix-1)
- [Patched an issue on L906LEV where four faces were lacking the double sided transparency flag.](#L906LEV-Multiplayer-Map-7-Fix-2)
- [Patched an issue on L906LEV where one railing was lacking the double sided transparency flag.](#L906LEV-Multiplayer-Map-7-Fix-3)
- [Patched an issue on L906LEV where one railing was lacking the double sided transparency flag.](#L906LEV-Multiplayer-Map-7-Fix-4)
- [Patched an issue on L906LEV where 8 different bridge sections had incorrect textures and UV coordinates.](#L906LEV-Multiplayer-Map-7-Fix-5)
- [Patched an issue on L906LEV where doors had incorrectly flipped textures.](#L906LEV-Multiplayer-Map-7-Fix-6)
- [Patched an issue on L905LEV where 2 textures were flipped when they shouldn't have been.](#L905LEV-Multiplayer-Map-6-Fix-1)
- [Patched an issue on L905LEV where lots of different textures were flipped when they shouldn't have been.](#L905LEV-Multiplayer-Map-6-Fix-2)
- [Patched an issue on L905LEV where two faces had their textures flipped (texture scan 905-1).](#L905LEV-Multiplayer-Map-6-Fix-3)
- [Patched an issue on L903LEV where some crates had incorrect textures.](#L903LEV-Multiplayer-Map-4-Fix-1)
- [Patched an issue on L903LEV where two pillars could be walked through.](#L903LEV-Multiplayer-Map-4-Fix-2)
- [Patched an issue on L901LEV where player storage was located within the level boundaries.](#L901LEV-Multiplayer-Map-2-Fix-1)
- [Patched an issue on L900LEV where four crates had textures which were sideways.](#L900LEV-Multiplayer-Map-1-Fix-1)
- [Patched an issue on L900LEV where multiple texture flags were incorrectly set.](#L900LEV-Multiplayer-Map-1-Fix-2)
- [Patched an issue on L900LEV where a vertex on a lift had a misplaced coordinate.](#L900LEV-Multiplayer-Map-1-Fix-3)
- [Patched an issue on L900LEV where a face was assigned the wrong texture index.](#L900LEV-Multiplayer-Map-1-Fix-4)
- [Patched an issue on L900LEV where the starting lift panel had its texture flipped.](#L900LEV-Multiplayer-Map-1-Fix-5)
- [Patched an issue on L111LEV where four crates had textures which were sideways.](#L111LEV-Fix-1)
- [Patched an issue on L111LEV where multiple texture flags were incorrectly set.](#L111LEV-Fix-2)
- [Patched an issue on L111LEV where a vertex on a lift had a misplaced coordinate.](#L111LEV-Fix-3)
- [Patched an issue on L111LEV where a face was assigned the wrong texture index.](#L111LEV-Fix-4)
- [Patched an issue on L111LEV where the starting lift panel had its texture flipped.](#L111LEV-Fix-5)
- [Patched an issue on L112LEV where a corner under the hanging sign was covered by two sets of faces.](#L112LEV-Fix-1)
- [Patched an issue on L141LEV where some crates had incorrect textures.](#L141LEV-Fix-1)
- [Patched an issue on L154LEV where one face had its texture flipped and rotated (texture scan 154-1).](#L154LEV-Fix-1)
- [Patched an issue on L161LEV where one face had an incorrect texture.](#L161LEV-Fix-1)
- [Patched an issue on L161LEV where one face on a door had an incorrect texture.](#L161LEV-Fix-2)
- [Patched an issue on L161LEV where one face had its texture rotated (texture scan 161-1).](#L161LEV-Fix-3)
- [Patched an issue on L161LEV where one face had its texture flipped and rotated (texture scan 161-2).](#L161LEV-Fix-4)
- [Patched an issue on L162LEV where one face on the starting door had an incorrect texture.](#L162LEV-Fix-1)
- [Patched an issue on L211LEV where three faces had their textures flipped and rotated (texture scan 211-1).](#L211LEV-Fix-1)
- [Patched an issue on L263LEV where one face had its texture flipped and rotated (texture scan 263-3).](#L263LEV-Fix-1)
- [Patched an issue on L311LEV where one face had its texture rotated (texture scan 311-1).](#L311LEV-Fix-1)
- [Patched an issue on L311LEV where one face had its texture rotated (texture scan 311-9).](#L311LEV-Fix-2)
- [Patched an issue on L311LEV where one face had its texture rotated (texture scan 311-12).](#L311LEV-Fix-3)
- [Patched an issue on L321LEV where three faces had their textures rotated (texture scan 321-1).](#L321LEV-Fix-1)
- [Patched an issue on L321LEV where three faces had their textures flipped and rotated (texture scan 321-2).](#L321LEV-Fix-2)
- [Patched an issue on L321LEV where three faces had their textures rotated (texture scan 321-3).](#L321LEV-Fix-3)
- [Patched an issue on L321LEV where three faces had their textures flipped and rotated (texture scan 321-4).](#L321LEV-Fix-4)
- [Patched an issue on L321LEV where one face had its texture rotated (texture scan 321-10).](#L321LEV-Fix-5)
- [Patched an issue on L321LEV where one face had its texture rotated (texture scan 321-11).](#L321LEV-Fix-6)
- [Patched an issue on L321LEV where one face had its texture rotated (texture scan 321-12).](#L321LEV-Fix-7)
- [Patched an issue on L321LEV where one face had its texture rotated (texture scan 321-13).](#L321LEV-Fix-8)
- [Patched an issue on L321LEV where one face had its texture flipped (texture scan 321-14).](#L321LEV-Fix-9)
- [Patched an issue on L321LEV where one face had its texture flipped (texture scan 321-15).](#L321LEV-Fix-10)
- [Patched an issue on L321LEV where one face had its texture flipped (texture scan 321-16).](#L321LEV-Fix-11)
- [Patched an issue on L322LEV where one face had its texture flipped (texture scan 322-2).](#L322LEV-Fix-1)
- [Patched an issue on L322LEV where one face had its texture flipped (texture scan 322-4).](#L322LEV-Fix-2)
- [Patched an issue on L322LEV where one face had its texture flipped (texture scan 322-5).](#L322LEV-Fix-3)
- [Patched an issue on L322LEV where one face had its texture flipped (texture scan 322-8).](#L322LEV-Fix-4)
- [Patched an issue on L323LEV where one face had its texture rotated (texture scan 323-2).](#L323LEV-Fix-1)
- [Patched an issue on L323LEV where one face had its texture rotated (texture scan 323-3).](#L323LEV-Fix-2)
- [Patched an issue on L323LEV where one face had its texture flipped (texture scan 323-4).](#L323LEV-Fix-3)
- [Patched an issue on L323LEV where one face had its texture flipped (texture scan 323-6).](#L323LEV-Fix-4)
- [Patched an issue on L323LEV where one face had its texture flipped (texture scan 323-9).](#L323LEV-Fix-5)
- [Patched an issue on L324LEV where one face had its texture rotated (texture scan 324-2).](#L324LEV-Fix-1)
- [Patched an issue on L324LEV where one face had its texture flipped (texture scan 324-3).](#L324LEV-Fix-2)
- [Patched an issue on L324LEV where one face had its texture flipped (texture scan 324-5).](#L324LEV-Fix-3)
- [Patched an issue on L324LEV where one face had its texture flipped (texture scan 324-8).](#L324LEV-Fix-4)
- [Patched an issue on L324LEV where one face had its texture flipped (texture scan 324-9).](#L324LEV-Fix-5)
- [Patched an issue on L325LEV where one face had its texture rotated (texture scan 325-2).](#L325LEV-Fix-1)
- [Patched an issue on L325LEV where one face had its texture flipped (texture scan 325-3).](#L325LEV-Fix-2)
- [Patched an issue on L325LEV where one face had its texture rotated (texture scan 325-6).](#L325LEV-Fix-3)
- [Patched an issue on L325LEV where one face had its texture rotated (texture scan 325-8).](#L325LEV-Fix-4)
- [Patched an issue on L331LEV where one face had its texture rotated (texture scan 331-2).](#L331LEV-Fix-1)
- [Patched an issue on L331LEV where one face had its texture rotated (texture scan 331-3).](#L331LEV-Fix-2)
- [Patched an issue on L331LEV where one face had its texture rotated (texture scan 331-4).](#L331LEV-Fix-3)
- [Patched an issue on L331LEV where one face had its texture flipped (texture scan 331-6).](#L331LEV-Fix-4)
- [Patched an issue on L331LEV where one face had its texture flipped (texture scan 331-7).](#L331LEV-Fix-5)
- [Patched an issue on L351LEV where the top row of the resin wall had two faces rotated and two flipped.](#L351LEV-Fix-1)
- [Patched an issue on L351LEV where two faces had their textures flipped and rotated (texture scan 351-3).](#L351LEV-Fix-2)
- [Patched an issue on L351LEV where one face had its texture rotated (texture scan 351-4).](#L351LEV-Fix-3)
- [Patched an issue on L351LEV where one face had its texture flipped and rotated (texture scan 351-5).](#L351LEV-Fix-4)
- [Patched an issue on L351LEV where one face had its texture flipped and rotated (texture scan 351-7).](#L351LEV-Fix-5)
- [Patched an issue on L351LEV where one face had its texture rotated (texture scan 351-9).](#L351LEV-Fix-6)
- [Patched an issue on L351LEV where one face had its texture flipped (texture scan 351-10).](#L351LEV-Fix-7)
- [Patched an issue on L351LEV where one face had its texture rotated (texture scan 351-11).](#L351LEV-Fix-8)
- [Patched an issue on L351LEV where one face had its texture rotated (texture scan 351-12).](#L351LEV-Fix-9)
- [Patched an issue on L351LEV where one face had its texture flipped and rotated (texture scan 351-13).](#L351LEV-Fix-10)
- [Patched an issue on L352LEV where one face had its texture rotated (texture scan 352-1).](#L352LEV-Fix-1)
- [Patched an issue on L352LEV where one face had its texture rotated (texture scan 352-2).](#L352LEV-Fix-2)
- [Patched an issue on L353LEV where two faces had their textures flipped (texture scan 353-1).](#L353LEV-Fix-1)
- [Patched an issue on L353LEV where one face had its texture rotated (texture scan 353-2).](#L353LEV-Fix-2)
- [Patched an issue on L353LEV where one face had its texture rotated (texture scan 353-3).](#L353LEV-Fix-3)
- [Patched an issue on L371LEV where a secret area was inaccessible.](#L371LEV-Fix-1)
- [Patched an issue on L371LEV where two faces had incorrect textures.](#L371LEV-Fix-2)
- [Patched an issue on L371LEV where one of the textures unused tiles showed up at the edges of some faces.](#L371LEV-Fix-3)
- [Patched an issue on L371LEV where eight faces had their textures flipped and rotated (texture scan 371-1).](#L371LEV-Fix-4)
- [Patched an issue on L371LEV where one face had its texture rotated (texture scan 371-3).](#L371LEV-Fix-5)
- [Patched an issue on L371LEV where one face had its texture rotated (texture scan 371-4).](#L371LEV-Fix-6)
- [Patched an issue on L371LEV where one face had its texture rotated (texture scan 371-5).](#L371LEV-Fix-7)
- [Patched an issue on L371LEV where the corner triangles of 14 wall recesses had their textures turned (28 faces).](#L371LEV-Fix-8)
- [Patched an issue on L381LEV where one face had its texture rotated (texture scan 381-1).](#L381LEV-Fix-1)
- [Patched an issue on L391LEV where 246 faces of the queen's egg sack had their texture turned.](#L391LEV-Fix-1)
- [Patched an issue on L391LEV where one face had its texture rotated (texture scan 391-1).](#L391LEV-Fix-2)
- [Patched an issue on L391LEV where one face had its texture rotated (texture scan 391-2).](#L391LEV-Fix-3)
- [Patched an issue on L391LEV where one face had its texture rotated (texture scan 391-3).](#L391LEV-Fix-4)
- [Removed duplicate and unused door and lift models from 21 levels.](#Duplicate-Door-And-Lift-Models)
- [Before and after screenshots of every patch, the Level Cleanup and the texture blanking, with the bytes trimmed and updated.](#Before-And-After-Screenshots)

## L909LEV Multiplayer Map 10 Fix 1

UV rotation corrected.

Found by texture scan 909-14. Face 3917 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L909LEV.txt

## L909LEV Multiplayer Map 10 Fix 2

UV rotation corrected.

Found by texture scan 909-15. Face 6022 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L909LEV.txt

## L909LEV Multiplayer Map 10 Fix 3

UV flipped.

Found by texture scan 909-19. Face 1631 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L909LEV.txt

## L909LEV Multiplayer Map 10 Fix 4

UV rotation corrected.

Found by texture scan 909-20. Face 9710 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L909LEV.txt

## L909LEV Multiplayer Map 10 Fix 5

UV flipped and rotation corrected.

Found by texture scan 909-23. Face 4541 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L909LEV.txt

## L909LEV Multiplayer Map 10 Fix 6

The start room could not be left.

The four cells just past the start room's doorway have attribute 1, a solid wall, though nothing is drawn there, so the corridor could be seen but not entered. They are set to open floor, as the same room is on L391.

Temporary, for testing: the room is most likely the multiplayer players' holding room, sealed on purpose (its lift carries developer graffiti such as "RIPLEY WOZ ERE" in textures that match nothing else in the level). This patch should be undone once multiplayer spawns are in.

Patch listed at Patch/L909LEV.txt

Applied by both ALTEngine and ALTViewer's patch button.

## L908LEV Multiplayer Map 9 Fix 1

UV rotation corrected.

Found by texture scan 908-1. Face 7758 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L908LEV.txt

## L908LEV Multiplayer Map 9 Fix 2

UV rotation corrected.

Found by texture scan 908-12. Face 11010 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L908LEV.txt

## L906LEV Multiplayer Map 7 Fix 1

Here is how the level was originally.

<div align="center">
  <img src="Images/L906LEV-unpatched.png" alt="L906LEV Original">
</div>

This would cause this to appear when close enough to the missing triangle.

<div align="center">
  <img src="Images/L906LEV-unpatched-tear.png" alt="L906LEV Original Close Up">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L906LEV-patched.png" alt="L906LEV Patched">
</div>

## L906LEV Multiplayer Map 7 Fix 2

Here is how the level was originally.

<div align="center">
  <img src="Images/L906LEV-rail.png" alt="L906LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L906LEV-railfix.png" alt="L906LEV Patched">
</div>

## L906LEV Multiplayer Map 7 Fix 3

Here is how the level was originally.

<div align="center">
  <img src="Images/L906LEV-missingrail.png" alt="L906LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L906LEV-missingrail-fixed.png" alt="L906LEV Patched">
</div>

## L906LEV Multiplayer Map 7 Fix 4

Here is how the level was originally.

<div align="center">
  <img src="Images/L906LEV-anotherrail.png" alt="L906LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L906LEV-anotherrail-fixed.png" alt="L906LEV Patched">
</div>

## L906LEV Multiplayer Map 7 Fix 5

Here is how the level was originally.

<div align="center">
  <img src="Images/L906LEV-bridge.png" alt="L906LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L906LEV-bridge-fixed.png" alt="L906LEV Patched">
</div>

This used to be fixed only when exporting the model, on top of a partial fix in the game files. All 53 bridge faces are now fixed in the level file itself: UV rotation corrected on 24, UV flipped and rotation corrected on 29. Applied by both ALTEngine and ALTViewer's patch button. The face list, with the old export remap for each, is in Patch/L906LEV.txt.

## L906LEV Multiplayer Map 7 Fix 6

Here is how the level was originally.

<div align="center">
  <img src="Images/L906-D000.png" alt="L906LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L906-D000-FIX.png" alt="L906LEV Patched">
</div>

## L905LEV Multiplayer Map 6 Fix 1

Here is how the level was originally.

<div align="center">
  <img src="Images/L905LEV-texture.png" alt="L905LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L905LEV-texture-fixed.png" alt="L905LEV Patched">
</div>

## L905LEV Multiplayer Map 6 Fix 2

Here is how the level was originally.

<div align="center">
  <img src="Images/L905-textured-pipe.png" alt="L905LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L905-textured-pipe-fixed.png" alt="L905LEV Patched">
</div>

## L905LEV Multiplayer Map 6 Fix 3

UV flipped.

Found by texture scan 905-1. Face 1673 and the face beside it at offset 0x23858 are corrected so the texture runs on from the faces around them.

Patch listed at Patch/L905LEV.txt

## L903LEV Multiplayer Map 4 Fix 1

[These fixes are the same as on L141LEV](#L141LEV-Fix-1)
	
This used to be fixed only when exporting the level model. It is now fixed in the level file, on the same faces at the same offsets as L141LEV. Applied by both ALTEngine and ALTViewer's patch button. Details in Patch/L903LEV.txt.

## L903LEV Multiplayer Map 4 Fix 2

L903LEV is built from L141LEV: the same grid and the same geometry. Four of its map cells lost their wall attribute along the way, so the two pillars at cells (11,43) and (11,48) could be walked into. The four cells are restored to L141LEV's values.

| Offset | Cell | Was | Now |
|---|---|---|---|
| 0x69002 | (12,42) | 00 | 14 |
| 0x69502 | (11,43) | 00 | 14 (pillar) |
| 0x6AE52 | (11,48) | 00 | 14 (pillar) |
| 0x6E862 | (49,59) | 00 | 15 |

Patch listed at Patch/L903LEV.txt

## L901LEV Multiplayer Map 2 Fix 1

This shows where four enemy types are spawned within the level, in other levels these are clustered together off of the map.

The four highlighted objects show where player movement was previously blocked.

I believe these enemy types are placeholders for the players. Image taken within an abandoned unity toolkit based off of my code.

<div align="center">
  <img src="Images/L901LEV-spawns.png" alt="L901LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L901LEV-spawns-fix.png" alt="L901LEV Patched">
</div>

## L900LEV Multiplayer Map 1 Fix 1

[These fixes are the same as on L111LEV](#L111LEV-Fix-1)

This used to be fixed only when exporting the level model. It is now fixed in the level file, on the same faces at the same offsets as L111LEV. Applied by both ALTEngine and ALTViewer's patch button. Details in Patch/L900LEV.txt.

## L900LEV Multiplayer Map 1 Fix 2

[These fixes are the same as on L111LEV](#L111LEV-Fix-2)

## L900LEV Multiplayer Map 1 Fix 3

A vertex on one of the lifts had a misplaced coordinate, the same bug at the same offset on both L111LEV and L900LEV.

Patch listed at Patch/new-fix.txt

Applied by both ALTEngine and ALTViewer's patch button.

## L900LEV Multiplayer Map 1 Fix 4

[The same as L111LEV Fix 4](#L111LEV-Fix-4): the wrong texture index, and the UV rotation that goes with it. L900LEV previously got neither.

Patch listed at Patch/another-fix.txt

Applied by both ALTEngine and ALTViewer's patch button.

## L900LEV Multiplayer Map 1 Fix 5

UV flipped.

The panel on the starting lift (face 10208) was mirrored the wrong way. Its flag byte is set to its mirrored partner, so the texture now reads the right way round. This used to be done only when exporting the model (and by the engine while drawing); it is now in the level file. [The same as L111LEV Fix 5](#L111LEV-Fix-5).

Patch listed at Patch/L900LEV.txt

Applied by both ALTEngine and ALTViewer's patch button.

## L111LEV Fix 1

These fixes are the same as on L900LEV

Here is how the level was originally.

<div align="center">
  <img src="Images/L111LEV-texture.png" alt="L111LEV Patched">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L111LEV-texture-fixed.png" alt="L111LEV Original">
</div>

This used to be fixed only when exporting the level model. The four crate faces are now fixed in the level file itself: one had the wrong texture index, one needed its UV rotation corrected, and two were upside down (a half-turn). Applied by both ALTEngine and ALTViewer's patch button. Details in Patch/L111LEV.txt.

## L111LEV Fix 2

These fixes are the same as on L900LEV

Here is how the level was originally.

<div align="center">
  <img src="Images/L111LEV-flags-unpatched.png" alt="L111LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L111LEV-flags-patched.png" alt="L111LEV Patched">
</div>

## L111LEV Fix 3

A vertex on one of the lifts had a misplaced coordinate, the same bug at the same offset on both L111LEV and L900LEV.

Patch listed at Patch/new-fix.txt

Applied by both ALTEngine and ALTViewer's patch button.

## L111LEV Fix 4

Patched an issue on L111LEV where a face was assigned the wrong texture index.

UV rotation corrected: with the right texture, the face also needed its texture turned a quarter-turn counter-clockwise. This used to be done by the engine while drawing; it is now in the level file, by rotating the face's vertex list.

Patch listed at Patch/another-fix.txt

Applied by both ALTEngine and ALTViewer's patch button.

<div align="center">
  <img src="Images/L111LEV-incorrect-texture-index.png" alt="L111LEV Patched">
</div>

## L111LEV Fix 5

UV flipped.

The panel on the starting lift (face 10208) was mirrored the wrong way. Its flag byte is set to its mirrored partner, so the texture now reads the right way round. This used to be done only when exporting the model (and by the engine while drawing); it is now in the level file.

Patch listed at Patch/L111LEV.txt

Applied by both ALTEngine and ALTViewer's patch button.

## L112LEV Fix 1

Overlapping faces.

A corner of the sloped base under the hanging sign was covered twice. Triangle 5843 covers the whole corner; a second triangle (5849, texture 0) and a strip (5850) cover exactly the same area again. The two sets fought for the same pixels, so pale slivers of the wrong texture showed through the corner. The patch makes 5849 and 5850 zero-area by pointing all their corners at vertex 985, one of the corners they already shared with 5843, which is left as the corner's only face.

Patch listed at Patch/L112LEV.txt

Applied by both ALTEngine and ALTViewer's patch button.

Before and after (rendered by ALTEngine, brightened):

<div align="center">
  <img src="Images/L112LEV-overlapping-faces.png" alt="L112LEV Original">
</div>

<div align="center">
  <img src="Images/L112LEV-overlapping-faces-fixed.png" alt="L112LEV Patched">
</div>

## L141LEV Fix 1

These fixes are the same as on L903LEV

Here is how the level was originally.

<div align="center">
  <img src="Images/L141LEV-texture.png" alt="L141LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L141LEV-texture-fixed.png" alt="L141LEV Patched">
</div>

Here is how the level was originally.

<div align="center">
  <img src="Images/L141LEV-texture-2.png" alt="L141LEV Original">
</div>

Here it is with the fixes I applied to the level.

<div align="center">
  <img src="Images/L141LEV-texture-fixed-2.png" alt="L141LEV Patched">
</div>

This used to be fixed only when exporting the level model. The crate faces are now fixed in the level file itself: eight had the wrong texture index (two of them also needed their UV rotation corrected), two needed only their UV rotation corrected, and one was already right in the file. Applied by both ALTEngine and ALTViewer's patch button. Details in Patch/L141LEV.txt.

The exact faces, their old export remaps and the bytes that now fix them are listed in Patch/L141LEV.txt.

## L154LEV Fix 1

UV flipped and rotation corrected.

Found by texture scan 154-1. Face 2772 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L154LEV.txt

## L161LEV Fix 1

Here is how the level was originally.

<div align="center">
  <img src="Images/L161LEV-texture.png" alt="L161LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L161LEV-texture-fixed.png" alt="L161LEV Patched">
</div>

This used to be fixed only when exporting the level model. The wall face is now fixed in the level file itself: it takes the correct texture index, with its UV rotation corrected (a half-turn). Applied by both ALTEngine and ALTViewer's patch button. Details in Patch/L161LEV.txt.

## L161LEV Fix 2

Here is how the level was originally.

<div align="center">
  <img src="Images/L161LEV-D002.png" alt="L161LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L161LEV-D002-FIX.png" alt="L161LEV Original">
</div>

## L161LEV Fix 3

UV rotation corrected.

Found by texture scan 161-1. Face 5522 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L161LEV.txt

## L161LEV Fix 4

UV flipped and rotation corrected.

Found by texture scan 161-2. Face 430 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L161LEV.txt

## L162LEV Fix 1

Here is how the level was originally.

<div align="center">
  <img src="Images/L162LEV-D001.png" alt="L161LEV Original">
</div>

Here it is with the fix I applied to the level.

<div align="center">
  <img src="Images/L162LEV-D001-FIX.png" alt="L161LEV Patched">
</div>

## L211LEV Fix 1

UV flipped and rotation corrected.

Found by texture scan 211-1. Faces 6455, 6734 and 6942 are corrected so the texture runs on from the faces around them.

Patch listed at Patch/L211LEV.txt

## L263LEV Fix 1

UV flipped and rotation corrected.

Found by texture scan 263-3. Face 2622 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L263LEV.txt

## L311LEV Fix 1

UV rotation corrected.

Found by texture scan 311-1. Face 7758 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L311LEV.txt

## L311LEV Fix 2

UV rotation corrected.

Found by texture scan 311-9. Face 5788 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L311LEV.txt

## L311LEV Fix 3

UV rotation corrected.

Found by texture scan 311-12. Face 11010 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L311LEV.txt

## L321LEV Fix 1

UV rotation corrected.

Found by texture scan 321-1. Faces 2961, 2962 and 2966 are corrected so the texture runs on from the faces around them.

Patch listed at Patch/L321LEV.txt

## L321LEV Fix 2

UV flipped and rotation corrected.

Found by texture scan 321-2. Faces 4163, 4164 and 4168 are corrected so the texture runs on from the faces around them.

Patch listed at Patch/L321LEV.txt

## L321LEV Fix 3

UV rotation corrected.

Found by texture scan 321-3. Faces 6143, 6144 and 6148 are corrected so the texture runs on from the faces around them.

Patch listed at Patch/L321LEV.txt

## L321LEV Fix 4

UV flipped and rotation corrected.

Found by texture scan 321-4. Faces 6714, 6715 and 6719 are corrected so the texture runs on from the faces around them.

Patch listed at Patch/L321LEV.txt

## L321LEV Fix 5

UV rotation corrected.

Found by texture scan 321-10. Face 888 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L321LEV.txt

## L321LEV Fix 6

UV rotation corrected.

Found by texture scan 321-11. Face 600 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L321LEV.txt

## L321LEV Fix 7

UV rotation corrected.

Found by texture scan 321-12. Face 6987 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L321LEV.txt

## L321LEV Fix 8

UV rotation corrected.

Found by texture scan 321-13. Face 7003 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L321LEV.txt

## L321LEV Fix 9

UV flipped.

Found by texture scan 321-14. Face 1906 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L321LEV.txt

## L321LEV Fix 10

UV flipped.

Found by texture scan 321-15. Face 2458 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L321LEV.txt

## L321LEV Fix 11

UV flipped.

Found by texture scan 321-16. Face 2461 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L321LEV.txt

## L322LEV Fix 1

UV flipped.

Found by texture scan 322-2. Face 2461 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L322LEV.txt

## L322LEV Fix 2

UV flipped.

Found by texture scan 322-4. Face 6596 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L322LEV.txt

## L322LEV Fix 3

UV flipped.

Found by texture scan 322-5. Face 1906 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L322LEV.txt

## L322LEV Fix 4

UV flipped.

Found by texture scan 322-8. Face 2458 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L322LEV.txt

## L323LEV Fix 1

UV rotation corrected.

Found by texture scan 323-2. Face 888 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L323LEV.txt

## L323LEV Fix 2

UV rotation corrected.

Found by texture scan 323-3. Face 3176 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L323LEV.txt

## L323LEV Fix 3

UV flipped.

Found by texture scan 323-4. Face 2461 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L323LEV.txt

## L323LEV Fix 4

UV flipped.

Found by texture scan 323-6. Face 1906 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L323LEV.txt

## L323LEV Fix 5

UV flipped.

Found by texture scan 323-9. Face 2458 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L323LEV.txt

## L324LEV Fix 1

UV rotation corrected.

Found by texture scan 324-2. Face 3176 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L324LEV.txt

## L324LEV Fix 2

UV flipped.

Found by texture scan 324-3. Face 2461 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L324LEV.txt

## L324LEV Fix 3

UV flipped.

Found by texture scan 324-5. Face 6596 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L324LEV.txt

## L324LEV Fix 4

UV flipped.

Found by texture scan 324-8. Face 2458 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L324LEV.txt

## L324LEV Fix 5

UV flipped.

Found by texture scan 324-9. Face 1906 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L324LEV.txt

## L325LEV Fix 1

UV rotation corrected.

Found by texture scan 325-2. Face 3176 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L325LEV.txt

## L325LEV Fix 2

UV flipped.

Found by texture scan 325-3. Face 6596 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L325LEV.txt

## L325LEV Fix 3

UV rotation corrected.

Found by texture scan 325-6. Face 888 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L325LEV.txt

## L325LEV Fix 4

UV rotation corrected.

Found by texture scan 325-8. Face 600 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L325LEV.txt

## L331LEV Fix 1

UV rotation corrected.

Found by texture scan 331-2. Face 2157 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L331LEV.txt

## L331LEV Fix 2

UV rotation corrected.

Found by texture scan 331-3. Face 3207 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L331LEV.txt

## L331LEV Fix 3

UV rotation corrected.

Found by texture scan 331-4. Face 47 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L331LEV.txt

## L331LEV Fix 4

UV flipped.

Found by texture scan 331-6. Face 5843 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L331LEV.txt

## L331LEV Fix 5

UV flipped.

Found by texture scan 331-7. Face 2235 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L331LEV.txt

## L351LEV Fix 1

The resin wall at the far end of the room is one picture split across its faces. The middle and bottom rows were laid out correctly, but on the top row the two corner faces were rotated a quarter-turn and the two centre faces were mirrored the wrong way.

UV rotation corrected on faces 178 and 198, UV flipped on faces 184 and 190.

Patch listed at Patch/L351LEV.txt

## L351LEV Fix 2

UV flipped and rotation corrected.

Found by texture scan 351-3. Faces 4009 and 4193 are corrected so the texture runs on from the faces around them.

Patch listed at Patch/L351LEV.txt

## L351LEV Fix 3

UV rotation corrected.

Found by texture scan 351-4. Face 10389 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L351LEV.txt

## L351LEV Fix 4

UV flipped and rotation corrected.

Found by texture scan 351-5. Face 4981 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L351LEV.txt

## L351LEV Fix 5

UV flipped and rotation corrected.

Found by texture scan 351-7. Face 11998 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L351LEV.txt

## L351LEV Fix 6

UV rotation corrected.

Found by texture scan 351-9. Face 3338 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L351LEV.txt

## L351LEV Fix 7

UV flipped.

Found by texture scan 351-10. Face 4441 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L351LEV.txt

## L351LEV Fix 8

UV rotation corrected.

Found by texture scan 351-11. Face 2826 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L351LEV.txt

## L351LEV Fix 9

UV rotation corrected.

Found by texture scan 351-12. Face 8695 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L351LEV.txt

## L351LEV Fix 10

UV flipped and rotation corrected.

Found by texture scan 351-13. Face 8713 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L351LEV.txt

## L352LEV Fix 1

UV rotation corrected.

Found by texture scan 352-1. Face 1798 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L352LEV.txt

## L352LEV Fix 2

UV rotation corrected.

Found by texture scan 352-2. Face 1846 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L352LEV.txt

## L353LEV Fix 1

UV flipped.

Found by texture scan 353-1. Faces 1920 and 1931 are corrected so the texture runs on from the faces around them.

Patch listed at Patch/L353LEV.txt

## L353LEV Fix 2

UV rotation corrected.

Found by texture scan 353-2. Face 1798 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L353LEV.txt

## L353LEV Fix 3

UV rotation corrected.

Found by texture scan 353-3. Face 1846 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L353LEV.txt

## L371LEV Fix 1

bambamalicious tracked down the fix for this issue, which turned out to just be a single incorrect byte.

## L371LEV Fix 2

One face towards the end of the level was textured with an unused tile instead of the correct texture, this is now fixed.

## L371LEV Fix 3

The empty textures on the fourth texture sheet were previously purple, which showed up on the edges of some faces in the level, the background is now a similar dark shade to the rest of the textures.

## L371LEV Fix 4

UV flipped and rotation corrected.

Found by texture scan 371-1. Faces 444, 445, 446, 447, 451, 452, 453 and 454 are corrected so the texture runs on from the faces around them.

Patch listed at Patch/L371LEV.txt

## L371LEV Fix 5

UV rotation corrected.

Found by texture scan 371-3. Face 161 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L371LEV.txt

## L371LEV Fix 6

UV rotation corrected.

Found by texture scan 371-4. Face 5355 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L371LEV.txt

## L371LEV Fix 7

UV rotation corrected.

Found by texture scan 371-5. Face 143 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L371LEV.txt

## L371LEV Fix 8

UV mapping corrected.

The level has 14 recesses in its walls, each with a band framed by four corner triangles. In every one, the top left and bottom right triangles had their texture running diagonally instead of carrying on the band. Those 28 triangles are mirror images of the two good corners, so their vertex order is reversed to mirror the texture, and they use the game's other triangle draw routine (5, which skips the back-face test) so they still show. The orange marks now run straight through the corners and the band.

Patch listed at Patch/L371LEV.txt

## L381LEV Fix 1

UV rotation corrected.

Found by texture scan 381-1. Face 100 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L381LEV.txt

## L391LEV Fix 1

The inner layer of the queen's egg sack is textured with strips that run from one ring of faces into the next, as they do on L263LEV. On L391LEV 246 of those faces had their texture turned a quarter-turn, so the strips ran around the sack instead of along it.

UV rotation corrected on all 246 faces.

Patch listed at Patch/L391LEV.txt

## L391LEV Fix 2

UV rotation corrected.

Found by texture scan 391-1. Face 2192 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L391LEV.txt

## L391LEV Fix 3

UV rotation corrected.

Found by texture scan 391-2. Face 674 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L391LEV.txt

## L391LEV Fix 4

UV rotation corrected.

Found by texture scan 391-3. Face 2652 is corrected so the texture runs on from the faces around it.

Patch listed at Patch/L391LEV.txt

## Duplicate Door And Lift Models

Each level keeps its door models (the D chunks) and lift models (the L chunks) in a table, and every door or lift points at one of them by number, so one model can serve any number of doors. Many levels carried copies that were byte-for-byte the same as an earlier model, and a few carried models that nothing used.

These are now removed. Each removal drops the chunks, corrects the file's size field and renumbers the doors and lifts that pointed at a copy, so they use the original model instead. Nothing in the game changes; the files are just smaller. A removal is only made if each chunk is exactly as expected (size and CRC-32), and it is made before every other patch.

Across 21 levels, 33 models and 38,316 bytes are removed.

| Level | Removed | Bytes saved |
|---|---|---|
| L122LEV | D001 (same as D000) | 668 |
| L131LEV | D001 (same as D000), D002 (unused) | 1,188 |
| L141LEV | D001 (same as D000) | 908 |
| L154LEV | D001 (same as D000), L001 (unused), L008 (unused) | 3,720 |
| L155LEV | D000 (unused) | 908 |
| L162LEV | D000 (unused) | 908 |
| L211LEV | D001 (same as D000), D002 (same as D000), D003 (same as D000) | 2,724 |
| L212LEV | D001 (same as D000) | 908 |
| L213LEV | L000 (unused) | 2,468 |
| L222LEV | D001 (same as D000) | 908 |
| L231LEV | D001 (same as D000), L001 (same as L000), L003 (same as L000) | 5,860 |
| L232LEV | D001 (same as D000), D002 (unused) | 1,088 |
| L391LEV | L000 (unused) | 1,660 |
| L901LEV | D001 (same as D000) | 668 |
| L902LEV | D001 (same as D000) | 668 |
| L903LEV | D001 (same as D000) | 908 |
| L904LEV | D001 (same as D000) | 908 |
| L905LEV | D001 (same as D000) | 908 |
| L906LEV | D001 (unused) | 908 |
| L907LEV | D001 (same as D000), L001 (same as L000), L002 (unused), L003 (same as L000) | 7,376 |
| L909LEV | L000 (unused), L002 (unused) | 2,056 |

Patch listed at Patch/DUPLICATE-MODELS.txt, with the full list and reasoning in Notes/duplicate-level-models.md. Both ALTEngine and ALTViewer apply it.

## Before And After Screenshots

Every patch ALTEngine makes to the level files and texture sheets, shown before and after from the same camera, with a red box round what changes. The screenshots are made by ALTEngine's own renderer, headless, from an untouched copy of the shipped files: "before" is the file with that one patch taken back out and "after" is the file as the install leaves it, so each pair shows that patch alone. Doors and lifts are placed as in the game. They are kept in `Images/<level>/`, named `<level>-fix<patch>-<n>-before.png` and `-after.png`, where `<patch>` is the patch's place in `ALTEngine/data/Patches.json` and `<n>` counts the separate places one patch changes.

Fixes that change nothing a still picture can show (wall attributes, flags with no visible effect, trigger data) are listed under their level without screenshots. The exact bytes of every fix are in the Patch folder.

### L111LEV

**Patch 2.** Texture flags wrongly set on 16 faces.

No screenshot: flag only, no change in a still view.

**Patch 4.** Texture flags wrongly set on the above-door wall sides.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L111LEV/L111LEV-fix04-1-before.png" width="320" alt="L111LEV-fix04-1 before"></td><td><img src="Images/L111LEV/L111LEV-fix04-1-after.png" width="320" alt="L111LEV-fix04-1 after"></td></tr>
<tr><td><img src="Images/L111LEV/L111LEV-fix04-2-before.png" width="320" alt="L111LEV-fix04-2 before"></td><td><img src="Images/L111LEV/L111LEV-fix04-2-after.png" width="320" alt="L111LEV-fix04-2 after"></td></tr>
</table>

**Patch 11.** Lift vertex with a misplaced coordinate.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L111LEV/L111LEV-fix11-1-before.png" width="320" alt="L111LEV-fix11-1 before"></td><td><img src="Images/L111LEV/L111LEV-fix11-1-after.png" width="320" alt="L111LEV-fix11-1 after"></td></tr>
</table>

**Patch 13.** One face with the wrong texture, turned a quarter-turn.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L111LEV/L111LEV-fix13-1-before.png" width="320" alt="L111LEV-fix13-1 before"></td><td><img src="Images/L111LEV/L111LEV-fix13-1-after.png" width="320" alt="L111LEV-fix13-1 after"></td></tr>
</table>

**Patch 16.** Starting lift panel mirrored the wrong way.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L111LEV/L111LEV-fix16-1-before.png" width="320" alt="L111LEV-fix16-1 before"></td><td><img src="Images/L111LEV/L111LEV-fix16-1-after.png" width="320" alt="L111LEV-fix16-1 after"></td></tr>
</table>

**Patch 43.** Four crate faces with the wrong texture or orientation.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L111LEV/L111LEV-fix43-1-before.png" width="320" alt="L111LEV-fix43-1 before"></td><td><img src="Images/L111LEV/L111LEV-fix43-1-after.png" width="320" alt="L111LEV-fix43-1 after"></td></tr>
<tr><td><img src="Images/L111LEV/L111LEV-fix43-2-before.png" width="320" alt="L111LEV-fix43-2 before"></td><td><img src="Images/L111LEV/L111LEV-fix43-2-after.png" width="320" alt="L111LEV-fix43-2 after"></td></tr>
<tr><td><img src="Images/L111LEV/L111LEV-fix43-3-before.png" width="320" alt="L111LEV-fix43-3 before"></td><td><img src="Images/L111LEV/L111LEV-fix43-3-after.png" width="320" alt="L111LEV-fix43-3 after"></td></tr>
<tr><td><img src="Images/L111LEV/L111LEV-fix43-4-before.png" width="320" alt="L111LEV-fix43-4 before"></td><td><img src="Images/L111LEV/L111LEV-fix43-4-after.png" width="320" alt="L111LEV-fix43-4 after"></td></tr>
</table>

**Patch 59.** Face 4150 flipped vertically, face 6528 given texture 194 and turned.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L111LEV/L111LEV-fix59-1-before.png" width="320" alt="L111LEV-fix59-1 before"></td><td><img src="Images/L111LEV/L111LEV-fix59-1-after.png" width="320" alt="L111LEV-fix59-1 after"></td></tr>
<tr><td><img src="Images/L111LEV/L111LEV-fix59-2-before.png" width="320" alt="L111LEV-fix59-2 before"></td><td><img src="Images/L111LEV/L111LEV-fix59-2-after.png" width="320" alt="L111LEV-fix59-2 after"></td></tr>
</table>

### L112LEV

**Patch 15.** Corner under the hanging sign covered by two sets of faces.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L112LEV/L112LEV-fix15-1-before.png" width="320" alt="L112LEV-fix15-1 before"></td><td><img src="Images/L112LEV/L112LEV-fix15-1-after.png" width="320" alt="L112LEV-fix15-1 after"></td></tr>
</table>

### L131LEV

**Patch 54.** UV review: a pillar face's texture and a face turned to match its neighbour.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L131LEV/L131LEV-fix54-1-before.png" width="320" alt="L131LEV-fix54-1 before"></td><td><img src="Images/L131LEV/L131LEV-fix54-1-after.png" width="320" alt="L131LEV-fix54-1 after"></td></tr>
</table>

### L141LEV

**Patch 45.** Ten Weyland-Yutani and grey crate faces with the wrong texture or orientation.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L141LEV/L141LEV-fix45-1-before.png" width="320" alt="L141LEV-fix45-1 before"></td><td><img src="Images/L141LEV/L141LEV-fix45-1-after.png" width="320" alt="L141LEV-fix45-1 after"></td></tr>
<tr><td><img src="Images/L141LEV/L141LEV-fix45-2-before.png" width="320" alt="L141LEV-fix45-2 before"></td><td><img src="Images/L141LEV/L141LEV-fix45-2-after.png" width="320" alt="L141LEV-fix45-2 after"></td></tr>
<tr><td><img src="Images/L141LEV/L141LEV-fix45-3-before.png" width="320" alt="L141LEV-fix45-3 before"></td><td><img src="Images/L141LEV/L141LEV-fix45-3-after.png" width="320" alt="L141LEV-fix45-3 after"></td></tr>
<tr><td><img src="Images/L141LEV/L141LEV-fix45-4-before.png" width="320" alt="L141LEV-fix45-4 before"></td><td><img src="Images/L141LEV/L141LEV-fix45-4-after.png" width="320" alt="L141LEV-fix45-4 after"></td></tr>
<tr><td><img src="Images/L141LEV/L141LEV-fix45-5-before.png" width="320" alt="L141LEV-fix45-5 before"></td><td><img src="Images/L141LEV/L141LEV-fix45-5-after.png" width="320" alt="L141LEV-fix45-5 after"></td></tr>
<tr><td><img src="Images/L141LEV/L141LEV-fix45-6-before.png" width="320" alt="L141LEV-fix45-6 before"></td><td><img src="Images/L141LEV/L141LEV-fix45-6-after.png" width="320" alt="L141LEV-fix45-6 after"></td></tr>
</table>

**Patch 55.** UV review: three crate faces (texture and flips).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L141LEV/L141LEV-fix55-1-before.png" width="320" alt="L141LEV-fix55-1 before"></td><td><img src="Images/L141LEV/L141LEV-fix55-1-after.png" width="320" alt="L141LEV-fix55-1 after"></td></tr>
<tr><td><img src="Images/L141LEV/L141LEV-fix55-2-before.png" width="320" alt="L141LEV-fix55-2 before"></td><td><img src="Images/L141LEV/L141LEV-fix55-2-after.png" width="320" alt="L141LEV-fix55-2 after"></td></tr>
</table>

**Patch 61.** The floor strip's two missing slices added to the sheet (textures 223 and 246).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L141LEV/141GFX-fix61-before.png" width="320" alt="141GFX-fix61 before"></td><td><img src="Images/L141LEV/141GFX-fix61-after.png" width="320" alt="141GFX-fix61 after"></td></tr>
</table>

**Patch 62.** End room floor laid as its first row and L111's spawn floor show (with the sheet's new slices).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L141LEV/L141LEV-fix62-1-before.png" width="320" alt="L141LEV-fix62-1 before"></td><td><img src="Images/L141LEV/L141LEV-fix62-1-after.png" width="320" alt="L141LEV-fix62-1 after"></td></tr>
</table>

### L154LEV

**Patch 20.** Texture scan review: 1 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L154LEV/L154LEV-fix20-1-before.png" width="320" alt="L154LEV-fix20-1 before"></td><td><img src="Images/L154LEV/L154LEV-fix20-1-after.png" width="320" alt="L154LEV-fix20-1 after"></td></tr>
</table>

### L161LEV

**Patch 7.** One face of door D002 with the wrong texture.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L161LEV/L161LEV-fix07-8-before.png" width="320" alt="L161LEV-fix07-8 before"></td><td><img src="Images/L161LEV/L161LEV-fix07-8-after.png" width="320" alt="L161LEV-fix07-8 after"></td></tr>
</table>

**Patch 21.** Texture scan review: 2 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L161LEV/L161LEV-fix21-1-before.png" width="320" alt="L161LEV-fix21-1 before"></td><td><img src="Images/L161LEV/L161LEV-fix21-1-after.png" width="320" alt="L161LEV-fix21-1 after"></td></tr>
<tr><td><img src="Images/L161LEV/L161LEV-fix21-2-before.png" width="320" alt="L161LEV-fix21-2 before"></td><td><img src="Images/L161LEV/L161LEV-fix21-2-after.png" width="320" alt="L161LEV-fix21-2 after"></td></tr>
</table>

**Patch 47.** The secretion-covered wall with the wrong texture.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L161LEV/L161LEV-fix47-1-before.png" width="320" alt="L161LEV-fix47-1 before"></td><td><img src="Images/L161LEV/L161LEV-fix47-1-after.png" width="320" alt="L161LEV-fix47-1 after"></td></tr>
</table>

**Patch 57.** Face 3758 put back as shipped (an earlier patch had used it for a missing quad; that quad is now the cleanup's record-0 face).

No screenshot: puts the shipped bytes back.

### L162LEV

**Patch 6.** One face of the starting door (D001) with the wrong texture.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L162LEV/L162LEV-fix06-1-before.png" width="320" alt="L162LEV-fix06-1 before"></td><td><img src="Images/L162LEV/L162LEV-fix06-1-after.png" width="320" alt="L162LEV-fix06-1 after"></td></tr>
<tr><td><img src="Images/L162LEV/L162LEV-fix06-2-before.png" width="320" alt="L162LEV-fix06-2 before"></td><td><img src="Images/L162LEV/L162LEV-fix06-2-after.png" width="320" alt="L162LEV-fix06-2 after"></td></tr>
</table>

### L211LEV

**Patch 22.** Texture scan review: 3 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L211LEV/L211LEV-fix22-1-before.png" width="320" alt="L211LEV-fix22-1 before"></td><td><img src="Images/L211LEV/L211LEV-fix22-1-after.png" width="320" alt="L211LEV-fix22-1 after"></td></tr>
</table>

### L232LEV

**Patch 58.** UV review: pillar faces drawn with see-through textures given the solid pillar ones.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L232LEV/L232LEV-fix58-1-before.png" width="320" alt="L232LEV-fix58-1 before"></td><td><img src="Images/L232LEV/L232LEV-fix58-1-after.png" width="320" alt="L232LEV-fix58-1 after"></td></tr>
<tr><td><img src="Images/L232LEV/L232LEV-fix58-2-before.png" width="320" alt="L232LEV-fix58-2 before"></td><td><img src="Images/L232LEV/L232LEV-fix58-2-after.png" width="320" alt="L232LEV-fix58-2 after"></td></tr>
</table>

### L263LEV

**Patch 23.** Texture scan review: 1 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L263LEV/L263LEV-fix23-1-before.png" width="320" alt="L263LEV-fix23-1 before"></td><td><img src="Images/L263LEV/L263LEV-fix23-1-after.png" width="320" alt="L263LEV-fix23-1 after"></td></tr>
</table>

### L311LEV

**Patch 24.** Texture scan review: 3 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L311LEV/L311LEV-fix24-1-before.png" width="320" alt="L311LEV-fix24-1 before"></td><td><img src="Images/L311LEV/L311LEV-fix24-1-after.png" width="320" alt="L311LEV-fix24-1 after"></td></tr>
<tr><td><img src="Images/L311LEV/L311LEV-fix24-2-before.png" width="320" alt="L311LEV-fix24-2 before"></td><td><img src="Images/L311LEV/L311LEV-fix24-2-after.png" width="320" alt="L311LEV-fix24-2 after"></td></tr>
<tr><td><img src="Images/L311LEV/L311LEV-fix24-3-before.png" width="320" alt="L311LEV-fix24-3 before"></td><td><img src="Images/L311LEV/L311LEV-fix24-3-after.png" width="320" alt="L311LEV-fix24-3 after"></td></tr>
</table>

### L321LEV

**Patch 25.** Texture scan review: 19 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L321LEV/L321LEV-fix25-1-before.png" width="320" alt="L321LEV-fix25-1 before"></td><td><img src="Images/L321LEV/L321LEV-fix25-1-after.png" width="320" alt="L321LEV-fix25-1 after"></td></tr>
<tr><td><img src="Images/L321LEV/L321LEV-fix25-2-before.png" width="320" alt="L321LEV-fix25-2 before"></td><td><img src="Images/L321LEV/L321LEV-fix25-2-after.png" width="320" alt="L321LEV-fix25-2 after"></td></tr>
<tr><td><img src="Images/L321LEV/L321LEV-fix25-3-before.png" width="320" alt="L321LEV-fix25-3 before"></td><td><img src="Images/L321LEV/L321LEV-fix25-3-after.png" width="320" alt="L321LEV-fix25-3 after"></td></tr>
<tr><td><img src="Images/L321LEV/L321LEV-fix25-4-before.png" width="320" alt="L321LEV-fix25-4 before"></td><td><img src="Images/L321LEV/L321LEV-fix25-4-after.png" width="320" alt="L321LEV-fix25-4 after"></td></tr>
<tr><td><img src="Images/L321LEV/L321LEV-fix25-5-before.png" width="320" alt="L321LEV-fix25-5 before"></td><td><img src="Images/L321LEV/L321LEV-fix25-5-after.png" width="320" alt="L321LEV-fix25-5 after"></td></tr>
<tr><td><img src="Images/L321LEV/L321LEV-fix25-6-before.png" width="320" alt="L321LEV-fix25-6 before"></td><td><img src="Images/L321LEV/L321LEV-fix25-6-after.png" width="320" alt="L321LEV-fix25-6 after"></td></tr>
<tr><td><img src="Images/L321LEV/L321LEV-fix25-7-before.png" width="320" alt="L321LEV-fix25-7 before"></td><td><img src="Images/L321LEV/L321LEV-fix25-7-after.png" width="320" alt="L321LEV-fix25-7 after"></td></tr>
<tr><td><img src="Images/L321LEV/L321LEV-fix25-8-before.png" width="320" alt="L321LEV-fix25-8 before"></td><td><img src="Images/L321LEV/L321LEV-fix25-8-after.png" width="320" alt="L321LEV-fix25-8 after"></td></tr>
</table>

### L322LEV

**Patch 26.** Texture scan review: 4 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L322LEV/L322LEV-fix26-1-before.png" width="320" alt="L322LEV-fix26-1 before"></td><td><img src="Images/L322LEV/L322LEV-fix26-1-after.png" width="320" alt="L322LEV-fix26-1 after"></td></tr>
<tr><td><img src="Images/L322LEV/L322LEV-fix26-2-before.png" width="320" alt="L322LEV-fix26-2 before"></td><td><img src="Images/L322LEV/L322LEV-fix26-2-after.png" width="320" alt="L322LEV-fix26-2 after"></td></tr>
<tr><td><img src="Images/L322LEV/L322LEV-fix26-3-before.png" width="320" alt="L322LEV-fix26-3 before"></td><td><img src="Images/L322LEV/L322LEV-fix26-3-after.png" width="320" alt="L322LEV-fix26-3 after"></td></tr>
</table>

### L323LEV

**Patch 27.** Texture scan review: 5 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L323LEV/L323LEV-fix27-1-before.png" width="320" alt="L323LEV-fix27-1 before"></td><td><img src="Images/L323LEV/L323LEV-fix27-1-after.png" width="320" alt="L323LEV-fix27-1 after"></td></tr>
<tr><td><img src="Images/L323LEV/L323LEV-fix27-2-before.png" width="320" alt="L323LEV-fix27-2 before"></td><td><img src="Images/L323LEV/L323LEV-fix27-2-after.png" width="320" alt="L323LEV-fix27-2 after"></td></tr>
<tr><td><img src="Images/L323LEV/L323LEV-fix27-3-before.png" width="320" alt="L323LEV-fix27-3 before"></td><td><img src="Images/L323LEV/L323LEV-fix27-3-after.png" width="320" alt="L323LEV-fix27-3 after"></td></tr>
<tr><td><img src="Images/L323LEV/L323LEV-fix27-4-before.png" width="320" alt="L323LEV-fix27-4 before"></td><td><img src="Images/L323LEV/L323LEV-fix27-4-after.png" width="320" alt="L323LEV-fix27-4 after"></td></tr>
</table>

### L324LEV

**Patch 28.** Texture scan review: 5 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L324LEV/L324LEV-fix28-1-before.png" width="320" alt="L324LEV-fix28-1 before"></td><td><img src="Images/L324LEV/L324LEV-fix28-1-after.png" width="320" alt="L324LEV-fix28-1 after"></td></tr>
<tr><td><img src="Images/L324LEV/L324LEV-fix28-2-before.png" width="320" alt="L324LEV-fix28-2 before"></td><td><img src="Images/L324LEV/L324LEV-fix28-2-after.png" width="320" alt="L324LEV-fix28-2 after"></td></tr>
<tr><td><img src="Images/L324LEV/L324LEV-fix28-3-before.png" width="320" alt="L324LEV-fix28-3 before"></td><td><img src="Images/L324LEV/L324LEV-fix28-3-after.png" width="320" alt="L324LEV-fix28-3 after"></td></tr>
<tr><td><img src="Images/L324LEV/L324LEV-fix28-4-before.png" width="320" alt="L324LEV-fix28-4 before"></td><td><img src="Images/L324LEV/L324LEV-fix28-4-after.png" width="320" alt="L324LEV-fix28-4 after"></td></tr>
</table>

### L325LEV

**Patch 29.** Texture scan review: 4 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L325LEV/L325LEV-fix29-1-before.png" width="320" alt="L325LEV-fix29-1 before"></td><td><img src="Images/L325LEV/L325LEV-fix29-1-after.png" width="320" alt="L325LEV-fix29-1 after"></td></tr>
<tr><td><img src="Images/L325LEV/L325LEV-fix29-2-before.png" width="320" alt="L325LEV-fix29-2 before"></td><td><img src="Images/L325LEV/L325LEV-fix29-2-after.png" width="320" alt="L325LEV-fix29-2 after"></td></tr>
<tr><td><img src="Images/L325LEV/L325LEV-fix29-3-before.png" width="320" alt="L325LEV-fix29-3 before"></td><td><img src="Images/L325LEV/L325LEV-fix29-3-after.png" width="320" alt="L325LEV-fix29-3 after"></td></tr>
</table>

### L331LEV

**Patch 30.** Texture scan review: 5 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L331LEV/L331LEV-fix30-1-before.png" width="320" alt="L331LEV-fix30-1 before"></td><td><img src="Images/L331LEV/L331LEV-fix30-1-after.png" width="320" alt="L331LEV-fix30-1 after"></td></tr>
<tr><td><img src="Images/L331LEV/L331LEV-fix30-2-before.png" width="320" alt="L331LEV-fix30-2 before"></td><td><img src="Images/L331LEV/L331LEV-fix30-2-after.png" width="320" alt="L331LEV-fix30-2 after"></td></tr>
<tr><td><img src="Images/L331LEV/L331LEV-fix30-3-before.png" width="320" alt="L331LEV-fix30-3 before"></td><td><img src="Images/L331LEV/L331LEV-fix30-3-after.png" width="320" alt="L331LEV-fix30-3 after"></td></tr>
<tr><td><img src="Images/L331LEV/L331LEV-fix30-4-before.png" width="320" alt="L331LEV-fix30-4 before"></td><td><img src="Images/L331LEV/L331LEV-fix30-4-after.png" width="320" alt="L331LEV-fix30-4 after"></td></tr>
<tr><td><img src="Images/L331LEV/L331LEV-fix30-5-before.png" width="320" alt="L331LEV-fix30-5 before"></td><td><img src="Images/L331LEV/L331LEV-fix30-5-after.png" width="320" alt="L331LEV-fix30-5 after"></td></tr>
</table>

### L351LEV

**Patch 18.** Resin wall top row: two faces rotated, two mirrored.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L351LEV/L351LEV-fix18-1-before.png" width="320" alt="L351LEV-fix18-1 before"></td><td><img src="Images/L351LEV/L351LEV-fix18-1-after.png" width="320" alt="L351LEV-fix18-1 after"></td></tr>
</table>

**Patch 31.** Texture scan review: 10 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L351LEV/L351LEV-fix31-1-before.png" width="320" alt="L351LEV-fix31-1 before"></td><td><img src="Images/L351LEV/L351LEV-fix31-1-after.png" width="320" alt="L351LEV-fix31-1 after"></td></tr>
<tr><td><img src="Images/L351LEV/L351LEV-fix31-2-before.png" width="320" alt="L351LEV-fix31-2 before"></td><td><img src="Images/L351LEV/L351LEV-fix31-2-after.png" width="320" alt="L351LEV-fix31-2 after"></td></tr>
<tr><td><img src="Images/L351LEV/L351LEV-fix31-3-before.png" width="320" alt="L351LEV-fix31-3 before"></td><td><img src="Images/L351LEV/L351LEV-fix31-3-after.png" width="320" alt="L351LEV-fix31-3 after"></td></tr>
<tr><td><img src="Images/L351LEV/L351LEV-fix31-4-before.png" width="320" alt="L351LEV-fix31-4 before"></td><td><img src="Images/L351LEV/L351LEV-fix31-4-after.png" width="320" alt="L351LEV-fix31-4 after"></td></tr>
<tr><td><img src="Images/L351LEV/L351LEV-fix31-5-before.png" width="320" alt="L351LEV-fix31-5 before"></td><td><img src="Images/L351LEV/L351LEV-fix31-5-after.png" width="320" alt="L351LEV-fix31-5 after"></td></tr>
<tr><td><img src="Images/L351LEV/L351LEV-fix31-6-before.png" width="320" alt="L351LEV-fix31-6 before"></td><td><img src="Images/L351LEV/L351LEV-fix31-6-after.png" width="320" alt="L351LEV-fix31-6 after"></td></tr>
<tr><td><img src="Images/L351LEV/L351LEV-fix31-7-before.png" width="320" alt="L351LEV-fix31-7 before"></td><td><img src="Images/L351LEV/L351LEV-fix31-7-after.png" width="320" alt="L351LEV-fix31-7 after"></td></tr>
<tr><td><img src="Images/L351LEV/L351LEV-fix31-8-before.png" width="320" alt="L351LEV-fix31-8 before"></td><td><img src="Images/L351LEV/L351LEV-fix31-8-after.png" width="320" alt="L351LEV-fix31-8 after"></td></tr>
<tr><td><img src="Images/L351LEV/L351LEV-fix31-9-before.png" width="320" alt="L351LEV-fix31-9 before"></td><td><img src="Images/L351LEV/L351LEV-fix31-9-after.png" width="320" alt="L351LEV-fix31-9 after"></td></tr>
</table>

### L352LEV

**Patch 32.** Texture scan review: 2 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L352LEV/L352LEV-fix32-1-before.png" width="320" alt="L352LEV-fix32-1 before"></td><td><img src="Images/L352LEV/L352LEV-fix32-1-after.png" width="320" alt="L352LEV-fix32-1 after"></td></tr>
<tr><td><img src="Images/L352LEV/L352LEV-fix32-2-before.png" width="320" alt="L352LEV-fix32-2 before"></td><td><img src="Images/L352LEV/L352LEV-fix32-2-after.png" width="320" alt="L352LEV-fix32-2 after"></td></tr>
</table>

### L353LEV

**Patch 33.** Texture scan review: 4 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L353LEV/L353LEV-fix33-1-before.png" width="320" alt="L353LEV-fix33-1 before"></td><td><img src="Images/L353LEV/L353LEV-fix33-1-after.png" width="320" alt="L353LEV-fix33-1 after"></td></tr>
<tr><td><img src="Images/L353LEV/L353LEV-fix33-2-before.png" width="320" alt="L353LEV-fix33-2 before"></td><td><img src="Images/L353LEV/L353LEV-fix33-2-after.png" width="320" alt="L353LEV-fix33-2 after"></td></tr>
<tr><td><img src="Images/L353LEV/L353LEV-fix33-3-before.png" width="320" alt="L353LEV-fix33-3 before"></td><td><img src="Images/L353LEV/L353LEV-fix33-3-after.png" width="320" alt="L353LEV-fix33-3 after"></td></tr>
</table>

### L371LEV

**Patch 9.** Secret area unreachable: lift 3 now takes the player down into the secret pit.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix09-2-before.png" width="320" alt="L371LEV-fix09-2 before"></td><td><img src="Images/L371LEV/L371LEV-fix09-2-after.png" width="320" alt="L371LEV-fix09-2 after"></td></tr>
</table>

**Patch 10.** Sheet background: a stray tile's background on page 3 that showed at the edges of some faces.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L371LEV/371GFX-fix10-before.png" width="320" alt="371GFX-fix10 before"></td><td><img src="Images/L371LEV/371GFX-fix10-after.png" width="320" alt="371GFX-fix10 after"></td></tr>
</table>

**Patch 34.** Texture scan review: 11 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix34-1-before.png" width="320" alt="L371LEV-fix34-1 before"></td><td><img src="Images/L371LEV/L371LEV-fix34-1-after.png" width="320" alt="L371LEV-fix34-1 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix34-2-before.png" width="320" alt="L371LEV-fix34-2 before"></td><td><img src="Images/L371LEV/L371LEV-fix34-2-after.png" width="320" alt="L371LEV-fix34-2 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix34-3-before.png" width="320" alt="L371LEV-fix34-3 before"></td><td><img src="Images/L371LEV/L371LEV-fix34-3-after.png" width="320" alt="L371LEV-fix34-3 after"></td></tr>
</table>

**Patch 49.** Corner triangles of the 14 wall recesses mapped diagonally.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-1-before.png" width="320" alt="L371LEV-fix49-1 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-1-after.png" width="320" alt="L371LEV-fix49-1 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-2-before.png" width="320" alt="L371LEV-fix49-2 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-2-after.png" width="320" alt="L371LEV-fix49-2 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-3-before.png" width="320" alt="L371LEV-fix49-3 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-3-after.png" width="320" alt="L371LEV-fix49-3 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-4-before.png" width="320" alt="L371LEV-fix49-4 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-4-after.png" width="320" alt="L371LEV-fix49-4 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-5-before.png" width="320" alt="L371LEV-fix49-5 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-5-after.png" width="320" alt="L371LEV-fix49-5 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-6-before.png" width="320" alt="L371LEV-fix49-6 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-6-after.png" width="320" alt="L371LEV-fix49-6 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-7-before.png" width="320" alt="L371LEV-fix49-7 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-7-after.png" width="320" alt="L371LEV-fix49-7 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-8-before.png" width="320" alt="L371LEV-fix49-8 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-8-after.png" width="320" alt="L371LEV-fix49-8 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-9-before.png" width="320" alt="L371LEV-fix49-9 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-9-after.png" width="320" alt="L371LEV-fix49-9 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-10-before.png" width="320" alt="L371LEV-fix49-10 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-10-after.png" width="320" alt="L371LEV-fix49-10 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-11-before.png" width="320" alt="L371LEV-fix49-11 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-11-after.png" width="320" alt="L371LEV-fix49-11 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-12-before.png" width="320" alt="L371LEV-fix49-12 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-12-after.png" width="320" alt="L371LEV-fix49-12 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-13-before.png" width="320" alt="L371LEV-fix49-13 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-13-after.png" width="320" alt="L371LEV-fix49-13 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix49-14-before.png" width="320" alt="L371LEV-fix49-14 before"></td><td><img src="Images/L371LEV/L371LEV-fix49-14-after.png" width="320" alt="L371LEV-fix49-14 after"></td></tr>
</table>

**Patch 50.** UV review round 1: recess corners.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix50-1-before.png" width="320" alt="L371LEV-fix50-1 before"></td><td><img src="Images/L371LEV/L371LEV-fix50-1-after.png" width="320" alt="L371LEV-fix50-1 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix50-2-before.png" width="320" alt="L371LEV-fix50-2 before"></td><td><img src="Images/L371LEV/L371LEV-fix50-2-after.png" width="320" alt="L371LEV-fix50-2 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix50-3-before.png" width="320" alt="L371LEV-fix50-3 before"></td><td><img src="Images/L371LEV/L371LEV-fix50-3-after.png" width="320" alt="L371LEV-fix50-3 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix50-4-before.png" width="320" alt="L371LEV-fix50-4 before"></td><td><img src="Images/L371LEV/L371LEV-fix50-4-after.png" width="320" alt="L371LEV-fix50-4 after"></td></tr>
</table>

**Patch 51.** UV review round 2: recess corners.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix51-1-before.png" width="320" alt="L371LEV-fix51-1 before"></td><td><img src="Images/L371LEV/L371LEV-fix51-1-after.png" width="320" alt="L371LEV-fix51-1 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix51-2-before.png" width="320" alt="L371LEV-fix51-2 before"></td><td><img src="Images/L371LEV/L371LEV-fix51-2-after.png" width="320" alt="L371LEV-fix51-2 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix51-3-before.png" width="320" alt="L371LEV-fix51-3 before"></td><td><img src="Images/L371LEV/L371LEV-fix51-3-after.png" width="320" alt="L371LEV-fix51-3 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix51-4-before.png" width="320" alt="L371LEV-fix51-4 before"></td><td><img src="Images/L371LEV/L371LEV-fix51-4-after.png" width="320" alt="L371LEV-fix51-4 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix51-5-before.png" width="320" alt="L371LEV-fix51-5 before"></td><td><img src="Images/L371LEV/L371LEV-fix51-5-after.png" width="320" alt="L371LEV-fix51-5 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix51-6-before.png" width="320" alt="L371LEV-fix51-6 before"></td><td><img src="Images/L371LEV/L371LEV-fix51-6-after.png" width="320" alt="L371LEV-fix51-6 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix51-7-before.png" width="320" alt="L371LEV-fix51-7 before"></td><td><img src="Images/L371LEV/L371LEV-fix51-7-after.png" width="320" alt="L371LEV-fix51-7 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix51-8-before.png" width="320" alt="L371LEV-fix51-8 before"></td><td><img src="Images/L371LEV/L371LEV-fix51-8-after.png" width="320" alt="L371LEV-fix51-8 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix51-9-before.png" width="320" alt="L371LEV-fix51-9 before"></td><td><img src="Images/L371LEV/L371LEV-fix51-9-after.png" width="320" alt="L371LEV-fix51-9 after"></td></tr>
</table>

**Patch 52.** UV review round 3: recess corners, two faces given the band's texture.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix52-1-before.png" width="320" alt="L371LEV-fix52-1 before"></td><td><img src="Images/L371LEV/L371LEV-fix52-1-after.png" width="320" alt="L371LEV-fix52-1 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix52-3-before.png" width="320" alt="L371LEV-fix52-3 before"></td><td><img src="Images/L371LEV/L371LEV-fix52-3-after.png" width="320" alt="L371LEV-fix52-3 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix52-5-before.png" width="320" alt="L371LEV-fix52-5 before"></td><td><img src="Images/L371LEV/L371LEV-fix52-5-after.png" width="320" alt="L371LEV-fix52-5 after"></td></tr>
</table>

**Patch 53.** UV review round 4: wrong textures, a face turned the wrong way, the circle panel.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix53-1-before.png" width="320" alt="L371LEV-fix53-1 before"></td><td><img src="Images/L371LEV/L371LEV-fix53-1-after.png" width="320" alt="L371LEV-fix53-1 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix53-2-before.png" width="320" alt="L371LEV-fix53-2 before"></td><td><img src="Images/L371LEV/L371LEV-fix53-2-after.png" width="320" alt="L371LEV-fix53-2 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix53-3-before.png" width="320" alt="L371LEV-fix53-3 before"></td><td><img src="Images/L371LEV/L371LEV-fix53-3-after.png" width="320" alt="L371LEV-fix53-3 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix53-4-before.png" width="320" alt="L371LEV-fix53-4 before"></td><td><img src="Images/L371LEV/L371LEV-fix53-4-after.png" width="320" alt="L371LEV-fix53-4 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix53-5-before.png" width="320" alt="L371LEV-fix53-5 before"></td><td><img src="Images/L371LEV/L371LEV-fix53-5-after.png" width="320" alt="L371LEV-fix53-5 after"></td></tr>
<tr><td><img src="Images/L371LEV/L371LEV-fix53-6-before.png" width="320" alt="L371LEV-fix53-6 before"></td><td><img src="Images/L371LEV/L371LEV-fix53-6-after.png" width="320" alt="L371LEV-fix53-6 after"></td></tr>
</table>

### L381LEV

**Patch 35.** Texture scan review: 1 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L381LEV/L381LEV-fix35-1-before.png" width="320" alt="L381LEV-fix35-1 before"></td><td><img src="Images/L381LEV/L381LEV-fix35-1-after.png" width="320" alt="L381LEV-fix35-1 after"></td></tr>
</table>

### L391LEV

**Patch 19.** Queen's egg sack inner layer: 246 faces turned to run on from each other.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L391LEV/L391LEV-fix19-1-before.png" width="320" alt="L391LEV-fix19-1 before"></td><td><img src="Images/L391LEV/L391LEV-fix19-1-after.png" width="320" alt="L391LEV-fix19-1 after"></td></tr>
<tr><td><img src="Images/L391LEV/L391LEV-fix19-2-before.png" width="320" alt="L391LEV-fix19-2 before"></td><td><img src="Images/L391LEV/L391LEV-fix19-2-after.png" width="320" alt="L391LEV-fix19-2 after"></td></tr>
<tr><td><img src="Images/L391LEV/L391LEV-fix19-3-before.png" width="320" alt="L391LEV-fix19-3 before"></td><td><img src="Images/L391LEV/L391LEV-fix19-3-after.png" width="320" alt="L391LEV-fix19-3 after"></td></tr>
<tr><td><img src="Images/L391LEV/L391LEV-fix19-4-before.png" width="320" alt="L391LEV-fix19-4 before"></td><td><img src="Images/L391LEV/L391LEV-fix19-4-after.png" width="320" alt="L391LEV-fix19-4 after"></td></tr>
</table>

**Patch 36.** Texture scan review: 3 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L391LEV/L391LEV-fix36-1-before.png" width="320" alt="L391LEV-fix36-1 before"></td><td><img src="Images/L391LEV/L391LEV-fix36-1-after.png" width="320" alt="L391LEV-fix36-1 after"></td></tr>
<tr><td><img src="Images/L391LEV/L391LEV-fix36-2-before.png" width="320" alt="L391LEV-fix36-2 before"></td><td><img src="Images/L391LEV/L391LEV-fix36-2-after.png" width="320" alt="L391LEV-fix36-2 after"></td></tr>
</table>

### L900LEV

**Patch 3.** Texture flags wrongly set on 16 faces (as L111LEV).

No screenshot: flag only.

**Patch 5.** Texture flags wrongly set on the above-door wall sides (as L111LEV).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L900LEV/L900LEV-fix05-1-before.png" width="320" alt="L900LEV-fix05-1 before"></td><td><img src="Images/L900LEV/L900LEV-fix05-1-after.png" width="320" alt="L900LEV-fix05-1 after"></td></tr>
<tr><td><img src="Images/L900LEV/L900LEV-fix05-2-before.png" width="320" alt="L900LEV-fix05-2 before"></td><td><img src="Images/L900LEV/L900LEV-fix05-2-after.png" width="320" alt="L900LEV-fix05-2 after"></td></tr>
</table>

**Patch 12.** Lift vertex with a misplaced coordinate (as L111LEV).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L900LEV/L900LEV-fix12-1-before.png" width="320" alt="L900LEV-fix12-1 before"></td><td><img src="Images/L900LEV/L900LEV-fix12-1-after.png" width="320" alt="L900LEV-fix12-1 after"></td></tr>
</table>

**Patch 14.** One face with the wrong texture, turned a quarter-turn (as L111LEV).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L900LEV/L900LEV-fix14-1-before.png" width="320" alt="L900LEV-fix14-1 before"></td><td><img src="Images/L900LEV/L900LEV-fix14-1-after.png" width="320" alt="L900LEV-fix14-1 after"></td></tr>
</table>

**Patch 17.** Starting lift panel mirrored the wrong way (as L111LEV).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L900LEV/L900LEV-fix17-1-before.png" width="320" alt="L900LEV-fix17-1 before"></td><td><img src="Images/L900LEV/L900LEV-fix17-1-after.png" width="320" alt="L900LEV-fix17-1 after"></td></tr>
</table>

**Patch 44.** Four crate faces with the wrong texture or orientation (as L111LEV).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L900LEV/L900LEV-fix44-1-before.png" width="320" alt="L900LEV-fix44-1 before"></td><td><img src="Images/L900LEV/L900LEV-fix44-1-after.png" width="320" alt="L900LEV-fix44-1 after"></td></tr>
<tr><td><img src="Images/L900LEV/L900LEV-fix44-2-before.png" width="320" alt="L900LEV-fix44-2 before"></td><td><img src="Images/L900LEV/L900LEV-fix44-2-after.png" width="320" alt="L900LEV-fix44-2 after"></td></tr>
<tr><td><img src="Images/L900LEV/L900LEV-fix44-3-before.png" width="320" alt="L900LEV-fix44-3 before"></td><td><img src="Images/L900LEV/L900LEV-fix44-3-after.png" width="320" alt="L900LEV-fix44-3 after"></td></tr>
<tr><td><img src="Images/L900LEV/L900LEV-fix44-4-before.png" width="320" alt="L900LEV-fix44-4 before"></td><td><img src="Images/L900LEV/L900LEV-fix44-4-after.png" width="320" alt="L900LEV-fix44-4 after"></td></tr>
</table>

**Patch 60.** Face 4150 flipped vertically, face 6528 given texture 194 and turned (as L111LEV).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L900LEV/L900LEV-fix60-1-before.png" width="320" alt="L900LEV-fix60-1 before"></td><td><img src="Images/L900LEV/L900LEV-fix60-1-after.png" width="320" alt="L900LEV-fix60-1 after"></td></tr>
<tr><td><img src="Images/L900LEV/L900LEV-fix60-2-before.png" width="320" alt="L900LEV-fix60-2 before"></td><td><img src="Images/L900LEV/L900LEV-fix60-2-after.png" width="320" alt="L900LEV-fix60-2 after"></td></tr>
</table>

### L901LEV

**Patch 8.** The multiplayer players' store placed inside the level.

No screenshot: player store position, not drawn.

### L903LEV

**Patch 42.** Two pillars that could be walked through.

No screenshot: cell attributes.

**Patch 46.** Ten crate faces with the wrong texture or orientation (as L141LEV).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L903LEV/L903LEV-fix46-1-before.png" width="320" alt="L903LEV-fix46-1 before"></td><td><img src="Images/L903LEV/L903LEV-fix46-1-after.png" width="320" alt="L903LEV-fix46-1 after"></td></tr>
<tr><td><img src="Images/L903LEV/L903LEV-fix46-2-before.png" width="320" alt="L903LEV-fix46-2 before"></td><td><img src="Images/L903LEV/L903LEV-fix46-2-after.png" width="320" alt="L903LEV-fix46-2 after"></td></tr>
<tr><td><img src="Images/L903LEV/L903LEV-fix46-3-before.png" width="320" alt="L903LEV-fix46-3 before"></td><td><img src="Images/L903LEV/L903LEV-fix46-3-after.png" width="320" alt="L903LEV-fix46-3 after"></td></tr>
<tr><td><img src="Images/L903LEV/L903LEV-fix46-4-before.png" width="320" alt="L903LEV-fix46-4 before"></td><td><img src="Images/L903LEV/L903LEV-fix46-4-after.png" width="320" alt="L903LEV-fix46-4 after"></td></tr>
<tr><td><img src="Images/L903LEV/L903LEV-fix46-5-before.png" width="320" alt="L903LEV-fix46-5 before"></td><td><img src="Images/L903LEV/L903LEV-fix46-5-after.png" width="320" alt="L903LEV-fix46-5 after"></td></tr>
<tr><td><img src="Images/L903LEV/L903LEV-fix46-6-before.png" width="320" alt="L903LEV-fix46-6 before"></td><td><img src="Images/L903LEV/L903LEV-fix46-6-after.png" width="320" alt="L903LEV-fix46-6 after"></td></tr>
</table>

**Patch 56.** UV review: three crate faces (as L141LEV).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L903LEV/L903LEV-fix56-1-before.png" width="320" alt="L903LEV-fix56-1 before"></td><td><img src="Images/L903LEV/L903LEV-fix56-1-after.png" width="320" alt="L903LEV-fix56-1 after"></td></tr>
<tr><td><img src="Images/L903LEV/L903LEV-fix56-2-before.png" width="320" alt="L903LEV-fix56-2 before"></td><td><img src="Images/L903LEV/L903LEV-fix56-2-after.png" width="320" alt="L903LEV-fix56-2 after"></td></tr>
</table>

**Patch 63.** The floor strip's two missing slices added to the sheet (as 141GFX).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L903LEV/903GFX-fix63-before.png" width="320" alt="903GFX-fix63 before"></td><td><img src="Images/L903LEV/903GFX-fix63-after.png" width="320" alt="903GFX-fix63 after"></td></tr>
</table>

**Patch 64.** End room floor laid as in L141LEV (with the sheet's new slices).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L903LEV/L903LEV-fix64-1-before.png" width="320" alt="L903LEV-fix64-1 before"></td><td><img src="Images/L903LEV/L903LEV-fix64-1-after.png" width="320" alt="L903LEV-fix64-1 after"></td></tr>
</table>

### L905LEV

**Patch 1.** Textures flipped when they should not have been, across the level.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-1-before.png" width="320" alt="L905LEV-fix01-1 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-1-after.png" width="320" alt="L905LEV-fix01-1 after"></td></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-2-before.png" width="320" alt="L905LEV-fix01-2 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-2-after.png" width="320" alt="L905LEV-fix01-2 after"></td></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-3-before.png" width="320" alt="L905LEV-fix01-3 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-3-after.png" width="320" alt="L905LEV-fix01-3 after"></td></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-4-before.png" width="320" alt="L905LEV-fix01-4 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-4-after.png" width="320" alt="L905LEV-fix01-4 after"></td></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-5-before.png" width="320" alt="L905LEV-fix01-5 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-5-after.png" width="320" alt="L905LEV-fix01-5 after"></td></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-6-before.png" width="320" alt="L905LEV-fix01-6 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-6-after.png" width="320" alt="L905LEV-fix01-6 after"></td></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-7-before.png" width="320" alt="L905LEV-fix01-7 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-7-after.png" width="320" alt="L905LEV-fix01-7 after"></td></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-8-before.png" width="320" alt="L905LEV-fix01-8 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-8-after.png" width="320" alt="L905LEV-fix01-8 after"></td></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-9-before.png" width="320" alt="L905LEV-fix01-9 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-9-after.png" width="320" alt="L905LEV-fix01-9 after"></td></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-10-before.png" width="320" alt="L905LEV-fix01-10 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-10-after.png" width="320" alt="L905LEV-fix01-10 after"></td></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-11-before.png" width="320" alt="L905LEV-fix01-11 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-11-after.png" width="320" alt="L905LEV-fix01-11 after"></td></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-12-before.png" width="320" alt="L905LEV-fix01-12 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-12-after.png" width="320" alt="L905LEV-fix01-12 after"></td></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix01-13-before.png" width="320" alt="L905LEV-fix01-13 before"></td><td><img src="Images/L905LEV/L905LEV-fix01-13-after.png" width="320" alt="L905LEV-fix01-13 after"></td></tr>
</table>

**Patch 37.** Texture scan review: 2 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L905LEV/L905LEV-fix37-1-before.png" width="320" alt="L905LEV-fix37-1 before"></td><td><img src="Images/L905LEV/L905LEV-fix37-1-after.png" width="320" alt="L905LEV-fix37-1 after"></td></tr>
</table>

### L906LEV

**Patch 0.** Vertex, transparency flags, railings, bridge textures and door textures (the five original L906LEV fixes, one patch).

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-1-before.png" width="320" alt="L906LEV-fix00-1 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-1-after.png" width="320" alt="L906LEV-fix00-1 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-2-before.png" width="320" alt="L906LEV-fix00-2 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-2-after.png" width="320" alt="L906LEV-fix00-2 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-3-before.png" width="320" alt="L906LEV-fix00-3 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-3-after.png" width="320" alt="L906LEV-fix00-3 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-4-before.png" width="320" alt="L906LEV-fix00-4 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-4-after.png" width="320" alt="L906LEV-fix00-4 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-5-before.png" width="320" alt="L906LEV-fix00-5 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-5-after.png" width="320" alt="L906LEV-fix00-5 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-6-before.png" width="320" alt="L906LEV-fix00-6 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-6-after.png" width="320" alt="L906LEV-fix00-6 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-7-before.png" width="320" alt="L906LEV-fix00-7 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-7-after.png" width="320" alt="L906LEV-fix00-7 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-8-before.png" width="320" alt="L906LEV-fix00-8 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-8-after.png" width="320" alt="L906LEV-fix00-8 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-9-before.png" width="320" alt="L906LEV-fix00-9 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-9-after.png" width="320" alt="L906LEV-fix00-9 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-10-before.png" width="320" alt="L906LEV-fix00-10 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-10-after.png" width="320" alt="L906LEV-fix00-10 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-11-before.png" width="320" alt="L906LEV-fix00-11 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-11-after.png" width="320" alt="L906LEV-fix00-11 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-12-before.png" width="320" alt="L906LEV-fix00-12 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-12-after.png" width="320" alt="L906LEV-fix00-12 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-13-before.png" width="320" alt="L906LEV-fix00-13 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-13-after.png" width="320" alt="L906LEV-fix00-13 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-14-before.png" width="320" alt="L906LEV-fix00-14 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-14-after.png" width="320" alt="L906LEV-fix00-14 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-15-before.png" width="320" alt="L906LEV-fix00-15 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-15-after.png" width="320" alt="L906LEV-fix00-15 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-16-before.png" width="320" alt="L906LEV-fix00-16 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-16-after.png" width="320" alt="L906LEV-fix00-16 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-17-before.png" width="320" alt="L906LEV-fix00-17 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-17-after.png" width="320" alt="L906LEV-fix00-17 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-18-before.png" width="320" alt="L906LEV-fix00-18 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-18-after.png" width="320" alt="L906LEV-fix00-18 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-19-before.png" width="320" alt="L906LEV-fix00-19 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-19-after.png" width="320" alt="L906LEV-fix00-19 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix00-20-before.png" width="320" alt="L906LEV-fix00-20 before"></td><td><img src="Images/L906LEV/L906LEV-fix00-20-after.png" width="320" alt="L906LEV-fix00-20 after"></td></tr>
</table>

**Patch 48.** 53 bridge faces with the wrong texture or orientation.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-1-before.png" width="320" alt="L906LEV-fix48-1 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-1-after.png" width="320" alt="L906LEV-fix48-1 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-2-before.png" width="320" alt="L906LEV-fix48-2 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-2-after.png" width="320" alt="L906LEV-fix48-2 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-3-before.png" width="320" alt="L906LEV-fix48-3 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-3-after.png" width="320" alt="L906LEV-fix48-3 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-4-before.png" width="320" alt="L906LEV-fix48-4 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-4-after.png" width="320" alt="L906LEV-fix48-4 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-5-before.png" width="320" alt="L906LEV-fix48-5 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-5-after.png" width="320" alt="L906LEV-fix48-5 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-6-before.png" width="320" alt="L906LEV-fix48-6 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-6-after.png" width="320" alt="L906LEV-fix48-6 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-7-before.png" width="320" alt="L906LEV-fix48-7 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-7-after.png" width="320" alt="L906LEV-fix48-7 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-8-before.png" width="320" alt="L906LEV-fix48-8 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-8-after.png" width="320" alt="L906LEV-fix48-8 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-9-before.png" width="320" alt="L906LEV-fix48-9 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-9-after.png" width="320" alt="L906LEV-fix48-9 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-10-before.png" width="320" alt="L906LEV-fix48-10 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-10-after.png" width="320" alt="L906LEV-fix48-10 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-11-before.png" width="320" alt="L906LEV-fix48-11 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-11-after.png" width="320" alt="L906LEV-fix48-11 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-12-before.png" width="320" alt="L906LEV-fix48-12 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-12-after.png" width="320" alt="L906LEV-fix48-12 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-13-before.png" width="320" alt="L906LEV-fix48-13 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-13-after.png" width="320" alt="L906LEV-fix48-13 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-14-before.png" width="320" alt="L906LEV-fix48-14 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-14-after.png" width="320" alt="L906LEV-fix48-14 after"></td></tr>
<tr><td><img src="Images/L906LEV/L906LEV-fix48-15-before.png" width="320" alt="L906LEV-fix48-15 before"></td><td><img src="Images/L906LEV/L906LEV-fix48-15-after.png" width="320" alt="L906LEV-fix48-15 after"></td></tr>
</table>

### L908LEV

**Patch 38.** Texture scan review: 2 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L908LEV/L908LEV-fix38-1-before.png" width="320" alt="L908LEV-fix38-1 before"></td><td><img src="Images/L908LEV/L908LEV-fix38-1-after.png" width="320" alt="L908LEV-fix38-1 after"></td></tr>
<tr><td><img src="Images/L908LEV/L908LEV-fix38-2-before.png" width="320" alt="L908LEV-fix38-2 before"></td><td><img src="Images/L908LEV/L908LEV-fix38-2-after.png" width="320" alt="L908LEV-fix38-2 after"></td></tr>
</table>

### L909LEV

**Patch 39.** Start room sealed again, as shipped (the multiplayer holding room).

No screenshot: cell attributes (walls), nothing drawn changes.

**Patch 40.** Second holding room sealed like the first.

No screenshot: cell attributes.

**Patch 41.** Texture scan review: 5 face(s) turned or mirrored to run on from their neighbours.

<table><tr><th>Before</th><th>After</th></tr>
<tr><td><img src="Images/L909LEV/L909LEV-fix41-1-before.png" width="320" alt="L909LEV-fix41-1 before"></td><td><img src="Images/L909LEV/L909LEV-fix41-1-after.png" width="320" alt="L909LEV-fix41-1 after"></td></tr>
<tr><td><img src="Images/L909LEV/L909LEV-fix41-2-before.png" width="320" alt="L909LEV-fix41-2 before"></td><td><img src="Images/L909LEV/L909LEV-fix41-2-after.png" width="320" alt="L909LEV-fix41-2 after"></td></tr>
<tr><td><img src="Images/L909LEV/L909LEV-fix41-3-before.png" width="320" alt="L909LEV-fix41-3 before"></td><td><img src="Images/L909LEV/L909LEV-fix41-3-after.png" width="320" alt="L909LEV-fix41-3 after"></td></tr>
<tr><td><img src="Images/L909LEV/L909LEV-fix41-4-before.png" width="320" alt="L909LEV-fix41-4 before"></td><td><img src="Images/L909LEV/L909LEV-fix41-4-after.png" width="320" alt="L909LEV-fix41-4 after"></td></tr>
<tr><td><img src="Images/L909LEV/L909LEV-fix41-5-before.png" width="320" alt="L909LEV-fix41-5 before"></td><td><img src="Images/L909LEV/L909LEV-fix41-5-after.png" width="320" alt="L909LEV-fix41-5 after"></td></tr>
</table>

### Level Cleanup

With Testing > Level Cleanup on, faces no player can ever see are taken out (Notes/cleanup-procedure.md). Nothing visible changes from where the player can stand, so those have no screenshots. What does show is what the cleanup puts back: each level's first cell, whose first face (record 0) the shipped files never draw, gets that face back, and L211LEV gets its one missing face.

<table><tr><th>Level</th><th>Before</th><th>After</th></tr>
<tr><td>L112LEV</td><td><img src="Images/L112LEV/L112LEV-cleanup-firstcell-before.png" width="320" alt="L112LEV-cleanup-firstcell before"></td><td><img src="Images/L112LEV/L112LEV-cleanup-firstcell-after.png" width="320" alt="L112LEV-cleanup-firstcell after"></td></tr>
<tr><td>L122LEV</td><td><img src="Images/L122LEV/L122LEV-cleanup-firstcell-before.png" width="320" alt="L122LEV-cleanup-firstcell before"></td><td><img src="Images/L122LEV/L122LEV-cleanup-firstcell-after.png" width="320" alt="L122LEV-cleanup-firstcell after"></td></tr>
<tr><td>L131LEV</td><td><img src="Images/L131LEV/L131LEV-cleanup-firstcell-before.png" width="320" alt="L131LEV-cleanup-firstcell before"></td><td><img src="Images/L131LEV/L131LEV-cleanup-firstcell-after.png" width="320" alt="L131LEV-cleanup-firstcell after"></td></tr>
<tr><td>L141LEV</td><td><img src="Images/L141LEV/L141LEV-cleanup-firstcell-before.png" width="320" alt="L141LEV-cleanup-firstcell before"></td><td><img src="Images/L141LEV/L141LEV-cleanup-firstcell-after.png" width="320" alt="L141LEV-cleanup-firstcell after"></td></tr>
<tr><td>L154LEV</td><td><img src="Images/L154LEV/L154LEV-cleanup-firstcell-before.png" width="320" alt="L154LEV-cleanup-firstcell before"></td><td><img src="Images/L154LEV/L154LEV-cleanup-firstcell-after.png" width="320" alt="L154LEV-cleanup-firstcell after"></td></tr>
<tr><td>L161LEV</td><td><img src="Images/L161LEV/L161LEV-cleanup-firstcell-before.png" width="320" alt="L161LEV-cleanup-firstcell before"></td><td><img src="Images/L161LEV/L161LEV-cleanup-firstcell-after.png" width="320" alt="L161LEV-cleanup-firstcell after"></td></tr>
<tr><td>L211LEV</td><td><img src="Images/L211LEV/L211LEV-cleanup-added-before.png" width="320" alt="L211LEV-cleanup-added before"></td><td><img src="Images/L211LEV/L211LEV-cleanup-added-after.png" width="320" alt="L211LEV-cleanup-added after"></td></tr>
<tr><td>L211LEV</td><td><img src="Images/L211LEV/L211LEV-cleanup-firstcell-before.png" width="320" alt="L211LEV-cleanup-firstcell before"></td><td><img src="Images/L211LEV/L211LEV-cleanup-firstcell-after.png" width="320" alt="L211LEV-cleanup-firstcell after"></td></tr>
<tr><td>L212LEV</td><td><img src="Images/L212LEV/L212LEV-cleanup-firstcell-before.png" width="320" alt="L212LEV-cleanup-firstcell before"></td><td><img src="Images/L212LEV/L212LEV-cleanup-firstcell-after.png" width="320" alt="L212LEV-cleanup-firstcell after"></td></tr>
<tr><td>L213LEV</td><td><img src="Images/L213LEV/L213LEV-cleanup-firstcell-before.png" width="320" alt="L213LEV-cleanup-firstcell before"></td><td><img src="Images/L213LEV/L213LEV-cleanup-firstcell-after.png" width="320" alt="L213LEV-cleanup-firstcell after"></td></tr>
<tr><td>L222LEV</td><td><img src="Images/L222LEV/L222LEV-cleanup-firstcell-before.png" width="320" alt="L222LEV-cleanup-firstcell before"></td><td><img src="Images/L222LEV/L222LEV-cleanup-firstcell-after.png" width="320" alt="L222LEV-cleanup-firstcell after"></td></tr>
<tr><td>L231LEV</td><td><img src="Images/L231LEV/L231LEV-cleanup-firstcell-before.png" width="320" alt="L231LEV-cleanup-firstcell before"></td><td><img src="Images/L231LEV/L231LEV-cleanup-firstcell-after.png" width="320" alt="L231LEV-cleanup-firstcell after"></td></tr>
<tr><td>L232LEV</td><td><img src="Images/L232LEV/L232LEV-cleanup-firstcell-before.png" width="320" alt="L232LEV-cleanup-firstcell before"></td><td><img src="Images/L232LEV/L232LEV-cleanup-firstcell-after.png" width="320" alt="L232LEV-cleanup-firstcell after"></td></tr>
<tr><td>L242LEV</td><td><img src="Images/L242LEV/L242LEV-cleanup-firstcell-before.png" width="320" alt="L242LEV-cleanup-firstcell before"></td><td><img src="Images/L242LEV/L242LEV-cleanup-firstcell-after.png" width="320" alt="L242LEV-cleanup-firstcell after"></td></tr>
<tr><td>L243LEV</td><td><img src="Images/L243LEV/L243LEV-cleanup-firstcell-before.png" width="320" alt="L243LEV-cleanup-firstcell before"></td><td><img src="Images/L243LEV/L243LEV-cleanup-firstcell-after.png" width="320" alt="L243LEV-cleanup-firstcell after"></td></tr>
<tr><td>L262LEV</td><td><img src="Images/L262LEV/L262LEV-cleanup-firstcell-before.png" width="320" alt="L262LEV-cleanup-firstcell before"></td><td><img src="Images/L262LEV/L262LEV-cleanup-firstcell-after.png" width="320" alt="L262LEV-cleanup-firstcell after"></td></tr>
<tr><td>L263LEV</td><td><img src="Images/L263LEV/L263LEV-cleanup-firstcell-before.png" width="320" alt="L263LEV-cleanup-firstcell before"></td><td><img src="Images/L263LEV/L263LEV-cleanup-firstcell-after.png" width="320" alt="L263LEV-cleanup-firstcell after"></td></tr>
<tr><td>L322LEV</td><td><img src="Images/L322LEV/L322LEV-cleanup-firstcell-before.png" width="320" alt="L322LEV-cleanup-firstcell before"></td><td><img src="Images/L322LEV/L322LEV-cleanup-firstcell-after.png" width="320" alt="L322LEV-cleanup-firstcell after"></td></tr>
<tr><td>L323LEV</td><td><img src="Images/L323LEV/L323LEV-cleanup-firstcell-before.png" width="320" alt="L323LEV-cleanup-firstcell before"></td><td><img src="Images/L323LEV/L323LEV-cleanup-firstcell-after.png" width="320" alt="L323LEV-cleanup-firstcell after"></td></tr>
<tr><td>L324LEV</td><td><img src="Images/L324LEV/L324LEV-cleanup-firstcell-before.png" width="320" alt="L324LEV-cleanup-firstcell before"></td><td><img src="Images/L324LEV/L324LEV-cleanup-firstcell-after.png" width="320" alt="L324LEV-cleanup-firstcell after"></td></tr>
<tr><td>L325LEV</td><td><img src="Images/L325LEV/L325LEV-cleanup-firstcell-before.png" width="320" alt="L325LEV-cleanup-firstcell before"></td><td><img src="Images/L325LEV/L325LEV-cleanup-firstcell-after.png" width="320" alt="L325LEV-cleanup-firstcell after"></td></tr>
<tr><td>L351LEV</td><td><img src="Images/L351LEV/L351LEV-cleanup-firstcell-before.png" width="320" alt="L351LEV-cleanup-firstcell before"></td><td><img src="Images/L351LEV/L351LEV-cleanup-firstcell-after.png" width="320" alt="L351LEV-cleanup-firstcell after"></td></tr>
<tr><td>L352LEV</td><td><img src="Images/L352LEV/L352LEV-cleanup-firstcell-before.png" width="320" alt="L352LEV-cleanup-firstcell before"></td><td><img src="Images/L352LEV/L352LEV-cleanup-firstcell-after.png" width="320" alt="L352LEV-cleanup-firstcell after"></td></tr>
<tr><td>L353LEV</td><td><img src="Images/L353LEV/L353LEV-cleanup-firstcell-before.png" width="320" alt="L353LEV-cleanup-firstcell before"></td><td><img src="Images/L353LEV/L353LEV-cleanup-firstcell-after.png" width="320" alt="L353LEV-cleanup-firstcell after"></td></tr>
<tr><td>L371LEV</td><td><img src="Images/L371LEV/L371LEV-cleanup-firstcell-before.png" width="320" alt="L371LEV-cleanup-firstcell before"></td><td><img src="Images/L371LEV/L371LEV-cleanup-firstcell-after.png" width="320" alt="L371LEV-cleanup-firstcell after"></td></tr>
<tr><td>L381LEV</td><td><img src="Images/L381LEV/L381LEV-cleanup-firstcell-before.png" width="320" alt="L381LEV-cleanup-firstcell before"></td><td><img src="Images/L381LEV/L381LEV-cleanup-firstcell-after.png" width="320" alt="L381LEV-cleanup-firstcell after"></td></tr>
<tr><td>L391LEV</td><td><img src="Images/L391LEV/L391LEV-cleanup-firstcell-before.png" width="320" alt="L391LEV-cleanup-firstcell before"></td><td><img src="Images/L391LEV/L391LEV-cleanup-firstcell-after.png" width="320" alt="L391LEV-cleanup-firstcell after"></td></tr>
<tr><td>L901LEV</td><td><img src="Images/L901LEV/L901LEV-cleanup-firstcell-before.png" width="320" alt="L901LEV-cleanup-firstcell before"></td><td><img src="Images/L901LEV/L901LEV-cleanup-firstcell-after.png" width="320" alt="L901LEV-cleanup-firstcell after"></td></tr>
<tr><td>L903LEV</td><td><img src="Images/L903LEV/L903LEV-cleanup-firstcell-before.png" width="320" alt="L903LEV-cleanup-firstcell before"></td><td><img src="Images/L903LEV/L903LEV-cleanup-firstcell-after.png" width="320" alt="L903LEV-cleanup-firstcell after"></td></tr>
<tr><td>L904LEV</td><td><img src="Images/L904LEV/L904LEV-cleanup-firstcell-before.png" width="320" alt="L904LEV-cleanup-firstcell before"></td><td><img src="Images/L904LEV/L904LEV-cleanup-firstcell-after.png" width="320" alt="L904LEV-cleanup-firstcell after"></td></tr>
<tr><td>L906LEV</td><td><img src="Images/L906LEV/L906LEV-cleanup-firstcell-before.png" width="320" alt="L906LEV-cleanup-firstcell before"></td><td><img src="Images/L906LEV/L906LEV-cleanup-firstcell-after.png" width="320" alt="L906LEV-cleanup-firstcell after"></td></tr>
<tr><td>L907LEV</td><td><img src="Images/L907LEV/L907LEV-cleanup-firstcell-before.png" width="320" alt="L907LEV-cleanup-firstcell before"></td><td><img src="Images/L907LEV/L907LEV-cleanup-firstcell-after.png" width="320" alt="L907LEV-cleanup-firstcell after"></td></tr>
<tr><td>L909LEV</td><td><img src="Images/L909LEV/L909LEV-cleanup-firstcell-before.png" width="320" alt="L909LEV-cleanup-firstcell before"></td><td><img src="Images/L909LEV/L909LEV-cleanup-firstcell-after.png" width="320" alt="L909LEV-cleanup-firstcell after"></td></tr>
</table>

### Texture Blanking

With Testing > Level Cleanup on, every part of a level's texture sheet that no face, animation frame, door or lift draws is filled with its page's background colour, the most common colour of the space blanked (Notes/texture-use.md). Each image shows the sheet's changed pages side by side.

<table><tr><th>Sheet</th><th>Before</th><th>After</th></tr>
<tr><td>111GFX</td><td><img src="Images/L111LEV/111GFX-blanked-before.png" width="320" alt="111GFX-blanked before"></td><td><img src="Images/L111LEV/111GFX-blanked-after.png" width="320" alt="111GFX-blanked after"></td></tr>
<tr><td>112GFX</td><td><img src="Images/L112LEV/112GFX-blanked-before.png" width="320" alt="112GFX-blanked before"></td><td><img src="Images/L112LEV/112GFX-blanked-after.png" width="320" alt="112GFX-blanked after"></td></tr>
<tr><td>113GFX</td><td><img src="Images/L113LEV/113GFX-blanked-before.png" width="320" alt="113GFX-blanked before"></td><td><img src="Images/L113LEV/113GFX-blanked-after.png" width="320" alt="113GFX-blanked after"></td></tr>
<tr><td>114GFX</td><td><img src="Images/L114LEV/114GFX-blanked-before.png" width="320" alt="114GFX-blanked before"></td><td><img src="Images/L114LEV/114GFX-blanked-after.png" width="320" alt="114GFX-blanked after"></td></tr>
<tr><td>115GFX</td><td><img src="Images/L115LEV/115GFX-blanked-before.png" width="320" alt="115GFX-blanked before"></td><td><img src="Images/L115LEV/115GFX-blanked-after.png" width="320" alt="115GFX-blanked after"></td></tr>
<tr><td>122GFX</td><td><img src="Images/L122LEV/122GFX-blanked-before.png" width="320" alt="122GFX-blanked before"></td><td><img src="Images/L122LEV/122GFX-blanked-after.png" width="320" alt="122GFX-blanked after"></td></tr>
<tr><td>131GFX</td><td><img src="Images/L131LEV/131GFX-blanked-before.png" width="320" alt="131GFX-blanked before"></td><td><img src="Images/L131LEV/131GFX-blanked-after.png" width="320" alt="131GFX-blanked after"></td></tr>
<tr><td>141GFX</td><td><img src="Images/L141LEV/141GFX-blanked-before.png" width="320" alt="141GFX-blanked before"></td><td><img src="Images/L141LEV/141GFX-blanked-after.png" width="320" alt="141GFX-blanked after"></td></tr>
<tr><td>154GFX</td><td><img src="Images/L154LEV/154GFX-blanked-before.png" width="320" alt="154GFX-blanked before"></td><td><img src="Images/L154LEV/154GFX-blanked-after.png" width="320" alt="154GFX-blanked after"></td></tr>
<tr><td>155GFX</td><td><img src="Images/L155LEV/155GFX-blanked-before.png" width="320" alt="155GFX-blanked before"></td><td><img src="Images/L155LEV/155GFX-blanked-after.png" width="320" alt="155GFX-blanked after"></td></tr>
<tr><td>161GFX</td><td><img src="Images/L161LEV/161GFX-blanked-before.png" width="320" alt="161GFX-blanked before"></td><td><img src="Images/L161LEV/161GFX-blanked-after.png" width="320" alt="161GFX-blanked after"></td></tr>
<tr><td>162GFX</td><td><img src="Images/L162LEV/162GFX-blanked-before.png" width="320" alt="162GFX-blanked before"></td><td><img src="Images/L162LEV/162GFX-blanked-after.png" width="320" alt="162GFX-blanked after"></td></tr>
<tr><td>211GFX</td><td><img src="Images/L211LEV/211GFX-blanked-before.png" width="320" alt="211GFX-blanked before"></td><td><img src="Images/L211LEV/211GFX-blanked-after.png" width="320" alt="211GFX-blanked after"></td></tr>
<tr><td>212GFX</td><td><img src="Images/L212LEV/212GFX-blanked-before.png" width="320" alt="212GFX-blanked before"></td><td><img src="Images/L212LEV/212GFX-blanked-after.png" width="320" alt="212GFX-blanked after"></td></tr>
<tr><td>213GFX</td><td><img src="Images/L213LEV/213GFX-blanked-before.png" width="320" alt="213GFX-blanked before"></td><td><img src="Images/L213LEV/213GFX-blanked-after.png" width="320" alt="213GFX-blanked after"></td></tr>
<tr><td>222GFX</td><td><img src="Images/L222LEV/222GFX-blanked-before.png" width="320" alt="222GFX-blanked before"></td><td><img src="Images/L222LEV/222GFX-blanked-after.png" width="320" alt="222GFX-blanked after"></td></tr>
<tr><td>231GFX</td><td><img src="Images/L231LEV/231GFX-blanked-before.png" width="320" alt="231GFX-blanked before"></td><td><img src="Images/L231LEV/231GFX-blanked-after.png" width="320" alt="231GFX-blanked after"></td></tr>
<tr><td>232GFX</td><td><img src="Images/L232LEV/232GFX-blanked-before.png" width="320" alt="232GFX-blanked before"></td><td><img src="Images/L232LEV/232GFX-blanked-after.png" width="320" alt="232GFX-blanked after"></td></tr>
<tr><td>242GFX</td><td><img src="Images/L242LEV/242GFX-blanked-before.png" width="320" alt="242GFX-blanked before"></td><td><img src="Images/L242LEV/242GFX-blanked-after.png" width="320" alt="242GFX-blanked after"></td></tr>
<tr><td>243GFX</td><td><img src="Images/L243LEV/243GFX-blanked-before.png" width="320" alt="243GFX-blanked before"></td><td><img src="Images/L243LEV/243GFX-blanked-after.png" width="320" alt="243GFX-blanked after"></td></tr>
<tr><td>262GFX</td><td><img src="Images/L262LEV/262GFX-blanked-before.png" width="320" alt="262GFX-blanked before"></td><td><img src="Images/L262LEV/262GFX-blanked-after.png" width="320" alt="262GFX-blanked after"></td></tr>
<tr><td>263GFX</td><td><img src="Images/L263LEV/263GFX-blanked-before.png" width="320" alt="263GFX-blanked before"></td><td><img src="Images/L263LEV/263GFX-blanked-after.png" width="320" alt="263GFX-blanked after"></td></tr>
<tr><td>311GFX</td><td><img src="Images/L311LEV/311GFX-blanked-before.png" width="320" alt="311GFX-blanked before"></td><td><img src="Images/L311LEV/311GFX-blanked-after.png" width="320" alt="311GFX-blanked after"></td></tr>
<tr><td>321GFX</td><td><img src="Images/L321LEV/321GFX-blanked-before.png" width="320" alt="321GFX-blanked before"></td><td><img src="Images/L321LEV/321GFX-blanked-after.png" width="320" alt="321GFX-blanked after"></td></tr>
<tr><td>322GFX</td><td><img src="Images/L322LEV/322GFX-blanked-before.png" width="320" alt="322GFX-blanked before"></td><td><img src="Images/L322LEV/322GFX-blanked-after.png" width="320" alt="322GFX-blanked after"></td></tr>
<tr><td>323GFX</td><td><img src="Images/L323LEV/323GFX-blanked-before.png" width="320" alt="323GFX-blanked before"></td><td><img src="Images/L323LEV/323GFX-blanked-after.png" width="320" alt="323GFX-blanked after"></td></tr>
<tr><td>324GFX</td><td><img src="Images/L324LEV/324GFX-blanked-before.png" width="320" alt="324GFX-blanked before"></td><td><img src="Images/L324LEV/324GFX-blanked-after.png" width="320" alt="324GFX-blanked after"></td></tr>
<tr><td>325GFX</td><td><img src="Images/L325LEV/325GFX-blanked-before.png" width="320" alt="325GFX-blanked before"></td><td><img src="Images/L325LEV/325GFX-blanked-after.png" width="320" alt="325GFX-blanked after"></td></tr>
<tr><td>331GFX</td><td><img src="Images/L331LEV/331GFX-blanked-before.png" width="320" alt="331GFX-blanked before"></td><td><img src="Images/L331LEV/331GFX-blanked-after.png" width="320" alt="331GFX-blanked after"></td></tr>
<tr><td>351GFX</td><td><img src="Images/L351LEV/351GFX-blanked-before.png" width="320" alt="351GFX-blanked before"></td><td><img src="Images/L351LEV/351GFX-blanked-after.png" width="320" alt="351GFX-blanked after"></td></tr>
<tr><td>352GFX</td><td><img src="Images/L352LEV/352GFX-blanked-before.png" width="320" alt="352GFX-blanked before"></td><td><img src="Images/L352LEV/352GFX-blanked-after.png" width="320" alt="352GFX-blanked after"></td></tr>
<tr><td>353GFX</td><td><img src="Images/L353LEV/353GFX-blanked-before.png" width="320" alt="353GFX-blanked before"></td><td><img src="Images/L353LEV/353GFX-blanked-after.png" width="320" alt="353GFX-blanked after"></td></tr>
<tr><td>371GFX</td><td><img src="Images/L371LEV/371GFX-blanked-before.png" width="320" alt="371GFX-blanked before"></td><td><img src="Images/L371LEV/371GFX-blanked-after.png" width="320" alt="371GFX-blanked after"></td></tr>
<tr><td>381GFX</td><td><img src="Images/L381LEV/381GFX-blanked-before.png" width="320" alt="381GFX-blanked before"></td><td><img src="Images/L381LEV/381GFX-blanked-after.png" width="320" alt="381GFX-blanked after"></td></tr>
<tr><td>391GFX</td><td><img src="Images/L391LEV/391GFX-blanked-before.png" width="320" alt="391GFX-blanked before"></td><td><img src="Images/L391LEV/391GFX-blanked-after.png" width="320" alt="391GFX-blanked after"></td></tr>
<tr><td>900GFX</td><td><img src="Images/L900LEV/900GFX-blanked-before.png" width="320" alt="900GFX-blanked before"></td><td><img src="Images/L900LEV/900GFX-blanked-after.png" width="320" alt="900GFX-blanked after"></td></tr>
<tr><td>901GFX</td><td><img src="Images/L901LEV/901GFX-blanked-before.png" width="320" alt="901GFX-blanked before"></td><td><img src="Images/L901LEV/901GFX-blanked-after.png" width="320" alt="901GFX-blanked after"></td></tr>
<tr><td>902GFX</td><td><img src="Images/L902LEV/902GFX-blanked-before.png" width="320" alt="902GFX-blanked before"></td><td><img src="Images/L902LEV/902GFX-blanked-after.png" width="320" alt="902GFX-blanked after"></td></tr>
<tr><td>903GFX</td><td><img src="Images/L903LEV/903GFX-blanked-before.png" width="320" alt="903GFX-blanked before"></td><td><img src="Images/L903LEV/903GFX-blanked-after.png" width="320" alt="903GFX-blanked after"></td></tr>
<tr><td>904GFX</td><td><img src="Images/L904LEV/904GFX-blanked-before.png" width="320" alt="904GFX-blanked before"></td><td><img src="Images/L904LEV/904GFX-blanked-after.png" width="320" alt="904GFX-blanked after"></td></tr>
<tr><td>905GFX</td><td><img src="Images/L905LEV/905GFX-blanked-before.png" width="320" alt="905GFX-blanked before"></td><td><img src="Images/L905LEV/905GFX-blanked-after.png" width="320" alt="905GFX-blanked after"></td></tr>
<tr><td>906GFX</td><td><img src="Images/L906LEV/906GFX-blanked-before.png" width="320" alt="906GFX-blanked before"></td><td><img src="Images/L906LEV/906GFX-blanked-after.png" width="320" alt="906GFX-blanked after"></td></tr>
<tr><td>907GFX</td><td><img src="Images/L907LEV/907GFX-blanked-before.png" width="320" alt="907GFX-blanked before"></td><td><img src="Images/L907LEV/907GFX-blanked-after.png" width="320" alt="907GFX-blanked after"></td></tr>
<tr><td>908GFX</td><td><img src="Images/L908LEV/908GFX-blanked-before.png" width="320" alt="908GFX-blanked before"></td><td><img src="Images/L908LEV/908GFX-blanked-after.png" width="320" alt="908GFX-blanked after"></td></tr>
<tr><td>909GFX</td><td><img src="Images/L909LEV/909GFX-blanked-before.png" width="320" alt="909GFX-blanked before"></td><td><img src="Images/L909LEV/909GFX-blanked-after.png" width="320" alt="909GFX-blanked after"></td></tr>
</table>

### Bytes Trimmed And Updated

Across the whole patching routine, on all 45 levels: **437,956 bytes trimmed** (38,316 by the duplicate and unused door and lift models, 399,640 by the Level Cleanup) and **2,757,525 bytes updated** (2,712 by the fixes to the levels, 24,342 by the fixes to the sheets and 2,730,471 by the texture blanking). The level files go from 18,835,536 bytes to 18,397,580. The table for every level is in Notes/patching-byte-counts.md.


## Easter Eggs

Here is a collection of the easter eggs I have found within the games files.

## Easter Egg 1

Object type 33 is the Steel Coil, which may be a nod to Grade 33 steel, the first widely adopted U.S. structural steel grade (ASTM A15, 1911).

<div align="center">
  <img src="Images/steel33.png" alt="Steel Grade 33">
</div>

The object type is stored in a byte (0-255) so if the assignment is random and every one of the 256 potential values is equally likely, the chance the Steel Coil specifically lands on 33 is 1 ÷ 256 = 0.00390625 = 0.390625% (~0.39%).

## Easter Egg 2

These two level textures contain the message "SATAN LIVES HERE"

<div align="center">
  <img src="Images/easteregg-endtimes.png" alt="End Times!">
</div>

351GFX_TP00 and 371GFX_TP00 are the names of the textures when extracted by this toolkit.

## Easter Egg 3

This texture contains a reference to the cheat "RIPLEYDOESITWITHBIGGUNS" as well as two messages.

<div align="center">
  <img src="Images/easteregg-cheats.png" alt="Cheats">
</div>

909GFX_TP00 is the name of the texture when extracted by this toolkit.