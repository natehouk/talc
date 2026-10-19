// Feature tests for Talc: transparency (wide and split band), threshold/range, band isolation in split mode,
// Single Vocal vs Allround detection, sidechain filters, look-ahead and latency, release, link, mid/side,
// audition, oversampling.
#include "../plugins/Talc/ParamDefs.hpp"
#include "../plugins/Talc/Presets.hpp"
#include <cstdio>
#include <memory>
#include <vector>
using namespace talc;

static int fails = 0;
static void check(bool ok, const char* fmt, double a = 0, double b = 0, double c = 0) { printf("%s  ", ok ? "PASS" : "FAIL"); printf(fmt, a, b, c); printf("\n"); fails += !ok; }
static const float fs = 48000;
typedef std::unique_ptr<Engine> EP;
typedef std::vector<float> V;
static EP make(int mode = kAllround) { EP e(new Engine()); e->p[kMode] = (float)mode; return e; }
static void run(Engine& e, const V& l, const V& r, V& L, V& R) {
    L.assign(l.size(), 0); R.assign(l.size(), 0);
    for (size_t i = 0; i < l.size(); i += 128) { const uint32_t n = (uint32_t)std::min<size_t>(128, l.size() - i); e.process(l.data() + i, r.data() + i, L.data() + i, R.data() + i, n); }
}
static V sine(float hz, float db, int n) { V v(n); const float a = dbToGain(db); for (int i = 0; i < n; ++i) v[i] = a * std::sin(2 * kPi * hz * i / fs); return v; }
static V add(const V& a, const V& b) { V r(a.size()); for (size_t i = 0; i < a.size(); ++i) r[i] = a[i] + b[i]; return r; }
static double toneDb(const V& v, float hz, size_t a, size_t b) {
    double c = 0, s = 0; for (size_t i = a; i < b; ++i) { c += v[i] * std::cos(2 * kPi * hz * i / fs); s += v[i] * std::sin(2 * kPi * hz * i / fs); }
    return 20 * std::log10(2 * std::sqrt(c * c + s * s) / (double)(b - a) + 1e-30);
}
static double gainAt(Engine& e, float hz, float db, int n = 48000) { V in = sine(hz, db, n), L, R; e.setSampleRate(fs); run(e, in, in, L, R); return toneDb(L, hz, n / 2, n) - db; }

