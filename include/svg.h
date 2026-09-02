#ifndef SVG_H
#define SVG_H

#include "raylib.h"

// Vector icon art. Every glyph is an SVG source string baked into the binary
// (no asset files — the 1.44MB budget and the "procedural, ships original"
// rule both apply), parsed and rasterized once by nanosvg into a mipmapped
// texture, then drawn tinted.
//
// Why textures and not primitives: real SVG gives true curves, true holes
// (the old ICON_GEAR/ICON_HOME faked theirs with a translucent black disc)
// and proper anti-aliasing, which raylib's primitive draws do not have.
//
// The masters are rasterized far above their largest on-screen device size
// (see SVG_ICON_PX/SVG_LOGO_PX in svg.c) and downsampled through mipmaps, so
// they stay sharp at any window scale. Nothing ever upscales them.

typedef enum {
  // Mouse pointer. First in the enum on purpose: svgPreloadStep() bakes in
  // enum order and the cursor is drawn from the splash's very first frame, so
  // being first is what keeps it off drawSvg's lazy-bake fallback. Carries its
  // own fill + outline colors (like SVG_LOGO) so it reads on any background —
  // a white-tinted arrow would vanish on the white splash.
  SVG_CURSOR,

  // UI icons — parallel to IconKind in ui.h.
  SVG_PLAY,
  SVG_PAUSE,
  SVG_GEAR,
  SVG_RETRY,
  SVG_BACK,
  SVG_HOME,
  SVG_EXIT,
  SVG_HEART,
  SVG_SHOP,
  SVG_MISSIONS,

  // Splash wordmark. Carries its own gold gradient, so it tints WHITE for the
  // logo and near-black for the drop-shadow pass.
  SVG_LOGO,

  // Coin glyph, split into the two theme colors it needs: the rim tints
  // COIN_LO, the face COIN_HI, and the face's punched-out star lets the rim
  // read through as a mint mark.
  SVG_COIN_RIM,
  SVG_COIN_FACE,

  // Power-up glyphs. These are illustrations, not flat icons: each one is
  // several theme colors, and a tint is uniform, so a glyph is split into one
  // layer per color role and drawSvg'd once per layer. They share a 24x24
  // viewBox in which 12 units == the world's POWERUP_RADIUS, so drawing a layer
  // at box = 2*R reproduces the old primitive geometry one-for-one.
  SVG_PU_MAGNET,
  SVG_PU_MAGNET_TIPS,
  SVG_PU_BALLOON_DK,
  SVG_PU_BALLOON,
  SVG_PU_BALLOON_GLOSS,
  SVG_PU_SHIELD_HEX, // rim / plate / sheen: the same hex at three sizes
  SVG_PU_SHIELD_CREST,
  SVG_PU_ROCKET_FIN,
  SVG_PU_ROCKET_BODY,
  SVG_PU_ROCKET_SHADE,
  SVG_PU_ROCKET_WINDOW,
  SVG_PU_ANCHOR,
  SVG_PU_ANCHOR_FLUKES,
  SVG_PU_DISC, // plain disc, reused for the dizzy lens rim and lens
  SVG_PU_SPIRAL,
  SVG_PU_FOG_DK,
  SVG_PU_FOG,
  SVG_PU_FOG_CROWN,
  SVG_PU_NO, // prohibition ring + slash, over a reused coin

  // Skin accessories, worn by the blob (player.c's HAT_SVG/GLS_SVG tables map
  // the Character slots onto these). Fixed-color art like SVG_LOGO — they bake
  // their own palette and draw tinted WHITE, so a mint skin doesn't turn its
  // gold crown mint.
  SVG_ACC_CAP,
  SVG_ACC_CROWN,
  SVG_ACC_BOW,
  SVG_ACC_GLASSES,
  SVG_ACC_SHADES,

  SVG_COUNT
} SvgId;

// Draws the glyph centered at (cx,cy) inside a `w` x `h` box, rotated `rotDeg`
// about that center, multiplied by `tint`. Rasterizes on first use. A glyph
// that failed to parse or allocate draws nothing.
//
// The non-square box exists for the blob's accessories: the body deforms
// non-uniformly (rx != ry during a crouch or a fall stretch), so a hat drawn in
// a square box would visibly detach from the head.
void drawSvgEx(SvgId id, float cx, float cy, float w, float h, float rotDeg,
               Color tint);
// Square-box shorthand — what every UI icon and power-up layer uses.
void drawSvg(SvgId id, float cx, float cy, float size, float rotDeg,
             Color tint);

// Rasterizes one pending glyph per call; true once every glyph is resident.
// The splash drains this cheap queue in one update so per-frame stepping does
// not add idle wall time before the menu.
bool svgPreloadStep(void);

void unloadSvgCache(void);

#endif // SVG_H
