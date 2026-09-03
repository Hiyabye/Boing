// Post-processing: one offscreen target, one fragment shader, four effects.
// See post.h for the doctrine (baked GLSL, supersampled target, quiet degrade).
#include "post.h"

#include "core.h"
#include "rlgl.h"

#include <stddef.h>

// Supersampling. 2x costs 4x fill but replaces the MSAA a RenderTexture can't
// have; above this render width the window is already dense enough that the
// aliasing it hides isn't worth a 5600^2 target (the 1400pt Retina setting).
#define POST_SS 2
#define POST_SS_MAX 1600

// ---------------------------------------------------------------------------
// The shader. Every effect is gated on its own uniform, so a branch is uniform
// across the whole draw (free on the GPU) and an inactive effect costs nothing.
// Tuning knobs are the `const float`s at the top — game feel is hand-tuned
// there, same as the #define block at the top of world.c.
// ---------------------------------------------------------------------------
static const char *POST_FS =
    "#version 330\n"
    "in vec2 fragTexCoord;\n"
    "out vec4 finalColor;\n"
    "uniform sampler2D texture0;\n"
    "uniform float uBloom;\n"
    "uniform float uFog;\n"
    "uniform float uCrt;\n"
    "uniform float uVig;\n"
    "uniform float uTime;\n"
    "uniform vec4 uRect;\n" // CRT tube in 0..1 uv: xy origin, zw size
    // --- knobs ---
    "const float BARREL = 0.16;\n"  // tube bulge
    "const float CHROMA = 0.010;\n" // radial r/b split at the edges
    "const float SCAN = 0.22;\n"    // scanline depth
    "const float LINES = 190.0;\n"  // scanlines across the tube, zoom-invariant
    "const float TRIADS = 110.0;\n" // phosphor triads across the tube
    "const float MASK = 0.22;\n"    // phosphor mask depth
    "const float BTHRESH = 0.30;\n" // bloom brightpass cut
    "const float BGAIN = 2.00;\n"   // bloom add
    "const vec3 FOGTINT = vec3(0.60, 0.63, 0.72);\n"
    // --- 2-octave value noise, for the fog ---
    "float h21(vec2 p){ return fract(sin(dot(p, vec2(41.3, 289.1))) * "
    "43758.5453); }\n"
    "float vnoise(vec2 p){\n"
    "  vec2 i = floor(p), f = fract(p);\n"
    "  f = f * f * (3.0 - 2.0 * f);\n"
    "  return mix(mix(h21(i), h21(i + vec2(1,0)), f.x),\n"
    "             mix(h21(i + vec2(0,1)), h21(i + vec2(1,1)), f.x), f.y);\n"
    "}\n"
    "void main(){\n"
    "  vec2 uv = fragTexCoord;\n"
    "  vec3 c = texture(texture0, uv).rgb;\n"
    // CRT, confined to uRect. Sampling coords are pushed outward with the
    // square of the radius, which is what makes the picture read as bulged;
    // anything that lands outside the tube after the warp is bezel, not image.
    "  if (uCrt > 0.0){\n"
    "    vec2 t = (uv - uRect.xy) / uRect.zw;\n"
    "    if (t.x >= 0.0 && t.x <= 1.0 && t.y >= 0.0 && t.y <= 1.0){\n"
    "      vec2 p = t * 2.0 - 1.0;\n"
    "      float r2 = dot(p, p);\n"
    "      p += p * (BARREL * uCrt) * r2;\n"
    "      vec2 ca = p * (CHROMA * uCrt) * r2;\n"
    "      vec2 w = p * 0.5 + 0.5;\n"
    "      if (w.x < 0.0 || w.x > 1.0 || w.y < 0.0 || w.y > 1.0){\n"
    "        c = vec3(0.0);\n"
    "      } else {\n"
    // The r/b offsets are clamped back inside the tube: unclamped they sample
    // the cabinet just past the glass and paint a red fringe along the warped
    // border.
    "        vec2 wr = clamp(p * 0.5 + 0.5 + ca * 0.5, 0.0, 1.0);\n"
    "        vec2 wb = clamp(p * 0.5 + 0.5 - ca * 0.5, 0.0, 1.0);\n"
    "        vec3 s;\n"
    "        s.r = texture(texture0, uRect.xy + wr * uRect.zw).r;\n"
    "        s.g = texture(texture0, uRect.xy + w * uRect.zw).g;\n"
    "        s.b = texture(texture0, uRect.xy + wb * uRect.zw).b;\n"
    "        s *= 1.0 - SCAN * uCrt * (0.5 - 0.5 * cos(w.y * 6.2831853 * "
    "LINES));\n"
    "        float ph = fract(w.x * TRIADS) * 3.0;\n"
    "        vec3 m = vec3(ph < 1.0 ? 1.0 : 0.0, (ph >= 1.0 && ph < 2.0) ? 1.0 "
    ": 0.0, ph >= 2.0 ? 1.0 : 0.0);\n"
    "        s *= mix(vec3(1.0), 0.75 + 0.75 * m, MASK * uCrt);\n"
    "        c = s;\n"
    "      }\n"
    "    }\n"
    "  }\n"
    // Bloom: the mip chain is the blur. Thresholding after the blur (rather
    // than a real bright-pass into its own target) is the cheap version.
    "  if (uBloom > 0.0){\n"
    "    vec3 b = textureLod(texture0, uv, 3.0).rgb * 0.6 +\n"
    "             textureLod(texture0, uv, 5.0).rgb * 0.4;\n"
    "    b = max(b - BTHRESH, vec3(0.0)) / (1.0 - BTHRESH);\n"
    "    c += b * b * (uBloom * BGAIN);\n"
    "  }\n"
    // Fog: a low mip is the "can't focus" blur, drifting noise is the murk.
    "  if (uFog > 0.0){\n"
    "    vec3 blur = textureLod(texture0, uv, 1.5).rgb;\n"
    "    float n = vnoise(uv * 3.0 + vec2(uTime * 0.05, uTime * 0.02)) * 0.65 "
    "+\n"
    "              vnoise(uv * 8.0 - vec2(uTime * 0.09, uTime * 0.03)) * "
    "0.35;\n"
    "    c = mix(c, blur, uFog * 0.55);\n"
    "    c = mix(c, FOGTINT, uFog * (0.30 + 0.34 * n));\n"
    "  }\n"
    "  c *= 1.0 - uVig * smoothstep(0.35, 0.85, length(uv - vec2(0.5)));\n"
    "  finalColor = vec4(c, 1.0);\n"
    "}\n";

