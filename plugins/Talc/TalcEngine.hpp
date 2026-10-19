// Talc — de-esser engine: "Single Vocal" (sibilance relative to the rest of the voice) and "Allround"
// (classic filtered-sidechain) detection, threshold and range, wide-band or linear-phase split-band
// processing, look-ahead up to 15 ms, stereo link, stereo / mid-only / side-only, 2x/4x oversampling of
// the gain stage, sidechain audition, and data for the UI (level history, analyzer ring).
// Clean-room implementation from public, generic techniques (TPT state-variable filters, RMS detectors,
// soft-knee gain computer with program-dependent release, Kaiser-windowed-sinc linear-phase FIR band split,
// windowed-sinc halfband oversampling). No third-party product code, assets or disassembly were used.
// License: MIT (see LICENSE).

#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <vector>

namespace talc {

static constexpr float kPi = 3.14159265358979f;
inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
inline float dbToGain(float db) { return std::exp(db * 0.11512925465f); }
inline float gainToDb(float g) { return 8.68588963807f * std::log(std::max(g, 1e-9f)); }

// ------------------------------------------------------------------ params (append-only)
enum Param : uint32_t {
    kMode, kThreshold, kRange, kProcessing, kHpFreq, kLpFreq, kLookahead, kLink, kChannel, kOversampling, kListen,
    kDisplayRange, kMidiOn,
    // outputs (read-only, for the UI)
    kOutLearn, kOutInL, kOutInR, kOutOutL, kOutOutR, kOutGr, kOutDet,
    kParamCount
};
enum Mode { kSingleVocal, kAllround };
enum Processing { kWideBand, kSplitBand };
enum Channel { kStereo, kMidOnly, kSideOnly };
static constexpr float kLpOpen = 19500.f;       // the low-pass at the top of its range is off
static constexpr float kKnee = 6.f;             // soft knee around the threshold, dB
static constexpr float kSlope = 0.85f;          // gain reduction per dB over the threshold (about 6.7:1)

// gain reduction (dB, >= 0) for a detector level
inline float reductionDb(float lvl, float thr, float range) {
    const float d = lvl - thr; float over;
    if (d <= -0.5f * kKnee) over = 0; else if (d >= 0.5f * kKnee) over = d; else { const float t = d + 0.5f * kKnee; over = t * t / (2.f * kKnee); }
    return std::min(over * kSlope, std::max(range, 0.f));
}

// ------------------------------------------------------------------ filters
// TPT state-variable filter, 2nd order Butterworth sections (cascaded twice: 24 dB/oct)
struct Svf2 {
    float ic1[2] = {}, ic2[2] = {};
    void reset() { ic1[0] = ic1[1] = ic2[0] = ic2[1] = 0; }
    inline float process(float x, float g, bool highpass) {
        static const float k[2] = { 1.84775907f, 0.76536686f };
        for (int i = 0; i < 2; ++i) {
            const float a1 = 1.f / (1.f + g * (g + k[i]));
            const float v3 = x - ic2[i], v1 = a1 * ic1[i] + g * a1 * v3, v2 = ic2[i] + g * v1;
            ic1[i] = 2.f * v1 - ic1[i]; ic2[i] = 2.f * v2 - ic2[i];
            x = highpass ? x - k[i] * v1 - v2 : v2;
        }
        return x;
    }
};

// ------------------------------------------------------------------ halfband oversampling (65 taps, Kaiser)
static constexpr int kHbHalf = 16, kHbC = 2 * kHbHalf;
inline double bessel0(double x) { double s = 1, term = 1; for (int k = 1; k < 40; ++k) { term *= (x / (2 * k)) * (x / (2 * k)); s += term; } return s; }
inline const float* halfbandTaps() {
    static float t[kHbHalf]; static bool done = false;
    if (!done) {
        done = true;
        const double beta = 8.0, N = 2 * kHbC, PI = 3.14159265358979323846; double sum = 0.5;
        for (int j = 0; j < kHbHalf; ++j) {
            const int k = 2 * j + 1; const double sinc = std::sin(PI * k / 2.0) / (PI * k / 2.0);
            const double r = (double)(k + kHbC) / N * 2 - 1, w = bessel0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / bessel0(beta);
            t[j] = (float)(0.5 * sinc * w); sum += 2 * t[j];
        }
        for (int j = 0; j < kHbHalf; ++j) t[j] = (float)(t[j] / sum);
    }
    return t;
}
struct HbUp {
    float buf[128] = {}; int pos = 0;
    void reset() { std::memset(buf, 0, sizeof buf); pos = 0; }
    inline void process(float x, float& y0, float& y1) {
        pos = (pos + 1) & 63; buf[pos] = buf[pos + 64] = x;
        const float* h = halfbandTaps(); const float* b = buf + pos + 64;
        y0 = b[-kHbHalf]; float s = 0;
        for (int j = 0; j < kHbHalf; ++j) s += h[j] * (b[-kHbHalf - j] + b[-kHbHalf + j + 1]);
        y1 = 2.f * s;
    }
};
struct HbDown {
    float buf[256] = {}; int pos = 0;
    void reset() { std::memset(buf, 0, sizeof buf); pos = 0; }
    inline float process(float x0, float x1) {
        pos = (pos + 1) & 127; buf[pos] = buf[pos + 128] = x0;
        pos = (pos + 1) & 127; buf[pos] = buf[pos + 128] = x1;
        const float* h = halfbandTaps(); const float* c = buf + pos + 128 - kHbC - 1;
        float s = 0.5f * c[0];
        for (int j = 0; j < kHbHalf; ++j) s += h[j] * (c[-(2 * j + 1)] + c[2 * j + 1]);
        return s;
    }
};
struct Oversampler {
    HbUp u1, u2; HbDown d1, d2;
    void reset() { u1.reset(); u2.reset(); d1.reset(); d2.reset(); }
    static int latency(int os) { return os == 1 ? 0 : (os == 2 ? 32 : 48); }
    inline void up(float x, int os, float* y) {
        if (os == 1) { y[0] = x; return; }
        float a, b; u1.process(x, a, b);
        if (os == 2) { y[0] = a; y[1] = b; return; }
        u2.process(a, y[0], y[1]); u2.process(b, y[2], y[3]);
    }
    inline float down(const float* y, int os) {
        if (os == 1) return y[0];
        if (os == 2) return d1.process(y[0], y[1]);
        const float a = d2.process(y[0], y[1]), b = d2.process(y[2], y[3]);
        return d1.process(a, b);
    }
};

// ------------------------------------------------------------------ engine
class Engine {
public:
    float p[kParamCount];
    // level history for the display: one point every 5 ms
    static constexpr int kHist = 2048;
    float hPeak[kHist], hGr[kHist], hDet[kHist];
    // analyzer ring (the input, mono) for the filter display
    static constexpr int kAna = 4096;
    float ana[kAna];
    struct AtomicIndex {
        std::atomic<uint32_t> v { 0 };
        AtomicIndex() {}
        AtomicIndex(const AtomicIndex& o) : v(o.v.load()) {}
        AtomicIndex& operator=(const AtomicIndex& o) { v.store(o.v.load()); return *this; }
        uint32_t load() const { return v.load(std::memory_order_acquire); }
        void store(uint32_t x) { v.store(x, std::memory_order_release); }
    } histWrite, anaWrite;

