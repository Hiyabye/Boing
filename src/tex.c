#include "tex.h"

#include "core.h"
#include "raylib.h"

#include <math.h>
#include <stddef.h>

// Master sizes. These are only ever downsampled (mipmaps + trilinear), same
// rule as the SVG glyphs: pick a size above the worst-case device footprint and
// never magnify past it. PUFF gets away with 128 because it is pure blur —
// there is no detail in it to lose.
#define PUFF_PX 128
#define NOISE_PX 256
#define FIELD_PX 512

// White with zero alpha. Fading to BLANK instead would drag RGB toward black,
// and the mipmap chain averages RGB across transparent texels — that is the
// classic dark halo around a soft sprite. Holding RGB at white keeps every mip
// level clean.
#define WHITE_CLEAR (Color){255, 255, 255, 0}

// --- pixel helpers -------------------------------------------------------
// Every GenImage* in raylib returns PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, so these
// walk `Color *` directly with no ImageFormat conversion.

// Grey noise -> white-with-alpha, so a tint can recolor it.
static void lumaToAlpha(Image *img) {
  Color *p = img->data;
  for (int i = 0, n = img->width * img->height; i < n; i++) {
    p[i] = (Color){255, 255, 255, p[i].r};
  }
}

// Multiply dst's alpha by src's luminance — how a shape mask and a noise field
// combine into one texture (a radial falloff x perlin = a cloud).
static void mulAlphaLuma(Image *dst, Image src) {
  Color *d = dst->data;
  const Color *s = src.data;
  for (int i = 0, n = dst->width * dst->height; i < n; i++) {
    d[i].a = (unsigned char)(d[i].a * s[i].r / 255);
  }
}

// Gamma the alpha ramp: g > 1 eats the faint tail (only bright wisps survive),
// g < 1 lifts it. The contrast knob for every generator below.
static void alphaCurve(Image *img, float g) {
  unsigned char lut[256];
  for (int i = 0; i < 256; i++) {
    lut[i] = (unsigned char)(255.0f * powf(i / 255.0f, g) + 0.5f);
  }
  Color *p = img->data;
  for (int i = 0, n = img->width * img->height; i < n; i++) {
    p[i].a = lut[p[i].a];
  }
}

// --- generators ----------------------------------------------------------

// Soft disc. density 0 = falloff starts at the very center; squaring it pulls
// the bright core in so the sprite reads as a glow, not a blurry ball.
static Image genPuff(int px) {
  Image im = GenImageGradientRadial(px, px, 0.0f, WHITE, WHITE_CLEAR);
  alphaCurve(&im, 2.0f);
  return im;
}

// One cloud body. The high density (0.45) leaves a broad flat core and squeezes
// the whole falloff into a narrow outer ring — so the perlin multiply chews
// lumps out of the *edge* while the middle stays solid. Multiplying a gentle
// falloff instead just yields a fuzzy ball with texture on it. The curve is < 1
// to win back the opacity the noise multiply costs.
// This is still a *round* body: a radial mask is a circle however much noise is
// inside it. The lumpy cloud silhouette comes from the arrangement — see
// drawCloud in background.c, which stamps three of these in a clump.
static Image genCloud(int px) {
  Image im = GenImageGradientRadial(px, px, 0.45f, WHITE, WHITE_CLEAR);
  Image n = GenImagePerlinNoise(px, px, 0, 0, 4.0f);
  mulAlphaLuma(&im, n);
  UnloadImage(n);
  alphaCurve(&im, 0.5f);
  return im;
}

