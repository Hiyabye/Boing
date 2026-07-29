// The game's vector font: 49 skeleton glyphs, one nanosvg raster into one
// atlas, wrapped in a raylib Font. See include/font.h for the why.
#include "font.h"
#include "core.h"

#include "nanosvg.h"
#include "nanosvgrast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// The design grid
// ---------------------------------------------------------------------------
// One CELL x CELL cell per glyph, and CELL is also Font.baseSize — so a
// fontSize of 30 scales the whole cell by 30/128. Every number in GLYPHS below
// is in these cell units.
//
//   ink cap top      y =  6      cap height 90 = 0.70 em, which matches the
//   ink baseline     y = 96      ink ratio of the default font this replaces,
//   ink below        y = 96..128 so existing sizes and y positions barely shift
//
// Paths are *skeletons*, stroked at STROKE with round caps, so a skeleton runs
// half a stroke inside the ink box: y = 13 (top) to y = 89 (baseline). The
// standard letter spans x = 12..64, giving ink 5..71 in an advance of 78 —
// i.e. ~5 units of sidebearing either side. Descenders do not exist (the set is
// caps-only); y = 96..128 is leading, plus room for the comma and Q tails.
#define CELL 128
#define STROKE 14
#define COLS 7
// A truncated SVG parses into garbage shapes rather than failing, so the
// wrapper is skipped rather than clipped if a path ever outgrows this.
#define SCRATCH 512

typedef struct {
  const char *d;     // skeleton path data, stroked not filled; "" for space
  unsigned char adv; // pen advance, cell units
} Glyph;

// Parallel to GLYPHS, and the source of the codepoint in each Font entry.
static const char FONT_CHARS[] =
    " !%()+,-.:?[]0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
#define GLYPH_COUNT ((int)(sizeof(FONT_CHARS) - 1))