    Engine() {
        std::fill(p, p + kParamCount, 0.f);
        p[kMode] = kSingleVocal; p[kThreshold] = -24; p[kRange] = 8; p[kProcessing] = kWideBand; p[kHpFreq] = 5500; p[kLpFreq] = 20000;
        p[kLookahead] = 0; p[kLink] = 1; p[kChannel] = kStereo; p[kOversampling] = 0; p[kListen] = 0; p[kDisplayRange] = 1; p[kMidiOn] = 1;
        for (int i = 0; i < kHist; ++i) { hPeak[i] = 0; hGr[i] = 0; hDet[i] = -120; }
        std::fill(ana, ana + kAna, 0.f);
    }
    void setSampleRate(float sr) {
        fs = sr;
        firHalf = (int)std::round(0.0053f * sr); firLen = 2 * firHalf + 1;          // linear-phase band split, ~5 ms latency
        int n = 1; while (n < (int)(0.015f * sr) + firLen + 64 + 8) n <<= 1;
        histN = n; for (auto& h : hist) h.assign(histN, 0.f);
        for (auto& b : firBuf) b.assign(2 * firLen, 0.f);
        fir.assign(firHalf + 1, 0.f);
        smooth = 1.f - std::exp(-1.f / (0.015f * sr));
        meterFall = std::exp(-1.f / (0.3f * sr));
        histSamples = std::max(1, (int)(0.005f * sr));
        configure();
        for (int c = 0; c < 2; ++c) { mIn[c] = mOut[c] = 0; gr[c] = 0; grSlow[c] = 0; }
        mGr = 0; mDet = -120;
    }
    void reset() { configure(); }
    uint32_t latency() const { return (uint32_t)(laSamples + (split ? firHalf : 0) + Oversampler::latency(os)); }
    float sampleRate() const { return fs; }
    float meterIn(int c) const { return gainToDb(mIn[c]); }
    float meterOut(int c) const { return gainToDb(mOut[c]); }
    float meterGr() const { return mGr; }
    float detectorDb() const { return mDet; }
    float grNow() const { return std::max(gr[0], gr[1]); }

