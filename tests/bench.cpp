// CPU: wide and split band, at 1x, 2x and 4x, with look-ahead.
#include "../plugins/Talc/ParamDefs.hpp"
#include <chrono>
#include <cstdio>
#include <memory>
using namespace talc;
int main() {
    const int SR = 48000;
    static float in[SR], L[SR], R[SR]; uint32_t r = 1; for (float& x : in) { r = r * 1664525u + 1013904223u; x = 0.3f * ((int32_t)r * (1.f / 2147483648.f)); }
    for (int proc = 0; proc < 2; ++proc) for (int os = 0; os < 3; ++os) {
        std::unique_ptr<Engine> e(new Engine()); e->p[kProcessing] = (float)proc; e->p[kOversampling] = (float)os; e->p[kLookahead] = 5; e->p[kThreshold] = -30; e->setSampleRate(SR);
        const int secs = 10; auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < secs; ++k) for (int b = 0; b < SR; b += 256) e->process(in + b, in + b, L + b, R + b, 256);
        const double el = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        printf("%s %dx: %.2f%% of one core\n", proc ? "split" : "wide ", os == 0 ? 1 : (os == 1 ? 2 : 4), 100 * el / secs);
    }
    return 0;
}
