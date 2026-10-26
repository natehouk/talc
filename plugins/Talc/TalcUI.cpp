// Talc — custom vector UI (DPF/NanoVG). Original design. License: MIT.
#include "DistrhoUI.hpp"
#include "TalcPlugin.hpp"
#include "UserPresets.hpp"
#include "ResizeHandle.hpp"
#include "Help.hpp"
#include "Fft.hpp"
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(_WIN32)
extern "C" __declspec(dllimport) void* __stdcall ShellExecuteA(void*, const char*, const char*, const char*, const char*, int);
#endif

START_NAMESPACE_DISTRHO

static void openURL(const char* url) {
#if defined(_WIN32)
    ShellExecuteA(nullptr, "open", url, nullptr, nullptr, 1);
#elif defined(__APPLE__)
    const std::string c = std::string("open '") + url + "' &"; if (std::system(c.c_str())) {}
#else
    const std::string c = std::string("xdg-open '") + url + "' >/dev/null 2>&1 &"; if (std::system(c.c_str())) {}
#endif
}
static bool gTipsOn = true;
static double nowSec() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

using namespace talc;
using DGL_NAMESPACE::ResizeHandle;

static constexpr float W = 1000.f, H = 660.f;

#define C_BG        Color(0x11, 0x11, 0x14)
#define C_PANEL     Color(0x19, 0x19, 0x1e)
#define C_PANEL_HI  Color(0x26, 0x26, 0x2d)
#define C_LINE      Color(0x32, 0x32, 0x3b)
#define C_TEXT      Color(0xec, 0xea, 0xf0)
#define C_DIM       Color(0x90, 0x8e, 0x9a)
#define C_FAINT     Color(0x50, 0x4e, 0x5a)
#define C_ACC       Color(0xd9, 0xd0, 0xea)       // talc: soft pearl
#define C_WAVE      Color(0xb4, 0xac, 0xc8)
#define C_CUT       Color(0xf0, 0x60, 0x6a)
#define C_DET       Color(0xf0, 0xd0, 0x6a)
#define C_DISPLAY   Color(0x0b, 0x0b, 0x0e)
#define C_FAV       Color(0xff, 0xc8, 0x5a)       // favourite star

static Color withAlpha(const Color& c, float a) { Color r(c); r.alpha = a; return r; }

enum WType { Knob, Toggle, Segment };
struct Ctl { WType type; uint32_t param; float x, y, r; const char* label; };    // Segment: r = width

// ---------------------------------------------------------------- layout
static constexpr float LX = 16, LY = 64, LW = 880, LH = 268;          // de-essing display
static constexpr float MX = 904, MY = 64, MW = 80, MH = 268;          // meters
static constexpr float KX = 16, KY = 344, KW = 440, KH = 300;         // main controls
static constexpr float FX = 464, FY = 344, FW = 520, FH = 300;        // sidechain / options
static constexpr float GX = FX + 16, GY = FY + 12, GW = FW - 32, GH = 150;   // sidechain filter display
static constexpr float SEG_H = 24;
static constexpr float F_LO = 200.f, F_HI = 22000.f;
static constexpr int SPEC_N = 244;

static std::vector<Ctl> buildControls() {
    std::vector<Ctl> c;
    c.push_back({ Segment, kMode, KX + KW / 2, KY + 34, 400, "" });
    c.push_back({ Knob, kThreshold, KX + 124, KY + 152, 50, "THRESHOLD" });
    c.push_back({ Knob, kRange, KX + 324, KY + 152, 38, "RANGE" });
    c.push_back({ Segment, kProcessing, KX + KW / 2, KY + 270, 400, "" });
    c.push_back({ Knob, kLookahead, FX + 52, FY + 196, 18, "LOOKAHEAD" });
    c.push_back({ Knob, kLink, FX + 140, FY + 196, 18, "STEREO LINK" });
    c.push_back({ Segment, kChannel, FX + 352, FY + 196, 288, "" });
    c.push_back({ Segment, kOversampling, FX + 100, FY + 278, 168, "" });
    c.push_back({ Segment, kDisplayRange, FX + 278, FY + 278, 150, "" });
    c.push_back({ Toggle, kListen, FX + 394, FY + 278, 0, "AUDITION" });
    return c;
}
static const std::vector<Ctl>& controls() { static std::vector<Ctl> c = buildControls(); return c; }

static const char* const* segLabels(uint32_t p, int& n) {
    static const char* const mode[] = { "SINGLE VOCAL", "ALLROUND" };
    static const char* const proc[] = { "WIDE BAND", "SPLIT BAND" };
    static const char* const ch[] = { "STEREO", "MID ONLY", "SIDE ONLY" };
    static const char* const os[] = { "OFF", "2X", "4X" };
    static const char* const range[] = { "6 DB", "12 DB", "24 DB" };
    switch (p) {
    case kMode: n = 2; return mode;
    case kProcessing: n = 2; return proc;
    case kChannel: n = 3; return ch;
    case kOversampling: n = 3; return os;
    case kDisplayRange: n = 3; return range;
    default: n = 0; return nullptr;
    }
}

static constexpr float PS_X = 290.f, PS_Y = 20.f, PS_W = 220.f, PS_H = 28.f, PS_ARROW = 28.f;
static constexpr float A_X = 520.f, AB_W = 30.f, COPY_X = 584.f, COPY_W = 46.f;
static constexpr float UNDO_X = 640.f, REDO_X = 692.f, UR_W = 48.f, LEARN_X = 750.f, LEARN_W = 66.f, MIDI_X = 820.f, MIDI_W = 48.f, HELP_X = 874.f, HELP_W = 48.f;
static constexpr float PM_COLW = 160.f, PM_ROW = 23.f, PM_HEAD = 22.f, PM_PAD = 8.f;
static constexpr int HIST = 1000;                  // 5 s at 200 points per second

class TalcUI : public UI {
public:
    TalcUI() : UI((uint)W, (uint)H), resizeHandle(this) {
        loadSharedResources();
        Engine* def = new Engine();
        for (uint32_t i = 0; i < kParamCount; ++i) { val[i] = def->p[i]; defv[i] = def->p[i]; }
        delete def;
        val[kOutInL] = val[kOutInR] = val[kOutOutL] = val[kOutOutR] = val[kOutDet] = -120; val[kOutGr] = 0;
        for (int& m : paramCC) m = -1;
        const double sf = getScaleFactor();
        if (sf != 1.0) setSize((uint)(W * sf), (uint)(H * sf));
        setGeometryConstraints((uint)(W * 0.6 * sf), (uint)(H * 0.6 * sf), true);
        if (isResizable()) resizeHandle.hide();
        userPresets = listUserPresets();
        std::copy(val, val + kParamCount, before);
        for (int i = 0; i < HIST; ++i) { hPk[i] = 0; hG[i] = 0; }
        for (int i = 0; i < SPEC_N; ++i) spec[i] = -120;
        fft.init(kFftN); fbuf.resize(kFftN);
        resetPeaks();
    }

protected:
    static constexpr int kFftN = 2048;
    float uiScale() const { return (float)getWidth() / W; }
    static bool isView(uint32_t p) { return (paramDef(p).hints & kHintNoAuto) != 0; }
    static bool isOut(uint32_t p) { return (paramDef(p).hints & kHintOutput) != 0; }

    void setValue(uint32_t p, float v) {
        const ParamDef& d = paramDef(p);
        v = clampf(v, d.min, d.max);
        if (d.hints & kHintInteger) v = std::round(v);
        if (v == val[p]) return;
        val[p] = v; setParameterValue(p, v); repaint();
    }
    void commit(uint32_t p, float v) { editParameter(p, true); setValue(p, v); editParameter(p, false); }