// ---------------------------------------------------------------------------
// State. All file-static: the subsystem has no presence in the Game context
// beyond the PostFx mailbox the screens write into.
// ---------------------------------------------------------------------------
static RenderTexture2D gScene;
static Shader gShader;
static bool gTried;  // shader load attempted (once per process)
static bool gFailed; // shader or framebuffer unavailable — run without post
static int locBloom, locFog, locCrt, locVig, locTime, locRect;

void postBegin(void) {
  if (gFailed) {
    return;
  }
  if (!gTried) {
    gTried = true;
    // NULL vertex source means raylib's default vertex shader. A GLSL error
    // makes raylib hand back the *default program*, which would blit the scene
    // untouched — treat that as a failure so we fall back to the plain,
    // MSAA-backed path instead of paying for a pointless offscreen round trip.
    gShader = LoadShaderFromMemory(NULL, POST_FS);
    if (!IsShaderValid(gShader) || gShader.id == rlGetShaderIdDefault()) {
      UnloadShader(gShader); // frees the locations raylib allocates on failure
      gShader = (Shader){0};
      gFailed = true;
      return;
    }
    locBloom = GetShaderLocation(gShader, "uBloom");
    locFog = GetShaderLocation(gShader, "uFog");
    locCrt = GetShaderLocation(gShader, "uCrt");
    locVig = GetShaderLocation(gShader, "uVig");
    locTime = GetShaderLocation(gShader, "uTime");
    locRect = GetShaderLocation(gShader, "uRect");
  }

  int ss = (GetRenderWidth() <= POST_SS_MAX) ? POST_SS : 1;
  int w = GetRenderWidth() * ss, h = GetRenderHeight() * ss;
  if (gScene.texture.width != w || gScene.texture.height != h) {
    if (gScene.id != 0) {
      UnloadRenderTexture(gScene);
    }
    gScene = LoadRenderTexture(w, h);
    if (!IsRenderTextureValid(gScene)) {
      UnloadRenderTexture(gScene); // also handles a partially-created target
      gScene = (RenderTexture2D){0};
      gFailed = true;
      return;
    }
    // Give the trilinear filter levels to point at before the first resolve
    // reads them (raylib warns on a chain-less mipmap filter); postEnd keeps
    // them current from here on.
    GenTextureMipmaps(&gScene.texture);
    SetTextureFilter(gScene.texture, TEXTURE_FILTER_TRILINEAR);
  }
  BeginTextureMode(gScene);
}

