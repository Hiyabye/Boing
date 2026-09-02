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

// Deliberately unsized. Writing `SVG_DEFS[SVG_COUNT]` makes the completeness
// assert below tautological — a `[N]`-sized array is N long whatever you put in
// it — so letting the initializer set the length is what gives the assert
// something real to catch. Same reasoning as TEX_DEFS in tex.c.
//
// These rows are *positional*, not designated, so this is the strongest form of
// the guard in the codebase: dropping a row anywhere shortens the array and
// fails the build. (Verified by deleting a middle row and watching
// static_assert_svg_defs_table_complete fire.) The cost of positional rows is
// that they must stay in exact SvgId order — the comment on each row is the
// only thing naming which glyph it is.
static const SvgDef SVG_DEFS[] = {
    // SVG_CURSOR: candy-comet mouse arrow. The 0.8 inset is half the outline,
    // so the outer tip lands exactly on the viewBox origin and remains the
    // true hotspot. Fixed pink-gold color keeps it readable on every theme.
    {SVG_HEAD
     "<defs><linearGradient id=\"c\" x1=\"1\" y1=\"1\" x2=\"11\" y2=\"20\""
     " gradientUnits=\"userSpaceOnUse\">"
     "<stop offset=\"0\" stop-color=\"#ffdf72\"/>"
     "<stop offset=\"0.34\" stop-color=\"#ff70b5\"/>"
     "<stop offset=\"1\" stop-color=\"#e52f88\"/></linearGradient></defs>"
     "<path fill=\"url(#c)\" stroke=\"#291d38\" stroke-width=\"1.6\""
     " stroke-linejoin=\"round\" d=\""
     "M0.8 0.8L0.8 17.8L5.0 14.0L7.8 21.2L11.0 19.8L8.2 12.6"
     "L14.4 12.6Z\"/>"
     "<path fill=\"#fff4bd\" opacity=\"0.9\" d=\"M2.2 2.4L2.2 11.7"
     "L4.2 9.9Z\"/><circle cx=\"3.2\" cy=\"3.1\" r=\"0.9\""
     " fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_PLAY: plump candy triangle with a carved gleam and sugar sparkles.
    {SVG_HEAD "<path fill-rule=\"evenodd\" fill=\"#fff\" stroke=\"#fff\""
              " stroke-width=\"1.6\" stroke-linejoin=\"round\" d=\""
              "M6.8 4.4L18.7 12L6.8 19.6Z"
              "M9.1 7.4C10.4 8 11.5 8.8 12.6 9.6C11.1 9.1 10.2 8.9 9.1 8.8Z\"/>"
              "<circle cx=\"19.5\" cy=\"5.1\" r=\"1.15\" fill=\"#fff\"/>"
              "<circle cx=\"21.4\" cy=\"7.6\" r=\"0.55\" fill=\"#fff\""
              " opacity=\"0.7\"/></svg>",
     SVG_ICON_PX},

    // SVG_PAUSE: two candy sticks with tiny pressed-in bubbles.
    {SVG_HEAD
     "<path fill-rule=\"evenodd\" fill=\"#fff\" d=\""
     "M8.2 3.7C6.45 3.7 5.6 4.8 5.6 6.3V17.7C5.6 19.2 6.45 20.3 8.2 "
     "20.3C9.95 20.3 10.8 19.2 10.8 17.7V6.3C10.8 4.8 9.95 3.7 8.2 3.7Z"
     "M8.2 6.2A0.75 0.75 0 1 0 8.2 7.7A0.75 0.75 0 1 0 8.2 6.2Z"
     "M15.8 3.7C14.05 3.7 13.2 4.8 13.2 6.3V17.7C13.2 19.2 14.05 20.3 "
     "15.8 20.3C17.55 20.3 18.4 19.2 18.4 17.7V6.3C18.4 4.8 17.55 3.7 "
     "15.8 3.7Z"
     "M15.8 6.2A0.75 0.75 0 1 0 15.8 7.7A0.75 0.75 0 1 0 15.8 6.2Z\"/>"
     "<circle cx=\"20.4\" cy=\"18.2\" r=\"0.8\" fill=\"#fff\""
     " opacity=\"0.65\"/></svg>",
     SVG_ICON_PX},

    // SVG_GEAR: eight rounded teeth, a true bore, and a candy hub.
    {SVG_HEAD
     "<path fill-rule=\"evenodd\" fill=\"#fff\" stroke=\"#fff\" "
     "stroke-width=\"1.1\" stroke-linejoin=\"round\" d=\""
     "M9.76 5.47L10.35 2.54L13.65 2.54L14.24 5.47L15.04 5.8L17.52 4.15L19.85 "
     "6.48L18.2 8.96L18.53 9.76L21.46 10.35L21.46 13.65L18.53 14.24L18.2 "
     "15.04L19.85 17.52L17.52 19.85L15.04 18.2L14.24 18.53L13.65 21.46L10.35 "
     "21.46L9.76 18.53L8.96 18.2L6.48 19.85L4.15 17.52L5.8 15.04L5.47 "
     "14.24L2.54 13.65L2.54 10.35L5.47 9.76L5.8 8.96L4.15 6.48L6.48 "
     "4.15L8.96 5.8Z"
     "M12 8.2A3.8 3.8 0 1 0 12 15.8A3.8 3.8 0 1 0 12 8.2Z\"/>"
     "<circle cx=\"12\" cy=\"12\" r=\"1.15\" fill=\"#fff\"/>"
     "<circle cx=\"20.6\" cy=\"4.2\" r=\"0.75\" fill=\"#fff\""
     " opacity=\"0.7\"/></svg>",
     SVG_ICON_PX},

    // SVG_RETRY: broad rewind swoop with a center sugar star.
    {SVG_HEAD
     "<path fill=\"#fff\" d=\"M19.5 7.4A9 9 0 1 1 10.4 3.2L10.9 6.25A5.9 "
     "5.9 0 1 0 16.95 9Z\"/>"
     "<path fill=\"#fff\" stroke=\"#fff\" stroke-width=\"0.8\""
     " stroke-linejoin=\"round\" d=\"M14.25 10.15L14.25 3.55L20.35 5.85Z\"/>"
     "<path fill=\"#fff\" d=\"M11.8 8.8L12.65 10.85L14.8 11.1L13.15 "
     "12.55L13.65 14.65L11.8 13.55L9.95 14.65L10.45 12.55L8.8 11.1L10.95 "
     "10.85Z\" opacity=\"0.75\"/></svg>",
     SVG_ICON_PX},

    // SVG_BACK: chunky arrow with a long soft tail and motion drops.
    {SVG_HEAD
     "<path fill=\"#fff\" stroke=\"#fff\" stroke-width=\"1.2\""
     " stroke-linejoin=\"round\" d=\"M10.2 4.6L2.8 12L10.2 19.4L10.2 "
     "15.1H20.2C21.2 15.1 21.7 14.35 21.7 13.45V10.55C21.7 9.65 21.2 8.9 "
     "20.2 8.9H10.2Z\"/>"
     "<circle cx=\"18.8\" cy=\"5.2\" r=\"1\" fill=\"#fff\""
     " opacity=\"0.7\"/><circle cx=\"21.5\" cy=\"6.3\" r=\"0.5\""
     " fill=\"#fff\" opacity=\"0.55\"/></svg>",
     SVG_ICON_PX},

    // SVG_HOME: candy cottage with chimney, round window, and open door.
    {SVG_HEAD
     "<path fill-rule=\"evenodd\" fill=\"#fff\" stroke=\"#fff\""
     " stroke-width=\"1.1\" stroke-linejoin=\"round\" d=\""
     "M2.4 10.9L12 2.9L21.6 10.9L19.7 10.9L19.7 20.6L4.3 20.6L4.3 "
     "10.9Z M9.9 20.6V15.1C9.9 14.35 10.45 13.8 11.2 13.8H12.8C13.55 "
     "13.8 14.1 14.35 14.1 15.1V20.6Z M7.1 12.2A1.65 1.65 0 1 0 7.1 "
     "15.5A1.65 1.65 0 1 0 7.1 12.2Z\"/>"
     "<path fill=\"#fff\" d=\"M16.4 3.5H19.1V7.4L16.4 5.15Z\"/>"
     "<circle cx=\"12.9\" cy=\"17.1\" r=\"0.45\" fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_EXIT: open candy door with an arrow stepping out.
    {SVG_HEAD "<path fill-rule=\"evenodd\" fill=\"#fff\" d=\"M3.2 "
              "3.1H14.1V8H11.2V6H6.1V18H11.2V16H14.1V20.9H3.2Z "
              "M5.9 5.8H11.4V18.2H5.9Z\"/>"
              "<path fill=\"#fff\" stroke=\"#fff\" stroke-width=\"1.1\""
              " stroke-linejoin=\"round\" d=\"M12.2 8.4H17V5.7L22.2 12L17 "
              "18.3V15.6H12.2Z\"/>"
              "<circle cx=\"8.2\" cy=\"12\" r=\"0.75\" fill=\"#fff\"/></svg>",
     SVG_ICON_PX},

    // SVG_HEART: plump candy heart with a cut gleam and orbiting sprinkles.
    {SVG_HEAD
     "<path fill-rule=\"evenodd\" fill=\"#fff\" d=\"M12 21C12 21 3.1 15.3 3.1 "
     "9.1C3.1 5.9 5.55 3.5 8.5 3.5C10.25 3.5 11.4 4.35 12 5.55C12.6 "
     "4.35 13.75 3.5 15.5 3.5C18.45 3.5 20.9 5.9 20.9 9.1C20.9 15.3 "
     "12 21 12 21Z M7.1 6.25C6.15 6.85 5.7 7.75 5.8 8.75C6.3 7.9 6.95 7.4 7.95 "
     "7.1Z\"/>"
     "<circle cx=\"19.9\" cy=\"3.4\" r=\"1\" fill=\"#fff\"/>"
     "<circle cx=\"22\" cy=\"5.9\" r=\"0.48\" fill=\"#fff\""
     " opacity=\"0.65\"/><circle cx=\"2.2\" cy=\"16.9\" r=\"0.65\""
     " fill=\"#fff\" opacity=\"0.65\"/></svg>",
     SVG_ICON_PX},

    // SVG_SHOP: striped candy stall with windows and a coin sign.
    {SVG_HEAD
     "<path fill-rule=\"evenodd\" fill=\"#fff\" d=\"M3.2 9.7H20.8V21H3.2Z "
     "M5.8 12.5H10.3V16.8H5.8Z M14.2 12.5H18.4V21H14.2Z\"/>"
     "<path fill=\"#fff\" stroke=\"#fff\" stroke-width=\"0.9\""
     " stroke-linejoin=\"round\" d=\"M4 3H20L22 8.1C22 9.8 20.65 10.8 19.15 "
     "10.8C17.9 10.8 17.15 10.15 16.6 9.2C16.05 10.15 15.25 10.8 14 10.8C12.75 "
     "10.8 12 10.15 11.45 9.2C10.9 10.15 10.1 10.8 8.85 10.8C7.6 10.8 6.85 "
     "10.15 6.3 9.2C5.75 10.15 5 10.8 3.75 10.8C2.25 10.8 1.1 9.8 2 8.1Z\"/>"
     "<circle cx=\"12\" cy=\"4.9\" r=\"1.15\" fill=\"none\""
     " stroke=\"#fff\" stroke-width=\"0.8\"/></svg>",
     SVG_ICON_PX},

    // SVG_MISSIONS: candy clipboard with two checks and a star seal.
    {SVG_HEAD
     "<rect x=\"3.1\" y=\"3.6\" width=\"17.8\" height=\"18.1\" rx=\"3.2\""
     " fill=\"none\" stroke=\"#fff\" stroke-width=\"2.2\"/>"
     "<rect x=\"8.2\" y=\"1.8\" width=\"7.6\" height=\"4.2\" rx=\"2.1\" "
     "fill=\"#fff\"/>"
     "<path d=\"M5.8 10.4L7.3 11.9L9.8 8.7M11.6 10.3H17.9M5.8 "
     "16L7.3 17.5L9.8 14.3M11.6 15.9H15.3\" fill=\"none\" stroke=\"#fff\""
     " stroke-width=\"1.7\" stroke-linecap=\"round\" "
     "stroke-linejoin=\"round\"/>"
     "<path fill=\"#fff\" d=\"M18.1 14.4L18.8 16L20.5 16.15L19.2 "
     "17.3L19.6 19L18.1 18.1L16.6 19L17 17.3L15.7 16.15L17.4 16Z\"/></svg>",
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

    // Outfit accessories. Unlike power-up layers these bake their own colors
    // and draw tinted WHITE, so a gold crown stays gold on every body color.
    // Each is authored where it wants to sit *inside* the
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

    // SVG_ACC_BOW: two loops and a knot.
    {SVG_HEAD "<path fill=\"#ff6fa5\" stroke=\"#d94a83\" stroke-width=\"1.3\""
              " stroke-linejoin=\"round\" d=\"M11 12L2.6 7.2L2.6 16.8Z\"/>"
              "<path fill=\"#ff6fa5\" stroke=\"#d94a83\" stroke-width=\"1.3\""
              " stroke-linejoin=\"round\" d=\"M13 12L21.4 7.2L21.4 16.8Z\"/>"
              "<circle cx=\"12\" cy=\"12\" r=\"2.6\" fill=\"#e85a95\" "
              "stroke=\"#d94a83\" stroke-width=\"1.1\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_PARTY: striped cone, pom-pom, and soft headband.
    {SVG_HEAD
     "<path fill=\"#6f68df\" stroke=\"#433b9e\" stroke-width=\"1.2\" "
     "stroke-linejoin=\"round\" d=\"M4 18.5L12 3L20 18.5Z\"/>"
     "<path fill=\"#ff73ab\" d=\"M7 12.7L17.5 13.7L19.2 17L5.3 15.9Z\"/>"
     "<path fill=\"#ffd85e\" d=\"M9.2 8.4L14.8 8.9L16.2 11.5L7.8 10.8Z\"/>"
     "<ellipse cx=\"12\" cy=\"19\" rx=\"9.5\" ry=\"1.8\" fill=\"#433b9e\"/>"
     "<circle cx=\"12\" cy=\"3\" r=\"2.2\" fill=\"#ffd85e\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_WIZARD: floppy midnight cone with original candy-star details.
    {SVG_HEAD
     "<path fill=\"#40377f\" stroke=\"#26204f\" stroke-width=\"1.2\" "
     "stroke-linejoin=\"round\" d=\"M3 18.5L9.5 5L15 2.5L14.2 8L21 18.5Z\"/>"
     "<ellipse cx=\"12\" cy=\"18.5\" rx=\"10\" ry=\"2.4\" fill=\"#292253\"/>"
     "<path fill=\"#ffd85e\" d=\"M10.7 9L11.5 10.7L13.4 10.9L12 12.2L12.4 "
     "14L10.7 13.1L9 14L9.4 12.2L8 10.9L9.9 10.7Z\"/>"
     "<circle cx=\"15.8\" cy=\"14.2\" r=\"1\" fill=\"#ff75ae\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_FLOWER: six candy petals on a small green comb.
    {SVG_HEAD "<path d=\"M8 19Q12 15 16 19\" fill=\"none\" stroke=\"#4c9b68\" "
              "stroke-width=\"2.2\" stroke-linecap=\"round\"/>"
              "<g fill=\"#ff77ad\" stroke=\"#d94a83\" stroke-width=\"0.8\">"
              "<circle cx=\"12\" cy=\"6.4\" r=\"3.2\"/><circle cx=\"12\" "
              "cy=\"17.6\" r=\"3.2\"/>"
              "<circle cx=\"7.2\" cy=\"9.2\" r=\"3.2\"/><circle cx=\"16.8\" "
              "cy=\"9.2\" r=\"3.2\"/>"
              "<circle cx=\"7.2\" cy=\"14.8\" r=\"3.2\"/><circle cx=\"16.8\" "
              "cy=\"14.8\" r=\"3.2\"/></g>"
              "<circle cx=\"12\" cy=\"12\" r=\"3.4\" fill=\"#ffd85e\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_HALO: floating gold ring with two tiny candy stars.
    {SVG_HEAD "<ellipse cx=\"12\" cy=\"12\" rx=\"9.5\" ry=\"3.6\" "
              "fill=\"none\" stroke=\"#f4c64e\" stroke-width=\"2.3\"/>"
              "<path fill=\"#fff4a8\" d=\"M5 5L5.8 6.6L7.6 6.8L6.3 8L6.6 9.8L5 "
              "9L3.4 9.8L3.7 8L2.4 6.8L4.2 6.6ZM19 4L19.6 5.2L21 5.4L20 "
              "6.3L20.2 7.7L19 7.1L17.8 7.7L18 6.3L17 5.4L18.4 5.2Z\"/>"
              "<path d=\"M5 11.5Q12 15 19 11.5\" fill=\"none\" "
              "stroke=\"#c78e25\" stroke-width=\".8\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_GLASSES: round wire frames.
    {SVG_HEAD "<circle cx=\"6.2\" cy=\"12\" r=\"4.3\" fill=\"none\" "
              "stroke=\"#332d3f\" stroke-width=\"1.5\"/>"
              "<circle cx=\"17.8\" cy=\"12\" r=\"4.3\" fill=\"none\" "
              "stroke=\"#332d3f\" stroke-width=\"1.5\"/>"
              "<path d=\"M10.5 11.4Q12 10.4 13.5 11.4\" fill=\"none\" "
              "stroke=\"#332d3f\" stroke-width=\"1.5\"/>"
              "<path d=\"M1.9 11L.6 9.8M22.1 11L23.4 9.8\" stroke=\"#332d3f\" "
              "stroke-width=\"1.5\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_SHADES: dark lenses, same eye spacing as the round frames.
    {SVG_HEAD
     "<rect x=\"1.7\" y=\"8.5\" width=\"8.5\" height=\"6.6\" rx=\"2.4\" "
     "fill=\"#23252e\"/>"
     "<rect x=\"13.8\" y=\"8.5\" width=\"8.5\" height=\"6.6\" rx=\"2.4\" "
     "fill=\"#23252e\"/>"
     "<rect x=\"10.2\" y=\"9.7\" width=\"3.6\" height=\"1.7\" rx=\".8\" "
     "fill=\"#4b4f5e\"/>"
     "<path fill=\"#5a6070\" d=\"M3.4 13.6L6.4 9.9H8L5 13.6Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_HEARTS: rose heart lenses with a gold bridge.
    {SVG_HEAD "<path fill=\"#ee4f8b\" stroke=\"#8d2e61\" stroke-width=\"1\" "
              "d=\"M6.2 17C2 13.8 1.2 10.6 2.8 8.8C4 7.4 5.5 7.8 6.2 9C7 7.8 "
              "8.5 7.4 9.7 8.8C11.3 10.6 10.4 13.8 6.2 17Z\"/>"
              "<path fill=\"#ee4f8b\" stroke=\"#8d2e61\" stroke-width=\"1\" "
              "d=\"M17.8 17C13.6 13.8 12.7 10.6 14.3 8.8C15.5 7.4 17 7.8 17.8 "
              "9C18.5 7.8 20 7.4 21.2 8.8C22.8 10.6 22 13.8 17.8 17Z\"/>"
              "<path d=\"M10.1 10.5Q12 9.5 13.9 10.5\" fill=\"none\" "
              "stroke=\"#d6a438\" stroke-width=\"1.4\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_STARS: five-point gold lenses on plum arms.
    {SVG_HEAD "<path fill=\"#ffd85e\" stroke=\"#8c6621\" stroke-width=\"1\" "
              "d=\"M6.2 6.3L7.8 9.5L11.3 10L8.7 12.5L9.3 16L6.2 14.4L3.1 "
              "16L3.7 12.5L1.1 10L4.6 9.5Z\"/>"
              "<path fill=\"#ffd85e\" stroke=\"#8c6621\" stroke-width=\"1\" "
              "d=\"M17.8 6.3L19.4 9.5L22.9 10L20.3 12.5L20.9 16L17.8 14.4L14.7 "
              "16L15.3 12.5L12.7 10L16.2 9.5Z\"/>"
              "<path d=\"M10 10.4Q12 9.5 14 10.4\" fill=\"none\" "
              "stroke=\"#4c315e\" stroke-width=\"1.5\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_MONOCLE: one sapphire lens with a dangling bead chain.
    {SVG_HEAD
     "<circle cx=\"17.4\" cy=\"11.4\" r=\"5\" fill=\"#87d6e8\" "
     "fill-opacity=\".35\" stroke=\"#d8b34c\" stroke-width=\"1.8\"/>"
     "<path d=\"M21 15Q22.5 18 19.5 22\" fill=\"none\" stroke=\"#d8b34c\" "
     "stroke-width=\"1.2\" stroke-dasharray=\"1.2 1\"/>"
     "<circle cx=\"19.3\" cy=\"22\" r=\"1.1\" fill=\"#ee5c99\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_GOGGLES: chunky mint rims and translucent blue lenses.
    {SVG_HEAD "<path d=\"M1 10L4 8M23 10L20 8M10.5 11Q12 10 13.5 11\" "
              "fill=\"none\" stroke=\"#315b61\" stroke-width=\"2\"/>"
              "<circle cx=\"6.4\" cy=\"12\" r=\"4.7\" fill=\"#77d8db\" "
              "stroke=\"#315b61\" stroke-width=\"1.8\"/>"
              "<circle cx=\"17.6\" cy=\"12\" r=\"4.7\" fill=\"#77d8db\" "
              "stroke=\"#315b61\" stroke-width=\"1.8\"/>"
              "<path d=\"M4.3 10L6.8 8.7M15.5 10L18 8.7\" stroke=\"#dfffff\" "
              "stroke-width=\"1.2\" stroke-linecap=\"round\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_VISOR: one wide neon lens with a bright diagonal glint.
    {SVG_HEAD
     "<path fill=\"#43d7df\" fill-opacity=\".8\" stroke=\"#263f69\" "
     "stroke-width=\"1.5\" stroke-linejoin=\"round\" d=\"M1.5 8.5Q12 6 22.5 "
     "8.5L20.5 15.5Q12 18 3.5 15.5Z\"/>"
     "<path fill=\"#ec5fa5\" d=\"M12.5 7.4L16 7.5L11.5 16.8L8 16.5Z\"/>"
     "<path d=\"M3 10Q12 8 21 10\" fill=\"none\" stroke=\"#d9ffff\" "
     "stroke-width=\"1\" stroke-linecap=\"round\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_MUSTACHE: two curled cocoa wings.
    {SVG_HEAD
     "<path fill=\"#4a2b27\" d=\"M12 10.8C9.2 7.8 6.8 9 6.2 11.2C4.4 10.5 2.5 "
     "11.5 1.5 14.3C5.2 16 9.2 15.2 12 12.5C14.8 15.2 18.8 16 22.5 14.3C21.5 "
     "11.5 19.6 10.5 17.8 11.2C17.2 9 14.8 7.8 12 10.8Z\"/>"
     "<path d=\"M3.2 13.6Q6 15 9.2 12.8M20.8 13.6Q18 15 14.8 12.8\" "
     "fill=\"none\" stroke=\"#7a4b3d\" stroke-width=\".8\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_PACIFIER: candy shield, ring, and white center button.
    {SVG_HEAD "<path fill=\"#63c7d8\" stroke=\"#317889\" stroke-width=\"1.2\" "
              "d=\"M5 9Q12 5 19 9V14Q12 19 5 14Z\"/>"
              "<circle cx=\"12\" cy=\"11.5\" r=\"3.2\" fill=\"#fff4d0\" "
              "stroke=\"#d9aa49\" stroke-width=\"1\"/>"
              "<path fill=\"none\" stroke=\"#ee5c99\" stroke-width=\"2\" "
              "d=\"M9 15A4.5 4.5 0 1 0 15 15\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_MASK: pleated lavender cloth with visible ear loops.
    {SVG_HEAD
     "<path d=\"M4 10C1 8 1 16 4 15M20 10C23 8 23 16 20 15\" fill=\"none\" "
     "stroke=\"#ede7ff\" stroke-width=\"1.4\"/>"
     "<path fill=\"#8f87d9\" stroke=\"#514a91\" stroke-width=\"1.2\" "
     "stroke-linejoin=\"round\" d=\"M4 8.5Q12 6.5 20 8.5L19 16Q12 20 5 16Z\"/>"
     "<path d=\"M7 11H17M7.5 13.5H16.5\" stroke=\"#c9c4f4\" stroke-width=\"1\" "
     "stroke-linecap=\"round\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_LOLLIPOP: spiral candy on a diagonal white stick.
    {SVG_HEAD
     "<path d=\"M8 20L16 10\" stroke=\"#fff4d0\" stroke-width=\"2\" "
     "stroke-linecap=\"round\"/>"
     "<circle cx=\"17\" cy=\"8.5\" r=\"5.2\" fill=\"#ff6ba8\" "
     "stroke=\"#b93673\" stroke-width=\"1.2\"/>"
     "<path d=\"M17 5.2C20 5.2 20.8 9.2 18 10.5C15.5 11.7 13.8 8.6 15.5 "
     "7.2C16.7 6.2 18.2 7.4 17.5 8.4\" fill=\"none\" stroke=\"#fff3ae\" "
     "stroke-width=\"1.1\" stroke-linecap=\"round\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_PARTY_HORN: striped paper horn with a round mouthpiece.
    {SVG_HEAD "<ellipse cx=\"5\" cy=\"12\" rx=\"3\" ry=\"2.4\" "
              "fill=\"#ffd85e\" stroke=\"#a87822\" stroke-width=\"1\"/>"
              "<path fill=\"#ff6fa5\" stroke=\"#9f3769\" stroke-width=\"1.1\" "
              "stroke-linejoin=\"round\" d=\"M7 9.5L22 5L22 19L7 14.5Z\"/>"
              "<path fill=\"#70d3d0\" d=\"M11 8.3L14.5 7.3L14.5 16.7L11 "
              "15.7ZM18 6.2L20.5 5.5V18.5L18 17.8Z\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_FANGS: a velvet grin with two bright candy fangs.
    {SVG_HEAD
     "<path fill=\"#3c233c\" stroke=\"#251529\" stroke-width=\"1\" d=\"M4 9Q12 "
     "16 20 9Q19 18 12 19Q5 18 4 9Z\"/>"
     "<path fill=\"#fff8e7\" d=\"M6.5 10.5L10 12L8 16.5ZM17.5 10.5L14 12L16 "
     "16.5Z\"/>"
     "<path d=\"M9.5 17Q12 18 14.5 17\" fill=\"none\" stroke=\"#df4f78\" "
     "stroke-width=\"1.2\" stroke-linecap=\"round\"/></svg>",
     SVG_ICON_PX},

    // SVG_ACC_BUBBLE: translucent gum bubble with a white curved shine.
    {SVG_HEAD "<circle cx=\"12\" cy=\"12\" r=\"8.5\" fill=\"#f06fac\" "
              "fill-opacity=\".78\" stroke=\"#a93673\" stroke-width=\"1.3\"/>"
              "<path d=\"M7 9Q9 5.5 13 5\" fill=\"none\" stroke=\"#ffd5ea\" "
              "stroke-width=\"1.8\" stroke-linecap=\"round\"/>"
              "<circle cx=\"6.3\" cy=\"11.5\" r=\"1\" fill=\"#fff0f7\" "
              "fill-opacity=\".8\"/></svg>",
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