// Starfield: sparse white noise for the stars, times a low-frequency perlin so
// they clump into bands and lanes instead of dusting evenly. The perlin costs
// most stars most of their brightness, so the curve is aggressive — without it
// the field is a barely-visible dusting.
//
// STAR_DENSITY is a *coarse* knob, and the reason is a raylib implementation
// detail worth knowing before reaching for it: GenImageWhiteNoise tests
// `GetRandomValue(0, 99) < (int)(factor * 100.0f)`, so the factor is quantized
// to whole percent and everything below 0.01 truncates to **zero stars**. The
// only steps that exist here are 1%, 2%, 3%… — 0.009 is not "slightly sparser
// than 0.01", it is an empty sky.
//
// STAR_THIN is the fine knob instead. Each star's alpha comes from the perlin
// multiply, so a gamma above 1 deletes the ones sitting in the perlin's dim
// regions outright and keeps the ones in its bright regions at full strength —
// the field thins into lanes and clumps (which is what the perlin was for) and
// every surviving star stays a sharp 1px dot. Enlarging the on-screen tile in
// background.c would also thin the field, but by magnifying 1px stars into
// bokeh; this doesn't.
//
// Judging it is its own trap: the contact strip downsamples 512px to a
// thumbnail, which eats 1px stars, so the field always looks sparser there than
// it does in game. Judge at full resolution or in-game, never off the strip's
// cell — that mistake is what put this at 2% and made the night sky snow.
#define STAR_DENSITY 0.01f
#define STAR_THIN 2.8f
static Image genStarfield(int px) {
  Image im = GenImageWhiteNoise(px, px, STAR_DENSITY);
  lumaToAlpha(&im);
  Image n = GenImagePerlinNoise(px, px, 0, 0, 2.0f);
  mulAlphaLuma(&im, n);
  UnloadImage(n);
  alphaCurve(&im, STAR_THIN);
  return im;
}

// Sky haze. The steep curve keeps only the top of the perlin range, so this is
// a few soft wisps rather than an even fog over the whole sky.
static Image genNebula(int px) {
  Image im = GenImagePerlinNoise(px, px, 0, 0, 4.0f);
  lumaToAlpha(&im);
  alphaCurve(&im, 3.0f);
  return im;
}

// Surface grain. Cellular is distance-to-nearest-seed, so cell centers come out
// dark (transparent) and the boundaries bright — a web of veins/cracks, which
// is what gives a flat rectangle the look of stone.
static Image genRock(int px) {
  Image im = GenImageCellular(px, px, px / 8);
  lumaToAlpha(&im);
  alphaCurve(&im, 1.5f);
  return im;
}

// Channel snow. The one texture kept opaque grey: it is drawn tinted WHITE over
// a dead tube, so it wants real black-to-white speckle, not an alpha mask.
static Image genStatic(int px) { return GenImageWhiteNoise(px, px, 0.5f); }

// --- table ---------------------------------------------------------------

typedef struct {
  Image (*gen)(int px);
  short px;
  bool repeat; // GL_REPEAT, for the ones drawn with an oversized source rect
} TexDef;

// Deliberately unsized and positional in exact TexId order: dropping any row
// shortens the table and makes the count check fail.
static const TexDef TEX_DEFS[] = {
    {genPuff, PUFF_PX, false},      {genCloud, NOISE_PX, false},
    {genStarfield, FIELD_PX, true}, {genNebula, NOISE_PX, false},
    {genRock, NOISE_PX, true},      {genStatic, NOISE_PX, true},
};
STATIC_ASSERT(sizeof(TEX_DEFS) / sizeof(TEX_DEFS[0]) == TEX_COUNT,
              tex_defs_table_complete);

static Texture2D gTex[TEX_COUNT];
static bool gBaked[TEX_COUNT];

// Marked baked either way, so a failure is not retried every frame — it just
// draws nothing. Same degrade-quietly contract as a glyph that fails to parse.
static void bake(TexId id) {
  gBaked[id] = true;
  if (TEX_DEFS[id].gen == NULL) {
    return;
  }
  Image im = TEX_DEFS[id].gen(TEX_DEFS[id].px);
  if (im.data == NULL) {
    return;
  }
  gTex[id] = LoadTextureFromImage(im);
  UnloadImage(im);
  if (gTex[id].id != 0) {
    GenTextureMipmaps(&gTex[id]);
    SetTextureFilter(gTex[id], TEXTURE_FILTER_TRILINEAR);
    if (TEX_DEFS[id].repeat) {
      SetTextureWrap(gTex[id], TEXTURE_WRAP_REPEAT);
    }
  }
}

