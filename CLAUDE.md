# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

"Boing!" (CMake project name `Boing`) — a small vertical-jumper game in C99 built on [raylib](https://www.raylib.com/). raylib is a **git submodule** pinned at `lib/raylib/` (see `.gitmodules`) and built from source as a CMake subdirectory, so there is no system dependency to install. A plain `git clone` leaves the submodule empty — clone with `--recurse-submodules` or run `git submodule update --init`.

## Design Philosophy: Homage vs. Plagiarism

This game takes inspiration from "Mega Jump" (Halfbrick). Inspiration is fine; copying is not. The rule for all future work — code, art, audio, gameplay, text — is: **everything ships original.** raylib and its licensed deps are the only third-party code.

**Homage (allowed) — borrow the *genre*, not the *game*:**
- Core mechanic class: endless vertical climb, collect-to-boost, dodge-or-die hazards.
- Generic power-up *concepts* (a magnet that pulls pickups, a rocket that thrusts up) — as long as behaviour, art, and naming are our own.
- Standard arcade structure: menu → play → game over → retry, high score, screen shake juice.

**Plagiarism / infringement (never) — the specifics that identify *their* product:**
- Copying or tracing exact asset designs (their character, coin, monster sprites, logo, fonts).
- Cloning their unique UI layout, screen composition, or menu flow pixel-for-pixel.
- Reusing proprietary names: the title "Mega Jump", named characters, named worlds, named power-ups from their game. Our title and every proper noun must be new (see naming brainstorm).
- Duplicating exact level configurations, spawn tables, or tuning values lifted from their game.
- Copying their music/SFX, or reproducing their audio by ear.

**Rules for future development:**
1. All assets are procedurally generated or authored here from scratch — no ripped/downloaded sprites, fonts, or audio. (The project already does this: procedural chiptune, primitive-drawn icons, no asset files.)
2. Proper nouns (title, characters, power-ups, worlds) are original inventions, not renamed copies.
3. Gameplay values are hand-tuned for *our* feel, not measured from theirs.
4. When in doubt, ask: "does this identify their product, or just the genre?" Genre = fine, product identity = stop.
5. Keep the code comment/README references to "Mega Jump" as *inspiration credit* only — never as a claim of association.

## Size limit — 1.44MB (hard constraint)

This project targets the [2PG Arcade 1.44MB contest](https://2pgarcade.com/contest-144mb.html): the final shipped game (executable + runtime + any assets, after decompression) must be **≤ 1,474,560 bytes** — one 3.5" floppy disk. Submission window closes 2026-09-04.

- The default `build/` **is** the shippable build: `CMakeLists.txt` defaults `CMAKE_BUILD_TYPE` to `MinSizeRel` and strips the binary in a POST_BUILD step, so `ls -l build/Boing` is the real shipped size — no separate `build-minsize`.
  ```sh
  ls -l build/Boing
  ```
  As of 2026-07: ~1.05MB stripped (1,047,608 bytes), ~427KB headroom.
- Prefer procedural generation (like the existing chiptune) over asset files; every asset counts against the budget.

## Build & run

```sh
cmake -S . -B build      # configure (first time, or after CMakeLists/new .c files)
cmake --build build      # compile; produces ./build/Boing
./build/Boing         # run
```

- `CMakeLists.txt` globs `src/*.c` (`GLOB_RECURSE`), so **adding or removing a `.c` file requires re-running `cmake -S . -B build`** for it to be picked up.
- The build defaults to `MinSizeRel` + auto-strip (the 1.44MB budget is the point), so `build/Boing` is already the size-optimized, stripped binary. Override with `-DCMAKE_BUILD_TYPE=Debug` for a debug build.
- macOS links Cocoa/IOKit/CoreVideo/OpenGL frameworks automatically (`if(APPLE)` block).
- `compile_flags.txt` exists only to feed include paths (`include/`, `lib/raylib/src`) to clangd/the editor — it is not part of the build.
- **clang-tidy runs on every build** (LLVM defaults via `.clang-tidy` — `clang-analyzer-*` only, kept minimal because the wider modules flood a raylib game with float↔int narrowing noise; warnings don't fail the build). It's scoped to our target (the `C_CLANG_TIDY` target property), never the raylib submodule.
- **clang-format** (LLVM style via `.clang-format`): `cmake --build build --target format` reformats `src/*.c` + `include/*.h` in place. Both tools are gated on being found, so a machine without them still builds.
- No test target is configured.

## Layout & architecture

Code lives in `src/` + `include/` as header/impl pairs. `src/main.c` owns the game loop and the `GameState` machine (MENU → PLAYING ⇄ PAUSED, PLAYING → GAMEOVER → PLAYING, plus SETTINGS reachable from MENU or PAUSED; pause HOME → MENU; menu EXIT sets a `quit` flag that ends the loop). It is an explicit FSM: a `Game` context struct (player/world/settings/highScore/state/…) threaded through a `STATES[]` dispatch table that maps each `GameState` to `{update, draw}` handler function pointers — the loop is just `STATES[state].update(&game)` then `.draw`. Gameplay runs on a **fixed 60Hz timestep accumulator** so physics is framerate-independent; the whole frame is drawn into a fixed 800×800 `RenderTexture` then blitted scaled to the window (`DrawTexturePro`, no letterbox) so the resolution setting can resize the window without touching draw code. MENU runs **attract/demo mode** (`updateAttract` wanders the blob); Space/PLAY carries the same blob straight into gameplay. MENU also has a `?` button that toggles a static "How to Play" overlay (`showHelp` flag on `Game` + `drawHelpOverlay`/`HELP_LINES` in `main.c`) — plain text, no interactive walkthrough. Gameplay is a [Mega Jump](https://www.halfbrick.com/games/megajump)-style endless climber: collecting coins boosts the player upward, falling below the view kills. Subsystems:

- **core** (`core.h`, header-only) — `GameState` enum, shared screen constants (`SCREEN_WIDTH`/`SCREEN_HEIGHT`/`FLOOR_Y`), and the shared `clampf` math helper. Modules read these instead of taking screen-size parameters.
- **theme** (`theme.h` + `theme.c`) — the palette is **runtime state** now (multi-theme). `Theme` is a struct of every scene `Color` (sky/sun/cloud/player/coin/grass/monster/power-up + a `confetti[6]`) plus a `backdrop` style (`BACKDROP_CLOUDS`/`BACKDROP_BLOCKS`) and a `track` id. `THEMES[]` in `theme.c` is the table (row 0 = Nature, row 1 = Arcade/"tetris"); adding a level is one more row. Each frame `themeUpdate(ascent)` recomputes the global `gTheme` by lerping the whole leading run of `Color` fields (via `offsetof(Theme, backdrop)`) between the two neighbor themes over `BAND_HEIGHT` (~16000 units of climb) — so the **whole scene morphs continuously** as you climb. The named `Color` macros (`CANDY_PINK`, `COIN_HI`, `CLOUD`, `GRAPE`, `SKY_TOP`, …) are now `#define`d to read `gTheme` fields, so every existing inline draw site is unchanged. `themeBlend(&lo,&hi,&f)` exposes the neighbor indices + factor for the background crossfade; `themeTrack()` gives the current music/SFX track (switches at the band midpoint). **Static initializers cannot use these macros anymore** (they're runtime lvalues): `POWERUP_DEFS[]` HUD colors use fixed raylib constants, and `main.c` calls `themeUpdate(0.0f)` once before `createPlayer` so the player copies the Nature palette.
- **player** (`player.c`/`.h`) — pure physics + input, knows nothing about the level. `Player` (float `Vector2` position/velocity/acceleration + `radius`, plus a `trail[TRAIL_LEN]` ring buffer for the motion trail). `createPlayer` returns the canonical spawn state (gravity lives in `acceleration.y`); `handlePlayerInput(player, targetX)` reads arrows and, with no arrow held, steers the blob toward `targetX` (the mouse column; `main.c` passes a sentinel to ignore it); `updatePlayer` integrates, damps, **wraps horizontally** (off one edge → back on the other, shifting the trail by the same delta so it doesn't streak), and pushes the trail; `updateAttract` is the menu demo — wander toward a random target at a steady pace, wrap the edges, bob at the floor line; `drawPlayer` renders a candy googly-blob: motion trail, velocity-driven squash-&-stretch (`DrawEllipse`), pupils that lean toward movement, and an expression mouth (`DrawRing` smile/frown or `DrawLineEx` neutral) selected by `player->expression` (`Expression` enum — set it on game events for future happy/sad reactions). Blob colors come from the player's `Character` (body/bodyDark) so a new character is just a different palette; the shared trail-write is `pushTrail`, and the attract wander state lives on `Player` (no hidden statics).
- **world** (`world.c`/`.h`) — level content, camera, score. World-space coordinates, up = −y; camera (`cameraY`, top of view) only moves up. Fixed pools all follow the same spawn-above-view / cull-below-view idiom: `Coin` (`MAX_COINS`, trail with random-walk x within `COIN_WANDER`, ~55px apart), one-way `Platform`s (`MAX_PLATFORMS`, ~300px apart, swept feet check, no tunneling), `Monster`s (`MAX_MONSTERS`, ~600px bands, 1-in-3 chance, drift horizontally, collision sets `world.dead`), `PowerUp`s (`MAX_POWERUPS`, ~1500px, Magnet or Rocket), and `Particle`s (`MAX_PARTICLES`, radial burst on coin-collect/bounce, gravity + decay). Power-up behaviour is table-driven: `POWERUP_DEFS[POWERUP_COUNT]` (duration/color/label/spawnWeight/pickupShake/invincible) indexed by `PowerUpType`, with per-type timers in `powerTimer[POWERUP_COUNT]` — a new power-up is a new enum value + one table row. `worldInvincible()` reads the `invincible` flag, so monster immunity is a query, not a per-type special-case. The uniform cull/draw scans use the local `CULL`/`FOREACH_ACTIVE` macros in `world.c`; spawn stays per-type (each pool's spawn logic genuinely differs). `updateWorld` owns floor/platform bounce, coin-collect boost, magnet pull, rocket thrust, and monster hits (all write `player->velocity.y` or `world.dead`), plus decaying `world.shake` (screen shake) and `world.flash` (collect pop-ring); game-over is `isPlayerFallen(...) || world.dead`; `getScore` = ascent/10 + 5·coins. `drawWorld` is all cartoon: grassy ground, cloud-pad platforms, spinning glowing coins, wobbling grape monsters with faces, bobbing/rotating power-ups (horseshoe magnet, flame-flicker rocket), confetti-fleck particles (`DrawRectanglePro`), and the pop ring. Tuning knobs (`COIN_BOOST`, `PLATFORM_BOUNCE`, `SHAKE_*`, …) are `#define`s at the top of `world.c` — game feel is hand-tuned there.
- **background** (`background.c`/`.h`) — procedural backdrops, no assets, **theme-aware**. `drawGameBackground(cameraY)` calls `themeUpdate(ascent)`, draws the sky gradient from the blended `gTheme` (so it morphs with altitude), then `themeBlend(&lo,&hi,&f)` and **crossfades the two neighbor themes' backdrops** — `drawBackdrop(THEMES[lo].backdrop, cameraY, 1-f)` over `drawBackdrop(THEMES[hi].backdrop, cameraY, f)` (shapes can't lerp, so they alpha-crossfade). Two backdrop styles share the same parallax idiom: `drawCloudsBackdrop` (sun + fluffy `drawCloud` clumps) and `drawBlocksBackdrop` (falling neon `drawBlock` tetromino cells). `drawMenuBackground` seeds `themeUpdate(0.0f)` (menu is always Nature) then draws sky + sun + big drifting clouds.
- **sound** (`sound.c`/`.h`) — procedural chiptune **plus procedural SFX**, no sample files, **per-theme with crossfade**. Music is now **multi-track**: `TRACKS[]` lists each theme's note array (`NOTES0` = Nature; `NOTES1` = Arcade — a **placeholder** with one blip + a comment for the melody to be filled in), and `buildLoop(track)` pre-renders one 48s loop per track into `loopBuffer[track]` at startup (each note gets an `adsr` envelope + `tone` timbre, under a shared A2 tonic-pedal pad tied to the loop length, peak-normalized). `updateAudio` streams 4096-sample chunks, **crossfading** `curTrack`/`prevTrack` by a `fade` gain (~2s, `FADE_SAMPLES`); `setTrack(track)` starts a fade (no-op if unchanged), driven each frame from `main.c` via `setTrack(themeTrack())`. SFX ride the same stream: a fixed `voices[MAX_VOICES]` pool, each effect synthesized per-sample in `renderVoice` (square/sine/saw + decay envelope, lengths in `sfxLen[]`) with an **Arcade variant** (harsher/brighter) selected by `curTrack`, summed onto the music through a `tanhf` limiter. Triggers `sfxCoin`/`sfxBounce`/`sfxPowerup`/`sfxHit`/`sfxSelect`; `world.c` fires the gameplay ones, `main.c`/`settings.c` fire `sfxSelect`.
- **ui** (`ui.c`/`.h`) — mouse-driven UI + shared text helpers, no assets. `virtualMouse()` maps window mouse coords into the fixed 800×800 render space (uniform scale, no letterbox) so all hit-testing matches the render target; `mouseInRect` tests against it and `clicked(rect)` is true on the frame a left-click lands inside it. `drawTextShadow`/`drawCenteredText` are the shared shadowed-text helpers used by every screen. `uiButton(rect, label, icon)` draws a rounded cartoon button with hover highlight + optional shape icon + label (draw-only; detect its click with `clicked(rect)` in the matching update handler). `drawIcon(kind, …)` builds every icon (`ICON_PLAY`/`PAUSE`/`GEAR`/`RETRY`/`ARROW_L`/`ARROW_R`/`BACK`/`HOME`/`EXIT`) from raylib primitives only (`DrawTriangle`/`DrawRing`/`DrawRectanglePro`/`DrawCircle`/`DrawLineEx`). Convention: each button is one `static` rect helper in `main.c`/`settings.c`, referenced by `uiButton(...)` in draw() and `clicked(...)` in update(). All menus/overlays are fully mouse-playable; keyboard paths stay in parallel.
- **settings** (`settings.c`/`.h`) — the options screen (framerate/resolution/volume) reachable from MENU or PAUSED. Rows are edited with `changeRow` and drawn with `drawRow`; values persist to a plain-text `settings` file beside the exe (runtime-generated, doesn't count against the budget). `main.c` applies the chosen resolution via the render-texture blit and framerate via `SetTargetFPS`.
- **persist** (`persist.c`/`.h`) — shared plain-text int-record I/O for the runtime files beside the exe. `readInts(path, buf, n)` / `writeInts(path, buf, n)` back both the `highscore` (1 int) and `settings` (3 ints; volume stored as an int percent) files, replacing the two copied fopen/fscanf skeletons. `readInts` returns false and leaves `buf` untouched on a missing/garbage file, so callers keep their defaults.

In-game drawing runs inside `BeginMode2D` with a `Camera2D` targeting `world.cameraY`; HUD/overlays draw outside it in screen space.

Built since: procedural SFX, monsters (dodge-or-die, `updateMonsters` sets `world.dead`), power-ups (Coin Magnet + Rocket, table-driven via `POWERUP_DEFS[]`), particle bursts (`spawnParticles`/`updateParticles`), high-score persistence (via the `persist` module, plain text `highscore` file beside the exe — runtime-generated, doesn't count against the 1.44MB budget), and a full bright-cartoon aesthetic pass (shared `theme.h` palette, googly-blob player, cloud/sky backgrounds, confetti + screen shake juice, warm arranged audio, shadowed HUD with score badge + power-up timer bar), a **settings screen** (framerate/resolution/volume, persisted to a `settings` text file; resolution via the render-texture blit), **framerate-independent physics** (fixed-timestep accumulator), **full mouse support** (the `ui` module — shape-icon buttons, mouse-follow steering), **horizontal screen-wrap**, **attract/demo mode** on the menu, and an **explicit FSM** (`Game` struct + `STATES[]` dispatch table; transitions route through `gotoState`, which records `prevState` so SETTINGS returns to whichever screen opened it).

Multi-theme scaffolding (2026-07): the palette is runtime (`Theme`/`THEMES[]`/`gTheme`), the scene morphs continuously nature→arcade over `BAND_HEIGHT` of climb, backdrops crossfade (clouds↔falling neon blocks), and music/SFX crossfade per theme (`TRACKS[]`, `setTrack`). The Arcade music track is a **placeholder** awaiting notes in `NOTES1`. Adding a third theme = one `THEMES[]` row (+ a new `BackdropStyle` case in `background.c` if it needs a new shape, + a `TRACKS[]` row for its music).

Planned but not built: the Arcade melody (fill `NOTES1`), Balloon/Shield power-ups, multiple characters, altitude-scaled difficulty, rocket invincibility.

## History note

Former `src_old/`/`include_old/` (an earlier float-physics iteration) were merged into `src/`/`include/` and deleted in 2026-07. The repo is now under git (initial commit `db3ef13`) and pushed to GitHub, so history exists from that point on.

## README

`README.md` (Korean) matches the actual raylib + CMake build. When build/layout facts change, keep it and this file in sync; on any conflict, trust this file and `CMakeLists.txt`.