float postScale(void) {
  if (gFailed || gScene.id == 0) {
    return GetScreenWidth() / (float)SCREEN_WIDTH;
  }
  return gScene.texture.width / (float)SCREEN_WIDTH;
}

void postEnd(const PostFx *fx) {
  if (gFailed || gScene.id == 0) {
    return;
  }
  EndTextureMode();

  // An empty tube rect would divide by zero in the shader.
  float crt =
      (fx->crtRect.width > 0.0f && fx->crtRect.height > 0.0f) ? fx->crt : 0.0f;
  // Every frame, not just the ones with bloom/fog: the resolve blit itself
  // minifies (the target is supersampled), so the trilinear filter reads mip 1
  // even with every effect off — a stale chain shows the frame it was baked on.
  GenTextureMipmaps(&gScene.texture);
  float t = (float)GetTime();
  float rect[4] = {fx->crtRect.x / SCREEN_WIDTH, fx->crtRect.y / SCREEN_HEIGHT,
                   fx->crtRect.width / SCREEN_WIDTH,
                   fx->crtRect.height / SCREEN_HEIGHT};
  SetShaderValue(gShader, locBloom, &fx->bloom, SHADER_UNIFORM_FLOAT);
  SetShaderValue(gShader, locFog, &fx->fog, SHADER_UNIFORM_FLOAT);
  SetShaderValue(gShader, locCrt, &crt, SHADER_UNIFORM_FLOAT);
  SetShaderValue(gShader, locVig, &fx->vignette, SHADER_UNIFORM_FLOAT);
  SetShaderValue(gShader, locTime, &t, SHADER_UNIFORM_FLOAT);
  SetShaderValue(gShader, locRect, rect, SHADER_UNIFORM_VEC4);

  BeginShaderMode(gShader);
  // Source height is negative: a framebuffer texture is bottom-up.
  DrawTexturePro(
      gScene.texture,
      (Rectangle){0, 0, (float)gScene.texture.width,
                  -(float)gScene.texture.height},
      (Rectangle){0, 0, (float)GetScreenWidth(), (float)GetScreenHeight()},
      (Vector2){0, 0}, 0.0f, WHITE);
  EndShaderMode();
}

void unloadPost(void) {
  if (gScene.id != 0) {
    UnloadRenderTexture(gScene);
  }
  if (gShader.locs != NULL) {
    UnloadShader(gShader);
  }
  gScene = (RenderTexture2D){0};
  gShader = (Shader){0};
  gTried = false;
  gFailed = false;
}

#ifdef POST_SELFTEST
// Contact strip for eyeballing the effects without playing to the neon band or
// waiting on a Fog drop, plus the asserts that catch a shader that silently
// stopped working. Needs a GL context (unlike the svg self-test). Build:
//   cc -DPOST_SELFTEST src/post.c -Iinclude -Ilib/raylib/src \
//      build/lib/raylib/raylib/libraylib.a -lm \
//      -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL
//      \ -o /tmp/posttest && /tmp/posttest /tmp/poststrip.png
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_W 400
#define CELL 200

static const PostFx TEST_CASES[] = {
    {0},                                            // baseline
    {.bloom = 1.0f},                                //
    {.fog = 1.0f},                                  //
    {.crt = 1.0f, .crtRect = {100, 140, 600, 460}}, //
    {.vignette = 1.0f},                             //
};
static const char *const TEST_NAMES[] = {"base", "bloom", "fog", "crt",
                                         "vignette"};
STATIC_ASSERT(sizeof(TEST_NAMES) / sizeof(TEST_NAMES[0]) ==
                  sizeof(TEST_CASES) / sizeof(TEST_CASES[0]),
              post_case_names_complete);

