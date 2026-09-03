# Boing Gameplay Theme Transition Roadmap

## Status

- Execution tracker; Phase 9 completed on 2026-09-03.
- Phase 0 baseline committed as `4e34256`.
- Locked direction: a diegetic closed loop.
- Current sequence: Sunny Hills -> Birdsong Skies -> Night Blocks -> Neon Maze -> Falling Blocks -> a rebuilt Sunny Hills.
- North star: players should notice that the world evolved, but never notice a technical cut.

## Purpose

Refactor the gameplay theme pipeline so visual art, spawned entities, mechanics, music, post-processing, difficulty, and HUD progression describe one continuous climb. The refactor must also make future themes easier to add without coupling unrelated systems to `THEME_COUNT`.

This is not a rewrite. Preserve the current fixed pools, procedural art, table-driven configuration, soundtrack synthesis, and self-test approach. Change ownership and coordination first, then add content.

## Hard constraints

- macOS-only C99 and raylib remain unchanged.
- Final decompressed `Boing.app` must stay at or below 1,474,560 bytes.
- Baseline bundle size when this roadmap was written: 1,405,427 bytes.
- No downloaded art, music, fonts, or other runtime assets.
- No new dependency, ECS, plugin system, scripting language, graph editor, or external audio middleware.
- All shipped names, artwork, sounds, mechanics, and layouts remain original.
- Use `Neon Maze`, `Falling Blocks`, and generic genre language in code and shipping copy. Do not ship branded game names or recognizable copied characters.
- Do not modify `CHANGELOG.md`.
- Do not combine a behavior-preserving refactor with a gameplay feature in the same step.
- Do not commit or push unless the user explicitly requests it in that future session.

## Completed discovery

- [x] Chosen progression topology: diegetic closed loop.
- [x] Traced `theme`, `world`, `background`, `sound`, `screen_play`, `main`, and `post` integration.
- [x] Ran `BG_SELFTEST` successfully.
- [x] Ran `WORLD_SELFTEST` successfully.
- [x] Ran `SOUND_SELFTEST` successfully.
- [x] Rendered a temporary current-transition matrix at 70%, 84%, 88%, 92%, 96%, and 99.5% of every edge.
- [x] Confirmed that individual era art is already strong and that coordination is the primary problem.

The temporary matrix is `/tmp/boing_transitions.png`. It is not durable and may disappear between sessions.

## Baseline diagnosis

### Themes are keyframes while gameplay treats them as chapters

`themeUpdate()` currently interpolates from the current palette to the next palette across the entire 20,000-unit band. A pure era exists only near phase zero. Gameplay mechanics and music still switch at the discrete band boundary.

### Palette and backdrop geometry use different timelines

The palette changes from phase 0.0, while incoming backdrop shapes wait until `BACKDROP_XFADE_START` at phase 0.85. Near the boundary, two complete scenes are alpha-crossfaded, causing ghosted overlap rather than a causal transformation.

### Entity era ownership is inconsistent

- Platforms mostly stamp behavior at spawn, which is the correct model.
- Platform and power-up spawners choose one global era before filling a whole screen of content.
- Monsters globally start or stop maze steering when `worldEra()` changes.
- All visible coins globally switch between minted-coin and pellet art.
- Content generated ahead of the camera may receive the wrong era for its world altitude.

### Music does not wait for a musical boundary

`setTrack()` resets the incoming loop to frame zero and starts an equal-power crossfade immediately. Gameplay tracks have different BPM, meter, harmony, and rhythmic density, so a one-bar fade does not guarantee a coherent seam.

### The HUD exposes the splice

The `LEVEL` progress bar, era banner, and level-up cue all point directly at the mathematical boundary. This conflicts with the desired seamless experience.

### Theme count owns unrelated policy

Current couplings include:

- Gameplay tracks are assumed to be the first `THEME_COUNT` track IDs.
- `THEME_COUNT == TRACK_MENU` is compile-checked.
- Era names and glow values live in separate parallel tables.
- `COIN_DOUBLE_CAP` equals `THEME_COUNT`.
- Finale logic assumes `THEME_COUNT - 1` is Falling Blocks.
- `levelTrack()` assumes `TRACK_L1` through `TRACK_L5`.
- `musicEraTransition()` indexes soundtrack metadata by era number.

These must be made explicit before adding theme rows.

## Target vocabulary

Use these terms consistently in code, comments, tests, and documentation:

- **Level:** monotonic number of gameplay bands reached.
- **Cycle:** completed passes through all gameplay eras.
- **Era:** one stable gameplay chapter with one primary rule.
- **Phase:** raw progress from 0.0 to 1.0 through an era.
- **Transition blend:** eased progress from 0.0 to 1.0 through the exit transition only.
- **Bridge:** the authored relationship from one era to the next.
- **Palette keyframe:** a visual-only intermediate such as twilight. It is not another era, level, track, mechanic, or mission increment.

## Target journey model

Use one pure altitude sampler. It should have no hidden dependency on draw order.

```c
typedef struct {
  int level;
  int cycle;
  EraId era;
  EraId next;
  float phase;
  float blend;
} ThemeSample;

ThemeSample themeSampleAt(float ascent);
ThemeSample themeSampleAtWorldY(float worldY);
```

The exact public names may change if the surrounding code reveals a clearer minimal API, but retain these semantics.

Use the existing coordinate model consistently:

```text
ascent at world y = max(0, SCREEN_HEIGHT / 2 - worldY)
```

Three consumers ask different questions through the same sampler:

- Peak ascent: monotonic gameplay progression, cycle, music, and HUD.
- View ascent: background and palette at the camera.
- Entity ascent: immutable era and introductory tuning at spawn.

Do not build a separate transition state machine. Altitude is already the authoritative progression coordinate. Audio alone may keep pending temporal state because musical quantization requires it.

## Initial pacing model

These are starting values, not permanent laws. Measure real traversal time before final tuning.

| Era phase | Purpose |
|---|---|
| 0.00-0.12 | Safe arrival and mechanic introduction |
| 0.12-0.42 | Development |
| 0.42-0.68 | Variation or combination |
| 0.68-0.78 | Mastery peak |
| 0.78-1.00 | Relief, visual handoff, and next-era preview |

Initial staggered visual windows:

| Channel | Suggested window |
|---|---|
| Palette and far atmosphere | 0.70-1.00 |
| Far backdrop shapes | 0.72-0.94 |
| Midground shapes | 0.80-0.98 |
| Foreground edge details | 0.88-1.00 |
| New mechanics | Determined by each entity's world Y |
| Audio transition request | Around 0.80 |
| Audio destination entry | Next legal beat, bar, or phrase |

Use an eased curve such as cubic smoothstep for visual weights. Keep raw `phase` separate from visual `blend` so HUD, pacing, and rendering do not accidentally share the wrong quantity.

# Execution roadmap

