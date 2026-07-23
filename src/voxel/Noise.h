#pragma once
#include <cmath>

namespace Noise {

static float fade(float t) { return t * t * t * (t * (t * 6.f - 15.f) + 10.f); }
static float lerp(float a, float b, float t) { return a + t * (b - a); }

static float hash(int x, int z) {
    int n = x * 1619 + z * 31337;
    n = (n << 13) ^ n;
    n = n * (n * n * 15731 + 789221) + 1376312589;
    return 1.f - (float)(n & 0x7fffffff) / 1073741824.f;
}

static float smooth(float x, float z) {
    int ix = (int)std::floor(x), iz = (int)std::floor(z);
    float fx = x - ix, fz = z - iz;
    float u = fade(fx), v = fade(fz);
    return lerp(
        lerp(hash(ix,   iz  ), hash(ix+1, iz  ), u),
        lerp(hash(ix,   iz+1), hash(ix+1, iz+1), u), v);
}

static float fractal(float x, float z, int oct = 4) {
    float val = 0, amp = 1, freq = 1, tot = 0;
    for (int i = 0; i < oct; i++) {
        val += smooth(x * freq, z * freq) * amp;
        tot += amp; amp *= 0.5f; freq *= 2.f;
    }
    return val / tot;
}

} // namespace Noise
