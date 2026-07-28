#include "svg.h"

#include "core.h"

#include "nanosvg.h"
#include "nanosvgrast.h"

#include <stdlib.h>
#include <string.h>

// Master raster sizes, picked per glyph from how large it ever draws. A glyph
// is rasterized well above its worst-case device size and downsampled through
// mipmaps, so it stays sharp; nothing is ever upscaled.
//   icons  draw at <= 40 logical px -> 40 * 2 (max window scale) * 2 (Retina)
//          = 160 device px, so 256 oversamples.
//   logo   draws at 231 logical px  -> 924 device px, so 1024 oversamples.
#define SVG_ICON_PX 256
#define SVG_LOGO_PX 1024

// nsvgParse() writes into its input, so sources are copied here first. Sized
// for the longest source below with room to spare; an over-long source is
// skipped rather than truncated (a truncated SVG parses into garbage shapes).
#define SVG_SCRATCH 4096

// Every glyph is authored here in a 24x24 viewBox, in white so the draw-time
// tint is exact. Holes are real: one path + fill-rule="evenodd" (the gear's
// bore, the house's door). Disjoint pieces are separate <path> elements rather
// than subpaths, so no winding rule can make an overlap cancel into a hole.
// Angular shapes get stroke="#fff" with a round linejoin, which rounds their
// corners without needing fillet geometry in the path data.
//
// Nothing here is traced from another icon set; the tooth/arc geometry was
// generated and the rest hand-authored for this game.
#define SVG_HEAD                                                               \
  "<svg viewBox=\"0 0 24 24\" xmlns=\"http://www.w3.org/2000/svg\">"

typedef struct {
  const char *src;
  short px; // master raster size for this glyph
} SvgDef;