Complete phases in order. Each numbered task is intended to fit in one focused future session unless testing reveals that it should be smaller.

## Phase 0: Make every boundary observable

Goal: create durable evidence before changing behavior.

### 0.1 Add a transition contact matrix

- [x] Extend the existing `BG_SELFTEST` in `src/background.c` rather than adding a test framework.
- [x] Render every outgoing edge, including Falling Blocks -> Sunny Hills.
- [x] Sample at least phases 0.70, 0.78, 0.84, 0.90, 0.96, 0.995, and the next era at 0.005.
- [x] Include pure era-start samples in the same sheet or preserve the existing era-start strip separately.
- [x] Keep the existing alpha-stomp behavior before composing the sheet.
- [x] Keep the existing non-flat and non-blown-out assertions.
- [x] Add a grayscale copy or a small luma/contrast report for gameplay readability.
- [x] Document columns and rows in the self-test comment.

Acceptance:

- One command exposes every visual seam, including the closed-loop edge.
- The current behavior is captured before transition timing changes.
- No shipped gameplay behavior changes.

Verification:

```sh
cc -DBG_SELFTEST src/background.c src/theme.c src/tex.c \
  -Iinclude -Ilib/raylib/src build/lib/raylib/raylib/libraylib.a -lm \
  -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL \
  -o /tmp/bgtest && /tmp/bgtest /tmp/bgstrip.png
```

### 0.2 Measure real era duration

- [x] Time several representative runs or use a temporary debug print to measure seconds per 20,000 units.
- [x] Include normal collection, missed coins, and at least one rocket-assisted climb.
- [x] Remove temporary logging before closing the task.
- [x] Record the observed range in this roadmap under "Measured tuning data."

Acceptance:

- Transition percentages can be judged as actual seconds, not only altitude fractions.
- No permanent telemetry or control panel is added.

### 0.3 Record the baseline gate

- [x] Run a clean build.
- [x] Record the current bundle size.
- [x] Run `BG_SELFTEST`, `WORLD_SELFTEST`, and `SOUND_SELFTEST`.
- [x] Launch `build/Boing.app` through Launch Services and play across at least one boundary if practical.

Acceptance:

- Build, tests, size, and current behavior are known before refactoring.

## Phase 1: Centralize era identity without changing behavior

Goal: remove parallel metadata and numeric assumptions while preserving output.

### 1.1 Publish one `EraId`

Files:

- `include/theme.h`
- `src/theme.c`
- `src/world.c`
- `src/screen_play.c`

Steps:

- [x] Move the private `Era` enum from `src/world.c` to `include/theme.h`.
- [x] Give entries semantic names: Hills, Skies, Night, Maze, Blocks, and count.
- [x] Make `THEME_COUNT` derive from or alias the public era count if keeping the macro is still useful.
- [x] Remove the duplicate private enum.
- [x] Preserve positional unsized tables and compile-time completeness checks.
- [x] Mutate one guarded table temporarily and verify that its `STATIC_ASSERT` still fails, then restore it.

Acceptance:

- Every gameplay and rendering system uses one era type and order.
- Runtime behavior and visual output are unchanged.

### 1.2 Move name and glow into `Theme`

Files:

- `include/theme.h`
- `src/theme.c`
- `src/screen_play.c`

Steps:

- [x] Add `const char *name` after all color fields and other non-color metadata.
- [x] Add `float glow` after the color run.
- [x] Populate both fields in every `THEMES[]` row.
- [x] Remove `ERA_NAMES[]` from `screen_play.c`.
- [x] Remove `GLOW[]` from `theme.c`.
- [x] Read names and glow from the authoritative theme row.
- [x] Preserve the `offsetof(Theme, backdrop)` color-run guard.

Acceptance:

- Era name, palette, backdrop, track, and glow have one authoritative row.
- HUD and post effects remain visually identical.

### 1.3 Add explicit bridge metadata

Files:

- `include/theme.h`
- `src/theme.c`

Steps:

- [x] Define a small `BridgeStyle` enum for the five ordered edges.
- [x] Add the outgoing bridge style to each theme row.
- [x] Use original internal names such as cloudline, twilight, signal, fragmentation, and reboot.
- [x] Add a completeness guard where appropriate.
- [x] Do not render bridges yet.

Acceptance:

- Falling Blocks -> Sunny Hills is represented explicitly rather than existing only as modulo arithmetic.
- No behavior or visuals change.

### 1.4 Decouple gameplay tracks from theme count

Files:

- `include/sound.h`
- `src/sound.c`
- `src/theme.c`
- `src/main.c`

Steps:

- [x] Rename gameplay track IDs from positional `TRACK_L1` names to semantic era names.
- [x] Add an explicit gameplay-track sentinel or a similarly direct test.
- [x] Remove `THEME_COUNT == TRACK_MENU` assumptions.
- [x] Update `levelTrack()` so it does not hard-code `TRACK_L5` as the range endpoint.
- [x] Make `musicEraTransition()` resolve track metadata through `THEMES[era].track` or accept the needed track IDs directly.
- [x] Keep every current loop, composition, playhead, and transition duration unchanged.
- [x] Update sound self-test assertions to use semantic names.

Acceptance:

- A theme may reuse a track.
- A visual-only bridge requires no track.
- Adding an era no longer silently depends on the location of `TRACK_MENU`.
- `SOUND_SELFTEST` output remains unchanged.

Phase 1 gate:

- [x] Clean build.
- [x] Relevant self-tests pass.
- [x] Transition and pure-era sheets match the baseline by eye.
- [x] Bundle remains within budget and approximately unchanged.

Launch verification also exposed a transient raylib platform-init failure that
made the old unconditional `CloseWindow()` call jump through unloaded GL
functions. `main()` now returns immediately when `IsWindowReady()` is false;
five consecutive Launch Services relaunches produced no further crash report.
All nine exported soundtrack WAVs were byte-identical to the Phase 0 commit.

Suggested future commit boundary if explicitly requested:

```text
refactor: centralize era metadata
```

## Phase 2: Introduce a pure journey sample

Goal: make one pure calculation authoritative for level, cycle, era, next era, phase, and transition blend.

### 2.1 Add `ThemeSample` with current behavior

Files:

- `include/theme.h`
- `src/theme.c`

Steps:

- [x] Add `ThemeSample` and `themeSampleAt()`.
- [x] Clamp negative ascent to zero.
- [x] Compute unbounded level, cycle, era, next era, and raw phase.
- [x] Initially set `blend = phase` so visual behavior remains unchanged.
- [x] Reimplement `themeLevelAt()` and `themeBandProgressAt()` through the pure sampler.
- [x] Keep compatibility wrappers temporarily if they reduce migration risk.
- [x] Add self-test assertions immediately before and after ordinary boundaries and the cycle boundary.

Acceptance:

- Sampling is deterministic and has no hidden global-state dependency.
- Existing output is unchanged.

### 2.2 Add world-Y sampling

