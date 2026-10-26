// Talc — hover tips and the in-plugin help pages. License: MIT.
#pragma once
#include "TalcEngine.hpp"

namespace talc {

static const char* const kManualURL = "https://github.com/natehouk/talc/blob/main/docs/MANUAL.md";

inline const char* paramTip(uint32_t p) {
    switch (p) {
    case kMode: return "SINGLE VOCAL: for one voice. Talc compares the high band with the rest of the voice and reacts only when sibilance dominates, so bright vowels and breathy notes are left alone. ALLROUND: a classic de-esser that reacts to everything in the sidechain band; for mixes, groups and instruments, or high-frequency limiting.";
    case kThreshold: return "The level where de-essing starts. The ring around the knob shows what the sidechain hears: when it passes the marker, Talc turns the sibilance down. Set it so only the esses cross it.";
    case kRange: return "The most Talc will turn the sibilance down, up to 24 dB. 4-8 dB is natural; more starts to sound lispy.";
    case kProcessing: return "WIDE BAND turns the whole signal down during an ess: the most natural on a single voice. SPLIT BAND turns down only the frequencies in the sidechain band (with linear-phase filters, so the rest is untouched); best for mixes and instruments. Split band adds about 5 ms of latency.";
    case kHpFreq: return "The bottom of the sidechain band: Talc listens to (and in split band, processes) frequencies above this. Drag the left handle in the filter display. Typical: 4-5 kHz for male voices, 5-7 kHz for female voices.";
    case kLpFreq: return "The top of the sidechain band. All the way up, it's off. Drag the right handle in the filter display.";
    case kLookahead: return "Lets Talc see up to 15 ms ahead, so it is already turning down when an ess starts. Adds that much latency (reported to your host). 0 ms is zero latency.";
    case kLink: return "How much the two channels share the gain reduction. 100%: both move together, so the stereo image stays put. 0%: each channel is de-essed on its own.";
    case kChannel: return "STEREO processes both channels. MID ONLY de-esses the centre (a lead vocal in a mix) and leaves the sides alone. SIDE ONLY de-esses only the stereo width.";
    case kOversampling: return "Applies the gain at 2x or 4x the sample rate with linear-phase filters, for cleaner very fast gain changes on bright material. Adds 32 or 48 samples of latency.";
    case kListen: return "Hear what the sidechain hears: only the band between the two filter handles. Use it to find the esses, then switch it off.";
    case kDisplayRange: return "Vertical range of the gain reduction trace in the display: 6, 12 or 24 dB.";
    case kMidiOn: return "Turns MIDI Learn on or off.";
    default: return "";
    }
}

enum TipTarget { kTipUndo, kTipRedo, kTipLearn, kTipMidi, kTipHelp, kTipA, kTipB, kTipCopy, kTipPrev, kTipPresets, kTipNext,
                 kTipDisplay, kTipFilter, kTipHandle, kTipMeters, kNumTipTargets };
inline const char* targetTitle(int t) {
    static const char* const n[kNumTipTargets] = { "Undo", "Redo", "MIDI Learn", "MIDI", "Help", "Setting A", "Setting B", "Copy A/B",
                                                   "Previous preset", "Presets", "Next preset", "De-essing display", "Sidechain filter", "Filter handle", "Meters" };
    return n[t];
}
inline const char* targetTip(int t) {
    switch (t) {
    case kTipUndo: return "Steps back through your edits, including preset loads and A/B switches. Cmd/Ctrl-Z.";
    case kTipRedo: return "Re-applies an edit you undid. Cmd/Ctrl-Shift-Z or Cmd/Ctrl-Y.";
    case kTipLearn: return "Click, then click any control and move a knob or fader on your MIDI controller to assign it. Right-click a control to unassign.";
    case kTipMidi: return paramTip(kMidiOn);
    case kTipHelp: return "Opens the help pages: modes, controls, the sidechain filter, MIDI and shortcuts.";
    case kTipA: case kTipB: return "Talc keeps two complete settings, A and B. Click the other letter to switch and compare.";
    case kTipCopy: return "Copies the current setting to the other slot.";
    case kTipPrev: return "Load the previous preset. The mouse wheel over the preset name also steps through presets.";
    case kTipPresets: return "Open the preset browser. A * after the name means you have changed it. In the browser, type to search names and categories, and click a star to keep a preset in Favourites.";
    case kTipNext: return "Load the next preset.";
    case kTipDisplay: return "The last five seconds of audio. Red shows what Talc took away: you can see exactly which syllables were de-essed and by how much. The red line from the top is the gain reduction.";
    case kTipFilter: return "The sidechain band over a live spectrum of the input: Talc listens to the frequencies between the two handles (and in split band, turns only those down). Drag a handle, or drag between them to move the whole band.";
    case kTipHandle: return "Drag to set this edge of the sidechain band. All the way to the right, the low-pass is off.";
    case kTipMeters: return "Input, gain reduction and output levels. Click to reset the peak readouts.";
    default: return "";
    }
}

struct HelpPage { const char* title; const char* body; };
static const HelpPage kHelpPages[] = {
    { "Overview",
      "Talc is a de-esser: it turns down harsh \"s\", \"sh\" and \"t\" sounds (sibilance) without dulling the rest of the sound.\n\n"
      "1. Pick SINGLE VOCAL for one voice, ALLROUND for anything else.\n"
      "2. Set the sidechain band (the filter display) around the sibilance. AUDITION lets you hear it.\n"
      "3. Lower THRESHOLD until the esses cross the marker on the ring around the knob, and the display turns red on them.\n"
      "4. Set RANGE for how much they are turned down.\n\n"
      "Hover over anything for a moment to see what it does." },
    { "Modes",
      "SINGLE VOCAL is made for a single voice. It compares the sidechain band with the whole voice: an ess is a moment when the high band dominates. "
      "Bright vowels, breathy notes and loud passages with lots of top end don't fool it, so it can work hard without making the singer lisp.\n\n"
      "ALLROUND is a classic de-esser: whenever the sidechain band is over the threshold, it turns down. Use it on mixes, groups, backing vocals, "
      "instruments (hi-hats, acoustic guitars, strings) or as a high-frequency limiter." },
    { "Processing",
      "WIDE BAND turns the whole signal down during an ess. On a single voice this is usually the most natural, because the ess and its body move together. Zero latency.\n\n"
      "SPLIT BAND turns down only the sidechain band. It uses linear-phase filters, so the rest of the signal is untouched and the two parts add back up perfectly. "
      "Best on mixes and instruments. Adds about 5 ms of latency.\n\n"
      "LOOKAHEAD (up to 15 ms) lets Talc react before an ess starts. OVERSAMPLING (2x, 4x) cleans up very fast gain changes." },
    { "Channels",
      "STEREO de-esses both channels; LINK sets how much they share the gain reduction (100% keeps the image steady).\n\n"
      "MID ONLY de-esses the centre of the stereo image: perfect for a lead vocal inside a mix. SIDE ONLY de-esses only the stereo width, for example harsh stereo cymbals or reverbs." },
    { "MIDI & shortcuts",
      "MIDI LEARN: click LEARN, click a control, move a controller. Right-click unassigns. The MIDI button switches MIDI off.\n\n"
      "A/B holds two complete settings; COPY copies the current one to the other slot. Every edit is an undo step (Cmd/Ctrl-Z, Cmd/Ctrl-Shift-Z).\n\n"
      "Double-click a knob to type a value; Cmd/Ctrl-click resets it; Shift-drag is fine adjustment. Esc closes this window." },
};
static constexpr int kNumHelpPages = sizeof(kHelpPages) / sizeof(kHelpPages[0]);

} // namespace talc
