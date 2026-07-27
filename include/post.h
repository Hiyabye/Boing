#ifndef POST_H
#define POST_H

#include "raylib.h"

// Post-processing. The whole frame is drawn into one offscreen target and
// resolved through a single fragment shader, whose GLSL source is a string
// literal baked into the binary — the same no-asset-files doctrine as svg.c
// (an asset would cost budget against the 1.44MB limit and add a missing-file
// failure mode).
//
// The target is deliberately *supersampled*: a RenderTexture is not
// multisampled, so routing the frame through one would drop the window's
// FLAG_MSAA_4X_HINT antialiasing. Rendering at 2x and downsampling on the
// resolve replaces it (and beats it — it smooths gradient interiors and the
// point-filtered bitmap font too, which MSAA never touched).
//
// Everything degrades quietly: if the framebuffer or the shader fails, the
// subsystem switches itself off and the game draws straight to the backbuffer
// exactly as it did before post existed — same contract as a music track whose
// buffer failed to allocate, or a glyph that failed to parse.

// Per-frame effect strengths. main() zeroes this before the draw pass and each
// screen's draw handler raises whatever it wants; postEnd consumes it.
typedef struct {
  float bloom;       // 0..1 glow on bright pixels (neon era)
  float fog;         // 0..1 Fog-debuff murk: blur + drifting noise + haze
  float crt;         // 0..1 CRT tube: barrel, chroma split, scanlines, mask
  float vignette;    // 0..1 corner darkening
  Rectangle crtRect; // 800-space rect the CRT is confined to (ignored if empty)
} PostFx;

// Bind the offscreen target for this frame's draw. Creates it on the first call
// and re-creates it whenever the window resolution changes.
void postBegin(void);
// 800-space -> target-pixel scale for the frame's matrix. Valid after
// postBegin, and the scale the drawing coordinate system is actually in — use
// this, not GetScreenWidth()/SCREEN_WIDTH, for anything that has to escape the
// matrix (a scissor rect).
float postScale(void);
// Resolve: blit the target back through the shader with `fx` applied.
void postEnd(const PostFx *fx);
void unloadPost(void);

#endif // POST_H