    void process(const float* inL, const float* inR, float* outL, float* outR, uint32_t n) {
        const int wantOs = osOf(p[kOversampling]), wantLa = laOf(p[kLookahead]); const bool wantSplit = p[kProcessing] > 0.5f;
        const int wantCh = (int)clampf(std::round(p[kChannel]), 0, 2);
        if ((wantOs != os || wantLa != laSamples || wantSplit != split || wantCh != chan) && fadeState == 0) fadeState = 1;
        const bool vocal = p[kMode] < 0.5f, listen = p[kListen] > 0.5f;
        const float hp = clampf(p[kHpFreq], 1000.f, 20000.f), lp = clampf(p[kLpFreq], 2000.f, 20000.f);
        const bool lpOn = lp < kLpOpen;
        if (hp != firHp || lp != firLp) designFir(hp, lp);
        const float gHp = std::tan(kPi * clampf(hp, 10.f, 0.45f * fs) / fs), gLp = std::tan(kPi * clampf(lp, 10.f, 0.45f * fs) / fs);
        const float thr = clampf(p[kThreshold], -60.f, 0.f), range = clampf(p[kRange], 0.f, 24.f), link = clampf(p[kLink], 0.f, 1.f);
        const float detC = 1.f - std::exp(-1.f / (0.0015f * fs)), fullC = 1.f - std::exp(-1.f / (0.010f * fs));
        const float attC = 1.f - std::exp(-1.f / (0.0004f * fs)), slowC = 1.f - std::exp(-1.f / (0.25f * fs));
        const int upLat = Oversampler::latency(os) / 2, firLat = split ? firHalf : 0;
        for (uint32_t i = 0; i < n; ++i) {
            sListen += ((listen ? 1.f : 0.f) - sListen) * smooth;
            hist[0][hp_] = inL[i]; hist[1][hp_] = inR[i];
            for (int c = 0; c < 2; ++c) mIn[c] = std::max(mIn[c] * meterFall, std::fabs(c ? inR[i] : inL[i]));
            { const uint32_t w = anaWrite.load() % kAna; ana[w] = 0.5f * (inL[i] + inR[i]); anaWrite.store(anaWrite.load() + 1); }
            // detector: the input earlier than the audio by the look-ahead
            const int di = (hp_ - firLat - upLat) & histMask;
            float d[2] = { hist[0][di], hist[1][di] };
            int nch = 2;
            if (chan == kMidOnly) { d[0] = 0.5f * (d[0] + d[1]); nch = 1; }
            else if (chan == kSideOnly) { d[0] = 0.5f * (d[0] - d[1]); nch = 1; }
            float lvl[2] = { -150, -150 };
            for (int c = 0; c < nch; ++c) {
                float s = hpf[c].process(d[c], gHp, true);
                if (lpOn) s = lpf[c].process(s, gLp, false);
                scOut[c] = s;
                hfMs[c] += (s * s - hfMs[c]) * detC;
                const float hfDb = 10.f * std::log10(std::max(2.f * hfMs[c], 1e-15f));
                if (vocal) {
                    // Single Vocal: sibilance is high-frequency energy that dominates the voice at that moment.
                    // Compare the band with the full signal; when the band is far below it (a bright vowel,
                    // a breathy note), the detector is pulled down so only real esses reach the threshold.
                    fullMs[c] += (d[c] * d[c] - fullMs[c]) * fullC;
                    const float fullDb = 10.f * std::log10(std::max(2.f * fullMs[c], 1e-15f));
                    const float share = hfDb - fullDb;                                 // <= ~0 dB
                    const float sib = clampf((share + 12.f) / 8.f, 0.f, 1.f);         // 1: sibilant, 0: not
                    lvl[c] = hfDb - (1.f - sib) * 18.f;
                } else lvl[c] = hfDb;
            }
            if (nch == 2 && link > 0) { const float mx = std::max(lvl[0], lvl[1]); for (int c = 0; c < 2; ++c) lvl[c] += (mx - lvl[c]) * link; }
            if (nch == 1) { lvl[1] = lvl[0]; scOut[1] = scOut[0]; }
            for (int c = 0; c < 2; ++c) {
                const float t = reductionDb(lvl[c], thr, range);
                // program-dependent release: a short ess recovers fast, a long one smoothly
                const float rel = 0.015f + 0.035f * clampf(grSlow[c] / 4.f, 0.f, 1.f);   // 15 ms .. 50 ms
                gr[c] += (t - gr[c]) * (t > gr[c] ? attC : 1.f - std::exp(-1.f / (rel * fs)));
                grSlow[c] += (gr[c] - grSlow[c]) * slowC;
                if (!(gr[c] >= 0.f && gr[c] < 200.f)) gr[c] = 0;
            }
            const float gNow[2] = { dbToGain(-gr[0]), dbToGain(-gr[1]) };
            // audio, delayed by the look-ahead; the split filter and the oversampled gain stage follow
            const int ai = (hp_ - laSamples) & histMask;
            float a[2] = { hist[0][ai], hist[1][ai] };
            float w[2];
            if (chan != kStereo) { const float m = 0.5f * (a[0] + a[1]), s = 0.5f * (a[0] - a[1]); a[0] = chan == kMidOnly ? m : s; a[1] = chan == kMidOnly ? s : m; }
            const int ncg = chan == kStereo ? 2 : 1;                                   // channels that get the gain
            for (int c = 0; c < 2; ++c) {
                float band = a[c], rest = 0.f;
                if (split) { firBuf[c][firPos] = firBuf[c][firPos + firLen] = a[c]; const float* b = &firBuf[c][firPos + 1];
                    // b[0] is the oldest of the last firLen samples; the centre tap is b[firHalf]
                    float s = fir[0] * b[firHalf];
                    for (int k = 1; k <= firHalf; ++k) s += fir[k] * (b[firHalf - k] + b[firHalf + k]);
                    band = s; rest = b[firHalf] - s; }
                if (c < ncg) {
                    float up[4], y[4]; ovs[c].up(band, os, up);
                    const float g0 = gPrev[c], g1 = gNow[c];
                    for (int k = 0; k < os; ++k) y[k] = up[k] * (g0 + (g1 - g0) * (k + 1.f) / os);
                    band = ovs[c].down(y, os); gPrev[c] = g1;
                } else {                                                               // untouched channel: same delay as the processed one
                    float up[4]; ovs[c].up(band, os, up); band = ovs[c].down(up, os);
                }
                if (split) { restBuf[c][restPos] = rest; rest = restBuf[c][(restPos - Oversampler::latency(os) + 64) & 63]; }
                w[c] = band + rest;
            }
            if (split) { firPos = (firPos + 1) % firLen; restPos = (restPos + 1) & 63; }
            if (chan != kStereo) { const float m = chan == kMidOnly ? w[0] : w[1], s = chan == kMidOnly ? w[1] : w[0]; w[0] = m + s; w[1] = m - s; }
            float o[2];
            for (int c = 0; c < 2; ++c) {
                float v = w[c];
                const float aud = chan == kStereo ? scOut[c] : (chan == kMidOnly ? scOut[0] : (c ? -scOut[0] : scOut[0]));
                v += (aud - v) * sListen;
                v *= fadeGain;
                if (!(std::fabs(v) < 64.f)) v = std::isfinite(v) ? std::copysign(64.f, v) : 0.f;
                o[c] = v; mOut[c] = std::max(mOut[c] * meterFall, std::fabs(v));
            }
            outL[i] = o[0]; outR[i] = o[1];
            const float g = std::max(gr[0], gr[1]);
            mGr = std::max(mGr * 0.9995f, g);
            mDet = std::max(lvl[0], lvl[1]) > mDet ? std::max(lvl[0], lvl[1]) : mDet + (std::max(lvl[0], lvl[1]) - mDet) * 0.0005f;
            // display history: input peak (at the audio's time), gain reduction, detector level
            const int hi = (hp_ - laSamples - firLat - upLat) & histMask;              // the audio this gain reduction is applied to
            hPk = std::max(hPk, std::max(std::fabs(hist[0][hi]), std::fabs(hist[1][hi])));
            hG = std::max(hG, g); hD = std::max(hD, std::max(lvl[0], lvl[1]));
            if (++hCount >= histSamples) {
                const uint32_t wi = histWrite.load() % kHist;
                hPeak[wi] = hPk; hGr[wi] = hG; hDet[wi] = hD;
                histWrite.store(histWrite.load() + 1);
                hPk = 0; hG = 0; hD = -200; hCount = 0;
            }
            hp_ = (hp_ + 1) & histMask;
            if (fadeState == 1) { fadeGain -= 1.f / 96.f; if (fadeGain <= 0) { configure(); fadeGain = 0; fadeState = 2; } }
            else if (fadeState == 2) { fadeGain += 1.f / 96.f; if (fadeGain >= 1) { fadeGain = 1; fadeState = 0; } }
        }
        for (int c = 0; c < 2; ++c) if (!(std::fabs(hfMs[c]) < 1e6f) || !(std::fabs(fullMs[c]) < 1e6f)) { hfMs[c] = fullMs[c] = 0; hpf[c].reset(); lpf[c].reset(); }
    }