    void parameterChanged(uint32_t i, float v) override {
        if (i >= kParamCount) return;
        if (i == kOutLearn) {
            if (v != lastLearnEcho && v > 0) {
                const int code = (int)v % 10000 - 1, prm = code / 128, cc = code % 128;
                for (int& m : paramCC) if (m == cc) m = -1;
                if (prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
                learnWaiting = -1;
            }
            lastLearnEcho = v; repaint(); return;
        }
        if (isOut(i)) {
            val[i] = v;
            if (i == kOutInL || i == kOutInR) pkIn = std::max(pkIn, v);
            if (i == kOutOutL || i == kOutOutR) pkOut = std::max(pkOut, v);
            return;
        }
        if (val[i] != v) { val[i] = v; if (!gesture) before[i] = v; repaint(); }
    }
    void stateChanged(const char* key, const char* value) override {
        if (std::strcmp(key, "midimap") != 0) return;
        for (int& m : paramCC) m = -1;
        const char* q = value;
        while (q && *q) {
            int cc = -1, prm = -1;
            if (std::sscanf(q, "%d:%d", &cc, &prm) == 2 && prm >= 0 && prm < (int)kParamCount) paramCC[prm] = cc;
            const char* s = std::strchr(q, ';'); if (!s) break; q = s + 1;
        }
        repaint();
    }
    void resetPeaks() { pkIn = pkOut = -120; }

    // ---- undo / redo / A-B (view settings are not part of a setting) -------------
    typedef std::vector<float> Snap;
    Snap snapshot() const { Snap s(val, val + kParamCount); s.push_back((float)abSlot); return s; }
    bool differs(const float* a, const float* b) const {
        for (uint32_t i = 0; i < kParamCount; ++i) if (!isOut(i) && !isView(i) && a[i] != b[i]) return true;
        return false;
    }
    void beginGesture() { gesture = true; std::copy(val, val + kParamCount, before); }
    void endGesture() {
        gesture = false;
        if (differs(before, val)) {
            Snap u(before, before + kParamCount); u.push_back((float)abSlot);
            undoStack.push_back(u); if (undoStack.size() > 200) undoStack.erase(undoStack.begin());
            redoStack.clear();
        }
        std::copy(val, val + kParamCount, before); repaint();
    }
    void applyAll(const float* s) { for (uint32_t i = 0; i < kParamCount; ++i) if (!isOut(i) && !isView(i) && val[i] != s[i]) commit(i, s[i]); }
    void restoreSnap(const Snap& s) {
        if (s.size() > kParamCount && (int)s[kParamCount] != abSlot) { abStore[abSlot] = snapshot(); abSlot = (int)s[kParamCount]; }
        applyAll(s.data()); std::copy(val, val + kParamCount, before);
    }
    void undo() { if (undoStack.empty()) return; redoStack.push_back(snapshot()); restoreSnap(undoStack.back()); undoStack.pop_back(); flash("Undo"); }
    void redo() { if (redoStack.empty()) return; undoStack.push_back(snapshot()); restoreSnap(redoStack.back()); redoStack.pop_back(); flash("Redo"); }
    void switchAB(int to) {
        if (to == abSlot) return;
        abStore[abSlot] = snapshot(); if (abStore[to].empty()) abStore[to] = snapshot();
        beginGesture(); applyAll(abStore[to].data()); endGesture();
        abSlot = to; flash(to == 0 ? "Setting A" : "Setting B");
    }
    void copyAB() { abStore[1 - abSlot] = snapshot(); flash(abSlot == 0 ? "Copied A to B" : "Copied B to A"); }

    // ---- formatting ---------------------------------------------------------
    bool lpOff() const { return val[kLpFreq] >= kLpOpen; }
    bool split() const { return val[kProcessing] > 0.5f; }
    static void fmtFreq(float v, char* b, size_t n) { if (v >= 1000) snprintf(b, n, v >= 10000 ? "%.1f kHz" : "%.2f kHz", v / 1000); else snprintf(b, n, "%.0f Hz", v); }
    void fmt(uint32_t p, char* b, size_t n) const {
        const float v = val[p];
        int ne = 0;
        if (const char* const* l = enumLabels(p, ne)) { snprintf(b, n, "%s", l[std::max(0, std::min(ne - 1, (int)std::round(v)))]); return; }
        const ParamDef& d = paramDef(p);
        if (d.hints & kHintBoolean) { snprintf(b, n, "%s", v > 0.5f ? "On" : "Off"); return; }
        switch (p) {
        case kThreshold: case kRange: snprintf(b, n, "%.1f dB", v); return;
        case kLpFreq: if (v >= kLpOpen) { snprintf(b, n, "Off"); return; } fmtFreq(v, b, n); return;
        case kHpFreq: fmtFreq(v, b, n); return;
        case kLookahead: if (v < 0.05f) snprintf(b, n, "0 ms"); else snprintf(b, n, "%.1f ms", v); return;
        default: snprintf(b, n, "%.0f%%", v * 100);
        }
    }
    bool parse(uint32_t p, const std::string& in, float& out) const {
        std::string s; for (char ch : in) if (!std::isspace((unsigned char)ch)) s += (char)std::tolower((unsigned char)ch);
        if (s.empty()) return false;
        if (p == kLpFreq && s == "off") { out = 20000; return true; }
        char* end = nullptr; double x = std::strtod(s.c_str(), &end);
        if (end == s.c_str()) return false;
        const std::string suf(end);
        if (suf == "k" || suf == "khz") x *= 1000;
        if (p == kLink) x /= 100.0;
        out = (float)x; return true;
    }

    // ---- drawing ------------------------------------------------------------
    void onNanoDisplay() override {
        const float s = uiScale();
        save(); scale(s, s);
        beginPath(); rect(0, 0, W, H); fillColor(C_BG); fill();
        fontFace(NANOVG_DEJAVU_SANS_TTF);
        drawHeader();
        drawDisplay();
        drawMeters();
        panel(KX, KY, KW, KH); panel(FX, FY, FW, FH);
        drawLeftPanel(); drawRightPanel();
        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) drawCtl(cs[i], i == active || i == hover);
        if (editing >= 0) drawEditBox();
        drawStatus();
        if (menuOpen) drawPresetMenu();
        if (saving) drawSaveBox();
        if (helpOpen) drawHelp();
        else if (tipShown && gTipsOn) drawTooltip();
        restore();
    }
    bool dimmed(const Ctl& c) const {
        if (c.param == kLink) return val[kChannel] > 0.5f;          // mid only / side only process one signal
        return false;
    }
    void panel(float x, float y, float w, float h) { beginPath(); roundedRect(x, y, w, h, 6); fillColor(C_PANEL); fill(); strokeColor(C_LINE); strokeWidth(1); stroke(); }
    void displayBox(float x, float y, float w, float h) { beginPath(); roundedRect(x, y, w, h, 6); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); strokeWidth(1); stroke(); }
    void caption(float cx, float y, const char* t, int align = ALIGN_CENTER) { fontSize(7.5f); textLetterSpacing(1.8f); textAlign(align | ALIGN_MIDDLE); fillColor(C_FAINT); text(cx, y, t, nullptr); }
    void drawHeaderButton(float x, float w, const char* label, bool on, bool hot, bool disabled) {
        beginPath(); roundedRect(x, PS_Y, w, PS_H, 5); fillColor(on ? withAlpha(C_ACC, 0.2f) : C_PANEL); fill();
        strokeColor(on ? C_ACC : (hot && !disabled ? C_DIM : C_LINE)); strokeWidth(1); stroke();
        fontSize(9); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(disabled ? C_FAINT : (on ? C_TEXT : C_DIM));
        text(x + w / 2, PS_Y + PS_H / 2 + 0.5f, label, nullptr);
    }
    void drawHeader() {
        // logo: three stacked flakes of talc
        for (int k = 0; k < 3; ++k) {
            const float x = 16 + k * 4.f, y = 18 + k * 9.f;
            beginPath(); moveTo(x + 4, y); lineTo(x + 28, y + 2); lineTo(x + 24, y + 11); lineTo(x, y + 9); closePath();
            fillColor(withAlpha(C_ACC, 0.30f + 0.22f * k)); fill();
            strokeColor(withAlpha(C_TEXT, 0.35f)); strokeWidth(0.8f); stroke();
        }
        fontSize(22); textLetterSpacing(9); fillColor(C_TEXT); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); text(56, 34, "TALC", nullptr);
        fontSize(9.5f); textLetterSpacing(2.5f); fillColor(C_DIM); text(178, 29, "DE-ESSER", nullptr);
        fontSize(7); textLetterSpacing(1.1f); fillColor(C_FAINT); text(178, 41, "EPSILON AUDIO 1.1", nullptr);
        drawPresetBar();
        for (int k = 0; k < 2; ++k) {
            const float x = A_X + k * AB_W; const bool on = abSlot == k, hot = hoverHeader == 6 + k;
            beginPath(); roundedRect(x, PS_Y, AB_W - 2, PS_H, 5); fillColor(on ? withAlpha(C_ACC, 0.25f) : C_PANEL); fill();
            strokeColor(on ? C_ACC : (hot ? C_DIM : C_LINE)); strokeWidth(1); stroke();
            fontSize(10); textLetterSpacing(0); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(on ? C_TEXT : C_DIM);
            text(x + (AB_W - 2) / 2, PS_Y + PS_H / 2 + 0.5f, k == 0 ? "A" : "B", nullptr);
        }
        drawHeaderButton(COPY_X, COPY_W, "COPY", false, hoverHeader == 8, false);
        drawHeaderButton(UNDO_X, UR_W, "UNDO", false, hoverHeader == 3, undoStack.empty());
        drawHeaderButton(REDO_X, UR_W, "REDO", false, hoverHeader == 4, redoStack.empty());
        drawHeaderButton(LEARN_X, LEARN_W, learnMode ? "LEARNING" : "LEARN", learnMode, hoverHeader == 1, false);
        drawHeaderButton(MIDI_X, MIDI_W, "MIDI", val[kMidiOn] > 0.5f, hoverHeader == 2, false);
        drawHeaderButton(HELP_X, HELP_W, "HELP", helpOpen, hoverHeader == 5, false);
        { char b[24]; snprintf(b, sizeof b, "%.1f ms", latencyMs);
          fontSize(7); textLetterSpacing(1.4f); textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(C_FAINT); text(W - 16, 28, "LATENCY", nullptr);
          fontSize(9.5f); textLetterSpacing(0.4f); fillColor(C_DIM); text(W - 16, 41, b, nullptr); }
    }

    // ---- de-essing display: the last five seconds, waveform mirrored around the centre ------
    float rangeDb() const { return displayRangeDb(val[kDisplayRange]); }
    static float waveNorm(float pk) { return clampf(std::pow(std::max(pk, 0.f), 0.75f), 0.f, 1.f); }    // between linear and log: de-essing stays visible
    void drawDisplay() {
        displayBox(LX, LY, LW, LH);
        const float x0 = LX + 40, x1 = LX + LW - 10, top = LY + 12, bot = LY + LH - 24, mid = (top + bot) / 2, half = (bot - top) / 2, dx = (x1 - x0) / (HIST - 1);
        const float R = rangeDb();
        beginPath();
        for (int db = -6; db >= -24; db -= 6) { const float o = half * waveNorm(dbToGain((float)db)); moveTo(x0, mid - o); lineTo(x1, mid - o); moveTo(x0, mid + o); lineTo(x1, mid + o); }
        strokeColor(Color(255, 255, 255, 7 / 255.f)); strokeWidth(1); stroke();
        beginPath(); moveTo(x0, mid); lineTo(x1, mid); strokeColor(Color(255, 255, 255, 16 / 255.f)); stroke();
        // gain reduction scale (left)
        fontSize(8); textLetterSpacing(0.5f); textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(withAlpha(C_CUT, 0.6f));
        for (int k = 0; k <= 4; ++k) {
            const float y = top + (bot - top) * k / 4.f; char b[12]; snprintf(b, sizeof b, k == 0 ? "0" : (R / 4 < 2 ? "-%.1f" : "-%.0f"), R * k / 4);
            text(x0 - 8, y, b, nullptr);
        }
        auto idx = [&](int k) { return (hPos + k) % HIST; };
        scissor(LX + 1, LY + 1, LW - 2, LH - 2);
        auto envelope = [&](bool after) {
            beginPath();
            for (int k = 0; k < HIST; ++k) { const int i = idx(k); const float pk = after ? hPk[i] * dbToGain(-hG[i]) : hPk[i];
                const float y = mid - half * waveNorm(pk); if (!k) moveTo(x0, y); else lineTo(x0 + k * dx, y); }
            for (int k = HIST - 1; k >= 0; --k) { const int i = idx(k); const float pk = after ? hPk[i] * dbToGain(-hG[i]) : hPk[i];
                lineTo(x0 + k * dx, mid + half * waveNorm(pk)); }
            closePath();
        };
        envelope(false); fillColor(withAlpha(C_CUT, 0.85f)); fill();                    // what was taken away shows through in red
        envelope(true); fillColor(Color(0x9a, 0x93, 0xb0)); fill();
        // gain reduction from the top
        beginPath(); moveTo(x0, top);
        for (int k = 0; k < HIST; ++k) lineTo(x0 + k * dx, top + (bot - top) * clampf(hG[idx(k)] / R, 0.f, 1.f));
        lineTo(x1, top); closePath(); fillColor(withAlpha(C_CUT, 0.10f)); fill();
        beginPath(); for (int k = 0; k < HIST; ++k) { const float y = top + (bot - top) * clampf(hG[idx(k)] / R, 0.f, 1.f); if (!k) moveTo(x0, y); else lineTo(x0 + k * dx, y); }
        strokeColor(C_CUT); strokeWidth(1.4f); stroke();
        resetScissor();
        fontSize(8); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_BOTTOM);
        fillColor(C_WAVE); text(x0, LY + LH - 6, "AUDIO", nullptr);
        fillColor(C_CUT); text(x0 + 56, LY + LH - 6, "REMOVED", nullptr);
        fillColor(withAlpha(C_CUT, 0.7f)); text(x0 + 130, LY + LH - 6, "GAIN REDUCTION (DB, LEFT SCALE)", nullptr);
        textAlign(ALIGN_RIGHT | ALIGN_BOTTOM);
        if (val[kListen] > 0.5f) { fillColor(C_DET); text(x1, LY + LH - 6, "AUDITIONING THE SIDECHAIN", nullptr); }
        else { fillColor(C_FAINT); text(x1, LY + LH - 6, "LAST 5 SECONDS", nullptr); }
    }
    void drawMeters() {
        displayBox(MX, MY, MW, MH);
        const float top = MY + 40, bot = MY + MH - 22, h = bot - top, R = rangeDb();
        auto y = [&](float db) { return top + h * clampf(-db / 60.f, 0.f, 1.f); };
        auto bar = [&](float x, float db, const Color& c) {
            beginPath(); rect(x, top, 8, h); fillColor(C_PANEL_HI); fill();
            if (db > -60) { const float yy = y(db); beginPath(); rect(x, yy, 8, bot - yy); fillColor(c); fill(); }
            if (db > 0.01f) { beginPath(); rect(x, top, 8, 3); fillColor(C_CUT); fill(); }
        };
        bar(MX + 8, val[kOutInL], Color(0x7a, 0x78, 0x86)); bar(MX + 18, val[kOutInR], Color(0x7a, 0x78, 0x86));
        { const float x = MX + 36, g = val[kOutGr]; beginPath(); rect(x, top, 8, h); fillColor(C_PANEL_HI); fill();
          const float gh = h * clampf(g / R, 0.f, 1.f); if (gh > 0.5f) { beginPath(); rect(x, top, 8, gh); fillColor(C_CUT); fill(); } }
        bar(MX + 54, val[kOutOutL], C_ACC); bar(MX + 64, val[kOutOutR], C_ACC);
        fontSize(7.5f); textLetterSpacing(1.2f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_FAINT);
        text(MX + 17, MY + 12, "IN", nullptr); text(MX + 40, MY + 12, "GR", nullptr); text(MX + 63, MY + 12, "OUT", nullptr);
        char b[16]; fontSize(8.5f); textLetterSpacing(0.2f);
        snprintf(b, sizeof b, "%.1f", pkIn); fillColor(pkIn > 0 ? C_CUT : C_DIM); text(MX + 17, MY + 27, pkIn > -100 ? b : "-inf", nullptr);
        snprintf(b, sizeof b, "%.1f", val[kOutGr]); fillColor(val[kOutGr] > 0.5f ? C_CUT : C_DIM); text(MX + 40, MY + 27, b, nullptr);
        snprintf(b, sizeof b, "%.1f", pkOut); fillColor(pkOut > 0 ? C_CUT : C_ACC); text(MX + 63, MY + 27, pkOut > -100 ? b : "-inf", nullptr);
        fontSize(7); fillColor(C_FAINT); text(MX + MW / 2, MY + MH - 10, "dBFS", nullptr);
    }
    void drawLeftPanel() {
        // the sidechain level, as a ring around the threshold knob, with the threshold marked on it
        const Ctl& t = controls()[1];
        const float a0 = 0.75f * 3.14159265f, a1 = 2.25f * 3.14159265f, rr = t.r + 11;
        beginPath(); arc(t.x, t.y, rr, a0, a1, NanoVG::CW); strokeColor(C_PANEL_HI); strokeWidth(4); lineCap(ROUND); stroke();
        if (detDb > -60.f) {
            const float n = toNorm(kThreshold, clampf(detDb, -60.f, 0.f)), ang = a0 + (a1 - a0) * n;
            if (ang > a0 + 0.01f) { beginPath(); arc(t.x, t.y, rr, a0, ang, NanoVG::CW); strokeColor(withAlpha(detDb > val[kThreshold] ? C_CUT : C_DET, 0.9f)); strokeWidth(4); stroke(); }
        }
        const float ta = a0 + (a1 - a0) * toNorm(kThreshold, val[kThreshold]);
        beginPath(); moveTo(t.x + std::cos(ta) * (rr - 5), t.y + std::sin(ta) * (rr - 5)); lineTo(t.x + std::cos(ta) * (rr + 5), t.y + std::sin(ta) * (rr + 5));
        strokeColor(C_TEXT); strokeWidth(2); lineCap(ROUND); stroke();
        // live gain reduction readout under the range knob
        const Ctl& r = controls()[2];
        beginPath(); rect(r.x - 40, r.y - r.r - 30, 80, 4); fillColor(C_PANEL_HI); fill();
        const float g = clampf(grNow / std::max(0.5f, val[kRange]), 0.f, 1.f);
        if (g > 0.01f) { beginPath(); rect(r.x - 40, r.y - r.r - 30, 80 * g, 4); fillColor(C_CUT); fill(); }
        caption(r.x, r.y - r.r - 38, "REDUCTION");
        beginPath(); moveTo(KX + 14, KY + 62); lineTo(KX + KW - 14, KY + 62); moveTo(KX + 14, KY + 244); lineTo(KX + KW - 14, KY + 244); strokeColor(C_LINE); strokeWidth(1); stroke();
    }
    void drawRightPanel() {
        drawFilter();
        caption(FX + 352, FY + 176, "CHANNELS"); caption(FX + 100, FY + 258, "OVERSAMPLING"); caption(FX + 278, FY + 258, "DISPLAY RANGE");
        beginPath(); moveTo(FX + 14, FY + 244); lineTo(FX + FW - 14, FY + 244); strokeColor(C_LINE); strokeWidth(1); stroke();
    }

    // ---- sidechain filter display ---------------------------------------------------
    static float fx(float f) { return GX + GW * std::log(clampf(f, F_LO, F_HI) / F_LO) / std::log(F_HI / F_LO); }
    static float fOfX(float x) { return F_LO * std::pow(F_HI / F_LO, clampf((x - GX) / GW, 0.f, 1.f)); }
    float hpX() const { return fx(val[kHpFreq]); }
    float lpX() const { return lpOff() ? GX + GW - 8 : fx(val[kLpFreq]); }
    float bandMag(float f) const {
        float m = 1.f / (1.f + std::pow(val[kHpFreq] / std::max(f, 1.f), 8.f));
        if (!lpOff()) m *= 1.f / (1.f + std::pow(f / val[kLpFreq], 8.f));
        return m;
    }
    void drawFilter() {
        displayBox(GX, GY, GW, GH);
        const float top = GY + 22, bot = GY + GH - 16;
        static const float grid[] = { 500, 1000, 2000, 5000, 10000, 20000 };
        static const char* const gl[] = { "500", "1k", "2k", "5k", "10k", "20k" };
        beginPath(); for (float f : grid) { moveTo(fx(f), GY + 4); lineTo(fx(f), bot); } strokeColor(Color(255, 255, 255, 8 / 255.f)); strokeWidth(1); stroke();
        fontSize(7.5f); textLetterSpacing(0.6f); textAlign(ALIGN_CENTER | ALIGN_BOTTOM); fillColor(C_FAINT);
        for (int i = 0; i < 6; ++i) text(fx(grid[i]), GY + GH - 3, gl[i], nullptr);
        scissor(GX + 1, GY + 1, GW - 2, GH - 2);
        // live spectrum of the input
        const float sx = GW / (SPEC_N - 1);
        auto sy = [&](float db) { return top + (bot - top) * clampf(-db / 84.f, 0.f, 1.f); };
        beginPath(); moveTo(GX, bot); for (int i = 0; i < SPEC_N; ++i) lineTo(GX + i * sx, sy(spec[i])); lineTo(GX + GW, bot); closePath();
        fillPaint(linearGradient(0, top, 0, bot, Color(255, 255, 255, 40 / 255.f), Color(255, 255, 255, 6 / 255.f))); fill();
        beginPath(); for (int i = 0; i < SPEC_N; ++i) { const float y = sy(spec[i]); if (!i) moveTo(GX, y); else lineTo(GX + i * sx, y); }
        strokeColor(withAlpha(C_DIM, 0.6f)); strokeWidth(1); stroke();
        // the band
        const float hx = hpX(), lx = lpX();
        beginPath(); rect(hx, GY, lx - hx, GH); fillColor(withAlpha(C_ACC, 0.07f)); fill();
        auto by = [&](float f) { const float db = 20.f * std::log10(std::max(bandMag(f), 1e-5f)); return top + (bot - top) * clampf(-db / 36.f, 0.f, 1.f); };
        beginPath(); for (int i = 0; i <= 240; ++i) { const float x = GX + GW * i / 240.f, y = by(fOfX(x)); if (!i) moveTo(x, y); else lineTo(x, y); }
        if (split()) {
            lineTo(GX + GW, bot); lineTo(GX, bot); closePath(); fillColor(withAlpha(C_CUT, 0.14f)); fill();
            beginPath(); for (int i = 0; i <= 240; ++i) { const float x = GX + GW * i / 240.f, y = by(fOfX(x)); if (!i) moveTo(x, y); else lineTo(x, y); }
        }
        strokeColor(C_ACC); strokeWidth(1.6f); stroke();
        resetScissor();
        // handles
        for (int k = 1; k <= 2; ++k) {
            const float x = k == 1 ? hx : lx; const bool hot = dragHandle == k || hoverHandle == k || ((dragHandle == 3 || hoverHandle == 3) && !(k == 2 && lpOff()));
            beginPath(); moveTo(x, GY + 14); lineTo(x, bot); strokeColor(withAlpha(C_ACC, hot ? 0.8f : 0.35f)); strokeWidth(1); stroke();
            beginPath(); circle(x, GY + 14, 6); fillColor(hot ? C_TEXT : C_ACC); fill(); strokeColor(C_DISPLAY); strokeWidth(1.5f); stroke();
            char b[24]; fmt(k == 1 ? kHpFreq : kLpFreq, b, sizeof b);
            fontSize(8.5f); textLetterSpacing(0.3f); fillColor(hot ? C_TEXT : C_DIM);
            const bool right = k == 1 ? x < GX + GW - 70 : x < GX + 70;
            textAlign((right ? ALIGN_LEFT : ALIGN_RIGHT) | ALIGN_MIDDLE); text(x + (right ? 10 : -10), GY + 14.5f, b, nullptr);
        }
        fontSize(7.5f); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_FAINT);
        text(GX + 8, GY + 26, split() ? "SIDECHAIN  ·  PROCESSED BAND" : "SIDECHAIN", nullptr);
    }
    void updateSpectrum(const Engine& e) {
        const uint32_t w = e.anaWrite.load();
        for (int i = 0; i < kFftN; ++i) {
            const float win = 0.5f - 0.5f * std::cos(2.f * 3.14159265f * i / (kFftN - 1));
            fbuf[i] = cpx(e.ana[(w - kFftN + i) & (Engine::kAna - 1)] * win, 0.f);
        }
        fft.run(fbuf.data(), false);
        const float fs = std::max(8000.f, e.sampleRate()), binHz = fs / kFftN;
        for (int i = 0; i < SPEC_N; ++i) {
            const float f0 = fOfX(GX + GW * (i - 0.5f) / (SPEC_N - 1)), f1 = fOfX(GX + GW * (i + 0.5f) / (SPEC_N - 1));
            int b0 = (int)(f0 / binHz), b1 = std::max(b0 + 1, (int)(f1 / binHz)); b1 = std::min(b1, kFftN / 2);
            float m = 0; for (int b = b0; b < b1; ++b) m = std::max(m, std::abs(fbuf[b]));
            const float fc = std::sqrt(f0 * f1);
            float db = 20.f * std::log10(m * 4.f / kFftN + 1e-9f) + 3.f * std::log2(fc / 1000.f);   // +3 dB/octave tilt
            spec[i] = db > spec[i] ? db : spec[i] + (db - spec[i]) * 0.25f;
        }
    }
    void drawKnobBody(float x, float y, float r, float n, bool bipolar, bool hot, bool dim, const Color& col) {
        const float a0 = 0.75f * 3.14159265f, a1 = 2.25f * 3.14159265f, ang = a0 + (a1 - a0) * n;
        const float tw = r > 30 ? 6.f : (r > 18 ? 4.f : 3.f);
        beginPath(); arc(x, y, r, a0, a1, NanoVG::CW); strokeColor(C_LINE); strokeWidth(tw); lineCap(ROUND); stroke();
        const float from = bipolar ? (a0 + a1) / 2 : a0;
        if (std::fabs(ang - from) > 0.001f) { beginPath(); arc(x, y, r, std::min(from, ang), std::max(from, ang), NanoVG::CW); strokeColor(dim ? C_FAINT : col); stroke(); }
        const float cr = r - (r > 30 ? 12 : (r > 18 ? 8 : 6));
        beginPath(); circle(x, y, cr);
        fillPaint(radialGradient(x - cr * 0.3f, y - cr * 0.4f, 0, cr * 1.4f, hot ? Color(0x3a, 0x3f, 0x4a) : C_PANEL_HI, Color(0x13, 0x14, 0x17)));
        fill(); strokeColor(Color(255, 255, 255, 18 / 255.f)); strokeWidth(1); stroke();
        beginPath(); moveTo(x + std::cos(ang) * cr * 0.35f, y + std::sin(ang) * cr * 0.35f); lineTo(x + std::cos(ang) * cr * 0.9f, y + std::sin(ang) * cr * 0.9f);
        strokeColor(dim ? C_DIM : C_TEXT); strokeWidth(r > 30 ? 3.f : 2.f); lineCap(ROUND); stroke();
    }
    void drawCtl(const Ctl& c, bool hot) {
        const uint32_t p = c.param; char buf[40]; const bool dim = dimmed(c);
        switch (c.type) {
        case Knob: {
            const bool bip = false;
            drawKnobBody(c.x, c.y, c.r, toNorm(p, val[p]), bip, hot, dim, p == kRange ? C_CUT : C_ACC);
            const bool big = c.r >= 30;
            fontSize(big ? 9.f : 8.f); textLetterSpacing(big ? 1.8f : 1.4f); textAlign(ALIGN_CENTER | ALIGN_TOP);
            fillColor(dim ? C_FAINT : C_DIM); text(c.x, c.y + c.r + 6, c.label, nullptr);
            fmt(p, buf, sizeof buf); fontSize(big ? 11.5f : 10.f); textLetterSpacing(0.5f); fillColor(dim ? C_FAINT : (hot ? C_ACC : Color(0xc8, 0xcc, 0xd6)));
            text(c.x, c.y + c.r + (big ? 19 : 17), buf, nullptr);
            break; }
        case Toggle: {
            const bool on = val[p] > 0.5f;
            const float w = 30, h = 16, x = c.x - w / 2, y = c.y - h / 2;
            beginPath(); roundedRect(x, y, w, h, h / 2); fillColor(on ? withAlpha(C_ACC, 0.25f) : C_PANEL_HI); fill(); strokeColor(on ? C_ACC : C_LINE); strokeWidth(1); stroke();
            beginPath(); circle(on ? x + w - h / 2 : x + h / 2, c.y, 5); fillColor(on ? C_ACC : C_DIM); fill();
            fontSize(8.5f); textLetterSpacing(1.5f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(on ? C_TEXT : C_DIM); text(c.x + w / 2 + 7, c.y + 0.5f, c.label, nullptr);
            break; }
        case Segment: {
            int n = 0; const char* const* names = segLabels(p, n);
            const int cur = (int)(val[p] + .5f); const float sw = c.r / n, x0 = c.x - c.r / 2;
            for (int i = 0; i < n; ++i) {
                const float x = x0 + sw * i;
                beginPath(); roundedRect(x + 1.5f, c.y - SEG_H / 2, sw - 3, SEG_H, 5); fillColor(i == cur ? C_ACC : C_PANEL_HI); fill();
                fontSize(p == kMode || p == kProcessing ? 9.5f : 8.5f); textLetterSpacing(1.4f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(i == cur ? C_BG : C_DIM); text(x + sw / 2, c.y + 0.5f, names[i], nullptr);
            }
            break; }
        }
        if (learnMode) {
            const int cc = paramCC[p]; const bool waiting = learnWaiting == (int)p;
            if (cc >= 0 || waiting) {
                const float by = c.type == Knob ? c.y - c.r - 12 : c.y - SEG_H / 2 - 8;
                snprintf(buf, sizeof buf, waiting ? "?" : "CC%d", cc);
                fontSize(8.5f); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE);
                const float bw = waiting ? 16 : 34;
                beginPath(); roundedRect(c.x - bw / 2, by - 7, bw, 14, 3); fillColor(waiting ? C_ACC : C_PANEL_HI); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
                fillColor(waiting ? C_BG : C_ACC); text(c.x, by + 0.5f, buf, nullptr);
            }
        }
    }
    void drawStatus() {
        std::string msg;
        if (statusFlash > 0) msg = statusText;
        else if (learnMode) msg = learnWaiting >= 0 ? std::string("Move a MIDI controller to assign: ") + paramDef(learnWaiting).name : "MIDI Learn: click a control, then move a controller.  Right-click a control to unassign.";
        if (!msg.empty()) { fontSize(10); textLetterSpacing(0.8f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_ACC); text(LX + 40, LY + 22, msg.c_str(), nullptr); }
        if (learnMode && learnWaiting < 0) { textAlign(ALIGN_RIGHT | ALIGN_MIDDLE); fillColor(hoverClear ? C_TEXT : C_DIM); text(LX + LW - 12, LY + 22, "CLEAR ALL ASSIGNMENTS", nullptr); }
        if (statusFlash > 0) { --statusFlash; repaint(); }
    }
    void flash(const std::string& s) { statusText = s; statusFlash = 60; repaint(); }
    void drawEditBox() {
        const Ctl& c = controls()[editing];
        const float w = 90, h = 22, x = c.x - w / 2, y = c.y + c.r + 14;
        beginPath(); roundedRect(x, y, w, h, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
        fontSize(12); textLetterSpacing(0.5f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(c.x, y + h / 2 + 0.5f, (editBuf + "|").c_str(), nullptr);
    }

    // ---- presets ------------------------------------------------------------------
    bool matchesValues(const float* v) const {
        for (uint32_t i = 0; i < kParamCount; ++i) {
            if (isOut(i) || isView(i)) continue;
            if (std::fabs(val[i] - v[i]) > 1e-4f * std::max(1.f, std::fabs(v[i]))) return false;
        }
        return true;
    }
    void applyValues(const float* v) { beginGesture(); applyAll(v); endGesture(); }
    void applyFactory(int idx) { idx = (idx % (int)kNumPresets + (int)kNumPresets) % (int)kNumPresets; applyValues(presets()[idx].v); curFactory = idx; curUser.clear(); repaint(); }
    void applyUser(const std::string& name) { float v[kParamCount]; std::copy(defv, defv + kParamCount, v); if (!loadUserPreset(name, v)) return; applyValues(v); std::copy(v, v + kParamCount, userVals); curUser = name; curFactory = -1; repaint(); }
    void programLoaded(uint32_t index) override { curFactory = (int)index; curUser.clear(); repaint(); }
    void step(int d) {
        const int nf = (int)kNumPresets, nu = (int)userPresets.size(), total = nf + nu;
        int cur = curFactory; if (!curUser.empty()) for (int i = 0; i < nu; ++i) if (userPresets[i] == curUser) cur = nf + i;
        if (cur < 0) cur = 0;
        const int next = ((cur + d) % total + total) % total;
        if (next < nf) applyFactory(next); else applyUser(userPresets[next - nf]);
    }
    void drawPresetBar() {
        const float x = PS_X, y = PS_Y, w = PS_W, h = PS_H, a = PS_ARROW;
        beginPath(); roundedRect(x, y, w, h, 5); fillColor(C_PANEL); fill(); strokeColor(menuOpen ? C_ACC : C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(x + a, y + 5); lineTo(x + a, y + h - 5); moveTo(x + w - a, y + 5); lineTo(x + w - a, y + h - 5); strokeColor(C_LINE); stroke();
        for (int side = 0; side < 2; ++side) {
            const bool hot = presetHover == (side == 0 ? 0 : 2);
            const float cx = side == 0 ? x + a / 2 : x + w - a / 2, cy = y + h / 2, d = side == 0 ? -1.f : 1.f;
            beginPath(); moveTo(cx - 3 * d, cy - 5); lineTo(cx + 3 * d, cy); lineTo(cx - 3 * d, cy + 5); closePath(); fillColor(hot ? C_ACC : C_DIM); fill();
        }
        std::string name; bool modified = false;
        if (!curUser.empty()) { name = curUser; modified = !matchesValues(userVals); }
        else if (curFactory >= 0) { name = presets()[curFactory].name; modified = !matchesValues(presets()[curFactory].v); }
        else name = "Custom";
        if (modified) name += " *";
        fontSize(11.5f); textLetterSpacing(0.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(presetHover == 1 || menuOpen ? C_TEXT : Color(0xd0, 0xd4, 0xdc));
        text(x + w / 2, y + h / 2 + 0.5f, name.c_str(), nullptr);
    }
    // preset browser: categories, user presets and favourites in columns; typing searches names and categories.
    // kind: 0 factory, 1 user, 2 save button, 3 search field, 4 favourite (idx into favs)
    struct MRect { float x, y, w, h; int kind; int idx; };
    static std::string lower(const std::string& s) { std::string r(s); for (char& c : r) c = (char)std::tolower((unsigned char)c); return r; }
    std::string favKey(int kind, int idx) const { return kind == 0 ? std::string("f:") + presets()[idx].name : (kind == 1 ? "u:" + userPresets[idx] : (idx < (int)favs.size() ? favs[idx] : std::string())); }
    bool isFav(const std::string& k) const { return std::find(favs.begin(), favs.end(), k) != favs.end(); }
    void toggleFav(const std::string& k) {
        if (k.empty()) return;
        auto it = std::find(favs.begin(), favs.end(), k); if (it != favs.end()) favs.erase(it); else favs.push_back(k);
        saveFavourites(favs); repaint();
    }
    static constexpr int kMenuCols = kNumCategories + 2, kMenuRows = 16;
    void menuLayout(float& mx, float& my, float& mw, float& mh, std::vector<MRect>& items) const {
        mw = PM_COLW * kMenuCols + 2 * PM_PAD; mx = std::min(W - mw - 8, std::max(8.f, PS_X + PS_W / 2 - mw / 2)); my = PS_Y + PS_H + 4;
        items.clear();
        const float top = my + PM_PAD + PM_ROW + 6 + PM_HEAD;
        int maxRows = 1;
        items.push_back({ mx + PM_PAD + PM_COLW, my + PM_PAD, 300, PM_ROW, 3, 0 });
        items.push_back({ mx + mw - PM_PAD - 150, my + PM_PAD, 150, PM_ROW, 2, 0 });
        if (!query.empty()) {
            const std::string q = lower(query); int k = 0;
            auto place = [&](int kind, int idx) { const int col = k / kMenuRows, row = k % kMenuRows; if (col >= kMenuCols) return; items.push_back({ mx + PM_PAD + col * PM_COLW, top + row * PM_ROW, PM_COLW - 4, PM_ROW, kind, idx }); maxRows = std::max(maxRows, row + 1); ++k; };
            for (uint32_t i = 0; i < kNumPresets; ++i) if (lower(presets()[i].name).find(q) != std::string::npos || lower(presets()[i].category).find(q) != std::string::npos) place(0, (int)i);
            for (int u = 0; u < (int)userPresets.size(); ++u) if (lower(userPresets[u]).find(q) != std::string::npos || std::string("user").find(q) != std::string::npos) place(1, u);
        } else {
            int rows[kNumCategories] = {};
            for (uint32_t i = 0; i < kNumPresets; ++i) {
                const char* cat = presets()[i].category;
                if (std::strcmp(cat, "Init") == 0) { items.push_back({ mx + PM_PAD, my + PM_PAD, PM_COLW - 4, PM_ROW, 0, (int)i }); continue; }
                for (int c = 0; c < kNumCategories; ++c)
                    if (std::strcmp(cat, kCategories[c]) == 0) { items.push_back({ mx + PM_PAD + c * PM_COLW, top + rows[c] * PM_ROW, PM_COLW - 4, PM_ROW, 0, (int)i }); maxRows = std::max(maxRows, ++rows[c]); }
            }
            for (int u = 0; u < (int)userPresets.size() && u < kMenuRows; ++u) items.push_back({ mx + PM_PAD + kNumCategories * PM_COLW, top + u * PM_ROW, PM_COLW - 4, PM_ROW, 1, u });
            maxRows = std::max(maxRows, std::min(kMenuRows, (int)userPresets.size()));
            for (int f = 0; f < (int)favs.size() && f < kMenuRows; ++f) items.push_back({ mx + PM_PAD + (kNumCategories + 1) * PM_COLW, top + f * PM_ROW, PM_COLW - 4, PM_ROW, 4, f });
            maxRows = std::max(maxRows, std::min(kMenuRows, (int)favs.size()));
        }
        mh = PM_PAD + PM_ROW + 6 + PM_HEAD + maxRows * PM_ROW + PM_PAD;
    }
    std::string itemLabel(const MRect& r) const {
        if (r.kind == 0) return presets()[r.idx].name;
        if (r.kind == 1) return userPresets[r.idx];
        if (r.kind == 4 && r.idx < (int)favs.size()) return favs[r.idx].substr(2);
        return "";
    }
    void drawPresetMenu() {
        float mx, my, mw, mh; std::vector<MRect> items; menuLayout(mx, my, mw, mh, items);
        beginPath(); rect(mx - 12, my - 4, mw + 24, mh + 20); fillPaint(boxGradient(mx, my + 2, mw, mh, 6, 14, Color(0, 0, 0, 0.6f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(mx, my, mw, mh, 6); fillColor(C_PANEL_HI); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        beginPath(); moveTo(mx + PM_PAD, my + PM_PAD + PM_ROW + 3); lineTo(mx + mw - PM_PAD, my + PM_PAD + PM_ROW + 3); strokeColor(C_LINE); stroke();
        fontSize(8.5f); textLetterSpacing(1.8f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE);
        const float hy = my + PM_PAD + PM_ROW + 6 + PM_HEAD / 2;
        if (query.empty()) {
            for (int c = 0; c < kMenuCols; ++c) {
                std::string up = c < kNumCategories ? kCategories[c] : (c == kNumCategories ? "User" : "Favourites"); for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
                fillColor(c < kNumCategories ? C_ACC : C_FAV); text(mx + PM_PAD + c * PM_COLW + 12, hy, up.c_str(), nullptr);
            }
            fontSize(10); textLetterSpacing(0.4f); fillColor(C_FAINT);
            if (userPresets.empty()) text(mx + PM_PAD + kNumCategories * PM_COLW + 12, hy + PM_HEAD / 2 + PM_ROW / 2, "(none saved yet)", nullptr);
            if (favs.empty()) text(mx + PM_PAD + (kNumCategories + 1) * PM_COLW + 12, hy + PM_HEAD / 2 + PM_ROW / 2, "(click a star)", nullptr);
        } else {
            int found = 0; for (const MRect& r : items) if (r.kind == 0 || r.kind == 1) ++found;
            char b[64]; snprintf(b, sizeof b, found ? "%d MATCHING PRESET%s" : "NO PRESET MATCHES", found, found == 1 ? "" : "S"); fillColor(C_ACC); text(mx + PM_PAD + 12, hy, b, nullptr);
        }
        for (int i = 0; i < (int)items.size(); ++i) {
            const MRect& r = items[i]; const bool hot = i == menuHover;
            if (r.kind == 2) {
                beginPath(); roundedRect(r.x, r.y + 1, r.w, r.h - 2, 4); fillColor(hot ? withAlpha(C_ACC, 0.25f) : C_PANEL); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
                fontSize(9.5f); textLetterSpacing(1.6f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(r.x + r.w / 2, r.y + r.h / 2 + 0.5f, "SAVE CURRENT AS...", nullptr);
                continue;
            }
            if (r.kind == 3) {             // search field
                beginPath(); roundedRect(r.x, r.y + 1, r.w, r.h - 2, 4); fillColor(C_DISPLAY); fill(); strokeColor(query.empty() ? C_LINE : C_ACC); strokeWidth(1); stroke();
                beginPath(); circle(r.x + 12, r.y + r.h / 2, 4); moveTo(r.x + 15, r.y + r.h / 2 + 3); lineTo(r.x + 18, r.y + r.h / 2 + 6); strokeColor(C_DIM); strokeWidth(1.3f); stroke();
                fontSize(11); textLetterSpacing(0.3f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(query.empty() ? C_FAINT : C_TEXT);
                text(r.x + 26, r.y + r.h / 2 + 0.5f, query.empty() ? "Type to search names and categories" : (query + "|").c_str(), nullptr);
                continue;
            }
            const std::string key = favKey(r.kind, r.idx);
            const bool isCur = r.kind == 0 ? (curUser.empty() && r.idx == curFactory) : (r.kind == 1 ? curUser == userPresets[r.idx] : false);
            const bool del = r.kind == 1 && pendingDelete == r.idx;
            if (hot || del) { beginPath(); roundedRect(r.x, r.y, r.w, r.h, 4); fillColor(del ? Color(0xd0, 0x50, 0x40, 0.35f) : withAlpha(C_ACC, 0.18f)); fill(); }
            if (isCur) { beginPath(); circle(r.x + 6, r.y + r.h / 2, 2.5f); fillColor(C_ACC); fill(); }
            fontSize(11); textLetterSpacing(0.3f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(hot || isCur || del ? C_TEXT : C_DIM);
            const std::string label = del ? "Delete? click again" : itemLabel(r);
            text(r.x + 12, r.y + r.h / 2 + 0.5f, label.c_str(), nullptr);
            // the favourite star
            const bool fav = isFav(key);
            if (fav || hot) drawStar(r.x + r.w - 10, r.y + r.h / 2, 5.f, fav);
        }
    }
    void drawStar(float cx, float cy, float rr, bool filled) {
        beginPath();
        for (int k = 0; k < 10; ++k) { const float a = -3.14159265f / 2 + k * 3.14159265f / 5, q = k % 2 ? rr * 0.45f : rr; if (!k) moveTo(cx + q * std::cos(a), cy + q * std::sin(a)); else lineTo(cx + q * std::cos(a), cy + q * std::sin(a)); }
        closePath();
        if (filled) { fillColor(C_FAV); fill(); } else { strokeColor(C_DIM); strokeWidth(1); stroke(); }
    }
    int menuHit(float x, float y) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it);
        for (int i = 0; i < (int)it.size(); ++i) if (x >= it[i].x && x < it[i].x + it[i].w && y >= it[i].y && y < it[i].y + it[i].h) return i;
        return -1; }
    MRect menuItem(int i) const { float mx, my, mw, mh; std::vector<MRect> it; menuLayout(mx, my, mw, mh, it); return it[i]; }
    void openMenu() { userPresets = listUserPresets(); favs = loadFavourites(); query.clear(); menuOpen = true; menuHover = -1; pendingDelete = -1; repaint(); }
    void applyFavourite(int idx) {
        if (idx >= (int)favs.size()) return;
        const std::string& k = favs[idx];
        if (k.compare(0, 2, "f:") == 0) { for (uint32_t i = 0; i < kNumPresets; ++i) if (k.substr(2) == presets()[i].name) { applyFactory((int)i); return; } }
        else applyUser(k.substr(2));
    }
    void drawSaveBox() {
        const float w = 360, h = 96, x = W / 2 - w / 2, y = 140;
        beginPath(); rect(0, 0, W, H); fillColor(Color(0, 0, 0, 0.45f)); fill();
        beginPath(); roundedRect(x, y, w, h, 8); fillColor(C_PANEL_HI); fill(); strokeColor(C_ACC); strokeWidth(1); stroke();
        fontSize(9.5f); textLetterSpacing(2.f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_DIM); text(x + 16, y + 14, "SAVE PRESET AS", nullptr);
        beginPath(); roundedRect(x + 16, y + 36, w - 32, 26, 4); fillColor(C_DISPLAY); fill(); strokeColor(C_LINE); stroke();
        fontSize(13); textLetterSpacing(0.4f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(x + 26, y + 49.5f, (saveName + "|").c_str(), nullptr);
        fontSize(8.5f); textLetterSpacing(1.2f); fillColor(C_FAINT); textAlign(ALIGN_RIGHT | ALIGN_TOP);
        text(x + w - 16, y + 72, saveError ? "COULD NOT SAVE - CHECK THE NAME" : "ENTER TO SAVE   ·   ESC TO CANCEL", nullptr);
    }

    // ---- interaction --------------------------------------------------------------
    static bool inRect(float x, float y, float rx, float ry, float rw, float rh) { return x >= rx && x < rx + rw && y >= ry && y < ry + rh; }
    int hit(float x, float y, int* seg = nullptr) const {
        const std::vector<Ctl>& cs = controls();
        for (int i = 0; i < (int)cs.size(); ++i) {
            const Ctl& c = cs[i];
            switch (c.type) {
            case Knob: { const float rr = c.r + 6; if ((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y) <= rr * rr) return i; break; }
            case Toggle: if (x >= c.x - 18 && x <= c.x + 110 && std::fabs(y - c.y) <= 11) return i; break;
            case Segment: { int n = 0; segLabels(c.param, n); const float x0 = c.x - c.r / 2;
                if (x >= x0 && x < x0 + c.r && std::fabs(y - c.y) <= SEG_H / 2 + 1) { if (seg) *seg = (int)((x - x0) / (c.r / n)); return i; } break; }
            }
        }
        return -1;
    }
    int headerHit(float x, float y) const {
        if (y < PS_Y || y > PS_Y + PS_H) return 0;
        if (x >= LEARN_X && x <= LEARN_X + LEARN_W) return 1;
        if (x >= MIDI_X && x <= MIDI_X + MIDI_W) return 2;
        if (x >= UNDO_X && x <= UNDO_X + UR_W) return 3;
        if (x >= REDO_X && x <= REDO_X + UR_W) return 4;
        if (x >= HELP_X && x <= HELP_X + HELP_W) return 5;
        if (x >= A_X && x < A_X + AB_W) return 6;
        if (x >= A_X + AB_W && x < A_X + 2 * AB_W) return 7;
        if (x >= COPY_X && x <= COPY_X + COPY_W) return 8;
        return 0;
    }
    int presetBarHit(float x, float y) const {
        if (y < PS_Y || y > PS_Y + PS_H || x < PS_X || x > PS_X + PS_W) return -1;
        return x < PS_X + PS_ARROW ? 0 : (x > PS_X + PS_W - PS_ARROW ? 2 : 1);
    }
    bool clearHit(float x, float y) const { return learnMode && learnWaiting < 0 && inRect(x, y, LX + LW - 190, LY + 12, 180, 20); }
    bool inDisplay(float x, float y) const { return inRect(x, y, LX, LY, LW, LH); }
    bool metersHit(float x, float y) const { return inRect(x, y, MX, MY, MW, MH); }
    bool filterHit(float x, float y) const { return inRect(x, y, GX, GY, GW, GH); }
    // 1: high-pass handle, 2: low-pass handle, 3: the band between them
    int handleHit(float x, float y) const {
        if (!filterHit(x, y)) return 0;
        const float dh = std::fabs(x - hpX()), dl = std::fabs(x - lpX());
        if (dh <= 9 || dl <= 9) return dh <= dl ? 1 : 2;
        if (x > hpX() && x < lpX()) return 3;
        return 0;
    }

    void setLearn(bool on) { learnMode = on; learnWaiting = -1; if (!on) setState("cmd", "cancel"); repaint(); }
    void endEdit(bool apply) {
        if (editing < 0) return;
        const uint32_t p = controls()[editing].param; float v;
        if (apply && parse(p, editBuf, v)) { beginGesture(); commit(p, v); endGesture(); }
        editing = -1; editBuf.clear(); repaint();
    }
    static float angleNorm(const Ctl& c, float x, float y) {
        float a = std::atan2(y - c.y, x - c.x);
        const float a0 = 0.75f * 3.14159265f;
        float rel = a - a0; while (rel < 0) rel += 2 * 3.14159265f;
        if (rel > 1.5f * 3.14159265f) rel = rel > 1.75f * 3.14159265f ? 0.f : 1.5f * 3.14159265f;
        return rel / (1.5f * 3.14159265f);
    }
    static float roundFreq(float f) { return f >= 2000 ? std::round(f / 10) * 10 : std::round(f); }
    void beginFilterDrag(int h, float x) {
        beginGesture(); dragHandle = h; dragX0 = x; dragHp0 = val[kHpFreq]; dragLp0 = val[kLpFreq];
        if (h != 2) editParameter(kHpFreq, true);
        if (h == 2 || (h == 3 && !lpOff())) editParameter(kLpFreq, true);
        if (h != 3) dragFilter(x);
    }
    void endFilterDrag() {
        if (dragHandle != 2) editParameter(kHpFreq, false);
        if (dragHandle == 2 || (dragHandle == 3 && dragLp0 < kLpOpen)) editParameter(kLpFreq, false);
        dragHandle = 0; endGesture();
    }
    void dragFilter(float x) {
        if (dragHandle == 1) setValue(kHpFreq, roundFreq(clampf(fOfX(x), 1000.f, 20000.f)));
        else if (dragHandle == 2) { const float f = fOfX(x); setValue(kLpFreq, f >= kLpOpen ? 20000.f : roundFreq(clampf(f, 2000.f, 20000.f))); }
        else if (dragHandle == 3) {
            float r = fOfX(x) / fOfX(dragX0);
            const bool lpOn = dragLp0 < kLpOpen;
            const float rMin = lpOn ? std::max(1000.f / dragHp0, 2000.f / dragLp0) : 1000.f / dragHp0;
            const float rMax = lpOn ? std::min(20000.f / dragHp0, 19000.f / dragLp0) : 20000.f / dragHp0;
            r = clampf(r, rMin, rMax);
            setValue(kHpFreq, roundFreq(dragHp0 * r));
            if (lpOn) setValue(kLpFreq, roundFreq(dragLp0 * r));
        }
    }

    bool onKeyboard(const KeyboardEvent& ev) override {
        if (!ev.press) return false;
        std::string* buf = saving ? &saveName : (editing >= 0 ? &editBuf : nullptr);
        if (!buf && menuOpen) {                              // typing in the preset browser searches
            if (ev.key == kKeyEscape) { if (!query.empty()) query.clear(); else menuOpen = false; }
            else if (ev.key == kKeyBackspace || ev.key == kKeyDelete) { if (!query.empty()) query.pop_back(); }
            else if (ev.key >= 32 && ev.key < 127 && query.size() < 32) query += (char)ev.key;
            else return false;
            menuHover = -1; pendingDelete = -1; repaint(); return true;
        }
        if (!buf) {
            const bool cmd = (ev.mod & (kModifierControl | kModifierSuper)) != 0;
            if (cmd && (ev.key == 'z' || ev.key == 'Z')) { if (ev.mod & kModifierShift) redo(); else undo(); return true; }
            if (cmd && (ev.key == 'y' || ev.key == 'Y')) { redo(); return true; }
            if (ev.key == kKeyEscape) { if (helpOpen) helpOpen = false; else if (menuOpen) menuOpen = false; else if (learnMode) setLearn(false); repaint(); return true; }
            return false;
        }
        if (ev.key == kKeyEnter) {
            if (saving) {
                if (saveUserPreset(saveName, val)) { userPresets = listUserPresets(); curUser = sanitizeName(saveName); curFactory = -1; std::copy(val, val + kParamCount, userVals); saving = false; saveError = false; }
                else saveError = true;
            } else endEdit(true);
            repaint(); return true;
        }
        if (ev.key == kKeyEscape) { if (saving) saving = false; else endEdit(false); repaint(); return true; }
        if (ev.key == kKeyBackspace || ev.key == kKeyDelete) { if (!buf->empty()) buf->pop_back(); repaint(); return true; }
        if (ev.key >= 32 && ev.key < 127 && buf->size() < (saving ? 40u : 12u)) { *buf += (char)ev.key; repaint(); return true; }
        return true;
    }

    bool onMouse(const MouseEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (ev.press) { statusFlash = 0; tipShown = false; tipSuppressed = tipKey; }
        if (helpOpen) { if (ev.press && ev.button == 1) helpClick(x, y); return true; }
        if (ev.button == 2 && ev.press) {                  // right-click (DPF numbers buttons 1 left, 2 right, 3 middle)
            if (menuOpen) { const int m = menuHit(x, y); if (m >= 0 && menuItem(m).kind == 1) { pendingDelete = menuItem(m).idx; repaint(); } return true; }
            if (learnMode) { const int i = hit(x, y); if (i >= 0) { const uint32_t p = controls()[i].param; char b[24]; snprintf(b, sizeof b, "unmap:%u", p); setState("cmd", b); paramCC[p] = -1; repaint(); } return true; }
            return false;
        }
        if (ev.button != 1) return false;
        if (!ev.press) {
            if (active >= 0) { editParameter(controls()[active].param, false); active = -1; endGesture(); return true; }
            if (dragHandle) { endFilterDrag(); return true; }
            return false;
        }
        if (editing >= 0) endEdit(true);
        if (menuOpen) {
            const int m = menuHit(x, y);
            if (m >= 0) {
                const MRect r = menuItem(m);
                if (r.kind == 3) return true;                       // the search field: just type
                if ((r.kind == 0 || r.kind == 1 || r.kind == 4) && x >= r.x + r.w - 20) { toggleFav(favKey(r.kind, r.idx)); return true; }
                if (r.kind == 2) { menuOpen = false; saving = true; saveError = false; saveName = curUser; }
                else if (r.kind == 1 && pendingDelete == r.idx) { deleteUserPreset(userPresets[r.idx]); if (curUser == userPresets[r.idx]) curUser.clear(); userPresets = listUserPresets(); pendingDelete = -1; repaint(); return true; }
                else { menuOpen = false; if (r.kind == 0) applyFactory(r.idx); else if (r.kind == 1) applyUser(userPresets[r.idx]); else applyFavourite(r.idx); }
            } else menuOpen = false;
            pendingDelete = -1; menuHover = -1; repaint(); return true;
        }
        const int hh = headerHit(x, y);
        if (hh == 1) { setLearn(!learnMode); return true; }
        if (hh == 2) { beginGesture(); commit(kMidiOn, val[kMidiOn] > 0.5f ? 0.f : 1.f); endGesture(); return true; }
        if (hh == 3) { undo(); return true; }
        if (hh == 4) { redo(); return true; }
        if (hh == 5) { helpOpen = true; repaint(); return true; }
        if (hh == 6 || hh == 7) { switchAB(hh - 6); return true; }
        if (hh == 8) { copyAB(); return true; }
        const int pb = presetBarHit(x, y);
        if (pb == 0) { step(-1); return true; }
        if (pb == 2) { step(1); return true; }
        if (pb == 1) { openMenu(); return true; }
        if (clearHit(x, y)) { setState("cmd", "clear"); for (int& m : paramCC) m = -1; repaint(); return true; }
        if (metersHit(x, y)) { resetPeaks(); repaint(); return true; }

        int seg = 0; const int i = hit(x, y, &seg);
        if (i >= 0) {
            const Ctl& c = controls()[i];
            const uint32_t p = c.param;
            if (learnMode) { learnWaiting = (int)p; char b[24]; snprintf(b, sizeof b, "learn:%u", p); setState("cmd", b); repaint(); return true; }
            if (isView(p)) { if (c.type == Toggle) commit(p, val[p] > 0.5f ? 0.f : 1.f); else if (c.type == Segment) commit(p, (float)seg); return true; }
            if (c.type == Toggle) { beginGesture(); commit(p, val[p] > 0.5f ? 0.f : 1.f); endGesture(); return true; }
            if (c.type == Segment) { beginGesture(); commit(p, (float)seg); endGesture(); return true; }
            if (ev.mod & (kModifierControl | kModifierSuper)) { beginGesture(); commit(p, defv[p]); endGesture(); return true; }
            if (i == lastClick && ev.time - lastClickTime < 350) {
                lastClick = -1; editing = i;
                char b[40]; fmt(p, b, sizeof b); editBuf = b;
                const size_t sp = editBuf.find(' '); if (sp != std::string::npos) editBuf.resize(sp);
                if (!editBuf.empty() && editBuf.back() == '%') editBuf.pop_back();
                repaint(); return true;
            }
            lastClick = i; lastClickTime = ev.time;
            beginGesture();
            active = i; dragY = y; dragNorm = toNorm(p, val[p]);
            const float dist = std::sqrt((x - c.x) * (x - c.x) + (y - c.y) * (y - c.y));
            circular = dist > c.r * 0.62f && c.r > 15;
            editParameter(p, true);
            if (circular) setValue(p, fromNorm(p, angleNorm(c, x, y)));
            repaint(); return true;
        }
        const int fh = handleHit(x, y);
        if (fh && !learnMode) {
            if (fh != 3 && (ev.mod & (kModifierControl | kModifierSuper))) { const uint32_t p = fh == 1 ? kHpFreq : kLpFreq; beginGesture(); commit(p, defv[p]); endGesture(); return true; }
            beginFilterDrag(fh, x); repaint(); return true;
        }
        return false;
    }

    bool onMotion(const MotionEvent& ev) override {
        const float s = uiScale(), x = ev.pos.getX() / s, y = ev.pos.getY() / s;
        if (saving) return true;
        if (helpOpen) { const int h = helpHit(x, y); if (h != helpHover) { helpHover = h; repaint(); } return true; }
        if (active >= 0) {
            const Ctl& c = controls()[active]; const uint32_t p = c.param;
            if (circular) { setValue(p, fromNorm(p, angleNorm(c, x, y))); return true; }
            const float sens = (ev.mod & kModifierShift) ? 1200.f : 240.f;
            dragNorm = clampf(dragNorm + (dragY - y) / sens, 0.f, 1.f); dragY = y;
            setValue(p, fromNorm(p, dragNorm));
            return true;
        }
        if (dragHandle) { dragFilter(x); return true; }
        if (menuOpen) { const int m = menuHit(x, y); if (m != menuHover) { menuHover = m; repaint(); } return true; }
        const int hh = headerHit(x, y); if (hh != hoverHeader) { hoverHeader = hh; repaint(); }
        const int pb = presetBarHit(x, y); if (pb != presetHover) { presetHover = pb; repaint(); }
        const bool hc = clearHit(x, y); if (hc != hoverClear) { hoverClear = hc; repaint(); }
        const int h = hit(x, y); if (h != hover) { hover = h; repaint(); }
        const int fh = h < 0 ? handleHit(x, y) : 0; if (fh != hoverHandle) { hoverHandle = fh; repaint(); }
        int key = -1;
        if (h >= 0) key = h; else if (hh > 0) key = 1000 + hh; else if (pb >= 0) key = 2000 + pb;
        else if (fh == 1 || fh == 2) key = 3000 + kTipHandle + (fh == 2 ? 100 : 0);
        else if (filterHit(x, y)) key = 3000 + kTipFilter;
        else if (inDisplay(x, y)) key = 3000 + kTipDisplay;
        else if (metersHit(x, y)) key = 3000 + kTipMeters;
        tipX = x; tipY = y;
        if (key != tipKey) { const bool was = tipShown; tipKey = key; tipSince = nowSec(); tipShown = false; if (key != tipSuppressed) tipSuppressed = -2; if (was) repaint(); }
        return false;
    }

    bool onScroll(const ScrollEvent& ev) override {
        const float s = uiScale(), sx = ev.pos.getX() / s, sy = ev.pos.getY() / s;
        if (menuOpen || saving || helpOpen) return true;
        if (presetBarHit(sx, sy) >= 0) { step(ev.delta.getY() > 0 ? -1 : 1); return true; }
        const bool fine = (ev.mod & kModifierShift) != 0;
        const float d = (ev.delta.getY() > 0 ? 1.f : -1.f) * (fine ? 0.004f : 0.02f);
        const int i = hit(sx, sy);
        if (i >= 0 && controls()[i].type == Knob) { const uint32_t p = controls()[i].param;
            beginGesture(); commit(p, fromNorm(p, toNorm(p, val[p]) + d)); endGesture(); return true; }
        const int fh = handleHit(sx, sy);
        if (fh == 1 || fh == 2) { const uint32_t p = fh == 1 ? kHpFreq : kLpFreq;
            beginGesture(); commit(p, roundFreq(fromNorm(p, toNorm(p, std::min(val[p], 20000.f)) + d))); endGesture(); return true; }
        return false;
    }

    void uiIdle() override {
        const double t = nowSec();
        if (t - lastPoll > 0.03) {
            lastPoll = t;
            if (TalcPlugin* const dsp = (TalcPlugin*)getPluginInstancePointer()) {
                const Engine& e = dsp->engine;
                const uint32_t w = e.histWrite.load();
                uint32_t from = lastHist; if (w - from > (uint32_t)HIST || w < from) from = w > (uint32_t)HIST ? w - HIST : 0;
                for (uint32_t k = from; k < w; ++k) {
                    const uint32_t r = k % Engine::kHist;
                    hPk[hPos] = e.hPeak[r]; hG[hPos] = e.hGr[r];
                    hPos = (hPos + 1) % HIST;
                }
                lastHist = w;
                detDb = e.detectorDb(); grNow = e.meterGr();
                latencyMs = e.latency() * 1000.f / std::max(1.f, e.sampleRate());
                updateSpectrum(e);
            }
            repaint();
        }
        if (gTipsOn && !tipShown && tipKey >= 0 && tipKey != tipSuppressed && active < 0 && !dragHandle && !menuOpen && !saving && !helpOpen && editing < 0
            && t - tipSince > 0.6) { tipShown = true; repaint(); }
    }

    // ---- hover tips ---------------------------------------------------------------
    bool tipContent(std::string& title, std::string& value, std::string& body) const {
        if (tipKey < 0) return false;
        if (tipKey < 1000) {
            const Ctl& c = controls()[tipKey]; const uint32_t p = c.param; char b[40]; fmt(p, b, sizeof b);
            title = paramDef(p).name; value = b; body = paramTip(p);
            if (c.type == Knob) body += "\n\nDrag up/down or grab the rim and turn. Double-click to type a value, Cmd/Ctrl-click to reset.";
            if (p == kLink && dimmed(c)) body += "\n\n(Not used in Mid Only or Side Only: only one signal is processed.)";
            if (paramCC[p] >= 0) { char cc[32]; snprintf(cc, sizeof cc, "   ·   CC%d", paramCC[p]); value += cc; }
            return true;
        }
        int t = -1;
        if (tipKey >= 3000) t = (tipKey - 3000) % 100;
        else if (tipKey >= 2000) t = kTipPrev + (tipKey - 2000);
        else { const int hh = tipKey - 1000;
            t = hh == 1 ? kTipLearn : hh == 2 ? kTipMidi : hh == 3 ? kTipUndo : hh == 4 ? kTipRedo : hh == 5 ? kTipHelp : hh == 6 ? kTipA : hh == 7 ? kTipB : kTipCopy; }
        title = targetTitle(t); value.clear(); body = targetTip(t);
        if (t == kTipUndo || t == kTipRedo) { const size_t n = t == kTipUndo ? undoStack.size() : redoStack.size(); char b[32]; snprintf(b, sizeof b, "%d step%s", (int)n, n == 1 ? "" : "s"); value = b; }
        if (t == kTipHandle) {
            const uint32_t p = tipKey >= 3100 ? kLpFreq : kHpFreq; char b[24]; fmt(p, b, sizeof b);
            title = paramDef(p).name; value = b; body = std::string(paramTip(p)) + "\n\nScroll to fine-tune, Cmd/Ctrl-click to reset.";
        }
        if (t == kTipFilter) { char a[24], b[24]; fmt(kHpFreq, a, sizeof a); fmt(kLpFreq, b, sizeof b); value = std::string(a) + " - " + b; }
        return true;
    }
    void drawTooltip() {
        std::string title, value, body;
        if (!tipContent(title, value, body)) return;
        const float w = 320, pad = 12;
        fontSize(11); textLetterSpacing(0.2f); textLineHeight(1.3f);
        float bb[4]; textBoxBounds(0, 0, w - 2 * pad, body.c_str(), nullptr, bb);
        const float h = pad + 16 + 8 + (bb[3] - bb[1]) + pad;
        float x = tipX + 16, y = tipY + 20;
        if (x + w > W - 8) x = tipX - w - 12;
        if (y + h > H - 8) y = tipY - h - 12;
        x = clampf(x, 8, W - w - 8); y = clampf(y, 8, H - h - 8);
        beginPath(); rect(x - 10, y - 4, w + 20, h + 18); fillPaint(boxGradient(x, y + 3, w, h, 6, 14, Color(0, 0, 0, 0.55f), Color(0, 0, 0, 0.f))); fill();
        beginPath(); roundedRect(x, y, w, h, 6); fillColor(Color(0x1e, 0x20, 0x25)); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(10); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_TEXT);
        std::string up = title; for (char& ch : up) ch = (char)std::toupper((unsigned char)ch);
        text(x + pad, y + pad + 1, up.c_str(), nullptr);
        if (!value.empty()) { fontSize(10.5f); textLetterSpacing(0.4f); textAlign(ALIGN_RIGHT | ALIGN_TOP); fillColor(C_ACC); text(x + w - pad, y + pad + 1, value.c_str(), nullptr); }
        fontSize(11); textLetterSpacing(0.2f); textLineHeight(1.3f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_DIM);
        textBox(x + pad, y + pad + 24, w - 2 * pad, body.c_str(), nullptr);
        textLineHeight(1.f);
    }

    // ---- help window ---------------------------------------------------------------
    static constexpr float HX = 120, HY = 50, HW = 760, HH = 560, TAB_W = 180, TAB_H = 34;
    int helpHit(float x, float y) const {
        if (x < HX || x > HX + HW || y < HY || y > HY + HH) return -2;
        for (int i = 0; i < kNumHelpPages; ++i) if (x >= HX + 12 && x < HX + 12 + TAB_W && y >= HY + 56 + i * TAB_H && y < HY + 56 + (i + 1) * TAB_H) return i;
        const float by = HY + HH - 44;
        if (y >= by && y < by + 28) {
            if (x >= HX + 12 && x < HX + 12 + TAB_W) return 100;
            if (x >= HX + HW - 330 && x < HX + HW - 140) return 101;
            if (x >= HX + HW - 124 && x < HX + HW - 16) return 102;
        }
        if (x >= HX + HW - 40 && y < HY + 40) return 102;
        return -1;
    }
    void helpClick(float x, float y) {
        const int h = helpHit(x, y);
        if (h >= 0 && h < kNumHelpPages) helpPage = h;
        else if (h == 100) gTipsOn = !gTipsOn;
        else if (h == 101) openURL(kManualURL);
        else if (h == 102 || h == -2) helpOpen = false;
        repaint();
    }
    void drawHelp() {
        beginPath(); rect(0, 0, W, H); fillColor(Color(0, 0, 0, 0.55f)); fill();
        beginPath(); roundedRect(HX, HY, HW, HH, 10); fillColor(C_PANEL); fill(); strokeColor(C_LINE); strokeWidth(1); stroke();
        fontSize(13); textLetterSpacing(4.f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(C_TEXT); text(HX + 22, HY + 28, "TALC HELP", nullptr);
        const float cx = HX + HW - 24, cy = HY + 26;
        beginPath(); moveTo(cx - 6, cy - 6); lineTo(cx + 6, cy + 6); moveTo(cx + 6, cy - 6); lineTo(cx - 6, cy + 6); strokeColor(helpHover == 102 ? C_TEXT : C_DIM); strokeWidth(1.6f); stroke();
        for (int i = 0; i < kNumHelpPages; ++i) {
            const float ty = HY + 56 + i * TAB_H;
            if (i == helpPage || i == helpHover) { beginPath(); roundedRect(HX + 12, ty + 2, TAB_W, TAB_H - 4, 5); fillColor(i == helpPage ? withAlpha(C_ACC, 0.16f) : C_PANEL_HI); fill(); }
            if (i == helpPage) { beginPath(); roundedRect(HX + 12, ty + 8, 3, TAB_H - 16, 1.5f); fillColor(C_ACC); fill(); }
            fontSize(11.5f); textLetterSpacing(0.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(i == helpPage ? C_TEXT : C_DIM);
            text(HX + 28, ty + TAB_H / 2 + 0.5f, kHelpPages[i].title, nullptr);
        }
        beginPath(); moveTo(HX + TAB_W + 26, HY + 56); lineTo(HX + TAB_W + 26, HY + HH - 56); strokeColor(C_LINE); strokeWidth(1); stroke();
        const float bx = HX + TAB_W + 46, bw = HW - TAB_W - 70;
        fontSize(15); textLetterSpacing(0.8f); textAlign(ALIGN_LEFT | ALIGN_TOP); fillColor(C_ACC); text(bx, HY + 60, kHelpPages[helpPage].title, nullptr);
        fontSize(12); textLetterSpacing(0.2f); textLineHeight(1.38f); fillColor(Color(0xc4, 0xc8, 0xd2));
        textBox(bx, HY + 92, bw, kHelpPages[helpPage].body, nullptr);
        textLineHeight(1.f);
        const float by = HY + HH - 44;
        beginPath(); moveTo(HX + 12, by - 10); lineTo(HX + HW - 12, by - 10); strokeColor(C_LINE); stroke();
        const float tw = 34, th = 18, tx = HX + 16, tyy = by + 14 - th / 2;
        beginPath(); roundedRect(tx, tyy, tw, th, th / 2); fillColor(gTipsOn ? withAlpha(C_ACC, 0.25f) : C_PANEL_HI); fill(); strokeColor(gTipsOn ? C_ACC : C_LINE); stroke();
        beginPath(); circle(gTipsOn ? tx + tw - th / 2 : tx + th / 2, by + 14, 6); fillColor(gTipsOn ? C_ACC : C_DIM); fill();
        fontSize(10); textLetterSpacing(1.6f); textAlign(ALIGN_LEFT | ALIGN_MIDDLE); fillColor(helpHover == 100 ? C_TEXT : C_DIM); text(tx + tw + 10, by + 14.5f, "HOVER TIPS", nullptr);
        auto btn = [&](float x0, float w0, const char* label, bool hot, bool primary) {
            beginPath(); roundedRect(x0, by, w0, 28, 5); fillColor(primary ? (hot ? withAlpha(C_ACC, 0.32f) : withAlpha(C_ACC, 0.16f)) : (hot ? C_PANEL_HI : C_PANEL)); fill();
            strokeColor(primary ? C_ACC : C_LINE); strokeWidth(1); stroke();
            fontSize(9.5f); textLetterSpacing(1.8f); textAlign(ALIGN_CENTER | ALIGN_MIDDLE); fillColor(C_TEXT); text(x0 + w0 / 2, by + 14.5f, label, nullptr);
        };
        btn(HX + HW - 330, 190, "FULL MANUAL (WEB)", helpHover == 101, false);
        btn(HX + HW - 124, 108, "CLOSE", helpHover == 102, true);
    }

private:
    float val[kParamCount], defv[kParamCount], userVals[kParamCount] = {}, before[kParamCount];
    int paramCC[kParamCount];
    int active = -1, hover = -1, lastClick = -1, editing = -1;
    uint lastClickTime = 0;
    float dragY = 0, dragNorm = 0; bool circular = false, gesture = false;
    int dragHandle = 0, hoverHandle = 0; float dragX0 = 0, dragHp0 = 0, dragLp0 = 0;
    std::vector<Snap> undoStack, redoStack;
    Snap abStore[2]; int abSlot = 0;
    std::string editBuf, statusText; int statusFlash = 0;
    int curFactory = 0, presetHover = -1, menuHover = -1, pendingDelete = -1, hoverHeader = 0;
    std::string curUser, query; std::vector<std::string> userPresets, favs;
    bool menuOpen = false, saving = false, saveError = false; std::string saveName;
    bool learnMode = false, hoverClear = false; int learnWaiting = -1; float lastLearnEcho = 0;
    int tipKey = -1, tipSuppressed = -2; double tipSince = 0; bool tipShown = false; float tipX = 0, tipY = 0;
    bool helpOpen = false; int helpPage = 0, helpHover = -1;
    float hPk[HIST], hG[HIST]; int hPos = 0; uint32_t lastHist = 0; double lastPoll = 0;
    float spec[SPEC_N]; Fft fft; std::vector<cpx> fbuf;
    float pkIn = -120, pkOut = -120, latencyMs = 0, detDb = -120, grNow = 0;
    ResizeHandle resizeHandle;

    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TalcUI)
};

UI* createUI() { return new TalcUI(); }

END_NAMESPACE_DISTRHO
