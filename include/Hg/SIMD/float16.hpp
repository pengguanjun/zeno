#pragma once

#include "simd.hpp"

namespace hg::SIMD {

  template <> struct _M<float, 16> {
    __m512 m;

    _M() {}
    explicit _M(__m512 m) : m(m) {}

    void setall(float x) { m = _mm512_set1_ps(x); }

    void set(float x, float y, float z, float w, float a, float b, float c, float d, float e,
             float f, float g, float h, float i, float j, float k, float l) {
      m = _mm512_set_ps(l, k, j, i, h, g, f, e, d, c, b, a, w, z, y, x);
    }

    void load(float const *p) { m = _mm512_load_ps(p); }

    void store(float *p) const { _mm512_store_ps(p, m); }

    void loadu(float const *p) { m = _mm512_loadu_ps(p); }

    void storeu(float *p) const { _mm512_storeu_ps(p, m); }

    template <int x, int y, int z, int w> void shuffle(_M const &l, _M const &r) {
      m = _mm512_shuffle_ps(l.m, r.m, _MM_SHUFFLE(w, z, y, x));
    }

    void unpackhi(_M const &l, _M const &r) { m = _mm512_unpackhi_ps(l.m, r.m); }

    void unpacklo(_M const &l, _M const &r) { m = _mm512_unpacklo_ps(l.m, r.m); }

    void movelh(_M const &l, _M const &r) { m = _mm512_movelh_ps(l.m, r.m); }

    void movehl(_M const &l, _M const &r) { m = _mm512_movehl_ps(l.m, r.m); }

    int movemask() const { return _mm512_movemask_ps(m); }

    float gets() const { return _mm512_cvtss_f32(m); }

    void dotp(_M const &l, _M const &r, int imm) { m = _mm512_dp_ps(l.m, r.m, imm); }

    void blend(_M const &l, _M const &r, int imm) { m = _mm512_blend_ps(l.m, r.m, imm); }

    void blendv(_M const &l, _M const &r, _M const &c) { m = _mm512_blendv_ps(l.m, r.m, c.m); }

#define _DEF_OP2P(x) \
  void x##p(_M const &l, _M const &r) { m = _mm512_##x##_ps(l.m, r.m); }
#define _DEF_OP2S(x) \
  void x##s(_M const &l, _M const &r) { m = _mm512_##x##_ss(l.m, r.m); }

#define _DEF_OP2(x) \
  _DEF_OP2P(x)      \
  _DEF_OP2S(x)

    _DEF_OP2P(add);
    _DEF_OP2P(sub);
    _DEF_OP2P(hadd);
    _DEF_OP2P(hsub);
    _DEF_OP2P(addsub);
    _DEF_OP2P(mul);
    _DEF_OP2P(div);
    _DEF_OP2P(min);
    _DEF_OP2P(max);
    _DEF_OP2P(or);
    _DEF_OP2P(and);
    _DEF_OP2P(xor);
    _DEF_OP2P(andnot);

#undef _DEF_OP2
#undef _DEF_OP2P
#undef _DEF_OP2S

#define _DEF_OP1(x) \
  void x##p(_M const &o) { m = _mm512_##x##_ps(o.m); }

    _DEF_OP1(rcp14);
    _DEF_OP1(sqrt);

#undef _DEF_OP1
  };

}  // namespace hg::SIMD
