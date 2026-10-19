// Play a synthetic "vocal" (voiced harmonics with bursts of sibilant noise) through every factory preset:
// report how much the esses and the voice change.
#include "../plugins/Talc/ParamDefs.hpp"
#include "../plugins/Talc/Presets.hpp"
#include <cstdio>
#include <memory>
#include <vector>
using namespace talc;
int main() {
    const int SR = 48000, N = SR * 3; int bad = 0;
    std::vector<float> voice(N), ess(N), x(N); uint32_t s = 9; float hp = 0, prev = 0;
    for (int i = 0; i < N; ++i) {
        const float t = i / (float)SR; float v = 0;
        for (int h = 1; h <= 12; ++h) v += std::sin(2 * kPi * 160 * h * t) / h;
        s = s * 1664525u + 1013904223u; const float w = (int32_t)s * (1.f / 2147483648.f);
        hp = 0.9f * (hp + w - prev); prev = w;                          // bright noise
        const bool sib = std::fmod(t, 0.6f) > 0.45f;                     // an ess every 0.6 s
        voice[i] = sib ? 0.f : 0.12f * v; ess[i] = sib ? 0.25f * hp : 0.f; x[i] = voice[i] + ess[i];
    }
    for (uint32_t p = 0; p < kNumPresets; ++p) {
        std::unique_ptr<Engine> e(new Engine()); for (uint32_t k = 0; k < kParamCount; ++k) e->p[k] = presets()[p].v[k]; e->setSampleRate(SR);
        std::vector<float> L(N), R(N); for (int i = 0; i < N; i += 512) e->process(x.data() + i, x.data() + i, L.data() + i, R.data() + i, std::min(512, N - i));
        const int lat = (int)e->latency(); double eIn = 0, eOut = 0, vIn = 0, vOut = 0; bool nf = false;
        for (int i = SR / 2; i < N; ++i) { const int j = i - lat; nf |= !std::isfinite(L[i]);
            if (ess[j] != 0) { eIn += x[j] * x[j]; eOut += L[i] * L[i]; } else { vIn += x[j] * x[j]; vOut += L[i] * L[i]; } }
        const double essDb = 10 * std::log10(eOut / eIn + 1e-20), voiceDb = 10 * std::log10(vOut / vIn + 1e-20);
        const bool ok = !nf && essDb < 0.1 && voiceDb > -3; bad += !ok;
        printf("%-20s %-12s esses %+5.1f dB  voice %+5.1f dB  latency %3d %s\n", presets()[p].name, presets()[p].category, essDb, voiceDb, lat, ok ? "" : "<-- CHECK");
    }
    printf("%u presets, %d flagged\n", kNumPresets, bad); return bad;
}