int main() {
    const int N = 48000;
    // 1. transparency below the threshold: wide band is a wire, split band rebuilds the input exactly (delayed)
    { EP w = make(); w->p[kThreshold] = 0; w->setSampleRate(fs);
      V in(N); uint32_t r = 5; for (float& x : in) { r = r * 1664525u + 1013904223u; x = 0.1f * ((int32_t)r * (1.f / 2147483648.f)); }
      V L, R; run(*w, in, in, L, R); double e1 = 0; for (int i = 0; i < N; ++i) e1 = std::max(e1, (double)std::fabs(L[i] - in[i]));
      EP s = make(); s->p[kThreshold] = 0; s->p[kProcessing] = kSplitBand; s->setSampleRate(fs); run(*s, in, in, L, R);
      const int lat = (int)s->latency(); double e2 = 0; for (int i = lat; i < N; ++i) e2 = std::max(e2, (double)std::fabs(L[i] - in[i - lat]));
      check(e1 < 1e-6 && e2 < 1e-5, "idle: wide band error %.1e, split band (delayed %.0f samples) error %.1e", e1, lat, e2); }
    // 2. threshold and range: 8 kHz tone 14 dB over -> 14 * 0.85 = 11.9 dB; range caps it
    { EP a = make(); a->p[kThreshold] = -24; a->p[kRange] = 24; const double g = gainAt(*a, 8000, -10);
      EP b = make(); b->p[kThreshold] = -24; b->p[kRange] = 6; const double c = gainAt(*b, 8000, -10);
      check(std::fabs(g + 11.9) < 0.6 && std::fabs(c + 6) < 0.2, "8 kHz, 14 dB over: %.2f dB (expect -11.9); range 6: %.2f dB", g, c); }
    // 3. low frequencies don't trigger it
    { EP a = make(); a->p[kThreshold] = -40; const double g = gainAt(*a, 300, -6);
      check(std::fabs(g) < 0.05, "a loud 300 Hz tone below the 5.5 kHz sidechain high-pass: %.3f dB", g); }
    // 4. split band only turns the high band down; wide band turns everything down
    { auto both = [&](int proc, double& lo, double& hi) { EP e = make(); e->p[kThreshold] = -30; e->p[kRange] = 12; e->p[kProcessing] = (float)proc; e->p[kHpFreq] = 5000; e->setSampleRate(fs);
          V in = add(sine(300, -10, N), sine(9000, -10, N)), L, R; run(*e, in, in, L, R); lo = toneDb(L, 300, N / 2, N) + 10; hi = toneDb(L, 9000, N / 2, N) + 10; };
      double wl, wh, sl, sh; both(kWideBand, wl, wh); both(kSplitBand, sl, sh);
      check(wl < -10 && std::fabs(wl - wh) < 0.3, "wide band: 300 Hz %.1f dB, 9 kHz %.1f dB (both turned down)", wl, wh);
      check(std::fabs(sl) < 0.1 && sh < -10, "split band: 300 Hz %.2f dB (untouched), 9 kHz %.1f dB", sl, sh); }
    // 5. Single Vocal ignores bright but non-sibilant sound; Allround doesn't
    { auto bright = [&](int mode) { EP e = make(mode); e->p[kThreshold] = -36; e->p[kRange] = 12; e->p[kProcessing] = kSplitBand; e->setSampleRate(fs);
          V in = add(sine(300, -4, N), sine(7000, -26, N)), L, R; run(*e, in, in, L, R); return toneDb(L, 7000, N / 2, N) + 26; };
      auto ess = [&](int mode) { EP e = make(mode); e->p[kThreshold] = -36; e->p[kRange] = 12; e->p[kProcessing] = kSplitBand; e->setSampleRate(fs);
          V in = add(sine(300, -30, N), sine(7000, -12, N)), L, R; run(*e, in, in, L, R); return toneDb(L, 7000, N / 2, N) + 12; };
      const double vb = bright(kSingleVocal), ab = bright(kAllround), ve = ess(kSingleVocal);
      check(vb > -1.5 && ab < -5 && ve < -9, "7 kHz under a loud vowel: Single Vocal %.1f dB, Allround %.1f dB; an ess on its own, Single Vocal %.1f dB", vb, ab, ve); }
    // 6. the sidechain low-pass excludes frequencies above it
    { EP a = make(); a->p[kThreshold] = -40; a->p[kHpFreq] = 3000; a->p[kLpFreq] = 6000; const double g = gainAt(*a, 15000, -6);
      EP b = make(); b->p[kThreshold] = -40; b->p[kHpFreq] = 3000; b->p[kLpFreq] = 6000; const double h = gainAt(*b, 4500, -6);
      check(std::fabs(g) < 0.3 && h < -6, "sidechain 3-6 kHz: a 15 kHz tone %.2f dB, a 4.5 kHz tone %.1f dB", g, h); }
    // 7. latency: look-ahead, split filter, oversampling; and the reported value is the real one
    { EP a = make(); a->p[kLookahead] = 15; EP b = make(); b->p[kProcessing] = kSplitBand; EP c = make(); c->p[kOversampling] = 2; c->p[kLookahead] = 5;
      for (Engine* x : { a.get(), b.get(), c.get() }) x->setSampleRate(fs);
      check(a->latency() == 720 && b->latency() == 254 && c->latency() == 288, "latency: 15 ms look-ahead %.0f, split band %.0f, 5 ms + 4x %.0f samples", a->latency(), b->latency(), c->latency());
      EP h = make(); h->p[kThreshold] = 0; h->p[kProcessing] = kSplitBand; h->p[kLookahead] = 3; h->p[kOversampling] = 1; h->setSampleRate(fs);
      V in(8192, 0.f), L, R; in[1000] = 0.5f; run(*h, in, in, L, R); int pk = 0; for (int i = 0; i < 8192; ++i) if (std::fabs(L[i]) > std::fabs(L[pk])) pk = i;
      check(pk - 1000 == (int)h->latency(), "an impulse comes out %.0f samples late, reported %.0f", pk - 1000, h->latency()); }
    // 8. look-ahead catches the start of an ess
    { auto onset = [&](float la) { EP e = make(); e->p[kThreshold] = -30; e->p[kRange] = 12; e->p[kLookahead] = la; e->setSampleRate(fs);
          V x(N); for (int i = 0; i < N; ++i) x[i] = (i >= 24000 ? 0.4f : 0.f) * std::sin(2 * kPi * 8000 * i / fs);
          V L, R; run(*e, x, x, L, R); const int lat = (int)e->latency(); float m = 0; for (int i = 24000 + lat; i < 24000 + lat + 48; ++i) m = std::max(m, std::fabs(L[i])); return gainToDb(m / 0.4f); };
      const double no = onset(0), yes = onset(3);
      check(yes < no - 4, "first millisecond of an ess: %.1f dB with 3 ms look-ahead, %.1f dB without", yes, no); }
    // 9. release: after the ess, the gain recovers within about 100 ms
    { EP e = make(); e->p[kThreshold] = -30; e->p[kRange] = 12; e->setSampleRate(fs);
      V x(N); for (int i = 0; i < N; ++i) x[i] = (i < 12000 ? 0.4f : 0.01f) * std::sin(2 * kPi * 8000 * i / fs);
      V L, R; run(*e, x, x, L, R);
      const double at150 = toneDb(L, 8000, 12000 + 7200, 12000 + 7680) - toneDb(x, 8000, 12000 + 7200, 12000 + 7680);
      check(std::fabs(at150) < 1.0, "150 ms after a quarter-second ess the gain is back to %.2f dB", at150); }
    // 10. stereo link, mid only, side only
    { auto right = [&](float link) { EP e = make(); e->p[kLink] = link; e->p[kThreshold] = -30; e->p[kRange] = 12; e->setSampleRate(fs);
          V l = sine(8000, -6, N), r = sine(8000, -50, N), L, R; run(*e, l, r, L, R); return toneDb(R, 8000, N / 2, N) + 50; };
      const double un = right(0), lk = right(1);
      check(std::fabs(un) < 0.1 && lk < -10, "quiet right channel: unlinked %.2f dB, linked %.1f dB", un, lk);
      V s = sine(8000, -6, N), ns(N), L, R; for (int i = 0; i < N; ++i) ns[i] = -s[i];
      EP m = make(); m->p[kChannel] = kMidOnly; m->p[kThreshold] = -30; m->p[kRange] = 12; m->setSampleRate(fs);
      run(*m, s, ns, L, R); const double sideThrough = toneDb(L, 8000, N / 2, N) + 6;
      m->setSampleRate(fs); run(*m, s, s, L, R); const double midDown = toneDb(L, 8000, N / 2, N) + 6;
      EP sd = make(); sd->p[kChannel] = kSideOnly; sd->p[kThreshold] = -30; sd->p[kRange] = 12; sd->setSampleRate(fs);
      run(*sd, s, s, L, R); const double midThrough = toneDb(L, 8000, N / 2, N) + 6;
      check(std::fabs(sideThrough) < 0.1 && midDown < -10 && std::fabs(midThrough) < 0.1, "mid only: side signal %.2f dB, mid %.1f dB; side only: mid signal %.2f dB", sideThrough, midDown, midThrough); }
    // 11. audition plays the filtered sidechain
    { EP e = make(); e->p[kListen] = 1; e->p[kHpFreq] = 5000; e->setSampleRate(fs);
      V in = add(sine(300, -6, N), sine(9000, -20, N)), L, R; run(*e, in, in, L, R);
      const double lo = toneDb(L, 300, N / 2, N) + 6, hi = toneDb(L, 9000, N / 2, N) + 20;
      check(lo < -50 && std::fabs(hi) < 0.5, "audition: 300 Hz %.0f dB, 9 kHz %.2f dB", lo, hi); }
    // 12. oversampling is transparent when idle, and presets stay finite
    { double worst = 0; for (int os = 1; os < 3; ++os) for (float hz : { 200.f, 3000.f, 12000.f, 18000.f }) { EP e = make(); e->p[kThreshold] = 0; e->p[kOversampling] = (float)os; worst = std::max(worst, std::fabs(gainAt(*e, hz, -12))); }
      bool ok = true; for (uint32_t i = 0; i < kNumPresets; ++i) { EP e(new Engine()); std::copy(presets()[i].v, presets()[i].v + kParamCount, e->p); ok &= std::isfinite(gainAt(*e, 7000, -10, 12000)); }
      check(worst < 0.05 && ok, "oversampling idle within %.3f dB up to 18 kHz; all %.0f presets finite", worst, kNumPresets); }
    printf("\n%s (%d failure%s)\n", fails ? "FAILED" : "ALL PASSED", fails, fails == 1 ? "" : "s");
    return fails ? 1 : 0;
}