// Exactly the characters the game can reach: '%' from the volume rows, '[' ']'
// from the mission checkboxes, '(' ')' '.' from the credits reel, '+' from the
// mission rewards, '-' from the reel's section headers and a negative "%d m".
// Adding one is an entry in FONT_CHARS plus a row here, in the same position;
// anything missing draws '?' (raylib's GetGlyphIndex picks it as the fallback).
//
// Positional and unsized, like SVG_DEFS: a row dropped anywhere shortens the
// array *and* desyncs it from FONT_CHARS, so the assert below catches it. The
// cost is that rows must stay in exact FONT_CHARS order.
static const Glyph GLYPHS[] = {
    {"", 44},                  // space
    {"M16 13V62M16 88v1", 32}, // !
    {"M65 13 23 89M13 25a12 12 0 1 0 24 0a12 12 0 1 0-24 0"
     "M47 77a12 12 0 1 0 24 0a12 12 0 1 0-24 0",
     88},                                                          // %
    {"M26 13Q2 51 26 89", 40},                                     // (
    {"M14 13Q38 51 14 89", 40},                                    // )
    {"M14 51H58M36 29v44", 72},                                    // +
    {"M22 80 14 100", 36},                                         // ,
    {"M12 54H48", 60},                                             // -
    {"M16 88v1", 32},                                              // .
    {"M16 44v1M16 82v1", 32},                                      // :
    {"M14 32Q14 13 34 13 54 13 54 32Q54 48 34 62V68M34 88v1", 68}, // ?
    {"M32 13H14v76h18", 42},                                       // [
    {"M14 13h18v76H14", 42},                                       // ]
    {"M37 13a25 38 0 1 0 0 76a25 38 0 1 0 0-76", 74},              // 0
    {"M22 28 38 13V89", 74},                                       // 1
    {"M13 30Q13 13 37 13 61 13 61 32Q61 48 13 89H62", 74},         // 2
    {"M14 24Q22 13 38 13Q60 13 60 31Q60 49 40 49Q60 49 60 69"
     "Q60 89 38 89Q19 89 13 78",
     74},                                                        // 3
    {"M46 89V13L12 66H62", 74},                                  // 4
    {"M60 13H18v34h20Q62 47 62 68Q62 89 38 89Q19 89 13 79", 74}, // 5
    {"M56 17Q48 13 38 13Q15 13 15 54v11Q15 89 38 89Q61 89 61 66"
     "Q61 45 38 45Q20 45 15 57",
     74},                    // 6
    {"M13 13h49L28 89", 74}, // 7
    {"M38 13a22 18 0 1 0 0 36a22 18 0 1 0 0-36"
     "M38 49a24 20 0 1 0 0 40a24 20 0 1 0 0-40",
     74}, // 8
    {"M20 85Q28 89 38 89Q61 89 61 48V37Q61 13 38 13Q15 13 15 36"
     "Q15 57 38 57Q56 57 61 45",
     74},                                // 9
    {"M12 89 40 13 68 89M24 64H56", 80}, // A
    {"M12 13V89M12 13h32Q64 13 64 32 64 51 44 51H12"
     "M12 51h34Q68 51 68 70 68 89 46 89H12",
     82},                                                                 // B
    {"M62 28Q52 13 38 13Q12 13 12 51Q12 89 38 89Q52 89 62 74", 78},       // C
    {"M12 13V89M12 13h24Q66 13 66 51 66 89 36 89H12", 80},                // D
    {"M62 13H12v76h50M12 51h40", 74},                                     // E
    {"M62 13H12v76M12 51h40", 74},                                        // F
    {"M62 28Q52 13 38 13Q12 13 12 51Q12 89 38 89Q62 89 62 68V56H42", 78}, // G
    {"M12 13v76M64 13v76M12 51h52", 78},                                  // H
    {"M17 13v76", 34},                                                    // I
    {"M50 13v56Q50 89 30 89Q14 89 12 74", 62},                            // J
    {"M12 13v76M62 13 20 52 64 89", 76},                                  // K
    {"M12 13v76h46", 68},                                                 // L
    {"M12 89V13l32 46 32-46v76", 90},                                     // M
    {"M12 89V13l52 76V13", 78},                                           // N
    {"M38 13a26 38 0 1 0 0 76a26 38 0 1 0 0-76", 78},                     // O
    {"M12 89V13h32Q66 13 66 34 66 55 44 55H12", 80},                      // P
    {"M38 13a26 38 0 1 0 0 76a26 38 0 1 0 0-76M46 72 66 94", 80},         // Q
    {"M12 89V13h32Q66 13 66 34 66 55 44 55H12M42 55 66 89", 80},          // R
    {"M62 26Q54 13 37 13Q14 13 14 31Q14 46 38 49Q64 52 64 70"
     "Q64 89 38 89Q19 89 12 76",
     78},                                         // S
    {"M12 13h56M40 13v76", 80},                   // T
    {"M12 13v46Q12 89 38 89Q64 89 64 59V13", 78}, // U
    {"M12 13 38 89 64 13", 78},                   // V
    {"M12 13 30 89 48 29 66 89 84 13", 98},       // W
    {"M12 13 64 89M64 13 12 89", 78},             // X
    {"M12 13 38 50v39M64 13 38 50", 78},          // Y
    {"M12 13h50L12 89h52", 78},                   // Z
};
STATIC_ASSERT(sizeof(GLYPHS) / sizeof(GLYPHS[0]) == GLYPH_COUNT,
              font_glyph_table_complete);

#define ROWS ((GLYPH_COUNT + COLS - 1) / COLS)
#define ATLAS_W (COLS * CELL)
#define ATLAS_H (ROWS * CELL)