bool texPreloadStep(void) {
  for (int i = 0; i < TEX_COUNT; i++) {
    if (!gBaked[i]) {
      bake((TexId)i);
      return false;
    }
  }
  return true;
}

Texture2D texGet(TexId id) {
  if (!gBaked[id]) {
    bake(id); // lazy fallback: only if something drew before the splash ended
  }
  return gTex[id];
}

void drawTex(TexId id, float cx, float cy, float w, float h, float rotDeg,
             Color tint) {
  Texture2D t = texGet(id);
  if (t.id == 0) {
    return;
  }
  DrawTexturePro(t, (Rectangle){0, 0, (float)t.width, (float)t.height},
                 (Rectangle){cx, cy, w, h}, (Vector2){w * 0.5f, h * 0.5f},
                 rotDeg, tint);
}

void unloadTexCache(void) {
  for (int i = 0; i < TEX_COUNT; i++) {
    if (gTex[i].id != 0) {
      UnloadTexture(gTex[i]);
    }
    gTex[i] = (Texture2D){0};
    gBaked[i] = false;
  }
}

#ifdef TEX_SELFTEST
// Contact strip for eyeballing the generators without launching the game, plus
// the assert that matters: a texture must not come out *flat*. Every generator
// here is a chain of multiplies and a gamma curve, and the way those break is
// silently — a wrong exponent or a zeroed mask yields a uniform image that
// still draws, just as a solid block or as nothing at all. Build:
//   cc -DTEX_SELFTEST src/tex.c -Iinclude -Ilib/raylib/src \
//      build/lib/raylib/raylib/libraylib.a -lm \
//      -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL
//      \ -o /tmp/textest && /tmp/textest /tmp/texsheet.png
// Runs headless (no InitWindow): the generators and ExportImage are CPU-only.
#include <assert.h>
#include <stdio.h>

#define CELL 192

int main(int argc, char **argv) {
  Image sheet = GenImageColor(CELL * TEX_COUNT, CELL, (Color){24, 24, 32, 255});

  for (int i = 0; i < TEX_COUNT; i++) {
    // Keep bake's quiet failure loud in the self-test before dereferencing it.
    assert(TEX_DEFS[i].gen != NULL && "TEX_DEFS row missing a generator");
    Image im = TEX_DEFS[i].gen(TEX_DEFS[i].px);
    assert(im.data != NULL && "generator returned no pixels");
    assert(im.format == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 &&
           "generator format changed — the pixel helpers assume RGBA8");

    // Span of the channel each texture actually carries its detail in: alpha
    // for the masks, luminance for the opaque snow. A flat image fails.
    const Color *p = im.data;
    int aLo = 255, aHi = 0, lLo = 255, lHi = 0;
    for (int k = 0, n = im.width * im.height; k < n; k++) {
      aLo = (p[k].a < aLo) ? p[k].a : aLo;
      aHi = (p[k].a > aHi) ? p[k].a : aHi;
      lLo = (p[k].r < lLo) ? p[k].r : lLo;
      lHi = (p[k].r > lHi) ? p[k].r : lHi;
    }
    int span = ((aHi - aLo) > (lHi - lLo)) ? (aHi - aLo) : (lHi - lLo);
    printf("%d: %4dpx  alpha %3d..%3d  luma %3d..%3d\n", i, TEX_DEFS[i].px, aLo,
           aHi, lLo, lHi);
    assert(span > 32 && "texture came out flat — generator chain is broken");

    Image c = ImageCopy(im);
    ImageResize(&c, CELL, CELL);
    ImageDrawImage(&sheet, c, i * CELL, 0, WHITE);
    UnloadImage(c);
    UnloadImage(im);
  }
  ExportImage(sheet, argc > 1 ? argv[1] : "texsheet.png");
  printf("ok — %d textures\n", TEX_COUNT);
  return 0;
}
#endif
