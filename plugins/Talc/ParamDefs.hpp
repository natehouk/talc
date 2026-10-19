// Parameter metadata shared by the DSP wrapper and the UI. License: MIT.
#pragma once
#include "TalcEngine.hpp"
#include <cstdio>

namespace talc {

static constexpr uint32_t kHintInteger = 1, kHintBoolean = 2, kHintLog = 4, kHintOutput = 8, kHintNoAuto = 16;
struct ParamDef { const char* name; const char* sym; const char* unit; float min, max; uint32_t hints; };

inline const ParamDef& paramDef(uint32_t i) {
    static ParamDef t[kParamCount];
    static bool done = false;
    if (!done) {
        done = true;
        auto set = [](uint32_t i, const char* n, const char* s, const char* u, float lo, float hi, uint32_t h) { t[i] = { n, s, u, lo, hi, h }; };
        const uint32_t I = kHintInteger, B = kHintBoolean | kHintInteger, O = kHintOutput, L = kHintLog;
        set(kMode,         "Mode",               "mode",      "",   0, 1, I);
        set(kThreshold,    "Threshold",          "thresh",    "dB", -60, 0, 0);
        set(kRange,        "Range",              "range",     "dB", 0, 24, 0);
        set(kProcessing,   "Processing",         "proc",      "",   0, 1, I);
        set(kHpFreq,       "Sidechain High Pass","hp",        "Hz", 1000, 20000, L);
        set(kLpFreq,       "Sidechain Low Pass", "lp",        "Hz", 2000, 20000, L);
        set(kLookahead,    "Lookahead",          "lookahead", "ms", 0, 15, 0);
        set(kLink,         "Stereo Link",        "link",      "",   0, 1, 0);
        set(kChannel,      "Channels",           "channel",   "",   0, 2, I);
        set(kOversampling, "Oversampling",       "oversamp",  "",   0, 2, I);
        set(kListen,       "Audition",           "listen",    "",   0, 1, B);
        set(kDisplayRange, "Display Range",      "disprange", "",   0, 2, I | kHintNoAuto);
        set(kMidiOn,       "MIDI",               "midion",    "",   0, 1, B);
        set(kOutLearn, "MIDI Learn",       "o_learn", "",   0, 10000000, O);
        set(kOutInL,   "Input Level L",    "o_inl",   "dB", -120, 24, O);
        set(kOutInR,   "Input Level R",    "o_inr",   "dB", -120, 24, O);
        set(kOutOutL,  "Output Level L",   "o_outl",  "dB", -120, 24, O);
        set(kOutOutR,  "Output Level R",   "o_outr",  "dB", -120, 24, O);
        set(kOutGr,    "Gain Reduction",   "o_gr",    "dB", 0, 40, O);
        set(kOutDet,   "Sidechain Level",  "o_det",   "dB", -120, 24, O);
    }
    return t[i];
}

inline const char* const* enumLabels(uint32_t p, int& count) {
    static const char* const mode[] = { "Single Vocal", "Allround" };
    static const char* const proc[] = { "Wide Band", "Split Band" };
    static const char* const ch[] = { "Stereo", "Mid Only", "Side Only" };
    static const char* const os[] = { "Off", "2x", "4x" };
    static const char* const range[] = { "6 dB", "12 dB", "24 dB" };
    switch (p) {
    case kMode: count = 2; return mode;
    case kProcessing: count = 2; return proc;
    case kChannel: count = 3; return ch;
    case kOversampling: count = 3; return os;
    case kDisplayRange: count = 3; return range;
    default: count = 0; return nullptr;
    }
}

inline float toNorm(uint32_t p, float v) {
    const ParamDef& d = paramDef(p);
    if (d.hints & kHintLog) return std::log(clampf(v, d.min, d.max) / d.min) / std::log(d.max / d.min);
    return (clampf(v, d.min, d.max) - d.min) / (d.max - d.min);
}
inline float fromNorm(uint32_t p, float n) {
    const ParamDef& d = paramDef(p);
    n = clampf(n, 0.f, 1.f);
    float v = (d.hints & kHintLog) ? d.min * std::pow(d.max / d.min, n) : d.min + n * (d.max - d.min);
    if (d.hints & kHintInteger) v = std::round(v);
    return v;
}
inline float displayRangeDb(float v) { const int k = (int)clampf(std::round(v), 0, 2); return k == 0 ? 6.f : (k == 1 ? 12.f : 24.f); }

} // namespace talc