// ---------------------------------------------------------------------------
// Baking
// ---------------------------------------------------------------------------
// Every glyph into one COLS x ROWS grid of cells. The caller owns the returned
// pixels (free them); `.data == NULL` on failure.
static Image buildAtlas(void) {
  Image im = {0};
  unsigned char *px = calloc((size_t)ATLAS_W * ATLAS_H, 4);
  if (px == NULL) {
    return im;
  }
  NSVGrasterizer *rast = nsvgCreateRasterizer();
  if (rast == NULL) {
    free(px);
    return im;
  }

  for (int i = 0; i < GLYPH_COUNT; i++) {
    // The wrapper is built here rather than stored per row: it is ~150 bytes
    // that would otherwise be repeated 49 times in the binary.
    char scratch[SCRATCH];
    int n =
        snprintf(scratch, sizeof(scratch),
                 "<svg viewBox=\"0 0 %d %d\" xmlns=\"http://www.w3.org/2000/"
                 "svg\"><path d=\"%s\" fill=\"none\" stroke=\"#fff\""
                 " stroke-width=\"%d\" stroke-linecap=\"round\""
                 " stroke-linejoin=\"round\"/></svg>",
                 CELL, CELL, GLYPHS[i].d, STROKE);
    if (n < 0 || n >= (int)sizeof(scratch)) {
      continue; // skipped, not truncated — the cell just stays empty
    }
    NSVGimage *svg = nsvgParse(scratch, "px", 96.0f);
    if (svg == NULL) {
      continue;
    }
    // Straight into the cell rather than via a scratch image: nsvgRasterize
    // clears exactly w*4 bytes per row, so a sub-buffer plus the full atlas
    // stride touches this cell and nothing else. Handing it the whole atlas
    // with a tx/ty offset instead would clear all of it on every glyph.
    unsigned char *cell =
        px +
        ((size_t)(i / COLS) * CELL * ATLAS_W + (size_t)(i % COLS) * CELL) * 4;
    float scale = (svg->width > 0.0f) ? CELL / svg->width : 1.0f;
    nsvgRasterize(rast, svg, 0.0f, 0.0f, scale, cell, CELL, CELL, ATLAS_W * 4);
    nsvgDelete(svg);
  }
  nsvgDeleteRasterizer(rast);

  // nanosvg blends premultiplied over a transparent target, so antialiased edge
  // texels land with RGB far below 255. A tint multiply then darkens every edge
  // — at 16px that is most of the glyph, and text reads muddy and thin — and
  // the mipmap chain averages that dark RGB into the classic halo. Alpha
  // carries the whole shape; RGB is uniform white and tinted at draw time.
  // Deliberately not ported to svg.c: its art is not white-only, and SVG_LOGO
  // carries a baked gold gradient this would erase.
  for (size_t p = 0; p < (size_t)ATLAS_W * ATLAS_H * 4; p += 4) {
    px[p] = px[p + 1] = px[p + 2] = 255;
  }

  im.data = px;
  im.width = ATLAS_W;
  im.height = ATLAS_H;
  im.mipmaps = 1;
  im.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
  return im;
}

#ifndef FONT_SELFTEST
static Font gFont;
static bool gBuilt;

static void build(void) {
  gBuilt = true; // a failure is not retried; gameFont keeps the default font
  Image im = buildAtlas();
  if (im.data == NULL) {
    return;
  }
  Texture2D tex = LoadTextureFromImage(im);
  free(im.data);
  if (tex.id == 0) {
    return;
  }
  GenTextureMipmaps(&tex);
  SetTextureFilter(tex, TEXTURE_FILTER_TRILINEAR);

  // One entry per authored glyph, then one alias per letter carrying the
  // lowercase codepoint over the same cell — so mixed-case copy renders
  // uppercase instead of falling through to '?', at zero atlas cost.
  Rectangle *recs = MemAlloc((unsigned)(GLYPH_COUNT + 26) * sizeof(*recs));
  GlyphInfo *info = MemAlloc((unsigned)(GLYPH_COUNT + 26) * sizeof(*info));
  if (recs == NULL || info == NULL) {
    MemFree(recs);
    MemFree(info);
    UnloadTexture(tex);
    return;
  }
  for (int i = 0; i < GLYPH_COUNT; i++) {
    recs[i] = (Rectangle){(float)((i % COLS) * CELL),
                          (float)((i / COLS) * CELL), CELL, CELL};
    // glyphPadding stays 0 and offsetX/Y stay 0: the sidebearings are already
    // inside the cell, and DrawTextCodepoint would grow both the source and the
    // destination rect by any padding declared here.
    info[i] = (GlyphInfo){.value = FONT_CHARS[i], .advanceX = GLYPHS[i].adv};
  }
  int n = GLYPH_COUNT;
  for (int i = 0; i < GLYPH_COUNT; i++) {
    if (FONT_CHARS[i] < 'A' || FONT_CHARS[i] > 'Z') {
      continue;
    }
    recs[n] = recs[i];
    info[n] = info[i];
    info[n].value = FONT_CHARS[i] + 32;
    n++;
  }

  gFont = (Font){.baseSize = CELL,
                 .glyphCount = n,
                 .glyphPadding = 0,
                 .texture = tex,
                 .recs = recs,
                 .glyphs = info};
}

