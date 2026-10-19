// Factory presets (each lists only what it changes from the engine defaults). License: MIT.
// Engine defaults: Single Vocal, threshold -24 dB, range 8 dB, wide band, sidechain 5.5 kHz high-pass, low-pass off,
// no look-ahead, linked, stereo, no oversampling.
#pragma once
#include "TalcEngine.hpp"
#include <algorithm>

namespace talc {

struct PV { uint32_t id; float v; };
struct PresetDef { const char* name; const char* category; const PV* pv; };
struct Preset { const char* name; const char* category; float v[kParamCount]; };

static const char* const kCategories[] = { "Vocals", "Instruments", "Mix & Master" };
static constexpr int kNumCategories = 3;

#define END { kParamCount, 0 }
namespace pv {
static const PV init[] = { END };
// ---------------------------------------------------------------- Vocals
static const PV femaleVox[] = { {kThreshold,-26}, {kRange,8}, {kHpFreq,6500}, {kLookahead,2}, END };
static const PV maleVox[]   = { {kThreshold,-26}, {kRange,8}, {kHpFreq,4500}, {kLookahead,2}, END };
static const PV gentle[]    = { {kThreshold,-22}, {kRange,4}, {kHpFreq,5500}, {kProcessing,kSplitBand}, END };
static const PV heavy[]     = { {kThreshold,-34}, {kRange,14}, {kHpFreq,5000}, {kLookahead,4}, END };
static const PV splitSoft[] = { {kThreshold,-30}, {kRange,10}, {kHpFreq,6000}, {kProcessing,kSplitBand}, {kLookahead,3}, END };
static const PV podcast[]   = { {kThreshold,-28}, {kRange,9}, {kHpFreq,4800}, {kLpFreq,11000}, {kLookahead,3}, END };
static const PV rap[]       = { {kThreshold,-24}, {kRange,10}, {kHpFreq,5200}, {kLookahead,1}, END };
static const PV backing[]   = { {kMode,kAllround}, {kThreshold,-30}, {kRange,10}, {kHpFreq,6000}, {kProcessing,kSplitBand}, END };
static const PV lispy[]     = { {kThreshold,-30}, {kRange,8}, {kHpFreq,3500}, {kLpFreq,8000}, {kProcessing,kSplitBand}, END };
// ---------------------------------------------------------------- Instruments
static const PV acousticGtr[]= { {kMode,kAllround}, {kThreshold,-30}, {kRange,6}, {kHpFreq,3500}, {kLpFreq,9000}, {kProcessing,kSplitBand}, END };
static const PV hats[]      = { {kMode,kAllround}, {kThreshold,-24}, {kRange,8}, {kHpFreq,7000}, {kProcessing,kSplitBand}, {kLookahead,1}, END };
static const PV cymbals[]   = { {kMode,kAllround}, {kThreshold,-26}, {kRange,10}, {kHpFreq,5000}, {kProcessing,kSplitBand}, {kLookahead,2}, END };
static const PV brass[]     = { {kMode,kAllround}, {kThreshold,-28}, {kRange,6}, {kHpFreq,2500}, {kLpFreq,7000}, {kProcessing,kSplitBand}, END };
static const PV strings[]   = { {kMode,kAllround}, {kThreshold,-32}, {kRange,5}, {kHpFreq,3000}, {kLpFreq,8000}, {kProcessing,kSplitBand}, END };
static const PV fizz[]      = { {kMode,kAllround}, {kThreshold,-34}, {kRange,8}, {kHpFreq,4000}, {kLpFreq,10000}, {kProcessing,kSplitBand}, END };
// ---------------------------------------------------------------- Mix & Master
static const PV mixDeEss[]  = { {kMode,kAllround}, {kThreshold,-30}, {kRange,4}, {kHpFreq,5500}, {kLpFreq,11000}, {kProcessing,kSplitBand}, {kChannel,kMidOnly}, {kLookahead,5}, END };
static const PV hfLimit[]   = { {kMode,kAllround}, {kThreshold,-18}, {kRange,6}, {kHpFreq,8000}, {kProcessing,kSplitBand}, {kLookahead,5}, {kOversampling,2}, END };
static const PV sideSoft[]  = { {kMode,kAllround}, {kThreshold,-34}, {kRange,6}, {kHpFreq,6000}, {kProcessing,kSplitBand}, {kChannel,kSideOnly}, END };
static const PV harshMaster[]= { {kMode,kAllround}, {kThreshold,-26}, {kRange,3}, {kHpFreq,2500}, {kLpFreq,6000}, {kProcessing,kSplitBand}, {kLookahead,5}, {kOversampling,1}, END };
static const PV broadcast[] = { {kMode,kAllround}, {kThreshold,-20}, {kRange,10}, {kHpFreq,6000}, {kLookahead,8}, {kOversampling,1}, END };
static const PV unlinked[]  = { {kMode,kAllround}, {kThreshold,-28}, {kRange,6}, {kHpFreq,6000}, {kProcessing,kSplitBand}, {kLink,0}, END };
} // namespace pv
#undef END

static const PresetDef kPresetDefs[] = {
    { "Init", "Init", pv::init },
    { "Female Vocal", "Vocals", pv::femaleVox }, { "Male Vocal", "Vocals", pv::maleVox }, { "Gentle", "Vocals", pv::gentle },
    { "Heavy", "Vocals", pv::heavy }, { "Split-Band Soft", "Vocals", pv::splitSoft }, { "Podcast", "Vocals", pv::podcast },
    { "Rap Vocal", "Vocals", pv::rap }, { "Backing Vocals", "Vocals", pv::backing }, { "Lisp Tamer", "Vocals", pv::lispy },
    { "Acoustic Guitar", "Instruments", pv::acousticGtr }, { "Hi-Hats", "Instruments", pv::hats }, { "Cymbals", "Instruments", pv::cymbals },
    { "Brass Bite", "Instruments", pv::brass }, { "String Rosin", "Instruments", pv::strings }, { "Amp Fizz", "Instruments", pv::fizz },
    { "Mix De-Esser (Mid)", "Mix & Master", pv::mixDeEss }, { "HF Limiter", "Mix & Master", pv::hfLimit }, { "Soft Sides", "Mix & Master", pv::sideSoft },
    { "Harsh Master", "Mix & Master", pv::harshMaster }, { "Broadcast", "Mix & Master", pv::broadcast }, { "Unlinked Stereo", "Mix & Master", pv::unlinked },
};
static constexpr uint32_t kNumPresets = sizeof(kPresetDefs) / sizeof(kPresetDefs[0]);

inline const Preset* presets() {
    static Preset table[kNumPresets];
    static bool done = false;
    if (done) return table;
    done = true;
    Engine* e = new Engine();
    for (uint32_t i = 0; i < kNumPresets; ++i) {
        table[i].name = kPresetDefs[i].name; table[i].category = kPresetDefs[i].category;
        std::copy(e->p, e->p + kParamCount, table[i].v);
        for (const PV* x = kPresetDefs[i].pv; x->id != kParamCount; ++x) table[i].v[x->id] = x->v;
    }
    delete e;
    return table;
}

} // namespace talc