Files:

- `include/theme.h`
- `src/theme.c`
- `include/world.h`
- `src/world.c`

Steps:

- [x] Add the smallest helper needed to sample an era from world Y.
- [x] Base it on `SCREEN_HEIGHT / 2 - worldY` so it matches the current peak and camera-center math.
- [x] Add or clarify world accessors for peak ascent and view ascent if callers otherwise duplicate formulas.
- [x] Keep peak progression monotonic.
- [x] Keep the visual view sample camera-relative so falling can reveal lower scenery again.

Acceptance:

- Peak, view, and entity sampling share one coordinate convention.
- Music and level events do not flap when the camera follows a fall.

### 2.3 Migrate consumers and remove draw-order dependence

Files:

- `src/background.c`
- `src/screen_play.c`
- `src/main.c`
- `src/theme.c`

Steps:

- [x] Pass or locally compute the appropriate `ThemeSample` at each consumer.
- [x] Make HUD phase read raw phase, not palette blend.
- [x] Make post glow derive from the sampled era pair and blend.
- [x] Remove reliance on `drawGameBackground()` having called `themeUpdate()` before HUD or post code.
- [x] Remove `bandLo`, `bandHi`, and `bandF` once no caller needs them.
- [x] Keep `gTheme` only as a temporary rendered palette if replacing every scene-color macro in one step would create unnecessary churn.

Acceptance:

- Calling order no longer determines phase, name, track, or glow.
- `themeSampleAt()` is the only owner of modulo and cycle math.

Phase 2 gate:

- [x] Mathematical boundary assertions pass.
- [x] Existing visual and audio output remain unchanged.
- [x] Clean build and size check pass.

`ThemeSample` now owns all runtime modulo and cycle math. Peak samples drive
progression, HUD, and music; view samples drive palette and glow. The existing
BG, WORLD, and SOUND self-tests pass, both contact sheets match the Phase 1
appearance by eye, and the clean bundle remains 1,389,043 bytes.

Suggested future commit boundary if explicitly requested:

```text
refactor: make theme progression a pure sample
```

## Phase 3: Give entities spatial era ownership

Goal: no live interactive object changes identity because the player crossed a global boundary.

### 3.1 Reproduce boundary ownership failures

Files:

- `src/world.c` under `WORLD_SELFTEST`

Steps:

- [x] Seed content on both sides of a band boundary.
- [x] Demonstrate that the current spawner can stamp a platform using the player's global era rather than the platform's Y.
- [x] Demonstrate that an existing monster changes steering when the global era changes.
- [x] Add assertions for the desired immutable behavior.
- [x] Keep the failing check and fix in the same session.

Acceptance:

- The self-test would fail against the old ownership behavior.

### 3.2 Stamp era on theme-sensitive entities

Files:

- `include/world.h`
- `src/world.c`

Steps:

- [x] Add a compact era field to `Coin`, `Platform`, `Monster`, and `PowerUp` where needed.
- [x] Verify actual struct sizes before and after. Prefer existing padding rather than reorganizing unrelated fields.
- [x] Compute era from each spawn cursor's world Y inside each spawner loop.
- [x] Do not compute one era before a loop that can cross a boundary.
- [x] Preserve existing platform flags and velocities as the final authority for platform behavior.

Acceptance:

- Every spawned object owns the era of its location.
- Filling a screen across a boundary stamps both eras correctly.

### 3.3 Make monster behavior local

Steps:

- [x] Change maze steering to check the monster's stamped era.
- [x] Existing non-maze monsters remain ordinary after the global level changes.
- [x] Existing maze monsters remain maze creatures while alive, even if they cross the boundary spatially.
- [x] Restrict pellet flee and doubling bonuses to maze-stamped monsters.
- [x] Preserve velocity caps and survivability tuning.

Acceptance:

- No live monster transforms at a level boundary.
- Pellet effects do not leak into unrelated eras.

### 3.4 Make coin appearance local

Steps:

- [x] Draw pellet-lane art from each coin's stamped era.
- [x] Do not globally restyle all live coins through `worldEra()`.
- [x] Keep collection radius, score, combo, and motion unchanged.

Acceptance:

- Coins above and below a boundary may visibly coexist without popping.

### 3.5 Make early-cycle economy explicit

Steps:

- [x] Replace the implicit `world->level < THEME_COUNT` condition with an explicit first-cycle rule.
- [x] Preserve current first-cycle double-coin behavior.
- [x] Ensure adding a visual-only bridge cannot alter economy.

Acceptance:

- Economy policy says what it means and is independent of incidental table layout.

Phase 3 gate:

- [x] `WORLD_SELFTEST` covers a boundary and the wrap.
- [x] No live object's collision or rule changes at a boundary.
- [x] Manual play across at least two boundaries feels continuous.
- [x] Build and size checks pass.

The new assertions failed against the old code at Hills -> Skies, Blocks ->
Hills, and when a Hills monster survived into Maze. All four entity era tags
fit existing padding: `Coin` 20, `Platform` 28, `Monster` 24, `PowerUp` 16,
and `World` 3,640 bytes before and after. A temporary long Rocket, removed
before the final build, drove a Launch Services run through Hills, Skies,
Night, and into Maze; static and drifting platforms visibly coexisted near the
first boundary. The clean bundle remains 1,389,043 bytes.

Suggested future commit boundary if explicitly requested:

```text
fix: stamp entity eras from world altitude
```

## Phase 4: Give every era a stable identity

Goal: stop spending the entire band interpolating toward the next era.

### 4.1 Separate phase from transition blend

Files:

- `src/theme.c`
- `include/theme.h`

Steps:

- [x] Keep `phase` linear from 0.0 to 1.0.
- [x] Start `blend` near 0.70-0.78, based on measured traversal time.
- [x] Use cubic smoothstep for the transition interval.
- [x] Guarantee continuity at phase 1.0 and the next era at phase 0.0.
- [x] Cover the Falling Blocks -> Sunny Hills wrap in the same assertions.

Acceptance:

- Every era has a substantial visually stable core.
- No palette jump appears at any boundary.

### 4.2 Validate color paths before adding color math

Steps:

- [x] Render the transition matrix with the new timing.
- [x] Inspect hue, luma, gameplay contrast, and additive brightness.
- [x] Hand-author a twilight color keyframe for Skies -> Night if the direct path remains muddy. The eased direct path stayed clean, so no key was added.
- [x] Hand-author reboot colors for Blocks -> Hills if the path remains washed out.
- [x] Only implement Oklab interpolation in `theme.c` if hand-authored keys and easing still fail visibly. It was not needed.
- [x] If Oklab is added, keep it file-local, clamp output safely, and measure bundle size immediately. No Oklab code was added.

Acceptance:

- Color paths look intentional at every sampled phase.
- Important objects remain readable in grayscale.
- No color library or generic color framework is added.

Phase 4 gate:

- [x] Transition matrix reviewed at full resolution.
- [x] Pure-era identity remains at least as strong as the baseline.
- [x] Bundle remains under the hard limit.

Raw phase remains linear, while visual blend now stays at zero through phase
0.78 and cubic-smoothsteps across the measured final 4-7 seconds. The direct
Skies -> Night path reads as a clean blue-hour descent, so it needed no extra
key. The washed-out Blocks -> Hills midpoint now passes through one compact
indigo-coral predawn key over atmosphere colors only. Full-color and grayscale
matrix review kept every edge readable, `BG_SELFTEST` and `WORLD_SELFTEST`
pass, Launch Services startup reaches the menu, and the clean bundle remains
1,389,043 bytes.

Suggested future commit boundary if explicitly requested:

```text
feat: give gameplay eras stable visual phases
```

## Phase 5: Replace full-scene dissolves with layered movement

Goal: make transitions spatial and causal rather than two scenes fading through each other.

### 5.1 Separate conceptual background depth passes

Files:

- `src/background.c`
- `include/background.h` only if a public signature truly needs to change

Steps:

- [x] Identify sky/atmosphere, far silhouette, midground decor, and near foreground content in every backdrop.
- [x] Keep implementations in `background.c` initially so helpers can remain static.
- [x] Do not split files merely because `background.c` is long.
- [x] Give far, mid, and near passes independently eased transition weights.
- [x] Ensure complementary outgoing/incoming weights do not over-brighten additive art.

Acceptance:

- Incoming distance layers appear before incoming near layers.
- Both complete scenes are never simultaneously dominant.

### 5.2 Make the sky transition through screen space

Steps:

- [x] Sample the palette at the screen-top world Y.
- [x] Sample the palette at the screen-bottom world Y.
- [x] Use the appropriate top and bottom sky colors for the gradient.
- [x] Confirm the top of the viewport enters the next era before the bottom.
- [x] Preserve camera fall behavior without changing monotonic level events.

Acceptance:

- The player appears to climb through a boundary rather than recolor one full-screen backdrop.

### 5.3 Add shared-motif bridge drawing

Implement one edge at a time. Reuse existing primitives and textures.

#### Hills -> Skies

- [x] Sink terrain and trees toward the lower view.
- [x] Let the river line detach into a wind ribbon.
- [x] Increase cloud dominance gradually.
- [x] Let pollen become distant airborne specks or birds.

#### Skies -> Night

- [x] Introduce twilight before deep night.
- [x] Fade first stars in before the sky is fully dark.
- [x] Transform the celestial disc from sun to moon through size, value, and detail, not tint alone.
- [x] Thin clouds into the first floating square islands.

#### Night -> Neon Maze

- [x] Align stars into rows or constellations.
- [x] Straighten constellation lines into orthogonal signal corridors.
- [x] Extend island seams into maze geometry.
- [x] Turn the celestial node into an original pulsing signal motif.

#### Neon Maze -> Falling Blocks

- [x] Separate maze junctions into cells.
- [x] Group cells into four-cell clusters.
- [x] Change scrolling motion gradually into falling motion.
- [x] Turn pellet highlights into block highlights.

The final reboot was deliberately left without geometry choreography for its later cross-system phase.

Acceptance for each edge:

- At least two properties are conserved: shape, motion, color/value, sound motif, or gameplay rule.
- The result reads as transformation rather than transparency.
- Central gameplay readability remains intact.

Phase 5 gate:

- [x] Four non-wrap visual edges pass the transition matrix review.
- [x] Foreground assertions still prove zero central-playfield changes.
- [x] No new render target or transition shader was required.

Backdrop art now hands off in three cubic-eased raw-phase windows: far at
0.76-0.94, mid at 0.82-0.98, and near at 0.90-1.00. The sky independently
samples its top and bottom world Y, so the upper view crosses first. Four
compact bridge drawings conserve each edge's shapes and motion; the wrap only
receives the generic depth staging, leaving its authored reboot for Phase 9.
The full-color and grayscale matrices passed luma, contrast, additive, and
edge-only foreground review. `BG_SELFTEST` and `WORLD_SELFTEST` pass, all pure
world scenes retain their identity, and the Universal 2 bundle is 1,405,459
bytes with 69,101 bytes free.

Suggested future commit boundaries if explicitly requested:

```text
refactor: stage backdrop transitions by depth
feat: choreograph non-wrap biome bridges
```

## Phase 6: Teach mechanics through the transition

Goal: new rules become understandable before they become dangerous.

### 6.1 Add a simple arrival envelope

Files:

- `src/world.c`

Steps:

- [x] Derive introductory tuning from the spawned entity's local era phase.
- [x] Keep the model formula-based rather than adding an AI director.
- [x] Reduce monster and debuff pressure near the end of the outgoing era and beginning of the incoming era.
- [x] Keep global difficulty monotonic and capped as before unless playtests justify a separate change.
- [x] Stamp tuned values at spawn so an entity does not change underfoot.

Acceptance:

- A transition follows mastery -> relief -> safe introduction.
- Existing high-altitude difficulty remains recognizable.

### 6.2 Author the first safe encounter for each mechanic

#### Skies drift

- [x] First drifting platforms are broad, slow, and near a useful route.
- [x] Motion and wind trails are visible before commitment.

#### Night crumble

- [x] Cracks are visible before landing.
- [x] The first fuse gives enough time to understand cause and effect.

#### Maze chase

- [x] The first maze creature begins far enough away to observe.
- [x] The special pellet appears before serious chase pressure.
- [x] Fleeing uses shape, motion, and sound as well as color.

#### Falling blocks

- [x] The first moving piece demonstrates descent without creating an unavoidable crush.
- [x] Safe top rails and dangerous sides remain visually distinct without relying on color alone.

Acceptance:

- Every mechanic has introduction, development, twist, and mastery opportunities.
- Transition overlap never combines two peak hazards without relief.

### 6.3 Preserve collision truth

- [x] Never morph a live interactive platform into different collision geometry.
- [x] Keep the shared tetromino art/collision table intact.
- [x] Restrict organic visual morphs to background art or newly spawned transition variants.
- [x] Ensure every visual warning corresponds to actual behavior.

Phase 6 gate:

- [x] `WORLD_SELFTEST` covers introductory variants or phase gates.
- [x] Fresh playtesters can predict each new rule after the first safe examples.
- [x] Deaths near boundaries are attributable to player decisions, not invisible state changes.

`encounterPressure()` now eases 0 -> 1 over phase 0.00-0.18, holds through
0.78, then eases back to zero at the boundary. Platforms and monsters stamp it
once. This produces route-aligned broad/slow Skies pads, longer introductory
Night fuses, a guaranteed docile Maze ghost followed by a route Pellet, and a
slow one-row first Block while suppressing new monsters and debuffs during
relief. Full-pressure density, speed, spacing, and debuff odds remain unchanged
under the existing capped difficulty. Frightened ghosts now add a fast quiver
and scared face to their flee motion, Pellet cue, blue body, and expiry flash.