Font gameFont(void) {
  return (gFont.texture.id != 0) ? gFont : GetFontDefault();
}

bool fontPreloadStep(void) {
  if (!gBuilt) {
    build();
  }
  return true; // the whole atlas lands in one call, so there is never a queue
}

// Not UnloadFont: these glyphs carry no per-glyph Image, so there is nothing
// for raylib's font-data teardown to do and no reason to depend on it.
void unloadGameFont(void) {
  if (gFont.texture.id != 0) {
    UnloadTexture(gFont.texture);
  }
  MemFree(gFont.recs);
  MemFree(gFont.glyphs);
  gFont = (Font){0};
  gBuilt = false;
}
#endif // !FONT_SELFTEST

#ifdef FONT_SELFTEST
// Bakes the atlas and checks the two things that break silently: a glyph whose
// path data produced nothing, and a glyph whose *metrics* are wrong. The second
// is the point — a contact sheet of cells looks perfect while the advances are
// off, and only a laid-out line of text shows it, so this writes one. Headless:
// nanosvg rasterizing and ExportImage are both CPU-only, no InitWindow. Build:
//   cc -DFONT_SELFTEST src/font.c lib/nanosvg_impl.c -Iinclude \
//      -Ilib/nanosvg/src -Ilib/raylib/src build/lib/raylib/raylib/libraylib.a \
//      -lm -framework Cocoa -framework IOKit -framework CoreVideo \
//      -framework OpenGL -o /tmp/fonttest && /tmp/fonttest /tmp/fontsheet.png
#include <assert.h>

#define SAMPLE "PACK MY BOX WITH 5 DOZEN JUGS! (QTY: 1.44MB) [100%]"
#define LINE_CELL                                                              \
  48 // sample-line cell size in px; the grid uses ATLAS/GRID_DIV
#define GRID_DIV 3

// FONT_CHARS index of a character, uppercasing first (the Font aliases
// lowercase onto the same cells). -1 if the font has no such glyph.
static int glyphIndex(char c) {
  const char *at = strchr(FONT_CHARS, (c >= 'a' && c <= 'z') ? c - 32 : c);
  return (at != NULL) ? (int)(at - FONT_CHARS) : -1;
}

// Ink bounds of one cell, in cell units. Returns the lit-pixel count.
static long cellInk(const unsigned char *px, int idx, int *x0, int *x1, int *y0,
                    int *y1) {
  const unsigned char *cell =
      px +
      ((size_t)(idx / COLS) * CELL * ATLAS_W + (size_t)(idx % COLS) * CELL) * 4;
  long lit = 0;
  *x0 = *y0 = CELL;
  *x1 = *y1 = -1;
  for (int y = 0; y < CELL; y++) {
    for (int x = 0; x < CELL; x++) {
      if (cell[(size_t)y * ATLAS_W * 4 + (size_t)x * 4 + 3] <= 96) {
        continue;
      }
      lit++;
      *x0 = (x < *x0) ? x : *x0;
      *x1 = (x > *x1) ? x : *x1;
      *y0 = (y < *y0) ? y : *y0;
      *y1 = (y > *y1) ? y : *y1;
    }
  }
  return lit;
}

