// nanosvg is header-only; this is the single translation unit that compiles its
// implementation. It sits beside the submodule rather than inside it (a
// submodule is upstream's tree), and out of src/ so the CMake glob leaves it to
// its own target — no -Wall -Wextra, no clang-tidy on third-party code.
//
// NANOSVG_ALL_COLOR_KEYWORDS is deliberately NOT defined: our SVG sources use
// hex colors only, so the ~150-entry CSS color-name table stays out of the
// binary.
#define NANOSVG_IMPLEMENTATION
#include "nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvgrast.h"