The first new self-test failed against Phase 5 because a Skies pad entered at
full speed. The completed checks cover both pressure endpoints, mature tuning,
immutable live values, actual broad-pad collision width, the armed intro fuse,
Maze placement/Pellet ordering, and boundary relief. An accelerated temporary
full-cycle app build, with no tracked debug path, showed every era and visual
cue in motion through two cycles. `WORLD_SELFTEST` passes, the clean app still occupies
1,405,459 bytes, and `Platform`/`Monster`/`World` are now 32/28/3,736 bytes.

Suggested future commit boundary if explicitly requested:

```text
feat: stage safe mechanic introductions by era phase
```

## Phase 7: Make soundtrack transitions musical

Goal: preserve the current procedural score while switching tracks on musically valid seams.

### 7.1 Add a pending gameplay-track request

Files:

- `src/sound.c`
- `include/sound.h` only if the public API must change

Steps:

- [x] Keep menu, shop, pause, credits, and result routing behavior unchanged.
- [x] For gameplay era changes, store a pending destination instead of switching immediately.
- [x] Compute the next legal outgoing beat, bar, or phrase boundary from current loop position and metadata.
- [x] Start the destination at frame zero on that boundary.
- [x] Define repeated-request and rapid-boundary behavior explicitly.
- [x] Keep invalid-track and unavailable-buffer degradation quiet.

Acceptance:

- A gameplay track never enters halfway through its first beat.
- A falling camera does not request the previous era again.

### 7.2 Avoid incompatible full-mix overlap

Steps:

- [x] Review each adjacent pair's BPM, meter, key center, and density.
- [x] Shorten or reshape crossfades where two full rhythm sections clash.
- [x] Strip or filter the outgoing mix before the incoming downbeat where practical.
- [x] Use the existing shared Boing motif as the common musical identity.
- [x] Prefer a short sustained pivot, fill, or nonmetric bridge for large tempo or meter changes.

Acceptance:

- Adjacent loops sound like sections of one score, not two tracks playing together.

### 7.3 Expand `musicEraTransition()` minimally

Steps:

- [x] Let the transition cue know both source and destination identity if needed.
- [x] Build pair-specific color from existing synthesis helpers and voice infrastructure.
- [x] Keep transition material on the music bus.
- [x] Do not add another full stereo loop solely for a short bridge unless synthesis limitations prove it necessary.

Acceptance:

- All five edges, including Blocks -> Hills, have a coherent acoustic handoff.

### 7.4 Extend sound checks

- [x] Assert that a pending switch waits until the selected musical boundary.
- [x] Assert that the destination starts at frame zero.
- [x] Exercise every adjacent era pair and the wrap.
- [x] Check peak, RMS, seam, width, and limiter behavior during transitions.
- [x] Add an optional export of short transition WAVs for listening.

Verification:

```sh
cc -DSOUND_SELFTEST src/sound.c -Iinclude -Ilib/raylib/src \
  build/lib/raylib/raylib/libraylib.a -lm -framework Cocoa \
  -framework IOKit -framework CoreVideo -framework OpenGL \
  -o /tmp/soundtest && /tmp/soundtest /tmp/boing-audio
```

Phase 7 gate:

- [x] Automated audio checks pass.
- [x] Every exported transition has been listened to on headphones and speakers.
- [x] Startup preload time and runtime PCM memory remain acceptable.
- [x] Bundle size remains within budget.

Gameplay routing now requests the next semantic track at raw phase 0.80, leaving
at least the measured final 20% of the era for scheduling. `setTrack()` holds a
gameplay request until the first outgoing bar with room for its lead-out;
repeated calls are inert and a rapid later destination waits until that
handoff finishes before frame-by-frame routing schedules it next. The outgoing
rhythm filters and fades before the seam, the destination starts at loop frame
zero, and its first beat
fades in without either full arrangement overlapping the other.

`HANDOFFS[]` explicitly covers all five ordered pairs. One short music-bus
voice carries the existing Boing motif from the source scale and timbre into
the destination, adding airy, twilight, signal, fragmentation, or reboot
color from the existing real-time synth helpers. No loop, asset, preload work,
or persistent PCM was added. The sound self-test now renders the live mixer,
checks timing, frame-zero entry, rapid requests, wrap scheduling, unavailable
buffers, RMS, peak, seam, stereo width, and limiter output, and optionally
exports all five handoff WAVs beside the nine loops. User audition approved all
five exports; external EBU R128 analysis measured -5.4 to -2.2 dBFS true peak.
The original nine loop exports remain byte-identical. The Universal 2 bundle
is 1,421,827 bytes with 52,733 bytes free.

Suggested future commit boundaries if explicitly requested:

```text
refactor: quantize gameplay music handoffs
feat: add musical bridges between eras
```

## Phase 8: Stop the HUD and post pass from exposing the seam

Goal: keep progression useful without announcing an implementation boundary.

### 8.1 Decide the HUD treatment before editing

Decision checkpoint for that future session:

- Recommended: replace exact level progress with continuous altitude and show cycle information only after reboot.
- Alternative: retain raw era phase but remove the boundary banner and hard fanfare.

Do not proceed until the user confirms which treatment they want if their preference is still unknown.

Decision: the user confirmed the recommended continuous-height treatment for
Phase 8.

### 8.2 Implement the chosen HUD model

Files:

- `src/screen_play.c`

Steps for the recommended model:

- [x] Replace the exact `LEVEL N` boundary bar with continuous height or journey progress.
- [x] Remove the era title from the exact mathematical boundary.
- [x] Keep the era name in the game-over summary.
- [x] If discovery titles remain, show them after safe arrival rather than at phase zero.
- [x] Preserve score, coin, combo, pause, and power-up readability.

Acceptance:

- The HUD no longer points directly at the splice.
- Players still understand long-term progress.

### 8.3 Route post effects from the eased sample

Files:

- `src/screen_play.c`
- `src/post.c` only if a genuinely new uniform is needed

Steps:

- [x] Keep bloom stable through each era's core.
- [x] Ease bloom through transition blend rather than raw full-band phase.
- [x] Reduce digital bloom naturally before daylight returns.
- [x] Do not reuse Fog or CRT language for biome transitions.
- [x] Do not add a new render target.

Acceptance:

- Post effects reinforce the environment without masking gameplay cues.

### 8.4 Remove last-era finale assumptions

Files:

- `src/main.c`
- `src/sound.c`
- `src/theme.c`

Steps:

- [x] Trigger finale intensity from the explicit reboot edge or bridge metadata.
- [x] Remove `journey.era == THEME_COUNT - 1` as the semantic test.
- [x] Keep the Falling Blocks finale layer tied to its semantic track or style.

Acceptance:

- Inserting a later era cannot silently move or disable finale behavior.