static const SvgDef SVG_DEFS[SVG_COUNT] = {
    // SVG_CURSOR — mouse arrow. The tip sits at the viewBox origin (the 0.8
    // inset is exactly half the stroke width, so the *outer* tip lands on 0,0),
    // which is what lets ui.c place the hotspot by offsetting half a box.
    // Baked colors, not white: the splash is white, and a white arrow on it
    // would be invisible.
    {SVG_HEAD "<path fill=\"#fff\" stroke=\"#2a2233\" stroke-width=\"1.6\""
              " stroke-linejoin=\"round\" d=\""
              "M0.8 0.8L0.8 17.8L5.0 14.0L7.8 21.2L11.0 19.8L8.2 12.6"
              "L14.4 12.6Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_PLAY — rounded right-pointing triangle.
    {SVG_HEAD
     "<path d=\"M9.3 6.6L17.2 12L9.3 17.4Z\" fill=\"#fff\" stroke=\"#fff\""
     " stroke-width=\"2.6\" stroke-linejoin=\"round\"/></svg>",
     SVG_ICON_PX},

    // SVG_PAUSE — two rounded bars.
    {SVG_HEAD "<rect x=\"6.6\" y=\"4.8\" width=\"4.3\" height=\"14.4\" "
              "rx=\"2.15\" fill=\"#fff\"/>"
              "<rect x=\"13.1\" y=\"4.8\" width=\"4.3\" height=\"14.4\" "
              "rx=\"2.15\" fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_GEAR — 8 trapezoid teeth around a real bore (evenodd). Generated:
    // tip r=9.6, root r=6.9, bore r=3.6, tooth 0.22/0.42 of the 45 deg pitch.
    {SVG_HEAD
     "<path fill-rule=\"evenodd\" fill=\"#fff\" stroke=\"#fff\" "
     "stroke-width=\"1.1\""
     " stroke-linejoin=\"round\" d=\""
     "M9.76 5.47L10.35 2.54L13.65 2.54L14.24 5.47L15.04 5.8L17.52 4.15L19.85 "
     "6.48"
     "L18.2 8.96L18.53 9.76L21.46 10.35L21.46 13.65L18.53 14.24L18.2 15.04"
     "L19.85 17.52L17.52 19.85L15.04 18.2L14.24 18.53L13.65 21.46L10.35 21.46"
     "L9.76 18.53L8.96 18.2L6.48 19.85L4.15 17.52L5.8 15.04L5.47 14.24L2.54 "
     "13.65"
     "L2.54 10.35L5.47 9.76L5.8 8.96L4.15 6.48L6.48 4.15L8.96 5.8Z"
     "M12 8.4C13.99 8.4 15.6 10.01 15.6 12C15.6 13.99 13.99 15.6 12 15.6"
     "C10.01 15.6 8.4 13.99 8.4 12C8.4 10.01 10.01 8.4 12 8.4Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_RETRY — 300 deg band with the gap at the top, arrowhead on the open
    // end pointing across it. Two paths: they overlap, and one nonzero path
    // would cancel the overlap to a hole.
    {SVG_HEAD
     "<path fill=\"#fff\" d=\"M18.51 7.14A8.5 8.5 0 1 1 10.52 4.23L11.01 6.99"
     "A5.7 5.7 0 1 0 16.37 8.94Z\"/>"
     "<path fill=\"#fff\" stroke=\"#fff\" stroke-width=\"1\" "
     "stroke-linejoin=\"round\""
     " d=\"M15.18 9.93L14.48 4.51L19.7 6.14Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_BACK — rounded left-pointing triangle, mirroring SVG_PLAY.
    {SVG_HEAD
     "<path d=\"M14.7 6.6L6.8 12L14.7 17.4Z\" fill=\"#fff\" stroke=\"#fff\""
     " stroke-width=\"2.6\" stroke-linejoin=\"round\"/></svg>",
     SVG_ICON_PX},

    // SVG_HOME — roof + walls in one silhouette, the door punched out
    // (evenodd).
    {SVG_HEAD "<path fill-rule=\"evenodd\" fill=\"#fff\" stroke=\"#fff\" "
              "stroke-width=\"1.4\""
              " stroke-linejoin=\"round\" d=\""
              "M12 3.5L20.8 11.1L19.3 11.1L19.3 20L4.7 20L4.7 11.1L3.2 11.1Z"
              "M10.3 20L10.3 14.4L13.7 14.4L13.7 20Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_EXIT — an X: one rounded bar, mirrored by the two rotations.
    {SVG_HEAD "<rect x=\"10.05\" y=\"3.4\" width=\"3.9\" height=\"17.2\" "
              "rx=\"1.95\" fill=\"#fff\""
              " transform=\"rotate(45 12 12)\"/>"
              "<rect x=\"10.05\" y=\"3.4\" width=\"3.9\" height=\"17.2\" "
              "rx=\"1.95\" fill=\"#fff\""
              " transform=\"rotate(-45 12 12)\"/></svg>",
     SVG_ICON_PX},

    // SVG_HEART — two bezier lobes meeting in a clean point, no circle seams.
    {SVG_HEAD "<path fill=\"#fff\" d=\"M12 20.8C12 20.8 3.4 15 3.4 9.1C3.4 6.2 "
              "5.7 3.9 8.5 3.9"
              "C10.3 3.9 11.4 4.8 12 5.9C12.6 4.8 13.7 3.9 15.5 3.9C18.3 3.9 "
              "20.6 6.2 20.6 9.1"
              "C20.6 15 12 20.8 12 20.8Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_SHOP — a stack of three coins, tapering slightly for perspective.
    {SVG_HEAD
     "<ellipse cx=\"12\" cy=\"5.6\" rx=\"7.2\" ry=\"2.6\" fill=\"#fff\"/>"
     "<ellipse cx=\"12\" cy=\"12\" rx=\"8\" ry=\"2.6\" fill=\"#fff\"/>"
     "<ellipse cx=\"12\" cy=\"18.4\" rx=\"8.8\" ry=\"2.6\" "
     "fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_MISSIONS — a checklist: one ticked item beside two task bars.
    {SVG_HEAD
     "<path d=\"M2.6 12.3L6.4 16.1L12.4 7.4\" fill=\"none\" stroke=\"#fff\""
     " stroke-width=\"3\" stroke-linecap=\"round\" stroke-linejoin=\"round\"/>"
     "<rect x=\"14.4\" y=\"5.9\" width=\"7.2\" height=\"3\" rx=\"1.5\" "
     "fill=\"#fff\"/>"
     "<rect x=\"14.4\" y=\"15.1\" width=\"7.2\" height=\"3\" rx=\"1.5\" "
     "fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_LOGO — serif "H" wordmark + flourish, in a 200x231 viewBox laid out
    // to match the splash screen's original 800-space geometry.
    //
    // Each serif meets its stem through a bracket: a quadratic whose control
    // point sits where the serif's horizontal underside and the stem's vertical
    // edge would cross, making the fillet tangent-continuous at both ends. That
    // curve is the whole reason this is vector art — the seven rectangles it
    // replaces could only butt the serifs onto the stems at a hard right angle.
    //
    // The gold gradient is baked in (it is fixed, not a theme color), so this
    // draws once tinted WHITE and once tinted near-black for the drop shadow.
    {"<svg viewBox=\"0 0 200 231\" xmlns=\"http://www.w3.org/2000/svg\"><defs>"
     "<linearGradient id=\"g\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"154\""
     " gradientUnits=\"userSpaceOnUse\">"
     "<stop offset=\"0\" stop-color=\"#ffe48c\"/>"
     "<stop offset=\"1\" stop-color=\"#c69422\"/></linearGradient></defs>"
     "<path fill=\"url(#g)\" d=\""
     "M26.5 0L75.5 0L75.5 12Q65 12 65 26L65 63L135 63L135 26Q135 12 124.5 12"
     "L124.5 0L173.5 0L173.5 12Q163 12 163 26L163 128Q163 142 173.5 142"
     "L173.5 154L124.5 154L124.5 142Q135 142 135 128L135 91L65 91L65 128"
     "Q65 142 75.5 142L75.5 154L26.5 154L26.5 142Q37 142 37 128L37 26"
     "Q37 12 26.5 12Z\"/>"
     // Flourish: hairline rule with round end caps.
     "<path d=\"M0 227L200 227\" stroke=\"#c69422\" stroke-width=\"3\""
     " stroke-linecap=\"round\"/>"
     "<circle cx=\"0\" cy=\"227\" r=\"4\" fill=\"#c69422\"/>"
     "<circle cx=\"200\" cy=\"227\" r=\"4\" fill=\"#c69422\"/></svg>",
     SVG_LOGO_PX},

    // SVG_COIN_RIM — the coin's outer disc, tinted COIN_LO.
    {"<svg viewBox=\"0 0 26 26\" xmlns=\"http://www.w3.org/2000/svg\">"
     "<ellipse cx=\"13\" cy=\"13\" rx=\"11\" ry=\"13\" fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_COIN_FACE — the inner face, tinted COIN_HI, with a star punched out
    // (evenodd) so the rim beneath reads through it as a struck mint mark.
    {"<svg viewBox=\"0 0 26 26\" xmlns=\"http://www.w3.org/2000/svg\">"
     "<path fill-rule=\"evenodd\" fill=\"#fff\" d=\""
     "M13 4C17.14 4 20.5 8.03 20.5 13C20.5 17.97 17.14 22 13 22"
     "C8.86 22 5.5 17.97 5.5 13C5.5 8.03 8.86 4 13 4Z"
     "M13 8.7L14.12 11.46L17.09 11.67L14.81 13.59L15.53 16.48L13 14.9"
     "L10.47 16.48L11.19 13.59L8.91 11.67L11.88 11.46Z\"/></svg>",
     SVG_ICON_PX},

    // --- Power-up layers -----------------------------------------------------
    // 12 units == POWERUP_RADIUS, so every "R * k" from the old primitive code
    // reads as "12 * k" here. Arc-based parts (the horseshoe, anchor arms,
    // spirals, prohibition ring) are true arcs now instead of the 12-24 segment
    // DrawRing fans they replace.

    // SVG_PU_MAGNET — horseshoe: one closed outline, outer arc over the top,
    // inner arc back, legs down. No hole needed, it is a U.
    {SVG_HEAD "<path fill=\"#fff\" d=\"M0 20.4L0 12A12 12 0 0 1 24 12L24 20.4"
              "L18 20.4L18 12A6 6 0 0 0 6 12L6 20.4Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_MAGNET_TIPS — the silver pole faces on the leg ends.
    {SVG_HEAD
     "<rect x=\"0\" y=\"18.4\" width=\"6\" height=\"2\" fill=\"#fff\"/>"
     "<rect x=\"18\" y=\"18.4\" width=\"6\" height=\"2\" fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_BALLOON_DK — shaded body + knot, under the lit face.
    {SVG_HEAD
     "<ellipse cx=\"12\" cy=\"12\" rx=\"10.2\" ry=\"12\" fill=\"#fff\"/>"
     "<path fill=\"#fff\" d=\"M10 22.6L14 22.6L12 24Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_BALLOON — the lit face, offset up-left toward the light.
    {SVG_HEAD "<ellipse cx=\"10.56\" cy=\"11.4\" rx=\"7.44\" ry=\"9.36\""
              " fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_BALLOON_GLOSS — specular streak.
    {SVG_HEAD "<ellipse cx=\"8.4\" cy=\"7.8\" rx=\"1.92\" ry=\"3.12\""
              " fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_SHIELD_HEX — drawn three times at shrinking sizes for the bevel
    // rim, the plate and the top sheen, so one glyph covers all three.
    {SVG_HEAD "<path fill=\"#fff\" d=\"M24 12L18 22.39L6 22.39L0 12L6 1.61"
              "L18 1.61Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_SHIELD_CREST — the pentagon crest; drawSvg spins it.
    {SVG_HEAD "<path fill=\"#fff\" d=\"M24 12L15.71 23.41L2.29 19.05L2.29 4.95"
              "L15.71 0.59Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_ROCKET_FIN — nose + both fins, one color. The nose is pulled in
    // from the old geometry, whose tip sat above the viewBox.
    {SVG_HEAD "<path fill=\"#fff\" d=\"M12 0.24L16.99 8.88L7.01 8.88Z"
              "M9.9 20.47L3.35 19.31L7.63 14.22Z"
              "M19.98 15.53L17.71 21.78L13.43 16.69Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_ROCKET_BODY — fuselage.
    {SVG_HEAD
     "<ellipse cx=\"12\" cy=\"12\" rx=\"6\" ry=\"10.8\" fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_ROCKET_SHADE — the darker sliver down the right of the fuselage.
    {SVG_HEAD "<ellipse cx=\"13.92\" cy=\"12\" rx=\"2.4\" ry=\"9.84\" "
              "fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_ROCKET_WINDOW — porthole (its highlight stays procedural).
    {SVG_HEAD "<circle cx=\"12\" cy=\"10.8\" r=\"3.12\" fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_ANCHOR — top ring (a real annulus, evenodd), shank, stock and the
    // swept arms, all one iron-colored path.
    {SVG_HEAD
     "<path fill-rule=\"evenodd\" fill=\"#fff\" d=\""
     "M12 0.96C13.99 0.96 15.6 2.57 15.6 4.56C15.6 6.55 13.99 8.16 12 8.16"
     "C10.01 8.16 8.4 6.55 8.4 4.56C8.4 2.57 10.01 0.96 12 0.96Z"
     "M12 2.64C13.06 2.64 13.92 3.5 13.92 4.56C13.92 5.62 13.06 6.48 12 6.48"
     "C10.94 6.48 10.08 5.62 10.08 4.56C10.08 3.5 10.94 2.64 12 2.64Z"
     "M10.8 6L13.2 6L13.2 19.8L10.8 19.8Z"
     "M6.6 10.2L17.4 10.2L17.4 12.12L6.6 12.12Z"
     "M20.8 15.2A9.36 9.36 0 0 1 3.2 15.2L5.23 14.46"
     "A7.2 7.2 0 0 0 18.77 14.46Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_ANCHOR_FLUKES — the two lighter blades on the arm tips.
    {SVG_HEAD "<path fill=\"#fff\" d=\"M6.52 19.15L0.79 20.16L2.78 14.69Z"
              "M23.8 16.85L20.06 21.31L18.07 15.84Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_DISC — plain disc; the dizzy lens draws it twice (dark rim, then
    // white face at 84%).
    {SVG_HEAD "<circle cx=\"12\" cy=\"12\" r=\"12\" fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_SPIRAL — four shrinking arc bands stepped 90 deg apart. Authored
    // unrotated; drawSvg spins it, and each eye spins the opposite way.
    {SVG_HEAD
     "<path fill=\"#fff\" d=\""
     "M16.32 12A4.32 4.32 0 1 1 7.94 10.52L8.5 10.73A3.72 3.72 0 1 0 15.72 12Z"
     "M12 15.36A3.36 3.36 0 1 1 13.15 8.84L12.94 9.41A2.76 2.76 0 1 0 12 14.76Z"
     "M9.6 12A2.4 2.4 0 1 1 14.26 12.82L13.69 12.62A1.8 1.8 0 1 0 10.2 12Z"
     "M12 10.56A1.44 1.44 0 1 1 11.51 13.35L11.71 12.79A0.84 0.84 0 1 0 12 "
     "11.16Z"
     "\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_FOG_DK — the two shaded puffs at the base of the bank. Pulled in
    // slightly from the old offsets, which spilled past the viewBox.
    {SVG_HEAD "<circle cx=\"6\" cy=\"14.4\" r=\"6\" fill=\"#fff\"/>"
              "<circle cx=\"18\" cy=\"14.4\" r=\"6\" fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_FOG — the main mist mass.
    {SVG_HEAD "<circle cx=\"12\" cy=\"10.8\" r=\"8.4\" fill=\"#fff\"/>"
              "<circle cx=\"7.2\" cy=\"12\" r=\"6\" fill=\"#fff\"/>"
              "<circle cx=\"16.8\" cy=\"12\" r=\"6\" fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_FOG_CROWN — the lit puff catching the light on top.
    {SVG_HEAD "<circle cx=\"10.2\" cy=\"9\" r=\"4.8\" fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_PU_NO — prohibition ring (true annulus) plus its slash, over a coin
    // reusing SVG_COIN_RIM/SVG_COIN_FACE.
    {SVG_HEAD
     "<path fill-rule=\"evenodd\" fill=\"#fff\" d=\""
     "M12 0C18.63 0 24 5.37 24 12C24 18.63 18.63 24 12 24"
     "C5.37 24 0 18.63 0 12C0 5.37 5.37 0 12 0Z"
     "M12 2.16C17.43 2.16 21.84 6.57 21.84 12C21.84 17.43 17.43 21.84 12 21.84"
     "C6.57 21.84 2.16 17.43 2.16 12C2.16 6.57 6.57 2.16 12 2.16Z\"/>"
     "<path d=\"M4.56 4.56L19.44 19.44\" stroke=\"#fff\" "
     "stroke-width=\"1.92\"/>"
     "</svg>",
     SVG_ICON_PX},

    // Skin accessories. Unlike the power-up layers these bake their own colors
    // and draw tinted WHITE — a crown that recolored with the skin would stop
    // being a gold crown. Each is authored where it wants to sit *inside* the
    // viewBox (rasterize() preserves that placement), and player.c picks the
    // box aspect, so the art squashes with the blob instead of scaling square.

    // SVG_ACC_CAP — baseball cap: semicircle dome, brim swept right.
    {SVG_HEAD "<path fill=\"#e2574d\" d=\"M2.6 14.6A8.4 8.4 0 0 1 19.4 "
              "14.6Z\"/>"
              "<ellipse cx=\"14.6\" cy=\"15.3\" rx=\"8.7\" ry=\"2.2\" "
              "fill=\"#c0403a\"/>"
              "<circle cx=\"11\" cy=\"6.6\" r=\"1.3\" fill=\"#f5d06a\"/>"
              "</svg>",
     SVG_ICON_PX},

    // SVG_ACC_CROWN — five-point crown on a jewelled band.
    {SVG_HEAD
     "<path fill=\"#f4c64e\" stroke=\"#d29a24\" stroke-width=\"1.3\""
     " stroke-linejoin=\"round\" d=\""
     "M3.0 18.4L4.4 8.0L8.4 12.8L12 6.2L15.6 12.8L19.6 8.0L21.0 18.4Z\"/>"
     "<rect x=\"3.0\" y=\"17.0\" width=\"18\" height=\"3.4\" rx=\"1.2\" "
     "fill=\"#e0ad34\"/>"
     "<circle cx=\"7.5\" cy=\"18.7\" r=\"1.05\" fill=\"#e0566e\"/>"
     "<circle cx=\"12\" cy=\"18.7\" r=\"1.05\" fill=\"#66c7e8\"/>"
     "<circle cx=\"16.5\" cy=\"18.7\" r=\"1.05\" fill=\"#8be08a\"/>"
     "</svg>",
     SVG_ICON_PX},

    // SVG_ACC_BOW — two loops and a knot. Disjoint pieces, separate paths.
    {SVG_HEAD "<path fill=\"#ff6fa5\" stroke=\"#d94a83\" stroke-width=\"1.3\""
              " stroke-linejoin=\"round\" d=\"M11.0 12L2.6 7.2L2.6 16.8Z\"/>"
              "<path fill=\"#ff6fa5\" stroke=\"#d94a83\" stroke-width=\"1.3\""
              " stroke-linejoin=\"round\" d=\"M13.0 12L21.4 7.2L21.4 16.8Z\"/>"
              "<circle cx=\"12\" cy=\"12\" r=\"2.6\" fill=\"#e85a95\" "
              "stroke=\"#d94a83\" stroke-width=\"1.1\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_GLASSES — round wire frames. Lens centers sit at x = 12 +/- 5.8,
    // which is what pins them over the eyes once player.c picks the box width.
    {SVG_HEAD
     "<circle cx=\"6.2\" cy=\"12\" r=\"4.3\" fill=\"none\" "
     "stroke=\"#332d3f\" stroke-width=\"1.5\"/>"
     "<circle cx=\"17.8\" cy=\"12\" r=\"4.3\" fill=\"none\" "
     "stroke=\"#332d3f\" stroke-width=\"1.5\"/>"
     "<path d=\"M10.5 11.4Q12 10.4 13.5 11.4\" fill=\"none\" "
     "stroke=\"#332d3f\" stroke-width=\"1.5\"/>"
     "<path d=\"M1.9 11.0L0.6 9.8\" stroke=\"#332d3f\" stroke-width=\"1.5\"/>"
     "<path d=\"M22.1 11.0L23.4 9.8\" stroke=\"#332d3f\" "
     "stroke-width=\"1.5\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_SHADES — dark lenses, same eye spacing as SVG_ACC_GLASSES.
    {SVG_HEAD
     "<rect x=\"1.7\" y=\"8.5\" width=\"8.5\" height=\"6.6\" rx=\"2.4\" "
     "fill=\"#23252e\"/>"
     "<rect x=\"13.8\" y=\"8.5\" width=\"8.5\" height=\"6.6\" rx=\"2.4\" "
     "fill=\"#23252e\"/>"
     "<rect x=\"10.2\" y=\"9.7\" width=\"3.6\" height=\"1.7\" rx=\"0.8\" "
     "fill=\"#4b4f5e\"/>"
     "<path fill=\"#5a6070\" d=\"M3.4 13.6L6.4 9.9L8.0 9.9L5.0 13.6Z\"/>"
     "</svg>",
     SVG_ICON_PX},
};
STATIC_ASSERT(sizeof(SVG_DEFS) / sizeof(SVG_DEFS[0]) == SVG_COUNT,
              svg_defs_table_complete);

// Shared rasterizer, created on first bake. It holds scanline/edge buffers that
// grow to the largest raster it has seen (1024px for the logo), and it is only
// ever needed while baking — so it is released as soon as the preload queue
// drains, and again on shutdown. A later lazy bake just recreates it.
static NSVGrasterizer *gRast = NULL;

static void releaseRasterizer(void) {
  if (gRast != NULL) {
    nsvgDeleteRasterizer(gRast);
    gRast = NULL;
  }
}

// Rasterizes one glyph into a freshly malloc'd px*px RGBA buffer (caller
// frees). Fits the SVG's larger side to `px` and centers it, so the box is
// square whatever the viewBox aspect is. NULL on any failure.
static unsigned char *rasterize(SvgId id, int px) {
  char scratch[SVG_SCRATCH];

  if (strlen(SVG_DEFS[id].src) >= sizeof(scratch)) {
    return NULL; // would truncate, and a half SVG parses into garbage
  }
  strcpy(scratch, SVG_DEFS[id].src); // nsvgParse writes into its input

  if (gRast == NULL && (gRast = nsvgCreateRasterizer()) == NULL) {
    return NULL;
  }
  NSVGimage *img = nsvgParse(scratch, "px", 96.0f);
  if (img == NULL) {
    return NULL;
  }

  unsigned char *buf = calloc((size_t)px * px, 4);
  if (buf != NULL) {
    float big = (img->width > img->height) ? img->width : img->height;
    float scale = (big > 0.0f) ? px / big : 1.0f;
    nsvgRasterize(gRast, img, (px - img->width * scale) * 0.5f,
                  (px - img->height * scale) * 0.5f, scale, buf, px, px,
                  px * 4);
  }
  nsvgDelete(img);
  return buf;
}

static Texture2D gTex[SVG_COUNT];
static bool gBaked[SVG_COUNT];

// Bakes one glyph into a mipmapped texture. Marked baked either way, so a
// failure is not retried every frame — it just draws nothing (same
// degrade-quietly contract as a music track whose buffer failed to allocate).
static void bake(SvgId id) {
  int px = SVG_DEFS[id].px;
  gBaked[id] = true;
  unsigned char *buf = rasterize(id, px);
  if (buf == NULL) {
    return;
  }
  Image im = {.data = buf,
              .width = px,
              .height = px,
              .mipmaps = 1,
              .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
  gTex[id] = LoadTextureFromImage(im);
  free(buf);
  if (gTex[id].id != 0) {
    GenTextureMipmaps(&gTex[id]);
    SetTextureFilter(gTex[id], TEXTURE_FILTER_TRILINEAR);
  }
}

bool svgPreloadStep(void) {
  for (int i = 0; i < SVG_COUNT; i++) {
    if (!gBaked[i]) {
      bake((SvgId)i);
      return false;
    }
  }
  releaseRasterizer(); // queue drained; nothing else needs it
  return true;
}

void drawSvgEx(SvgId id, float cx, float cy, float w, float h, float rotDeg,
               Color tint) {
  if (!gBaked[id]) {
    bake(id); // lazy fallback: reached only if something drew before the splash
  }
  if (gTex[id].id == 0) {
    return;
  }
  DrawTexturePro(
      gTex[id],
      (Rectangle){0, 0, (float)gTex[id].width, (float)gTex[id].height},
      (Rectangle){cx, cy, w, h}, (Vector2){w * 0.5f, h * 0.5f}, rotDeg, tint);
}

void drawSvg(SvgId id, float cx, float cy, float size, float rotDeg,
             Color tint) {
  drawSvgEx(id, cx, cy, size, size, rotDeg, tint);
}

void unloadSvgCache(void) {
  for (int i = 0; i < SVG_COUNT; i++) {
    if (gTex[i].id != 0) {
      UnloadTexture(gTex[i]);
    }
    gTex[i] = (Texture2D){0};
    gBaked[i] = false;
  }
  releaseRasterizer();
}

#ifdef SVG_SELFTEST
// Contact sheet for eyeballing the glyphs without launching the game. Build:
//   cc -DSVG_SELFTEST src/svg.c lib/nanosvg_impl.c -Iinclude -Ilib/nanosvg/src
//   \
//      -Ilib/raylib/src build/lib/raylib/raylib/libraylib.a -lm \
//      -framework Cocoa -framework IOKit -framework CoreVideo -framework OpenGL
//      \ -o /tmp/svgtest && /tmp/svgtest /tmp/svgsheet.png
// Runs headless (no InitWindow): rasterize() and ExportImage are both CPU-only.
#include <assert.h>
#include <stdio.h>

#define CELL 128
#define COLS 5

int main(int argc, char **argv) {
  int rows = (SVG_COUNT + COLS - 1) / COLS;
  Image sheet =
      GenImageColor(CELL * COLS, CELL * rows, (Color){40, 40, 48, 255});
  size_t longest = 0;

  for (int i = 0; i < SVG_COUNT; i++) {
    // rasterize() silently skips a source that would overflow the scratch
    // buffer, so prove every one fits rather than finding out via a blank
    // glyph.
    size_t len = strlen(SVG_DEFS[i].src);
    assert(len < SVG_SCRATCH && "SVG source outgrew SVG_SCRATCH");
    longest = (len > longest) ? len : longest;

    unsigned char *buf = rasterize((SvgId)i, CELL);
    assert(buf != NULL && "glyph failed to rasterize");

    // Opaque pixels prove the path data actually produced geometry.
    long lit = 0;
    for (int p = 3; p < CELL * CELL * 4; p += 4) {
      lit += (buf[p] > 128);
    }
    printf("%2d: %6ld lit px  (%zu B src)\n", i, lit, len);
    assert(lit > 200 && "glyph rasterized (nearly) empty — bad path data");

    Image g = {buf, CELL, CELL, 1, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8};
    ImageDrawImage(&sheet, g, (i % COLS) * CELL, (i / COLS) * CELL, WHITE);
    free(buf);
  }
  ExportImage(sheet, argc > 1 ? argv[1] : "svgsheet.png");
  releaseRasterizer();
  printf("ok — %d glyphs, longest source %zu B of %d\n", SVG_COUNT, longest,
         SVG_SCRATCH);
  return 0;
}
#endif