// A stand-in scene: dark field, bright neon bars and discs (bloom bait), a
// mid-grey ramp (fog/vignette bait). Drawn in 800-space like the real screens.
static void testScene(void) {
  ClearBackground((Color){14, 8, 34, 255});
  DrawRectangleGradientV(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
                         (Color){40, 16, 72, 255}, (Color){10, 6, 36, 255});
  for (int i = 0; i < 5; i++) {
    DrawRectangle(90 + i * 130, 120, 26, 380, (Color){255, 90, 240, 255});
    DrawCircle(103 + i * 130, 600, 34, (Color){255, 232, 107, 255});
  }
  DrawRectangle(0, 700, SCREEN_WIDTH, 100, (Color){120, 120, 130, 255});
  DrawText("BOING", 250, 300, 90, RAYWHITE);
}

// Render one frame with `fx` applied and read the resolved backbuffer back.
// Drawn twice and read on the second pass: the first frame after a target or
// shader is created reads back empty, and a warm-up here is cheaper than making
// every caller remember it.
static Image shot(const PostFx *fx) {
  Image im = {0};
  for (int pass = 0; pass < 2; pass++) {
    BeginDrawing();
    postBegin();
    float s = postScale();
    rlPushMatrix();
    rlScalef(s, s, 1.0f);
    testScene();
    rlPopMatrix();
    postEnd(fx);
    if (pass == 1) {
      im = LoadImageFromScreen();
    }
    EndDrawing();
  }
  return im;
}

// Mean absolute per-channel difference, 0..255. Proves an effect actually
// reached the framebuffer instead of being optimized away or mis-bound.
static double diff(Image a, Image b) {
  const unsigned char *pa = a.data, *pb = b.data;
  long n = (long)a.width * a.height * 4, sum = 0;
  for (long i = 0; i < n; i++) {
    sum += abs((int)pa[i] - (int)pb[i]);
  }
  return (double)sum / (double)n;
}

int main(int argc, char **argv) {
  SetTraceLogLevel(LOG_WARNING);
  SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_HIGHDPI);
  InitWindow(TEST_W, TEST_W, "post selftest");

  const int N = sizeof(TEST_CASES) / sizeof(TEST_CASES[0]);

  Image base = shot(&TEST_CASES[0]);
  // postBegin ran at least once, so the shader verdict is in.
  assert(!gFailed &&
         "post shader failed to compile or framebuffer unavailable");
  assert(gShader.id != rlGetShaderIdDefault() &&
         "fell back to raylib's shader");
  assert(gScene.id != 0 && "scene target not created");
  assert(gScene.texture.width == GetRenderWidth() * POST_SS &&
         "target is not supersampled");

  Image strip = GenImageColor(CELL * N, CELL, BLACK);
  for (int i = 0; i < N; i++) {
    Image im = shot(&TEST_CASES[i]);
    double d = diff(base, im);
    printf("%-9s mean delta %6.2f\n", TEST_NAMES[i], d);
    if (i > 0) {
      assert(d > 1.0 &&
             "effect changed nothing — uniform not reaching the GPU");
    }
    ImageResize(&im, CELL, CELL);
    ImageDrawImage(&strip, im, i * CELL, 0, WHITE);
    UnloadImage(im);
  }
  // Vignette must darken the corners and leave the middle alone.
  Image vig = shot(&TEST_CASES[4]);
  Color mid = GetImageColor(vig, vig.width / 2, vig.height / 2);
  Color corner = GetImageColor(vig, 2, 2);
  Color baseCorner = GetImageColor(base, 2, 2);
  Color baseMid = GetImageColor(base, base.width / 2, base.height / 2);
  printf("corner %d->%d  mid %d->%d (%dx%d)\n", baseCorner.r, corner.r,
         baseMid.r, mid.r, base.width, base.height);
  assert(corner.r < baseCorner.r && "vignette did not darken the corner");
  assert(mid.r == baseMid.r && "vignette darkened the centre");
  UnloadImage(vig);

  ExportImage(strip, argc > 1 ? argv[1] : "poststrip.png");
  UnloadImage(strip);
  UnloadImage(base);
  printf("ok — %d cases, %dpt window, %dx%d target\n", N, TEST_W,
         gScene.texture.width, gScene.texture.height);
  unloadPost();
  CloseWindow();
  return 0;
}
#endif