Phase 8 gate:

- [x] HUD, post, and audio remain synchronized through a full cycle.
- [x] UI remains readable under every transition palette.

The top-center HUD now reports continuous run height instead of `LEVEL N` and
a resetting phase bar. The boundary era callout and its `World.levelBanner`
timer are gone; the authoritative era name remains on the game-over card.
Cycle information is delayed through the reboot's 18% safe-arrival window, so
`CYCLE 2` appears after the rebuilt Hills settle rather than at the modulo
boundary. Later loops hold their previous cycle number through the same window.
Score, coin, combo, pause, and power-up layouts remain unchanged and readable.

Post already consumed the camera-relative `ThemeSample` through `themeGlow()`,
so no production shader or render-target change was needed. New sampler checks
lock bloom to the stable core, verify its eased Blocks -> Hills descent, and
confirm the daylight value at the wrap. Finale intensity now keys off the
current theme's explicit `BRIDGE_REBOOT`; `renderFinale()` remains gated on
semantic `TRACK_BLOCKS`, with a new negative sound check for other tracks.

A temporary 20-second-per-era app sweep, removed before the final build, ran
HUD, post, and audio through every edge and one complete cycle. Twelve captures
covered stable eras, outgoing transitions, the exact reboot, and phase 0.20 of
the rebuilt Hills with `COMBO x12`; the HUD remained readable, showed no cycle
change at the seam, and revealed `CYCLE 2` only after safe arrival. `BG`,
`WORLD`, `POST`, and `SOUND` self-tests pass. Removing the banner timer reduced
`World` from 3,736 to 3,732 bytes. The Universal 2 bundle remains 1,421,827
bytes with 52,733 bytes free.

Suggested future commit boundary if explicitly requested:

```text
feat: make era progression seamless in the HUD and post pass
```

## Phase 9: Author the diegetic reboot

Goal: make Falling Blocks -> Sunny Hills the strongest and most intentional edge.

Treat this as several focused sessions, not one large cross-system edit.

### 9.1 Reboot visual story

Files:

- `src/background.c`
- `src/theme.c`

Steps:

- [x] Falling cells lock together into a distant horizon.
- [x] The horizon softens into mountain ridges.
- [x] Colored cells become patchwork field shapes.
- [x] A grid line bends into the river.
- [x] Loose pixels lose alignment, round off, and become pollen or seeds.
- [x] Falling motion slows into settling, then changes into rising or sprouting motion.
- [x] Dark green moves through an authored aurora or predawn keyframe before daylight blue.
- [x] A bright cell expands into the new sun.
- [x] Avoid a flash, fade-to-black, static burst, or global wipe that merely conceals the reset.

Acceptance:

- The final transition explains why nature returns.
- The transition matrix has no pale wash or double-scene ghosting.

### 9.2 Reboot gameplay handoff

Files:

- `src/world.c`
- `include/world.h` only if a stamped transition variant needs one compact field

Steps:

- [x] Old falling pieces remain dangerous and immutable.
- [x] New pieces visibly settle or lock into dependable boards.
- [x] Stable boards gain cloud, grass, or organic treatment without changing live collision.
- [x] First-cycle baseline platforms spawn above them.
- [x] Lower monster and debuff pressure during the handoff.
- [x] Increment cycle without resetting camera, player, score, combo, or world pools.

Acceptance:

- The rule transition is Falling -> Settling -> Stable.
- No teleport, global reset, or live geometry morph occurs.

### 9.3 Reboot audio handoff

Files:

- `src/sound.c`

Steps:

- [x] Remove breakbeat layers in musical order.
- [x] Hold the shared motif in a neutral voice.
- [x] Soften digital timbres into mallets or plucks.
- [x] Enter Sunny Hills on a clean downbeat.
- [x] Preserve energy continuity without overlapping both full drum arrangements.

Acceptance:

- The score communicates rebirth rather than restarting playback.

### 9.4 Optional cycle variation

Only do this if repeated-cycle playtests still make the loop feel mechanically repetitive.

- [ ] Use cycle as a small seed or phase offset for noninteractive decor.
- [ ] Keep mechanics and critical visual cues stable.
- [ ] Avoid random theme order.
- [ ] Avoid a new persistence field.

Acceptance:

- Later dawns feel renewed without changing the authored route or adding save complexity.

Optional cycle variation was not added: the authored reboot made the repeated
route read as renewed without another seed, mechanic, or persistence field.

Phase 9 gate:

- [x] Play through or simulate one complete cycle.
- [x] No tester reports a cut or unexplained reset.
- [x] Testers can explain why Falling Blocks became stable terrain.
- [x] All visual, world, sound, build, format, and size checks pass.

The reboot now has its own far and mid choreography. Falling cells converge on
a level horizon, the line softens onto the incoming mountain ridge, cell rows
widen and recolor into patchwork fields, and one straight well line bends into
the existing river spline. Loose square pixels round into rising seeds and
sprouts. A faint aurora rides the existing indigo-coral predawn key while one
bright grid cell moves into place, rounds, and expands into the Sunny sun. The
bridge hands directly to ordinary Hills art at phase zero, with no flash, wipe,
extra target, or new palette system.

Gameplay uses no new platform field. From Blocks phase 0.88, newly spawned
route-aligned I rows set `vy = 0`, which is otherwise impossible for a
Blocks-stamped platform. They therefore use the existing one-board collision
path from birth, draw four fading cell seams beneath a continuous safe rail,
and gain cloud backing and pressure-scaled sprouts. Older pieces retain their
positive `vy`, tetromino collision, descent, and crush behavior; normal Hills
boards spawn above the locked rows. The existing pressure envelope keeps
monsters and debuffs quiet. `WORLD_SELFTEST` now proves the full Falling ->
Settled -> Stable order, fixed collision, low pressure, later-cycle ownership,
and preservation of camera, coins, combo, bonus, and live pools.

The two-beat audio reboot now removes high breakbeat detail during beat one and
kick/bass during beat two. A neutral A shared by E harmonic minor and D major
holds the center while the Boing motif softens from quantized Blocks tone into
a synthesized mallet; Sunny still enters at loop frame zero with no full-mix
overlap. The exported reboot measures -15.6 LUFS integrated, -2.2 dBFS true
peak, and a 0.012 sample seam; the other four handoffs and all nine loops remain
byte-identical.

A temporary measured-speed app sweep, removed before the final build, ran the
real spawners, HUD, post, and audio through all five eras and the wrap. Captures
showed falling pieces below, locked green/cloud boards through predawn, and
ordinary white Hills pads above with no state reset. The transition matrix and
user review both read the same causal rebirth. `BG`, `WORLD`, and `SOUND`
self-tests pass. The Universal 2 bundle remains 1,421,827 bytes with 52,733
bytes free.

Suggested future commit boundaries if explicitly requested:

```text
feat: transform falling blocks into a rebooted dawn
feat: complete the closed-loop gameplay handoff
```

