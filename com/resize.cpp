// stb_image_resize2 uses C99 compound literals in its NEON/AVX/SSE SIMD paths
// (e.g. stbir_make16), which are not valid C++ and are rejected under
// -Werror,-Wc99-extensions. This TU compiles the implementation as C++, so force
// the scalar path. The cap only fires at load time on oversized substitute PNGs,
// so scalar resize performance is a non-issue. Cross-TU linkage is C-linkage
// (STBIRDEF is extern "C" under __cplusplus), matching the declarations included
// from bak/textureFactory.cpp.
#define STBIR_NO_SIMD
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "com/stb_image_resize2.h"