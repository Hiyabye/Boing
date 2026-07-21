# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

"Mega Jump" (CMake project name `Megajump`) — a small vertical-jumper game in C99 built on [raylib](https://www.raylib.com/). raylib is **vendored** at `lib/raylib/` and built from source as a CMake subdirectory, so there is no system dependency to install.

## Size limit — 1.44MB (hard constraint)

This project targets the [2PG Arcade 1.44MB contest](https://2pgarcade.com/contest-144mb.html): the final shipped game (executable + runtime + any assets, after decompression) must be **≤ 1,474,560 bytes** — one 3.5" floppy disk. Submission window closes 2026-09-04.

- Measure with a size-optimized build, not the default (unoptimized ~2.2MB):
  ```sh
  cmake -S . -B build-minsize -DCMAKE_BUILD_TYPE=MinSizeRel
  cmake --build build-minsize
  strip build-minsize/Megajump
  ls -l build-minsize/Megajump
  ```
  As of 2026-07: ~1.03MB stripped, ~430KB headroom.
- Prefer procedural generation (like the existing chiptune) over asset files; every asset counts against the budget.

## Build & run

```sh
cmake -S . -B build      # configure (first time, or after CMakeLists/new .c files)
cmake --build build      # compile; produces ./build/Megajump
./build/Megajump         # run
```

- `CMakeLists.txt` globs `src/*.c` (`GLOB_RECURSE`), so **adding or removing a `.c` file requires re-running `cmake -S . -B build`** for it to be picked up.
- macOS links Cocoa/IOKit/CoreVideo/OpenGL frameworks automatically (`if(APPLE)` block).
- `compile_flags.txt` exists only to feed include paths (`include/`, `lib/raylib/src`) to clangd/the editor — it is not part of the build.
- No test target and no linter are configured.

## Layout & architecture

Code lives in `src/` + `include/` as header/impl pairs. `src/main.c` owns the game loop and the `GameState` machine (MENU → PLAYING ⇄ PAUSED, PLAYING → GAMEOVER → PLAYING, plus SETTINGS reachable from MENU or PAUSED; pause HOME → MENU; menu EXIT sets a `quit` flag that ends the loop). It is an explicit FSM: a `Game` context struct (player/world/settings/highScore/state/…) threaded through a `STATES[]` dispatch table that maps each `GameState` to `{update, draw}` handler function pointers — the loop is just `STATES[state].update(&game)` then `.draw`. Gameplay runs on a **fixed 60Hz timestep accumulator** so physics is framerate-independent; the whole frame is drawn into a fixed 800×800 `RenderTexture` then blitted scaled to the window (`DrawTexturePro`, no letterbox) so the resolution setting can resize the window without touching draw code. MENU runs **attract/demo mode** (`updateAttract` wanders the blob); Space/PLAY carries the same blob straight into gameplay. Gameplay is a [Mega Jump](https://www.halfbrick.com/games/megajump)-style endless climber: collecting coins boosts the player upward, falling below the view kills. Subsystems:

- **core** (`core.h`, header-only) — `GameState` enum, shared screen constants (`SCREEN_WIDTH`/`SCREEN_HEIGHT`/`FLOOR_Y`), and the shared `clampf` math helper. Modules read these instead of taking screen-size parameters.
- **theme** (`theme.h`, header-only) — the shared bright-cartoon palette. Named `Color` macros (`CANDY_PINK`, `COIN_HI`, `CLOUD`, `GRAPE`, `SKY_TOP`/`SKY_TOP_HIGH`, …) plus `CONFETTI_COLORS`/`CONFETTI_COUNT` for particle bursts. Every draw site pulls colors from here so the game reads as one set.
- **player** (`player.c`/`.h`) — pure physics + input, knows nothing about the level. `Player` (float `Vector2` position/velocity/acceleration + `radius`, plus a `trail[TRAIL_LEN]` ring buffer for the motion trail). `createPlayer` returns the canonical spawn state (gravity lives in `acceleration.y`); `handlePlayerInput(player, targetX)` reads arrows and, with no arrow held, steers the blob toward `targetX` (the mouse column; `main.c` passes a sentinel to ignore it); `updatePlayer` integrates, damps, **wraps horizontally** (off one edge → back on the other, shifting the trail by the same delta so it doesn't streak), and pushes the trail; `updateAttract` is the menu demo — wander toward a random target at a steady pace, wrap the edges, bob at the floor line; `drawPlayer` renders a candy googly-blob: motion trail, velocity-driven squash-&-stretch (`DrawEllipse`), belly highlight, and pupils that lean toward movement.
- **world** (`world.c`/`.h`) — level content, camera, score. World-space coordinates, up = −y; camera (`cameraY`, top of view) only moves up. Fixed pools all follow the same spawn-above-view / cull-below-view idiom: `Coin` (`MAX_COINS`, trail with random-walk x within `COIN_WANDER`, ~55px apart), one-way `Platform`s (`MAX_PLATFORMS`, ~300px apart, swept feet check, no tunneling), `Monster`s (`MAX_MONSTERS`, ~600px bands, 1-in-3 chance, drift horizontally, collision sets `world.dead`), `PowerUp`s (`MAX_POWERUPS`, ~1500px, Magnet or Rocket), and `Particle`s (`MAX_PARTICLES`, radial burst on coin-collect/bounce, gravity + decay). `updateWorld` owns floor/platform bounce, coin-collect boost, magnet pull, rocket thrust, and monster hits (all write `player->velocity.y` or `world.dead`), plus decaying `world.shake` (screen shake) and `world.flash` (collect pop-ring); game-over is `isPlayerFallen(...) || world.dead`; `getScore` = ascent/10 + 5·coins. `drawWorld` is all cartoon: grassy ground, cloud-pad platforms, spinning glowing coins, wobbling grape monsters with faces, bobbing/rotating power-ups (horseshoe magnet, flame-flicker rocket), confetti-fleck particles (`DrawRectanglePro`), and the pop ring. Tuning knobs (`COIN_BOOST`, `PLATFORM_BOUNCE`, `SHAKE_*`, …) are `#define`s at the top of `world.c` — game feel is hand-tuned there.
- **background** (`background.c`/`.h`) — procedural backdrops, no assets. Bright cartoon sky shared by both: `drawCloud` helper (overlapping circles), a glowing `drawSun`, and fluffy clouds. `drawMenuBackground`: sky gradient + sun + big drifting clouds. `drawGameBackground(cameraY)`: day→dusk-violet gradient over `HIGH_ALTITUDE` of climbing, with 3 parallax cloud layers driven by `cameraY`.
- **sound** (`sound.c`/`.h`) — procedural chiptune **plus procedural SFX**, no sample files. `buildLoop()` pre-renders one loop into `loopBuffer`: each note gets an `adsr` envelope (no clicks) and a `tone` timbre (fundamental + 2 harmonics + gentle vibrato), summed under a soft A2 tonic-pedal pad, then peak-normalized to 0.9. `updateAudio` streams it in 4096-sample chunks to a raylib `AudioStream`. SFX ride the same stream: a fixed `voices[MAX_VOICES]` pool, each effect synthesized per-sample in `renderVoice` (square/sine/saw + short-attack decay envelope, lengths in `sfxLen[]`), summed onto the music and passed through a `tanhf` soft limiter. Header exposes triggers `sfxCoin`/`sfxBounce`/`sfxPowerup`/`sfxHit`/`sfxSelect`; `world.c` fires the gameplay ones, menu/settings navigation (`main.c`/`settings.c`) fires `sfxSelect`.
- **ui** (`ui.c`/`.h`) — mouse-driven UI + shared text helpers, no assets. `virtualMouse()` maps window mouse coords into the fixed 800×800 render space (uniform scale, no letterbox) so all hit-testing matches the render target; `mouseInRect` tests against it and `clicked(rect)` is true on the frame a left-click lands inside it. `drawTextShadow`/`drawCenteredText` are the shared shadowed-text helpers used by every screen. `uiButton(rect, label, icon)` draws a rounded cartoon button with hover highlight + optional shape icon + label (draw-only; detect its click with `clicked(rect)` in the matching update handler). `drawIcon(kind, …)` builds every icon (`ICON_PLAY`/`PAUSE`/`GEAR`/`RETRY`/`ARROW_L`/`ARROW_R`/`BACK`/`HOME`/`EXIT`) from raylib primitives only (`DrawTriangle`/`DrawRing`/`DrawRectanglePro`/`DrawCircle`/`DrawLineEx`). Convention: each button is one `static` rect helper in `main.c`/`settings.c`, referenced by `uiButton(...)` in draw() and `clicked(...)` in update(). All menus/overlays are fully mouse-playable; keyboard paths stay in parallel.

In-game drawing runs inside `BeginMode2D` with a `Camera2D` targeting `world.cameraY`; HUD/overlays draw outside it in screen space.

Built since: procedural SFX, monsters (dodge-or-die, `updateMonsters` sets `world.dead`), power-ups (Coin Magnet + Rocket, timers in `World`), particle bursts (`spawnParticles`/`updateParticles`), high-score persistence (`loadHighScore`/`saveHighScore` in `main.c`, plain text `highscore` file beside the exe — runtime-generated, doesn't count against the 1.44MB budget), and a full bright-cartoon aesthetic pass (shared `theme.h` palette, googly-blob player, cloud/sky backgrounds, confetti + screen shake juice, warm arranged audio, shadowed HUD with score badge + power-up timer bar), a **settings screen** (framerate/resolution/volume, persisted to a `settings` text file; resolution via the render-texture blit), **framerate-independent physics** (fixed-timestep accumulator), **full mouse support** (the `ui` module — shape-icon buttons, mouse-follow steering), **horizontal screen-wrap**, **attract/demo mode** on the menu, and an **explicit FSM** (`Game` struct + `STATES[]` dispatch table).

Planned but not built: Balloon/Shield power-ups, multiple characters, multiple worlds, rocket invincibility.

## History note

Former `src_old/`/`include_old/` (an earlier float-physics iteration) were merged into `src/`/`include/` and deleted in 2026-07. The repo is not under git; don't assume history exists.

## README caveat

`README.md` describes an aspirational structure (miniaudio, a `Makefile`, `resource.rc`) that does **not** match the actual build — the real project uses raylib + CMake. Trust this file and `CMakeLists.txt` over the README.
