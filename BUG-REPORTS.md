# ALTEngine bug reports

Bugs reported by players, newest last. Each entry keeps the report as it was given, what was found, and
where it stands. When one is fixed, the fix goes in `PATCH-NOTES.md` under the version that carries it, and
the entry here is marked fixed with that version.

---

## 1. Cutscenes skipped in languages other than English

- **Reported by:** Pox83, on Discord (0.9.7)
- **Status:** fixed in 0.9.8
- **Report:** "When using a language other than English, skip the cutscenes, as they are identified by a
  different name."

**What happens.** With French, Italian or Spanish chosen, ALTEngine looks only for that language's video:
the name with the language's letter in front, cut to eight characters (`INTRO` becomes `FINTRO.AVI`,
`GAMEOVER` becomes `FGAMEOVE.AVI`; `Bootstrap/Localization.h`, `LocalizedBaseName`). If that file isn't
there, for instance on an English disc or installation, the video is skipped without a word on screen
(only "Could not find ... - skipping." in the log).

The same lookup is in two places:
- `main.cpp`: the start-up videos (the intro);
- `Screens/GameOverSequence.h`, `PlayOne`: the death videos and the game-complete video.

**Fixed (0.9.8).** When the chosen language's video isn't there, the game plays the one that is: it tries the
chosen language's name, then the English name (no letter), then French, Italian and Spanish. One function,
`FindLocalizedVideo` in `Bootstrap/Localization.h`, does this for the intro, the death videos and the
game-complete video.

The other per-language files had the same gap and now fall back the same way: the mission briefing text
(`MISSION?.TXT`) and the computer and panel graphics (`PNL0GFX?.16`, `PNL1GFX?.16`) try the chosen
language's letter first, then every other letter (U, E, F, I, S, G, J). Before, with French, Italian or
Spanish chosen, a disc without that language showed no briefing and no panel graphics. The speech has no
language versions, so it needs nothing.

---

## 2. Smoothed textures: marks on the main door, pixels missing from the firing pulse rifle

- **Reported by:** a player on Discord (0.9.7)
- **Status:** rifle likely fixed in 0.9.9; door open, screenshot needed
- **Report:** "Images with the "smoothed" filter applied show artifacts on the main door, and pixels are missing
  from the pulse rifle when it fires."

**What is known.** Smoothed (Options > Graphics > Quality) switches the level's and the models' textures to
bilinear filtering (`Renderer/ModelRenderer.cpp`, `samplerLinear`). The textures are packed together on shared
sheets, so at the edge of a face the filter blends in a little of the texture next to it on the sheet: the
likely cause of the marks on the door. Cut-out texels (the see-through ones) are dropped where the filtered
alpha falls under one half (`model.frag`, `clip(color.a - 0.5)`), which can eat thin edges.

The weapon in hand is not filtered at all (it is drawn with nearest sampling whatever the setting,
`Renderer/WeaponView.cpp`), so the rifle's missing pixels may be the muzzle flash or the rounds, which are
drawn in the world, or something else.

**To do.** Screenshots of both, Smoothed and Original, to see which faces and which part of the rifle. Then
the likely fix for the door: keep each face's texture lookup half a texel inside its own rectangle on the
sheet while smoothing (Original is unaffected and stays as it is).


**Update (0.9.9).** The pulse rifle half is likely fixed: the port was drawing the rifle's GRENADE art when it
fired (PULSE.B16's sections run idle, grenade, firing, reload, and the port took them in state order), so its
firing frames were the wrong pictures - with or without smoothing. The door's marks are still open: a
screenshot would confirm.
---

## 3. Lift and bridge switches can only be used once

- **Reported by:** two players on Discord (0.9.7)
- **Status:** fixed in 0.9.8
- **Reports:**
  - "For me, the biggest problem is that some switches for elevators and bridges cannot be pressed more than
    once - they don't become reactivatable - which forces you to replay the level."
  - "For example, I'm playing level 2.3.2, and the elevators can't be reactivated if you don't hit the switches
    on the first try; the small switches become unpressable, leaving you stuck inside that area. In the
    original game, those small pillars could be reactivated whenever needed."

**What happened.** The port kept every switch thrown for good once used. In the original, throwing a switch
(FUN_00036f4c) is handed the trigger's uses left: on a trigger's last use the switch stays thrown (state 1), but
on any other (unlimited, or more than one left) it is given state 2, and the switch's own task (LAB_0003846c,
then 0x380f4) plays its 8 frames, holds, and 72 ticks after the throw puts state 2 back to 0 (0x38380), ready
again. Only the four switch types (0x16, 0x18, 0x1a, 0x1b) run that task; cryopods and the rest stay thrown.