    // the split-band filter's magnitude at f (for the display): 0..1
    float splitMag(float f) const {
        const float hp = clampf(p[kHpFreq], 1000.f, 20000.f), lp = clampf(p[kLpFreq], 2000.f, 20000.f);
        float m = 1.f / (1.f + std::pow(hp / std::max(f, 1.f), 8.f));
        if (lp < kLpOpen) m *= 1.f / (1.f + std::pow(f / lp, 8.f));
        return m;
    }

private:
    static int osOf(float v) { const int k = (int)clampf(std::round(v), 0, 2); return k == 0 ? 1 : (k == 1 ? 2 : 4); }
    int laOf(float ms) const { return (int)std::round(clampf(ms, 0.f, 15.f) * 0.001f * fs); }
    // Kaiser-windowed sinc band-pass (or high-pass when the low-pass is open), linear phase, symmetric
    void designFir(float hp, float lp) {
        firHp = hp; firLp = lp;
        const double beta = 7.0, PI = 3.14159265358979323846, i0b = bessel0(beta);
        const double fh = hp / fs, fl = lp < kLpOpen ? lp / fs : 0.5;
        for (int k = 0; k <= firHalf; ++k) {
            auto lpTap = [&](double fc) { return k == 0 ? 2 * fc : std::sin(2 * PI * fc * k) / (PI * k); };
            const double r = (double)k / firHalf, w = bessel0(beta * std::sqrt(std::max(0.0, 1 - r * r))) / i0b;
            fir[k] = (float)((lpTap(fl) - lpTap(fh)) * w);
        }
    }
    void configure() {
        os = osOf(p[kOversampling]); laSamples = laOf(p[kLookahead]); split = p[kProcessing] > 0.5f; chan = (int)clampf(std::round(p[kChannel]), 0, 2);
        histMask = histN - 1;
        for (int c = 0; c < 2; ++c) { ovs[c].reset(); gPrev[c] = dbToGain(-gr[c]); std::fill(firBuf[c].begin(), firBuf[c].end(), 0.f); std::fill(restBuf[c], restBuf[c] + 64, 0.f); }
        firPos = 0; restPos = 0; firHp = firLp = -1;
        sListen = p[kListen] > 0.5f ? 1.f : 0.f;
        fadeState = 0; fadeGain = 1;
    }

    float fs = 48000.f, smooth = 0.01f, meterFall = 0.999f;
    int os = 1, laSamples = 0, chan = kStereo, fadeState = 0; bool split = false; float fadeGain = 1;
    std::vector<float> hist[2]; int histN = 1, histMask = 0, hp_ = 0;
    std::vector<float> firBuf[2], fir; int firHalf = 255, firLen = 511, firPos = 0; float firHp = -1, firLp = -1;
    float restBuf[2][64] = {}; int restPos = 0;
    Oversampler ovs[2];
    Svf2 hpf[2], lpf[2];
    float hfMs[2] = {}, fullMs[2] = {}, gr[2] = {}, grSlow[2] = {}, gPrev[2] = { 1, 1 }, scOut[2] = {}, sListen = 0;
    float mIn[2] = {}, mOut[2] = {}, mGr = 0, mDet = -120;
    float hPk = 0, hG = 0, hD = -200; int hCount = 0, histSamples = 240;
};

} // namespace talc
