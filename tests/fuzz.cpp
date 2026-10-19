// Random parameters (including structural changes) at 44.1-192 kHz with loud noise: output always finite and bounded.
#include "../plugins/Talc/ParamDefs.hpp"
#include <cstdio>
#include <memory>
#include <random>
using namespace talc;
int main() {
    std::mt19937 g(23); std::uniform_real_distribution<float> u(0, 1); int bad = 0; float pk = 0;
    for (int sr : { 44100, 48000, 96000, 192000 }) {
        std::unique_ptr<Engine> e(new Engine()); e->setSampleRate((float)sr);
        for (int it = 0; it < 1500; ++it) {
            for (uint32_t i = 0; i < kParamCount; ++i) {
                const ParamDef& d = paramDef(i); if (d.hints & kHintOutput) continue;
                if (u(g) < 0.8f) continue;
                float v = (u(g) < .3f) ? (u(g) < .5f ? d.min : d.max) : (d.hints & kHintLog) ? d.min * std::pow(d.max / d.min, u(g)) : d.min + (d.max - d.min) * u(g);
                if (d.hints & kHintInteger) v = std::round(v);
                e->p[i] = v;
            }
            float in[400], L[400], R[400]; const float amp = u(g) < .2f ? 0 : (u(g) < .5f ? 8.f : 0.7f);
            for (float& x : in) x = amp * (2 * u(g) - 1);
            e->process(in, in, L, R, 400);
            for (int k = 0; k < 400; ++k) { if (!std::isfinite(L[k]) || !std::isfinite(R[k])) bad++; pk = std::max(pk, std::max(std::fabs(L[k]), std::fabs(R[k]))); }
        }
    }
    printf("fuzz nonfinite=%d peak=%.2f\n", bad, pk);
    return bad || pk > 64.f;
}
