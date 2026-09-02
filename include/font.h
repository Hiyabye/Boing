#ifndef FONT_H
#define FONT_H

#include "raylib.h"

#include <stdbool.h>

// The game's own vector font — the text half of the art pipeline.
//
// Same no-asset-files doctrine as svg.c: every glyph is an SVG path baked into
// the binary, rasterized once by nanosvg into a single atlas and wrapped in a
// raylib Font, so text draws through DrawTextEx/MeasureTextEx instead of
// raylib's chunky 10px default bitmap font.
//
// Division of labour across the three art subsystems:
//   a letterform             -> here
//   a crisp authored outline -> an SvgId
//   noise or a soft falloff  -> a TexId
//
// The glyphs are skeletons, not outlines: each is a stroked path at one uniform
// width with round caps and joins. That is what keeps 49 glyphs consistent with
// each other and cheap in the binary (~40 bytes of path data apiece).
//
// Nothing outside ui.c should need this — ui.c's uiText/uiTextWidth are the
// public text surface, and every screen goes through those.

// The font, or raylib's default until the atlas is resident (and permanently if
// the bake failed — quiet degrade, same contract as a glyph that fails to
// parse). Never returns a font with no texture: MeasureTextEx measures such a
// font as zero-width, which would collapse every centered string to x = 0.
Font gameFont(void);

// Bakes the whole atlas on its first call, rather than one glyph per frame
// across 49 of them. Driven by the splash beside the SVG and texture queues.
bool fontPreloadStep(void);

void unloadGameFont(void);

#endif // FONT_H