On L232, lift 3 (cells 22,29) is called by two switches (triggers 7 and 23, unlimited uses, each "throw the
switch, then add 2 to the lift"); after one ride the lift came back down and could not be called again.

**Fixed (0.9.8).** `Game/Triggers.cpp` passes the trigger's uses to the switch handler; `GameplayScreen.cpp`
gives such a switch state 2 and counts it back to 0 after 72 ticks (a saved game restarts the wait). 35
switches on 11 levels are of this kind: L131, L141 (10), L213, L231, L232, L262, L311, L331, L371, L905, L908.

---

## 4. L232: a missing texture in the boiler room, near a grate

- **Reported by:** a player on Discord (0.9.7), with report 3
- **Status:** fixed in 0.9.9
- **Report:** "By the way, in this level, a texture is missing in the boiler room, near a grate."

**What it is.** Edward found the hole (Face Index Display, Level Cleanup off): the wall between faces 179, 3, 1
and 7. It is the level's record 0 (vertices 6552, 6553, 6550, 6551, texture 194), the first face of the level's
first cell, which the original never draws. With Level Cleanup on (the default) ALTEngine already puts it back,
with its shipped texture 194; with it off, it is missing.

**Fixed (0.9.9).** The missing faces are now a global stage of their own (`data/MissingFaces.json`, ALTViewer's
`MissingFaces.cs`), run after every byte patch with or without the Level Cleanup, so the wall is there in the
original game too. Record 0's copy is given texture 193, flipped horizontally (routine 2), as Edward picked.

---

## 5. Face hugger on the face: part of the view shows in the bottom-left corner

- **Reported by:** a player on Discord (0.9.7)
- **Status:** open, screenshot needed
- **Report:** "In general, from the very beginning of the game - when you are attacked by a facehugger - a small
  portion of the image remains visible in the bottom-left corner; this isn't present in the original game,
  which I often play via DOSBox."

**What is known.** Since 0.9.6 the two empty rows under each frame of the face hugger's art are filled with
copies of its last two rows (`Screens/FaceHugOverlay.h`, `RepeatBottomRows`), so the art reaches the bottom of
the screen. Where those rows are themselves see-through, at the left, the copies are too, and the view shows
through: probably this corner. The original draws on a 320x200 screen, so its bottom rows may simply be off
screen.

**To do.** A screenshot (ideally the same moment in DOSBox), to see whether the gap is in the art's own rows
or below them. That decides the fix: fill the see-through corner, or move the art down. To fold into the
planned file fix of FINGERS.B16 (the ALTEngine-only patch).

---

## 6. Smart gun: firing again and again, its firing sound stops

- **Reported by:** Edward, in play (0.9.8)
- **Status:** fixed in 0.9.9
- **Report:** "repetitive firing of the smart gun makes the firing sound stop"

**What happened.** Two things together.
- The smart gun's original firing animation has four frames drawn from two images; the port counted images
  instead of frames, rejected the original animation, and used a stand-in without the sound cue. Its report
  then went through the general sound voices, which refuse a sound that is still playing.
- And SDL3 never reported a finished sound as finished: without a flush it keeps a few resampler frames
  queued for ever. So after the first shot the report was "still playing" for good, and refused.

**Fixed (0.9.9).** Every one-shot sound is flushed (`Audio/SfxPlayer.cpp`), so it reads finished once played
- which also fixes sounds going missing at random everywhere. And each weapon state now maps its frames to
images (`Renderer/WeaponData.h`), so the smart gun, shotgun, pulse rifle and flamethrower play their own
animations with their own cues.