int main(int argc, char **argv) {
  Image atlas = buildAtlas();
  assert(atlas.data != NULL && "atlas failed to allocate or rasterize");

  for (int i = 0; i < GLYPH_COUNT; i++) {
    size_t len = strlen(GLYPHS[i].d);
    assert(len + 200 < SCRATCH && "glyph path outgrew SCRATCH");

    int x0, x1, y0, y1;
    long lit = cellInk(atlas.data, i, &x0, &x1, &y0, &y1);
    if (FONT_CHARS[i] == ' ') {
      assert(lit == 0 && "the space glyph drew ink");
      continue;
    }
    printf("%2d '%c'  %6ld px  x %3d..%-3d adv %3d  y %3d..%d\n", i,
           FONT_CHARS[i], lit, x0, x1, GLYPHS[i].adv, y0, y1);
    // A lone round dot ('.') is only ~150px at this stroke width, so the floor
    // has to sit under that — it is catching an *empty* cell, not a small one.
    assert(lit > 100 && "glyph rasterized (nearly) empty — bad path data");

    // The metric checks. Ink outside [0, adv] collides with the neighbouring
    // letter or opens a hole; nothing else in the build would notice.
    assert(x0 >= 0 && x1 < GLYPHS[i].adv && "ink escapes its own advance");

    // Letters and digits share one cap box. A glyph authored at the wrong
    // height looks fine alone in its cell and obviously broken in a line.
    bool capBox = (FONT_CHARS[i] >= 'A' && FONT_CHARS[i] <= 'Z') ||
                  (FONT_CHARS[i] >= '0' && FONT_CHARS[i] <= '9');
    if (capBox) {
      assert(y0 <= 10 && "glyph does not reach the cap line");
      assert(y1 >= 92 && "glyph does not reach the baseline");
    }
  }

  // Contact sheet, plus one line laid out with the real advances underneath it
  // — the grid proves the shapes, only the line shows the rhythm.
  int advSum = 0;
  for (const char *s = SAMPLE; *s != '\0'; s++) {
    int idx = glyphIndex(*s);
    assert(idx >= 0 && "sample string uses a character the font lacks");
    advSum += GLYPHS[idx].adv;
  }
  int gridW = ATLAS_W / GRID_DIV, gridH = ATLAS_H / GRID_DIV;
  int lineW = advSum * LINE_CELL / CELL + 8;
  Image sheet = GenImageColor((gridW > lineW) ? gridW : lineW,
                              gridH + LINE_CELL + 8, (Color){40, 40, 48, 255});

  Image cells = ImageCopy(atlas);
  ImageResize(&cells, gridW, gridH);
  ImageDrawImage(&sheet, cells, 0, 0, WHITE);
  UnloadImage(cells);

  int pen = 0;
  for (const char *s = SAMPLE; *s != '\0'; s++) {
    int idx = glyphIndex(*s);
    Rectangle src = {(float)((idx % COLS) * CELL), (float)((idx / COLS) * CELL),
                     CELL, CELL};
    Rectangle dst = {4.0f + (float)pen * LINE_CELL / CELL, (float)gridH + 4.0f,
                     LINE_CELL, LINE_CELL};
    ImageDrawImagePro(&sheet, atlas, src, dst, (Vector2){0, 0}, 0.0f, WHITE);
    pen += GLYPHS[idx].adv;
  }

  ExportImage(sheet, argc > 1 ? argv[1] : "fontsheet.png");
  printf("ok — %d glyphs, %dx%d atlas, sample line %d cell units\n",
         GLYPH_COUNT, ATLAS_W, ATLAS_H, advSum);
  UnloadImage(sheet);
  free(atlas.data);
  return 0;
}
#endif // FONT_SELFTEST
