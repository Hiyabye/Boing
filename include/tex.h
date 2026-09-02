#ifndef TEX_H
#define TEX_H

#include "raylib.h"

#include <stdbool.h>

// Runtime-generated textures — the noise/soft-edge half of the art pipeline.
//
// Same no-asset-files doctrine as svg.c, one step further: these carry no
// source data at all, just a generator call. raylib's GenImage* family
// (SUPPORT_IMAGE_GENERATION) builds them at startup, so a cloud with real
// internal structure or a sky full of stars costs zero bytes of the 1.44MB
// budget.
//
// Division of labour with the `svg` subsystem:
//   noise or a soft falloff  -> here
//   a crisp authored outline -> an SvgId
//   reshapes every frame     -> stays immediate-mode primitives
//
// Like the glyphs, every texture is authored **white with alpha** and tinted at
// draw time — a tint multiplies uniformly, so the theme recolors them for free.
// TEX_STATIC is the one exception (opaque grey noise, drawn tinted WHITE).
typedef enum {
  TEX_PUFF,      // soft radial falloff disc: glows, halos, particles, trail
  TEX_CLOUD,     // perlin x radial falloff: a cloud body with real structure
  TEX_STARFIELD, // clumped white-noise stars, tileable
  TEX_NEBULA,    // low-frequency perlin haze, for sky depth
  TEX_ROCK,      // cellular surface grain, tileable
  TEX_STATIC,    // white noise, tileable — the credits channel snow
  TEX_COUNT
} TexId;

// The baked texture, for draws that need their own source rect (tiling, a
// jittered offset). `.id == 0` if the bake failed — check before using.
Texture2D texGet(TexId id);

// Centered draw, the common case: `cx,cy` is the center of a w x h box.
void drawTex(TexId id, float cx, float cy, float w, float h, float rotDeg,
             Color tint);

// Generates one texture per call; true once the queue has drained. The splash
// drains this cheap queue in one update beside the SVG and font bakes.
bool texPreloadStep(void);

void unloadTexCache(void);

#endif // TEX_H
