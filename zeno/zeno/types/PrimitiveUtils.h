#pragma once

#include <zeno/utils/api.h>
#include <zeno/types/PrimitiveObject.h>
#include <string>

namespace zeno {

ZENO_API void prim_quads_to_tris(PrimitiveObject *prim);
ZENO_API void prim_polys_to_tris(PrimitiveObject *prim);
ZENO_API void prim_polys_to_tris_with_uv(PrimitiveObject *prim);
ZENO_API void primCalcNormal(PrimitiveObject *prim, float flip = 1.0f);
ZENO_API void primLineSort(PrimitiveObject *prim, bool reversed = false);
ZENO_API void primLineDistance(PrimitiveObject *prim, std::string resAttr, int start = 0);
ZENO_API void primFilter(PrimitiveObject *prim, std::string tagAttr, int tagValue, bool isInversed = false);

}
