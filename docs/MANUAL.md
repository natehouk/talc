# Talc 1.1 — User Manual

Talc is a de‑esser. It turns down harsh "s", "sh", "z" and "t" sounds (sibilance) without dulling the rest of the sound. Typical uses:

- taming sibilance on lead and backing vocals, voice‑over and podcasts
- softening bright hi‑hats, cymbals, acoustic guitar and string noise
- gentle high‑frequency limiting on groups, mixes and masters

Talc has two detection modes, wide‑band or linear‑phase split‑band processing, look‑ahead up to 15 ms, mid‑only and side‑only processing with adjustable stereo linking, and up to 4× oversampling. A scrolling display shows exactly which syllables were de‑essed.

Talc runs as VST3, AU, CLAP and VST2 on macOS, Windows and Linux.

**Contents**

1. [Installing](#1-installing)
2. [A quick tour](#2-a-quick-tour)
3. [How a de‑esser works](#3-how-a-de-esser-works)
4. [Modes](#4-modes)
5. [Main controls](#5-main-controls)
6. [The sidechain filter](#6-the-sidechain-filter)
7. [Processing: wide band and split band](#7-processing-wide-band-and-split-band)
8. [Look‑ahead, channels and oversampling](#8-look-ahead-channels-and-oversampling)
9. [The display and meters](#9-the-display-and-meters)
10. [Knobs](#10-knobs)
11. [A/B, undo and redo](#11-ab-undo-and-redo)
12. [MIDI](#12-midi)
13. [Presets](#13-presets)
14. [Help and hover tips](#14-help-and-hover-tips)
15. [Recipes](#15-recipes)
16. [Parameter reference](#16-parameter-reference)
17. [Troubleshooting](#17-troubleshooting)

---

## 1. Installing

Download the zip for your system from the project's **Releases** page and copy the plugin files:

| System | Copy | To |
|---|---|---|
| macOS | `Talc.vst3`, `Talc.component` (AU), `Talc.clap` | `~/Library/Audio/Plug-Ins/VST3`, `.../Components`, `.../CLAP` |
| Windows | the `Talc.vst3` folder, `Talc.clap` | `C:\Program Files\Common Files\VST3\`, `...\CLAP\` |
| Linux | `Talc.vst3`, `Talc.clap` | `~/.vst3/`, `~/.clap/` |

**macOS:** if the build isn't notarized and macOS blocks it, open Terminal and run:

```
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/*/Talc*
```

Rescan plugins in your DAW. Talc appears under **Epsilon Audio** as a dynamics effect, with a stereo input and output. Use it on mono tracks as usual.

---

## 2. A quick tour

![Talc](../screenshot.png)

- **Header:** preset browser, A/B and COPY, UNDO/REDO, LEARN (MIDI Learn) and MIDI on/off, HELP, and the current latency.
- **De‑essing display** (top), the last five seconds:
  - the audio waveform, mirrored around the centre line
  - in **red**, what Talc took away, so you can see which syllables were de‑essed and by how much
  - the red line from the top is the gain reduction, read on the scale at the left
- **Meters** (top right): input, gain reduction and output.
- **Left panel:** the MODE (SINGLE VOCAL / ALLROUND), the big THRESHOLD knob with the sidechain level ring around it, RANGE with a live reduction bar, and PROCESSING (WIDE BAND / SPLIT BAND).
- **Right panel:** the sidechain filter over a live spectrum, LOOKAHEAD, STEREO LINK, CHANNELS, OVERSAMPLING, DISPLAY RANGE and AUDITION.

---

## 3. How a de‑esser works

A de‑esser is a compressor that listens only to the high frequencies. Talc sends a copy of the input, the **sidechain**, through a band‑pass filter set around the sibilance (the band between the two handles in the filter display). When that band gets louder than the **threshold**, Talc turns the signal down, by up to the **range**.

- Above the threshold, Talc turns down by 0.85 dB for every dB over, with a soft 6 dB knee, up to the range.
- It reacts in about half a millisecond and lets go again in 15–50 ms. Release is program‑dependent: it lets go quickly after a short "t" and more smoothly after a long "sss", so the voice never pumps.
- Every setting moves smoothly, so you can automate or adjust anything while the music plays.

---

## 4. Modes

- **SINGLE VOCAL** is made for one voice. Talc compares the sidechain band with the whole voice: an ess is a moment where the high band dominates. Bright vowels, breathy notes and loud passages with a lot of top end don't fool it, so it can work hard without making the singer lisp. Use it on a single lead vocal, voice‑over or podcast voice.
- **ALLROUND** is a classic de‑esser: whenever the sidechain band is over the threshold, it turns down. Use it on backing vocal groups, instruments, mixes, or as a high‑frequency limiter.

---

## 5. Main controls

- **THRESHOLD** (−60 – 0 dB): where de‑essing starts. The ring around the knob shows the sidechain level: yellow while it is below the threshold, red when it crosses. The white mark on the ring is the threshold. Lower the threshold until only the esses turn the ring red.
- **RANGE** (0 – 24 dB): the most Talc will turn down. 4–8 dB sounds natural; more starts to sound lispy. The bar above the knob shows how much of the range is in use right now.

---

## 6. The sidechain filter

The filter display shows a live spectrum of the input with the sidechain band on top.

- **Drag the left handle** to set the high‑pass (1–20 kHz): Talc listens to frequencies above it. Typical: 4–5 kHz for male voices, 5–7 kHz for female voices.
- **Drag the right handle** to set the low‑pass (2–20 kHz). All the way to the right, it is off.
- **Drag between the handles** to move the whole band.
- Scroll over a handle to fine‑tune it; Cmd/Ctrl‑click a handle to reset it.
- **AUDITION** lets you hear only the sidechain band. Sweep the band until the esses are loudest, then switch AUDITION off.

The filters are 24 dB/oct, so the band is well defined but not surgical.

---

## 7. Processing: wide band and split band

- **WIDE BAND** turns the whole signal down during an ess. On a single voice this is usually the most natural choice, because the ess and its body move together. It adds no latency.
- **SPLIT BAND** turns down only the sidechain band. Talc splits the signal with **linear‑phase** filters, so the rest is untouched and the two parts add back up perfectly when nothing is being de‑essed. Best on mixes, groups and instruments. In split‑band mode the display shades the processed band in red. It adds about 5.3 ms of latency (254 samples at 48 kHz).

---

## 8. Look‑ahead, channels and oversampling

- **LOOKAHEAD** (0–15 ms) lets Talc see ahead, so it is already turning down when an ess starts. 1–3 ms catches sharp "t" and "ts" sounds. It adds that much latency; at 0 ms Talc has none.
- **CHANNELS:**
  - **STEREO** de‑esses both channels.
  - **MID ONLY** de‑esses the centre of the stereo image: perfect for a lead vocal in a mix.
  - **SIDE ONLY** de‑esses only the stereo width, for example harsh stereo cymbals or reverbs.
- **STEREO LINK** (0–100%) sets how much the left and right channels share the gain reduction. At 100% both move together and the stereo image stays put; at 0% each side is de‑essed on its own. It has no effect in MID ONLY and SIDE ONLY.
- **OVERSAMPLING** (OFF, 2×, 4×) applies the gain changes at a higher sample rate with linear‑phase filters, for cleaner very fast gain changes on bright material. It adds 32 (2×) or 48 (4×) samples of latency.

Your host compensates for all of Talc's latency automatically. The header shows the total.

---

## 9. The display and meters

- The de‑essing display scrolls over five seconds. The waveform shows the audio after de‑essing; the red edges are what was removed.
- **DISPLAY RANGE** (6, 12 or 24 dB) sets the scale of the gain‑reduction line.
- **AUDITIONING THE SIDECHAIN** appears at the bottom right while AUDITION is on.
- The meters show input peak, gain reduction and output peak; the numbers above them hold the peaks. Click the meters to reset them.

---

## 10. Knobs

- **Drag** up or down; **Shift** for fine adjustment. **Grab the rim** of a knob to turn it in a circle.
- **Mouse wheel** turns a knob.
- **Double‑click** to type a value, for example `-30`, `8`, `2.5` or `6.5k`. For the low‑pass, type `off`.
- **Cmd/Ctrl‑click** resets to the default.

---

## 11. A/B, undo and redo

- **A / B** hold two complete settings. Click the other letter to switch; the first time, B starts as a copy of A.
- **COPY** copies the current setting into the other slot.
- Every edit is an undo step, including preset loads and A/B switches.
  - Undo: Cmd/Ctrl‑Z
  - Redo: Cmd/Ctrl‑Shift‑Z or Cmd/Ctrl‑Y
- DISPLAY RANGE is a view setting: it is saved with your project, but presets, A/B and undo don't change it.

---

## 12. MIDI

- **MIDI Learn:**
  1. Click **LEARN**.
  2. Click a control.
  3. Move a knob or fader on your controller.
  4. Click **LEARN** again (or press Esc) to finish.

  Right‑click a control while learning to unassign it. **CLEAR ALL ASSIGNMENTS** removes them all. Assignments are saved with your project.
- The **MIDI** button switches learned controllers off.

---

## 13. Presets

22 factory presets in three groups, plus your own:

| Group | Presets |
|---|---|
| Vocals | Female Vocal, Male Vocal, Gentle, Heavy, Split‑Band Soft, Podcast, Rap Vocal, Backing Vocals, Lisp Tamer |
| Instruments | Acoustic Guitar, Hi‑Hats, Cymbals, Brass Bite, String Rosin, Amp Fizz |
| Mix & Master | Mix De‑Esser (Mid), HF Limiter, Soft Sides, Harsh Master, Broadcast, Unlinked Stereo |

Thresholds depend on how loud your material is, so adjust THRESHOLD first.

- **SAVE CURRENT AS…** stores your own presets, in one of these folders:
  - macOS: `~/Library/Audio/Presets/Epsilon Audio/Talc`
  - Windows: `%APPDATA%\Epsilon Audio\Talc\Presets`
  - Linux: `~/.config/epsilon-audio/talc/presets`
- To delete one of your presets, right‑click it in the browser and click again to confirm.

**Search and favourites.** With the browser open, just type to narrow the list to presets whose name or category matches; Backspace edits, Esc clears the search and then closes the browser. Click the star at the right of a preset to add it to the FAVOURITES column, and again to remove it. Favourites are kept in `favourites.txt` in your presets folder.

---

## 14. Help and hover tips

- Rest the pointer on any control, the display, the filter, its handles or the meters to see what it does and its current value.
- **HELP** opens the in‑plugin pages. The switch at the bottom of the help window turns tips on or off.
- **FULL MANUAL (WEB)** opens this manual.

---

## 15. Recipes

- **Lead vocal:**
  1. SINGLE VOCAL, WIDE BAND.
  2. Switch AUDITION on and drag the band until the esses are loudest (usually 5–7 kHz for female voices, 4–5 kHz for male voices). Switch AUDITION off.
  3. Lower THRESHOLD until the ring turns red only on the esses.
  4. RANGE 6–8 dB, LOOKAHEAD 2 ms.
- **Vocal inside a finished mix:** ALLROUND, SPLIT BAND, CHANNELS MID ONLY, a narrow band around 5–10 kHz, RANGE 3–4 dB (see *Mix De‑Esser (Mid)*).
- **Bright hi‑hats:** ALLROUND, SPLIT BAND, high‑pass around 7 kHz, LOOKAHEAD 1 ms.
- **Harsh master:** ALLROUND, SPLIT BAND, a band at 2.5–6 kHz, RANGE 2–3 dB, LOOKAHEAD 5 ms, 2× oversampling.
- **High‑frequency limiter:** ALLROUND, high‑pass 8 kHz, a high threshold (around −18 dB), 4× oversampling (see *HF Limiter*).
- **Too much de‑essing?** Raise the threshold or lower the range before anything else. In split band, a narrower band also helps.

---

## 16. Parameter reference

There are 13 parameters: 12 automatable controls and 1 view setting. New parameters are only ever added at the end, so sessions and automation keep working across versions.

- **Main:** Mode, Threshold (−60 – 0 dB), Range (0–24 dB), Processing.
- **Sidechain:** Sidechain High Pass (1–20 kHz), Sidechain Low Pass (2–20 kHz, off at the top), Audition.
- **Options:** Lookahead (0–15 ms), Stereo Link (0–100%), Channels (Stereo, Mid Only, Side Only), Oversampling (Off, 2×, 4×), MIDI.
- **View setting** (not automatable): Display Range.

Talc also reports input and output levels, gain reduction and the sidechain level to hosts that show plugin outputs.

---

## 17. Troubleshooting

- **Talc doesn't appear in my DAW.** Check the install folders above and rescan plugins. On macOS, run the `xattr` command from section 1.
- **Nothing happens.** Lower THRESHOLD until the ring around it turns red on the esses. Check that RANGE isn't at 0 dB and that the band covers the sibilance (use AUDITION).
- **The singer lisps.** Lower RANGE, raise THRESHOLD, or try SINGLE VOCAL. In ALLROUND, SPLIT BAND keeps more of the voice's body.
- **The first part of an ess slips through.** Add 1–3 ms of LOOKAHEAD.
- **The latency changed.** Split band adds about 5 ms, look‑ahead up to 15 ms and oversampling 32–48 samples. Your host compensates automatically.
