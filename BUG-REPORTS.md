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