## Phase 10: Add transition keyframes before full eras

Goal: improve continuity without bloating mechanics, missions, soundtrack buffers, or progression counts.

### 10.1 Blue Hour keyframe

Position: Skies -> Night.

- [ ] Add an authored twilight palette or edge treatment.
- [ ] Do not increment level or mission-era progress.
- [ ] Do not add a separate music loop.
- [ ] Reuse sun, clouds, birds, stars, and current sound layers.

### 10.2 Constellation Lock keyframe

Position: Night -> Neon Maze.

- [ ] Align stars into an increasingly regular signal pattern.
- [ ] Preserve original imagery rather than recreating a known product's maze or characters.
- [ ] Do not add a separate mechanic unless promoted to a full era later.

### 10.3 Reboot Dawn keyframe

Position: Falling Blocks -> Sunny Hills.

- [ ] Keep it as bridge data and rendering, not another `THEMES[]` gameplay row.
- [ ] Confirm that missions and first-cycle coin economy remain unchanged.

Phase 10 gate:

- [ ] Visual keyframes do not affect level count, difficulty, economy, track count, or task progression.

## Phase 11: Add new full eras one at a time

Do not start this phase until Phases 0-10 are green.

A full era must satisfy all of these:

- It is necessary as a substantial gameplay chapter, not merely visual glue.
- It has one clear primary rule.
- It has a safe introduction, development, twist, and mastery section.
- It has a causal bridge from the previous era and toward the next.
- It preserves the player's core controls and readable collision language.
- It uses original names, art, sound, tuning, and layouts.
- It can reuse a current sound style or track when that is musically sufficient.
- It passes all transition, world, sound, and size gates.

### Candidate A: Twilight Thermals

Position: Skies -> Night.

Possible identity:

- Visible current lanes influence horizontal movement.
- First currents help the player reach coins.
- Later currents complicate routing.
- Wind ribbons, birds, and dimming light naturally explain the mechanic.

Reject or redesign it if it merely duplicates drifting platforms.

### Candidate B: Constellation Circuit

Position: Night -> Neon Maze.

Possible identity:

- Star nodes connect into predictable pulse routes.
- Safe pulses demonstrate timing before hazards depend on them.
- Geometry gradually becomes orthogonal.

Reject or redesign it if pulse timing merely repeats Night's crumble mechanic.

### Candidate C: Reboot Garden

Position: Falling Blocks -> Sunny Hills.

Possible identity:

- Falling cells lock into stable platforms.
- Stable cells sprout organic decoration.
- The mechanic converts the previous danger into the next baseline rule.

This is the strongest first candidate because it directly strengthens the chosen closed loop.

### Full-era implementation checklist

For each new era:

- [ ] Write its one-sentence identity.
- [ ] Name its one primary gameplay rule.
- [ ] Define what visual motif arrives from the previous era.
- [ ] Define what motif leaves toward the next era.
- [ ] Define stable palette and any edge-only keyframes.
- [ ] Define far, mid, and foreground art.
- [ ] Define shape, motion, and non-color mechanic cues.
- [ ] Define the first safe encounter.
- [ ] Define development, twist, and mastery beats.
- [ ] Decide whether it needs a new track, can share a track, or only needs live layers.
- [ ] Define both incoming and outgoing musical bridges.
- [ ] Add the positional table row and mutate it once to prove the completeness guard fires.
- [ ] Add world behavior checks.
- [ ] Add the era and both adjacent edges to contact sheets.
- [ ] Listen to both adjacent audio transitions.
- [ ] Measure preload time, runtime memory, executable size, and full bundle size.
- [ ] Play through the era from its previous transition to its next transition.

Only then add another era.

## Phase 12: Cleanup after behavior settles

Goal: remove migration scaffolding and synchronize documentation without gratuitous churn.

### 12.1 Remove obsolete compatibility surfaces

- [ ] Remove old theme-global blend wrappers once no caller uses them.
- [ ] Remove stale comments describing whole-band palette interpolation.
- [ ] Remove old level-track ordering assumptions.
- [x] Remove unused banner state if the chosen HUD no longer needs it.
- [ ] Remove temporary tuning logs and debug paths.
- [ ] Search for all `THEME_COUNT`, `worldEra`, old track names, and old bridge constants.

### 12.2 Consider file splits only after evidence

Do not split files by default.

Possible later split criteria:

- `background.c` has enough full eras that separate backdrop implementations are regularly conflicting.
- `world.c` rendering changes independently enough from simulation to justify `world_draw.c`.
- A split can preserve static ownership without creating a large private-header surface.

If those conditions are absent, leave the files together.

### 12.3 Synchronize documentation

- [ ] Update `AGENTS.md` after implementation is final.
- [ ] Update the Korean `README.md` where build, theme count, gameplay, or architecture facts changed.
- [ ] Update inline comments to match the final sampler, bridge, entity ownership, and audio scheduling.
- [ ] Update current bundle size using a fresh build.
- [ ] Do not modify generated files or `CHANGELOG.md`.

### 12.4 Final verification

- [ ] Configure if files were added or removed: `cmake -S . -B build`.
- [ ] Build: `cmake --build build`.
- [ ] Check formatting.
- [ ] Run every relevant self-test in `AGENTS.md`.
- [ ] Run unrelated self-tests too if shared headers or core rendering changed.
- [ ] Launch through `open build/Boing.app`.
- [ ] Play menu -> gameplay -> pause -> settings -> resume -> game over -> retry.
- [ ] Play or simulate all era edges and one complete closed loop.
- [ ] Confirm only system frameworks and `/usr/lib` dependencies with `otool -L`.
- [ ] Measure the complete decompressed app bundle.

Bundle check:

```sh
find build/Boing.app -type f -exec stat -f %z {} \; | \
  awk '{ total += $1 } END { print total }'
```

Formatting check:

```sh
clang-format --dry-run -Werror src/*.c include/*.h
```

Final acceptance:

- The bundle is at or below 1,474,560 bytes.
- There are no new compiler, clang-tidy, format, or self-test failures.
- No live entity changes mechanic or collision at an era boundary.
- Every visual edge, including reboot, reads as a transformation.
- Every musical edge begins on a coherent musical boundary.
- Fresh players cannot identify a technical cut.
- Fresh players can correctly describe the new mechanic and its cues.
- All art, names, music, sound, and layouts remain original.

# Transition choreography reference

Use this table while implementing or reviewing each edge.

| Edge | Preserve | Transform | Gameplay handoff | Audio handoff |
|---|---|---|---|---|
| Hills -> Skies | Clouds, sun, curved lines | Terrain sinks, river becomes wind, pollen becomes airborne life | Stable pads below, slow drifting pads above | Air and shaker enter, shared motif reaches Skies downbeat |
| Skies -> Night | Celestial disc, clouds, drift | Twilight, stars appear, clouds harden into islands | Moving pads below, clearly cracked pads above | Percussion thins, reverb grows, neutral pivot reaches Night |
| Night -> Maze | Stars, square seams, light points | Constellations align into signal corridors | Old monsters stay ordinary, new maze creatures chase | Quantized pulse enters before Maze phrase |
| Maze -> Blocks | Grid, neon, right angles | Corridors detach into falling cell clusters | Chase pressure fades, first falling piece demonstrates safely | Electro rhythm resolves through a fill into breakbeat |
| Blocks -> Hills | Cells, green/cyan, shared motif | Cells lock into ridges, fields, river, seeds, and dawn | Falling pieces below, settling boards, stable pads above | Breakbeat deconstructs, motif softens into sunny mallets |

# Review rubric for every transition

A transition is ready only when all answers are yes.

## Visual continuity

- [ ] Does at least one salient shape visibly become another shape?
- [ ] Does motion preserve direction or change with anticipation?
- [ ] Is there a stable color or value anchor?
- [ ] Do far layers change before near layers?
- [ ] Is the player still the strongest focal element?
- [ ] Does grayscale preserve platform, hazard, collectible, and player separation?
- [ ] Is there no full-screen flash, cut, or double-scene ghosting?

## Gameplay continuity

- [ ] Do old entities preserve their original behavior?
- [ ] Are new entities visible before the player commits?
- [ ] Is the first example safe enough to observe?
- [ ] Does the mechanic use shape, motion, and sound rather than color alone?
- [ ] Is dangerous old/new overlap deliberately limited?
- [ ] Does visual geometry match collision exactly?

## Audio continuity

- [ ] Is the destination requested before it is needed?
- [ ] Does the actual switch occur on a legal musical boundary?
- [ ] Are meter, harmony, timbre, volume, and rhythm all considered?
- [ ] Is the shared Boing motif or another anchor audible?
- [ ] Are incompatible full rhythm sections prevented from clashing?
- [ ] Are tails preserved without causing a peak or muddy overlap?

## Closed-loop continuity

- [ ] Does the outgoing era visibly cause the incoming era?
- [ ] Does cycle increment without resetting the run?
- [ ] Does the reboot preserve world-space entity ownership?
- [ ] Does the return to nature feel authored rather than modulo-driven?

# Per-session working procedure

Use this checklist at the start and end of every future roadmap session.

## Start

- [ ] Read `AGENTS.md` and the relevant section of this roadmap completely.
- [ ] Run `git status --short` and preserve unrelated user changes.
- [ ] Inspect every caller of the functions being changed.
- [ ] Confirm the task is the next unmet dependency in this roadmap.
- [ ] Record the current bundle size if the task can affect code size.
- [ ] Reproduce the current behavior through the closest available end-to-end path.

## Work

- [ ] Implement only the selected roadmap task.
- [ ] Reuse current tables, helpers, pools, textures, and synthesis infrastructure.
- [ ] Keep behavior-preserving work separate from intentional gameplay changes.
- [ ] Add the smallest runnable check for each nontrivial branch or new rule.
- [ ] Avoid speculative abstractions and new dependencies.

## Finish

- [ ] Run the focused self-test.
- [ ] Run the clean build and formatting check.
- [ ] Run broader tests if shared behavior changed.
- [ ] Launch and inspect the user-visible path.
- [ ] Measure the complete app bundle.
- [ ] Mark completed roadmap checkboxes and record tuning findings.
- [ ] Summarize changed files, verification, and remaining next task.
- [ ] Do not commit unless explicitly requested.

# Measured tuning data

Fill this section as future sessions gather evidence.

| Measurement | Baseline | Latest | Notes |
|---|---:|---:|---|
| App bundle bytes | 1,405,427 | 1,421,827 | Clean Universal 2 MinSizeRel bundle; Phase 7 consumed one 16,384-byte Mach-O page |
| Typical seconds per era | Unknown | 19.22-32.42 | Five runs each: normal median 22.20, missed-coin median 30.88, one-rocket median 19.28 |
| Transition window seconds | Unknown | 4.23-7.13 | Phase 0.78-1.00, derived as 22% of the observed era range |
| Audio preload duration | Unknown | 1.30 s | Median of five `-Os` nine-loop bakes; no transition loop or added preload work |
| Gameplay track PCM memory | Unknown | 19,990,280 bytes | Five interleaved stereo int16 loops; unchanged in Phase 7 |
| Boundary death rate | Unknown | Unknown | Use lightweight playtest notes, not shipped telemetry |

Phase 0 pacing used a temporary fixed-60-Hz harness compiled directly against
`updatePlayer()` and `updateWorld()`, with five deterministic seeds per case.
It isolated vertical pacing from hazard and random-pickup luck: the normal route
kept every third coin row, the missed-coin route kept every fourth, and the
boosted route added one base 150-frame Rocket at 30% ascent to the normal route.
The harness and the temporary in-game boundary boost were removed afterward;
no telemetry or pacing controls ship.

# Research basis

These sources informed the roadmap. They are design evidence and practitioner guidance, not universal formulas. Validate all timing and tuning in Boing itself.

1. Gradual visual change and change blindness:
   https://journals.sagepub.com/doi/10.1068/p3104
2. Safe introduction, development, twist, and mastery:
   https://www.gamedeveloper.com/design/the-structure-of-fun-learning-from-i-super-mario-3d-land-i-s-director
3. Challenge, ability, feedback, and player control:
   https://jenovachen.com/flowingames/Flow_in_games_final.pdf
4. Build-up, peak, fade, and relaxation pacing:
   https://cdn.fastly.steamstatic.com/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf
5. Procedural systems punctuated by handcrafted sequences:
   https://www.gamedeveloper.com/business/road-to-the-igf-team-alto-s-i-alto-s-odyssey-i-
6. Progress-linked audiovisual layering in Tetris Effect:
   https://www.gamedeveloper.com/audio/game-audio-analysis---tetris-effect
7. Meter, timbre, pitch, volume, and phrase-aware adaptive music:
   https://www.mtosmt.org/issues/mto.19.25.3/mto.19.25.3.medina.gray.html
8. Perceptually even color interpolation goals in Oklab:
   https://bottosson.github.io/posts/oklab/
9. Redundant non-color cues for essential gameplay information:
   https://learn.microsoft.com/en-us/xbox/accessibility/xbox-accessibility-guidelines/103
10. Palette control, landmarks, and gameplay readability:
    https://www.gamedeveloper.com/art/lessons-in-color-theory-for-i-spyro-the-dragon-i-
11. Biomes and seasons used to create a sense of journey:
    https://www.gamedeveloper.com/design/designing-biomes-to-create-memorable-adventures-in-i-assassin-s-creed-valhalla-i-
12. Anticipation, easing, follow-through, and smoothly blended action:
    https://learn.microsoft.com/en-us/windows/win32/lwef/animation-principles
