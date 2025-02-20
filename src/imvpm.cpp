// imVPM App

//--------------------------------------------
// ABOUT THE MEANING OF THE 'static' KEYWORD:
//--------------------------------------------
// In this demo code, we frequently use 'static' variables inside functions.
// A static variable persists across calls. It is essentially a global variable but declared inside the scope of the function.
// Think of "static int n = 0;" as "global int n = 0;" !
// We do this IN THE DEMO because we want:
// - to gather code and data in the same place.
// - to make the demo source code faster to read, faster to change, smaller in size.
// - it is also a convenient way of storing simple UI related information as long as your function
//   doesn't need to be reentrant or used in multiple threads.
// This might be a pattern you will want to use in your code, but most of the data you would be working
// with in a complex codebase is likely going to be stored outside your functions.

//-----------------------------------------
// ABOUT THE CODING STYLE OF OUR DEMO CODE
//-----------------------------------------
// The Demo code in this file is designed to be easy to copy-and-paste into your application!
// Because of this:
// - We never omit the ImGui:: prefix when calling functions, even though most code here is in the same namespace.
// - We try to declare static variables in the local scope, as close as possible to the code using them.
// - We never use any of the helpers/facilities used internally by Dear ImGui, unless available in the public API.
// - We never use maths operators on ImVec2/ImVec4. For our other sources files we use them, and they are provided
//   by imgui.h using the IMGUI_DEFINE_MATH_OPERATORS define. For your own sources file they are optional
//   and require you either enable those, either provide your own via IM_VEC2_CLASS_EXTRA in imconfig.h.
//   Because we can't assume anything about your support of maths operators, we cannot use them in imgui_demo.cpp.

// Navigating this file:
// - In Visual Studio: CTRL+comma ("Edit.GoToAll") can follow symbols in comments, whereas CTRL+F12 ("Edit.GoToImplementation") cannot.
// - With Visual Assist installed: ALT+G ("VAssistX.GoToImplementation") can also follow symbols in comments.

/*

Index of this file:

// [SECTION] Helper classes
// [SECTION] App state
// [SECTION] Forward declarations
// [SECTION] Helpers
//
// [SECTION] Backend callbacks
//   App init function           / AppInit()
//   App config function         / AppConfig()
//   App draw frame function     / AppDrawFrame()
//   App destroy function        / AppDestroy()
//
// [SECTION] Main window
//   Widgets
//   User input handler          / InputControl()
//   Window draw function        / Draw()
//   Message display function    / ProcessLog()
//
// [SECTION] Child windows
//   Spectrum plot               / SpectrumWindow()
//   Settings window             / SettingsWindow()
//
// [SECTION] Standard windows
//   About window                / ShowAboutWindow()

*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define NOMINMAX

// bumps FFT resolution (and the detection precision) at the cost of performance drop
// see assets/dump/charts.ods:match-compare for comparison results
//#define ANALYZER_BASE_FREQ FREQ_A2
// pitch history buffer capacity, seconds
#define ANALYZER_ANALYZE_SPAN 60
#include "Analyzer.hpp"
#include "AudioHandler.h"
#include "fonts.h"
#include <IconsFontAwesome6.h>
#include "version.h"

// System includes
#include <cstdio>           // vsnprintf, snprintf, printf
#include <cmath>            // sin, fmod, fabs
#include <algorithm>        // min, max
#include <vector>
#include <limits>
#if !defined(_MSC_VER) || _MSC_VER >= 1800
#include <inttypes.h>       // PRId64/PRIu64, not avail in some MinGW headers.
#endif
#include <sys/stat.h>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <portable-file-dialogs.h>
#include <SimpleIni.h>
#include <popl.hpp>

#define IMGUI_APP
#include "imgui_local.h"

#define _UNUSED_ [[maybe_unused]]

using namespace logger;

// Visual Studio warnings
#ifdef _MSC_VER
#pragma warning (disable: 4127)     // condition expression is constant
#pragma warning (disable: 4996)     // 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#pragma warning (disable: 26451)    // [Static Analyzer] Arithmetic overflow : Using operator 'xxx' on a 4 byte value and then casting the result to an 8 byte value. Cast the value to the wider type before calling operator 'xxx' to avoid overflow(io.2).
#endif

// Clang/GCC warnings with -Weverything
#if defined(__clang__)
#if __has_warning("-Wunknown-warning-option")
#pragma clang diagnostic ignored "-Wunknown-warning-option"         // warning: unknown warning group 'xxx'                     // not all warnings are known by all Clang versions and they tend to be rename-happy.. so ignoring warnings triggers new warnings on some configuration. Great!
#endif
#pragma clang diagnostic ignored "-Wunknown-pragmas"                // warning: unknown warning group 'xxx'
#pragma clang diagnostic ignored "-Wexit-time-destructors"          // warning: declaration requires an exit-time destructor    // exit-time destruction order is undefined. if MemFree() leads to users code that has been disabled before exit it might cause problems. ImGui coding style welcomes static/globals.
#pragma clang diagnostic ignored "-Wunused-macros"                  // warning: macro is not used                               // we define snprintf/vsnprintf on Windows so they are available, but not always used.
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"  // warning: zero as null pointer constant                   // some standard header variations use #define NULL 0
#pragma clang diagnostic ignored "-Wdouble-promotion"               // warning: implicit conversion from 'float' to 'double' when passing argument to function  // using printf() is a misery with this as C++ va_arg ellipsis changes float to double.
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wpragmas"                  // warning: unknown option after '#pragma GCC diagnostic' kind
#pragma GCC diagnostic ignored "-Wdouble-promotion"         // warning: implicit conversion from 'float' to 'double' when passing argument to function
#pragma GCC diagnostic ignored "-Wmisleading-indentation"   // [__GNUC__ >= 6] warning: this 'if' clause does not guard this statement      // GCC 6.0+ only. See #883 on GitHub.
#endif

// Format specifiers for 64-bit values (hasn't been decently standardized before VS2013)
#if !defined(PRId64) && defined(_MSC_VER)
#define PRId64 "I64d"
#define PRIu64 "I64u"
#elif !defined(PRId64)
#define PRId64 "lld"
#define PRIu64 "llu"
#endif

#define FCLAMP(V, MN, MX)     std::fmax((MN), std::fmin((V), (MX)))

#define _WIDESTR(s) L ## s
#define WIDESTR(s) _WIDESTR(s)

// Enforce cdecl calling convention for functions called by the standard library,
// in case compilation settings changed the default to e.g. __vectorcall
#ifndef IMGUI_CDECL
#ifdef _MSC_VER
#define IMGUI_CDECL __cdecl
#else
#define IMGUI_CDECL
#endif
#endif

#if !defined(PATH_MAX)
#define PATH_MAX 4096
#endif // !PATH_MAX

#if defined(_WIN32)
typedef std::wstring pathstr_t;
#else
typedef std::string pathstr_t;
#endif

//-----------------------------------------------------------------------------
// [SECTION] Helper classes
//-----------------------------------------------------------------------------

class HoldingAnalyzer : public Analyzer
{
public:
    HoldingAnalyzer(std::mutex &_mtx) :
        mtx(_mtx),
        hold_fft_data(new double[FFTSIZE]),
        hold_pitch_buf(new float[PITCH_BUF_SIZE])
    {
        unhold();
    }

    bool on_hold() { return onhold; }

    void hold()
    {
        std::unique_lock<std::mutex> lock(mtx);
        std::copy(fft_data.get(), fft_data.get() + FFTSIZE, hold_fft_data.get());
        std::copy(pitch_buf.get(), pitch_buf.get() + PITCH_BUF_SIZE, hold_pitch_buf.get());
        hold_total_analyze_cnt =  total_analyze_cnt;
        hold_peak_freq         =  peak_freq;
        hold_pitch_buf_pos     =  pitch_buf_pos;

        fft_data_x           =  hold_fft_data;
        pitch_buf_x          =  hold_pitch_buf;
        total_analyze_cnt_x  = &hold_total_analyze_cnt;
        peak_freq_x          = &hold_peak_freq;
        pitch_buf_pos_x      = &hold_pitch_buf_pos;
        onhold = true;
    }

    void unhold()
    {
        std::unique_lock<std::mutex> lock(mtx);
        fft_data_x          =  fft_data;
        pitch_buf_x         =  pitch_buf;
        total_analyze_cnt_x = &total_analyze_cnt;
        peak_freq_x         = &peak_freq;
        pitch_buf_pos_x     = &pitch_buf_pos;
        onhold = false;
    }

    double get_peak_freq()
    {
        return *peak_freq_x;
    }

    std::shared_ptr<const double[]> get_fft_buf()
    {
        return fft_data_x;
    }

    std::shared_ptr<const float[]> get_pitch_buf()
    {
        return pitch_buf_x;
    }

    size_t get_pitch_buf_pos()
    {
        return *pitch_buf_pos_x;
    }

    size_t get_total_analyze_cnt()
    {
        return *total_analyze_cnt_x;
    }

protected:
    std::mutex &mtx;
    bool onhold;
    size_t hold_total_analyze_cnt;
    const size_t *total_analyze_cnt_x;
    double hold_peak_freq;
    const double *peak_freq_x;
    std::shared_ptr<double[]> hold_fft_data;
    std::shared_ptr<const double[]> fft_data_x;
    std::shared_ptr<float[]> hold_pitch_buf;
    std::shared_ptr<const float[]> pitch_buf_x;
    size_t hold_pitch_buf_pos;
    const size_t *pitch_buf_pos_x;
};

//-----------------------------------------------------------------------------
// [SECTION] App state
//-----------------------------------------------------------------------------

// CONSTANTS
#define WINDOW_TITLE       "imVocalPitchMonitor" // window basic title
#define CONFIG_FILENAME    "imvpmrc" // config file name
#define FONT               Font      // symbol names that was given to binary_to_compressed_c when font data being created
#define FONT_NOTES         FontMono
#define FONT_ICONS_REGULAR FARegular
#define FONT_ICONS_SOLID   FASolid
static constexpr float    font_def_sz = 20.0f;  // default font size, px
static constexpr float   font_icon_sz = 16.0f;  // default font icon size, px
static constexpr float font_widget_sz = 28.0f;  // widget icons size, px
static constexpr float   font_grid_sz = 20.0f;  // grid font size, px
static constexpr float  font_pitch_sz = 36.0f;  // pitch font size, px
static constexpr float  font_tuner_sz = 16.0f;  // tuner font size, px
static constexpr float     rul_margin = 4.0f;   // ruler label margin, px
static constexpr float   top_feat_pos = 42.0f;  // top features vertical position, px: pitch note indicator, tuner, frequency
static constexpr float         c_dist = 100.0f; // interval width, Cents
static constexpr float         dc_max = 400.0f; // plot: max diff between data points, Cents

// LUTs
static constexpr const char *lut_note[][12] = {
                                             {  "C",  "C",  "D",  "D",  "E",  "F",  "F",  "G",  "G",  "A",  "A",  "B" }, // NoteNamesEnglish, has to be the first
                                             {  "Do", "Do", "Re", "Re", "Mi", "Fa", "Fa", "Sol","Sol","La", "La", "Si"}  // NoteNamesRomance
                                              };
static constexpr const char *lut_semisym[] = {   "",  "♯",  " " }; // semitone symbol selector, [0] elem for labels with no semitone indicator
static constexpr       char lut_number[][12] = {  // gives note number or 0 for semitone
                                             {   1 ,   0 ,   2 ,   3 ,   0 ,   4 ,   0 ,   5 ,   6 ,   0 ,   7 ,   0  }, // Minor scale
                                             {   1 ,   0 ,   2 ,   0 ,   3 ,   4 ,   0 ,   5 ,   0 ,   6 ,   0 ,   7  }  // Major scale
                                               };
static constexpr       float   lut_linew[] = { // plot features line widths
                                // Normal: Semitone,  C,    D,    E,    F,    G,    A,    B
                                               1.5f, 2.5f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f,
                                // Chromatic:   C,  C♯/D♭,  D,  D♯/E♭,  E,    F,  F♯/G♭,  G,  G♯/A♭,  A,  A♯/B♭,  B
                                               2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f,
                                // tempo:    normal  meter
                                               0.5f, 2.0f,
                                // plot data: pitch
                                               1.5f
                                             };

// orig palette
constexpr ImU32 palette[] = {
    IM_COL32(255,   0,   0, 255),  // RED
    IM_COL32(255, 192, 203, 255),  // PINK
    IM_COL32(128,   0, 128, 255),  // PURPLE
    IM_COL32( 75,   0, 130, 255),  // INDIGO
    IM_COL32(  0,   0, 255, 255),  // BLUE
    IM_COL32(173, 216, 230, 255),  // LIGHT BLUE
    IM_COL32(  0, 255, 255, 255),  // CYAN
    IM_COL32(  0, 128, 128, 255),  // TEAL
    IM_COL32(  0, 128,   0, 255),  // GREEN
    IM_COL32(144, 238, 144, 255),  // LIGHT GREEN
    IM_COL32(  0, 255,   0, 255),  // LIME
    IM_COL32(255, 255,   0, 255),  // YELLOW
    IM_COL32(255, 165,   0, 255),  // ORANGE
    IM_COL32(128, 128, 128, 255),  // LIGHT GRAY
    IM_COL32( 64,  64,  64, 255),  // GRAY
    IM_COL32( 32,  32,  32, 255)   // DARK GRAY
};
constexpr const char *palette_names[] = {
       "RED",        "PINK", "PURPLE",    "INDIGO",
      "BLUE",  "LIGHT BLUE",   "CYAN",      "TEAL",
     "GREEN", "LIGHT GREEN",   "LIME",    "YELLOW",
    "ORANGE",  "LIGHT GRAY",   "GRAY", "DARK GRAY"
};
enum {
    ColorRed = 0,
    ColorPink,
    ColorPurple,
    ColorIndigo,
    ColorBlue,
    ColorLightBlue,
    ColorCyan,
    ColorTeal,
    ColorGreen,
    ColorLightGreen,
    ColorLime,
    ColorYellow,
    ColorOrange,
    ColorLightGray,
    ColorGray,
    ColorDarkGray,
    ColorCount = IM_ARRAYSIZE(palette)
};

// scale list, first element is the default, last element is used to calculate selector width
constexpr const char *scale_list[] = {
    "C Major",  "C Minor",  "C♭ Major", "C♯ Major", "C♯ Minor", "D Major",  "D Minor", "D♭ Major",
    "D♯ Minor", "E Major",  "E Minor",  "E♭ Major", "E♭ Minor", "F Major",  "F Minor", "F♯ Major",
    "F♯ Minor", "G Major",  "G Minor",  "G♭ Major", "G♯ Minor", "A Major",  "A Minor", "A♭ Major",
    "A♭ Minor", "A♯ Minor", "B Major",  "B Minor",  "B♭ Major", "B♭ Minor", "Chromatic"
};

// SETTINGS
// range values
static constexpr float VolThresMax =     50.0f;  // Analyzer: volume threshold max value
static constexpr float VolThresMin =      0.0f;  // Analyzer: volume threshold min value
static constexpr float VolThresDef =      2.0f;  // Analyzer: volume threshold default value [2.0f]
static constexpr float PitchCalibMax =  450.0f;  // pitch calibration max value, Hz
static constexpr float PitchCalibMin =  430.0f;  // pitch calibration min value, Hz
static constexpr float PitchCalibDef =  440.0f;  // pitch calibration, Hz, default value [440]
static constexpr int   PlotRangeMax =     8400;  // plot: absolute upper boundary, Cents,  6000..9500, default 8400
static constexpr int   PlotRangeMin =        0;  // plot: absolute lower boundary, Cents, -1200..1200, default 0
static constexpr float PlotPosDef =    2400.0f;  // plot: position, Cents, default value [2400.0f]
static constexpr float PlotXZoomMax =    25.0f;  // plot: horizontal zoom max vaule, px
static constexpr float PlotXZoomMin =     2.0f;  // plot: horizontal zoom min value, px
static constexpr float PlotXZoomDef =     4.0f;  // plot: horizontal zoom, px, default value [4.0f]
static constexpr float PlotXZoomSpd =     0.5f;  // plot: horizontal mouse zoom factor, px
static constexpr float PlotYZoomMax =    10.0f;  // plot: vertical zoom max value, ruler font heights
static constexpr float PlotYZoomMin =     1.0f;  // plot: vertical zoom min value, ruler font heights
static constexpr float PlotYZoomDef =     3.0f;  // plot: vertical zoom, ruler font heights, default value [3.0f]
static constexpr float PlotYZoomSpd =     0.5f;  // plot: vertical mouse zoom factor, ruler font heights
static constexpr float PlotYScrlSpd =    50.0f;  // plot: vertical mouse scroll factor, Cents
static constexpr bool  PlotAScrlDef =     true;  // plot: vertical autoscroll enabled by default
static constexpr int   PlotAScrlVelMax =    10;  // plot: vertical autoscroll max value, Cents
static constexpr int   PlotAScrlVelMin =     1;  // plot: vertical autoscroll min value, Cents
static constexpr int   PlotAScrlVelDef =     3;  // plot: vertical autoscroll velocity, Cents, default value [3]
static constexpr int   PlotAScrlMar =      100;  // plot: vertical autoscroll margin, Cents
static constexpr double PlotAScrlGrace =   3.0;  // plot: vertical autoscroll grace period, seconds
static constexpr bool  PlotRulRightDef = false;  // plot: ruller on the right side, default value [false]
static constexpr bool  PlotSemiLinesDef = true;  // plot: draw semitone lines if not on Chromatic scale, default value
static constexpr bool  PlotSemiLblsDef = false;  // plot: show semitone labels, default value
static constexpr bool  ShowFreqDef =     false;  // show average pitch frequency by default [false]
static constexpr bool  ShowTunerDef =     true;  // show tuner by default [true]
static constexpr int   TunerSmoothMax =      5;  // tuner smoothing samples max value
static constexpr int   TunerSmoothMin =      1;  // tuner smoothing samples min value
static constexpr int   TunerSmoothDef =      3;  // tuner smoothing samples, default value [3]
static constexpr bool  MetronomeDef =    false;  // metronome pulse by default [false]
static constexpr int   TempoValueMax =     250;  // plot: tempo max value, beats per minute
static constexpr int   TempoValueMin =      20;  // plot: tempo min value, beats per minute
static constexpr int   TempoValueDef =     120;  // plot: tempo, beats per minute, default value [120]
static constexpr bool  TempoGridDef =    false;  // plot: draw tempo grid by default [false]
static constexpr bool  ButtonHoldDef =    true;  // UI: show HOLD button by default [true]
static constexpr bool  ButtonScaleDef =   true;  // UI: show scale selector by default [true]
static constexpr bool  ButtonTempoDef =   true;  // UI: show tempo settings button by default [true]
static constexpr bool  ButtonDevsDef =    true;  // UI: show device selector by default [true]
static constexpr bool  ClickToHoldDef =   true;  // UI: click on canvas to toggle hold / pause [true]
static constexpr float SeekStepMax =     30.0f;  // UI: seek step maximum, seconds
static constexpr float SeekStepMin =      0.5f;  // UI: seek step minimum, seconds
static constexpr float SeekStepDef =      5.0f;  // UI: seek step default value, seconds
static constexpr float CustomScaleMax =   4.0f;  // UI: custom scaling maximum value
static constexpr float CustomScaleMin =   0.5f;  // UI: custom scaling minimum value

// selections
enum {                                           // settings: note naming scheme
    NoteNamesEnglish = 0,                          //   English
    NoteNamesRomance = 1,                          //   Romance
    NoteNamesCount   = IM_ARRAYSIZE(lut_note),   // scheme count
    NoteNamesDef     = NoteNamesEnglish          // default value
};
enum {                                    // plot: octave offset
    OctOffsetA3  = 0,                       // A3
    OctOffsetA4  = 1,                       // A4
    OctOffsetMax = OctOffsetA4,           // max value
    OctOffsetMin = OctOffsetA3,           // min value
    OctOffsetDef = OctOffsetA4            // default
};
enum {                              // plot: scale transpose
    TransposeC   =  0,                //   C  Inst
    TransposeBb  =  2,                //   B♭ Inst
    TransposeEb  = -3,                //   E♭ Inst
    TransposeF   = -5,                //   F  Inst
    TransposeMax = 11,              // max value
    TransposeMin = -11,             // min value
    TransposeDef = TransposeC       // default value
};
enum {                              // plot: tempo metering scheme
    TempoMeterNone = 0,               // None
    TempoMeter3_4  = 3,               // 3/4
    TempoMeter4_4  = 4,               // 4/4
    TempoMeterMax = TempoMeter4_4,  // max value
    TempoMeterMin = TempoMeterNone, // min value
    TempoMeterDef = TempoMeter4_4   // default value
};

static bool       first_run = true;
static float      vol_thres = VolThresDef;       // Analyzer: volume threshold
static float         x_zoom = PlotXZoomDef;      // plot: horizontal zoom, px
static float         y_zoom = PlotYZoomDef;      // plot: vertical zoom, ruler font heights
static float          c_pos = PlotPosDef;        // plot: current bottom position, Cents
static bool       rul_right = PlotRulRightDef;   // plot: vertical axis ruler position
static bool      semi_lines = PlotSemiLinesDef;  // plot: draw semitone lines
static bool       semi_lbls = PlotSemiLblsDef;   // plot: show semitone labels
static int       oct_offset = OctOffsetDef;      // plot: octave offset
static float    calibration = PitchCalibDef;     // plot: pitch calibration value, Hz
static int        transpose = TransposeDef;      // plot: scale transpose, Keys
static bool      autoscroll = PlotAScrlDef;      // plot: vertical autoscroll enabled
static int      y_ascrl_vel = PlotAScrlVelDef;   // plot: vertical autoscroll velocity, Cents
static bool       show_freq = ShowFreqDef;       // show average pitch frequency
static bool      show_tuner = ShowTunerDef;      // show tuner
static int       note_names = NoteNamesDef;      // note naming scheme
static bool       metronome = MetronomeDef;      // metronome pulse enabled
static int        tempo_val = TempoValueDef;     // plot: tempo value, beats per minute
static bool      tempo_grid = TempoGridDef;      // plot: show tempo grid
static int      tempo_meter = TempoMeterDef;     // plot: tempo meter
static bool        but_hold = ButtonHoldDef;     // UI: show HOLD button
static bool       but_scale = ButtonScaleDef;    // UI: show scale selector
static bool       but_tempo = ButtonTempoDef;    // UI: show tempo settings button
static bool     but_devices = ButtonDevsDef;     // UI: show device selector
static bool      click_hold = ClickToHoldDef;    // UI: click on canvas to toggle hold / pause
static float      seek_step = SeekStepDef;       // UI: seek step value, seconds
static bool  custom_scaling = false;             // UI: custom scaling enabled 
static float   custom_scale = 1.0f;              // UI: custom scaling value
static float    play_volume = 1.0f;              // playback volume
static bool            mute = false;             // playback muted
static std::string scale_str(scale_list[0]);     // scale selected
static char record_dir[PATH_MAX] = {};           // record directory path

// Plot palete, fixed order up to and including pitch
std::initializer_list<ImU32> DefaultPlotColors = {
    palette[ColorDarkGray],       //   Semitone
    palette[ColorLightGray],      //   1st Tonic
    palette[ColorGray],           //   2nd Supertonic
    palette[ColorGray],           //   3rd Mediant
    palette[ColorGray],           //   4th Subdominant
    palette[ColorGray],           //   5th Dominant
    palette[ColorGray],           //   6th Subtonic
    palette[ColorGray],           //   7th Leading
    palette[ColorGray],           //   C
    palette[ColorGray],           //   C♯/D♭
    palette[ColorGray],           //   D
    palette[ColorGray],           //   D♯/E♭
    palette[ColorGray],           //   E
    palette[ColorGray],           //   F
    palette[ColorGray],           //   F♯/G♭
    palette[ColorGray],           //   G
    palette[ColorGray],           //   G♯/A♭
    palette[ColorGray],           //   A
    palette[ColorGray],           //   A♯/B♭
    palette[ColorGray],           //   B
    palette[ColorGray],           //   tempo
    IM_COL32(0, 0, 0, 0),         //   RESERVED
    palette[ColorYellow],         //   pitch
    IM_COL32(0, 0, 128, 255),     //   metronome
    IM_COL32(255, 255, 255, 255), //   note
    IM_COL32(204, 204, 204, 255)  //   tuner
};
enum {
    PlotIdxSemitone = 0,
    PlotIdxTonic,
    PlotIdxSupertonic,
    PlotIdxMediant,
    PlotIdxSubdominant,
    PlotIdxDominant,
    PlotIdxSubtonic,
    PlotIdxLeading,
    PlotIdxChromaC,
    PlotIdxChromaCsBb,
    PlotIdxChromaD,
    PlotIdxChromaDsEb,
    PlotIdxChromaE,
    PlotIdxChromaF,
    PlotIdxChromaFsGb,
    PlotIdxChromaG,
    PlotIdxChromaGsAb,
    PlotIdxChromaA,
    PlotIdxChromaAsBb,
    PlotIdxChromaB,
    PlotIdxTempo,
    PlotIdxReserved,
    PlotIdxPitch,
    PlotIdxMetronome,
    PlotIdxNote,
    PlotIdxTuner
};
static std::vector<ImU32> plot_colors(DefaultPlotColors);

std::initializer_list<ImU32> DefaultUIColors = {
    IM_COL32(200, 200, 200, 255),
    IM_COL32( 15,  15,  15, 240),
    IM_COL32( 20,  20,  20, 240),
    IM_COL32(200, 200, 200, 255),
    IM_COL32(  0,   0,   0,   0),
    IM_COL32(255, 255, 255,  30),
    IM_COL32(255, 255, 255,  60),
    IM_COL32(255,   0,  50, 200),
    IM_COL32(200,   0,  40, 255),
    IM_COL32(255,   0,  50, 255),
    IM_COL32(255,  60,  60, 255),
    IM_COL32(255, 255,  70, 255),
    IM_COL32(100, 100, 100, 255)
};
enum {
    UIIdxText = 0,
    UIIdxWndBg,
    UIIdxPopBg,
    UIIdxWidgetText,
    UIIdxWidget,
    UIIdxWidgetHovered,
    UIIdxWidgetActive,
    UIIdxProgress,
    UIIdxCapture,
    UIIdxRecord,
    UIIdxMsgErr,
    UIIdxMsgWarn,
    UIIdxMsgDbg
};
static std::vector<ImU32> UI_colors(DefaultUIColors);

// STATE
static ImFont      *font_def =  nullptr;  // default font ptr
static ImFont   *font_widget =  nullptr;  // widget icons font ptr
static ImFont     *font_grid =  nullptr;  // grid font ptr
static ImFont    *font_pitch =  nullptr;  // pitch font ptr
static ImFont    *font_tuner =  nullptr;  // tuner font ptr
static bool   fonts_reloaded =    false;  // fonts reloaded flag
static float        ui_scale =      1.0;  // DPI scaling factor
static float        x_offset =     0.0f;  // horizontal panning offset
static bool      x_off_reset =    false;  // reset offset flag
static float      x_zoom_min =     0.0f;  // plot horizontal zoom min limit based on window size,
                                          //           DPI scale and Analyzer's pitch array size
static float           c_top =     0.0f;  // current top plot position, Cents
static float           c_max =  9500.0f;  // current max plot position, Cents
static float           c_min = -1200.0f;  // current min plot position, Cents
static float         c_calib =     0.0f;  // calibrated and transposed offset, Cents
static int         scale_key =        0;  // plot: scale key
static bool      scale_major =     true;  // plot: scale is Major
static bool     scale_chroma =    false;  // plot: scale is Chromatic
static float      x_peak_off =     0.0f;  // peak note position offset
static ImVec2      rullbl_sz;             // vertical axis ruler size
static double  y_ascrl_grace =      0.0;  // autoscroll grace
static ImVec2      widget_sz;             // UI widget size
static float   widget_margin;             // UI widget margin
static float  widget_padding;             // UI widget padding
static float    menu_spacing;             // UI menu spacing
static ImVec2       tempo_sz;             // UI tempo widget size
static float   scale_sel_wdt;             // UI scale selector width
static ImVec2    hold_btn_sz;             // UI hold button size
static float    progress_hgt;             // UI playback progress height
static bool   progress_hover =    false;  // UI playback progress is hovered
static std::vector<double> f_peak_buf(TunerSmoothDef, -1.0);    // peak frequency averaging buffer
static size_t f_peak_buf_pos =        0;  // peak frequency averaging buffer position
static pathstr_t config_file;             // path to the config file
static bool        ro_config = false;     // config file is read-only or can't be accessed
static std::string last_file;             // path to the last active file
static std::string open_dir;              // directory of the last file open
static std::string cmd_options;           // command line options help message

typedef std::unique_ptr<pfd::open_file> unique_open_file;
static unique_open_file open_file_dlg = nullptr; // open file dialog operation
typedef std::unique_ptr<pfd::select_folder> unique_select_folder;
static unique_select_folder select_folder_dlg = nullptr; // select folder dialog operation

static std::mutex analyzer_mtx;
static HoldingAnalyzer analyzer(analyzer_mtx);
static Logger msg_log;
static AudioHandler audiohandler(&msg_log, 44100 /* Fsample */, 2 /* channels */, AudioHandler::FormatF32 /* sample format */, AudioHandler::FormatS16 /* record format */, Analyzer::ANALYZE_INTERVAL /* cb interval */);
static AudioHandler::State ah_state;      // frame-locked handler state
static uint64_t ah_len = 0, ah_pos = 0;   // handler length and position, applicable only to playback and record

// window handling
typedef enum {
    wsClosed = 0,
    wsOpen,
    wsOpened,
    wsClosing
} WndState;
static WndState wnd_settings = wsClosed;  // Settings popup state
static WndState    wnd_about = wsClosed;  // About popup state
static bool     wnd_spectrum = false;     // show spectrum window

//-----------------------------------------------------------------------------
// [SECTION] Forward Declarations, Helpers
//-----------------------------------------------------------------------------

// UI routines
//  widgets
static void Menu();                       // menu widget
static void CaptureDevices();             // capture device selection widget
static void PlaybackDevices();            // playback device selection and volume control widget
static void AudioControl();               // AudioHandler control widget
static void TempoControl();               // tempo control widget
static void ScaleSelector(bool from_settings = false); // scale selection widget
static void HoldButton();                 // HOLD button
static void PlaybackProgress();           // playback progress bar
static bool ColorPicker(const char *label, ImU32 &color, float split = 0.0f, bool alpha = false); // color picker with palette

// main window routines
static void InputControl();               // handle controls
static void Draw();                       // main drawing routine
static void ProcessLog();                 // AudioHandler log messages

// child windows
static void SpectrumWindow(bool *show);   // spectrum window
static void SettingsWindow();             // settings window

enum {
    TextAlignLeft = 0,
    TextAlignCenter = -1,
    TextAlignRight = -2,
    TextAlignBottom = -2,
    TextAlignMiddle = -1,
    TextAlignTop = 0
};
static void AddTextAligned(ImVec2 at, int alignH, int alignV, ImU32 color, ImDrawList* draw_list, const char *str);
static void AddFmtTextAligned(ImVec2 at, int alignH, int alignV, ImU32 color, ImDrawList* draw_list, const char *fmt, ...);

static void AddNoteLabel(ImVec2 at, int alignH, int alignV, int note, int sharpidx, int octave, ImU32 color, ImDrawList* draw_list);

//-----------------------------------------------------------------------------
// [SECTION] Helpers
//-----------------------------------------------------------------------------

#if defined(_WIN32)
static pathstr_t GetConfigPath()
{
    // try local file first
    wchar_t buf[MAX_PATH] = { 0 };
    DWORD count = GetModuleFileNameW(NULL, buf, MAX_PATH);
    while (count > 0 && count < IM_ARRAYSIZE(buf))
    {
        pathstr_t path = buf;
        auto sep = path.rfind('\\');
        if (sep == std::string::npos)
            break;

        path.resize(sep + 1);
        path += WIDESTR(CONFIG_FILENAME);

        struct _stat sb;
        int ret = _wstat(path.c_str(), &sb);
        if (ret != 0 || (sb.st_mode & _S_IFDIR))
            break;

        return path;
    }

    std::string path = pfd::internal::getenv("APPDATA");
    if (path.empty())
        path = pfd::path::home();
    path += "\\" CONFIG_FILENAME;

    return pfd::internal::str2wstr(path);
}
#else
static pathstr_t GetConfigPath()
{
    char buf[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", buf, IM_ARRAYSIZE(buf));
    while (count > 0 && count < IM_ARRAYSIZE(buf))
    {
        std::string path = buf;
        auto sep = path.rfind('/');
        if (sep == std::string::npos)
            break;

        path.resize(sep + 1);
        path += CONFIG_FILENAME;

        struct stat sb;
        int ret = stat(path.c_str(), &sb);
        if (ret != 0 || (sb.st_mode & S_IFDIR))
            break;

        return path;
    }

    return pfd::path::home() + "/.config/" CONFIG_FILENAME;
}
#endif

// by Joseph @ https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c
static inline bool ends_with(std::string const & value, std::string const & ending)
{
    if (ending.size() > value.size()) return false;
    return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

static std::string TruncateUTF8String(const std::string &str, size_t max_len)
{
    if (str.size() < max_len)
        return str;
    if (max_len <= 1)
        return "";

    size_t pos, last_pos = 0;
    for (pos = 0; pos < str.size() && max_len; max_len--)
    {
        unsigned char byte = static_cast<unsigned char>(str[pos]);
        size_t cnt;
        if (byte < 0x80)
            cnt = 1;
        else if ((byte >> 5) == 0x6)
            cnt = 2;
        else if ((byte >> 4) == 0xe)
            cnt = 3;
        else if ((byte >> 3) == 0x1e)
            cnt = 4;
        else // invalid sequence
            break;

        last_pos = pos;
        pos += cnt;
    }

    if (pos >= str.size())
        return str;

    return str.substr(0, last_pos) + "…";
}

static void AddNoteLabel(ImVec2 at, int alignH, int alignV, int note, int sharpidx, int octave, ImU32 color, ImDrawList* draw_list)
{
    static char label[16];

    if (octave >= 0)
        snprintf(label, IM_ARRAYSIZE(label), "%s%s%d", lut_note[note_names][note], lut_semisym[sharpidx], octave);
    else
        snprintf(label, IM_ARRAYSIZE(label), "%s%s", lut_note[note_names][note], lut_semisym[sharpidx]);
    ImVec2 size = ImGui::CalcTextSize(label);
    at.x += size.x / 2.0f * alignH;
    at.y += size.y / 2.0f * alignV;
    draw_list->AddText(at, color, label);
}

static void AddTextAligned(ImVec2 at, int alignH, int alignV, ImU32 color, ImDrawList* draw_list, const char *str)
{
    ImVec2 size = ImGui::CalcTextSize(str);
    at.x += size.x / 2.0f * alignH;
    at.y += size.y / 2.0f * alignV;
    draw_list->AddText(at, color, str);    
}

static void AddFmtTextAligned(ImVec2 at, int alignH, int alignV, ImU32 color, ImDrawList* draw_list, const char *fmt, ...)
{
    static char str[256];
    va_list args;
    va_start(args, fmt);
    int cnt = vsnprintf(str, IM_ARRAYSIZE(str) - 1, fmt, args);
    va_end(args);
    if (cnt <= 0)
        return;
    str[cnt] = '\0';

    ImVec2 size = ImGui::CalcTextSize(str);
    at.x += size.x / 2.0f * alignH;
    at.y += size.y / 2.0f * alignV;
    draw_list->AddText(at, color, str);
}

static void AlignTempo(size_t position = 0)
{
    std::lock_guard<std::mutex> lock(analyzer_mtx);
    analyzer.set_total_analyze_cnt(Analyzer::PITCH_BUF_SIZE + position); // offset for panning
    analyzer.clearData();
}

// audio control wrappers
static void Play(const char *file = nullptr)
{
    if (file && *file)
        last_file = file;

    analyzer.clearData();
    analyzer.unhold();
    audiohandler.stop();
    audiohandler.play(file);
    x_off_reset = true;
}

static void Seek(uint64_t seek_to_frame)
{
    if (!ah_state.canSeek())
        return;

    seek_to_frame = std::min(seek_to_frame, ah_len);

    analyzer.clearData();
    audiohandler.seek(seek_to_frame - seek_to_frame % Analyzer::ANALYZE_INTERVAL); // align to analyzer frame
}

static void Seek(double seek_to_second, bool relative = false)
{
    if (!ah_state.canSeek())
        return;

    uint64_t frame;

    if (!relative)
    {
        if (seek_to_second <= 0.0)
            frame = 0;
        else
            frame = std::min((uint64_t)(seek_to_second * audiohandler.sampleRateHz), ah_len);
    }
    else
    {
        int64_t relframes = seek_to_second * audiohandler.sampleRateHz;
        if (relframes >= 0)
            frame = ((uint64_t)relframes < ah_len - ah_pos) ? ah_pos + relframes : ah_len;
        else
            frame = ((uint64_t)-relframes < ah_pos) ? ah_pos + relframes : 0;
    }

    analyzer.clearData();
    audiohandler.seek(frame - frame % Analyzer::ANALYZE_INTERVAL);
}

static void Capture()
{
    if (!ah_state.isCapturing())
    {
        analyzer.clearData();
        audiohandler.stop();
        audiohandler.capture();
        x_off_reset = true;
    }
}

static void Record(const char *file = nullptr)
{
    if (!file && ah_state.isRecording())
    {
        Capture();
        return;
    }

    if (file)
    {
        last_file = file;

        // if given filename has any extention other than .wav, replace it with .wav as miniaudio will encode as WAV anyway
        for ( auto i = last_file.rbegin(); i != last_file.rend(); i++ )
        {
            if (*i == '\\' || *i == '/')
                break;
            else if (*i == '.')
            {
                last_file.resize(last_file.rend() - i - 1);
                break;
            }
        }
    }
    else
    {
        last_file = record_dir[0] ? record_dir : pfd::path::home();
        last_file += pfd::path::separator();
        char timeString[64];
        std::time_t time = std::time({});
        std::strftime(timeString, IM_ARRAYSIZE(timeString), "%Y%m%d_%H%M%S", std::localtime(&time));
        last_file += timeString;
    }

    last_file += ".wav";

    analyzer.clearData();
    analyzer.unhold();
    audiohandler.stop();
    audiohandler.record(last_file.c_str());
    x_off_reset = true;
}

static void Pause()
{
    if (ah_state.canPause())
        audiohandler.pause();
}

static void Resume()
{
    if (ah_state.canResume())
    {
        analyzer.unhold();
        audiohandler.resume();
        x_off_reset = true;
    }
}

static void ToggleHold()
{
    if (analyzer.on_hold())
    {
        analyzer.unhold();
        x_off_reset = true;
    }
    else
        analyzer.hold();
}

static void TogglePause()
{
    if (!ah_state.isPlaying())
        return ToggleHold();

    if (ah_state.canPause())
        audiohandler.pause();
    else if (ah_state.canResume())
    {
        analyzer.unhold();
        audiohandler.resume();
        x_off_reset = true;
    }
}

static inline void AdjustVolume()
{
    audiohandler.setPlaybackVolumeFactor(mute ? 0 : play_volume);
}

static inline void ToggleMute()
{
    mute = !mute;
    AdjustVolume();
}

static inline void ToggleFullscreen()
{
    if (ImGui::SysWndState == ImGui::WSMaximized)
        ImGui::SysRestore();
    else
        ImGui::SysMaximize();
}

static void OpenAudioFile()
{
    open_file_dlg = unique_open_file(new pfd::open_file("Open file for playback", !open_dir.empty() ? open_dir : pfd::path::home(),
                            { "Audio files (.wav .mp3 .ogg .flac"
 #if defined(HAVE_OPUS)
                            " .opus"
 #endif
                            ")", "*.wav *.mp3 *.ogg *.flac"
 #if defined(HAVE_OPUS)
                            " *.opus"
 #endif
                            , "All Files", "*" }));
}

static inline void BrowseDirectory()
{
    select_folder_dlg = unique_select_folder(new pfd::select_folder("Select record directory", record_dir[0] ? record_dir : pfd::path::home()));
}

static inline void UpdateCalibration()
{
    c_calib = (float)((std::log2(440.0) - std::log2((double)calibration)) * 12.0 * 100.0) + (float)(transpose * 100);
}

static void UpdateScale()
{
    if (scale_str.compare(0, std::string::npos, "Chromatic") == 0)
    {
        scale_chroma = true;
        return;
    }

    char ch1 = scale_str.at(0);
    const char **names = (const char**)lut_note[0];
    int key = 0;
    for (int n = 0; n < IM_ARRAYSIZE(lut_note[0]); n++)
    {
        if (ch1 == names[n][0])
        {
            key = n;
            break;
        }
    }

    if (scale_str.compare(1, IM_ARRAYSIZE("♯") - 1, "♯") == 0)
        key++;
    else if (scale_str.compare(1, IM_ARRAYSIZE("♭") - 1, "♭") == 0)
        key--;

    scale_key = (key + IM_ARRAYSIZE(lut_note[0])) % IM_ARRAYSIZE(lut_note[0]);
    scale_major = ends_with(scale_str, "Major");
    scale_chroma = false;
}

static void UpdatePeakBuf(int size)
{
    f_peak_buf.clear();
    f_peak_buf.resize(size, -1.0);
    f_peak_buf_pos = 0;
}

static bool GetIniValue(const CSimpleIniA &ini, const char *section, const char *key, int &value, int min, int max)
{
    const char *pv = ini.GetValue(section, key);
    if (pv == nullptr)
        return false;

    char *end;
    auto llval = strtoll(pv, &end, 0);
    if (*end != '\0' || (int)llval > max || (int)llval < min)
        return false;

    value = (int)llval;

    return true;
}

static bool GetIniValue(const CSimpleIniA &ini, const char *section, const char *key, float &value, float min, float max)
{
    const char *pv = ini.GetValue(section, key);
    if (pv == nullptr)
        return false;

    char *end;
    auto fval = strtod(pv, &end);
    if (*end != '\0' || fval > max || fval < min)
        return false;

    value = (float)fval;

    return true;
}

static bool GetIniValue(const CSimpleIniA &ini, const char *section, const char *key, bool &value)
{
    constexpr const char *values[] = { "false", "true", "off", "on", "no", "yes", "0", "1" };

    const char *pv = ini.GetValue(section, key);
    if (pv == nullptr)
        return false;

    for (size_t i = 0; i < IM_ARRAYSIZE(values); i++)
        if (strcmp(pv, values[i]) == 0)
            return value = (bool)(i % 2), true;

    return false;
}

static bool GetIniValue(const CSimpleIniA &ini, const char *section, const char *key, char *value, size_t value_sz)
{
    const char *pv = ini.GetValue(section, key);
    if (pv == nullptr)
        return false;

    size_t len = strlen(pv);
    if (len == 0 || (len + 1) > value_sz)
        return false;

    memcpy(value, pv, len);
    value[len] = '\0';

    return true;
}

static bool GetIniValue(const CSimpleIniA &ini, const char *section, const char *key, std::string &value,  const char **values, size_t el_count)
{
    const char *pv = ini.GetValue(section, key);
    if (pv == nullptr)
        return false;

    for (size_t i = 0; i < el_count; i++)
        if (strcmp(pv, values[i]) == 0)
            return value = values[i], true;

    return true;
}

#define VA_ARGS(...) , ##__VA_ARGS__
#define GETVAL(sec, var, ...) GetIniValue(ini, sec, #var, var VA_ARGS(__VA_ARGS__))
static void LoadSettings()
{
    config_file = GetConfigPath();

    if (!config_file.empty())
    {
 #if defined(_WIN32)
        if (_waccess_s(config_file.c_str(), 02) == EACCES)
            ro_config = true;
 #else
        if (access(config_file.c_str(), W_OK) < 0 && errno == EACCES)
            ro_config = true;
 #endif

        CSimpleIniA ini(true /* Unicode */);
        if (ini.LoadFile(config_file.c_str()) == SI_OK)
        {
            GETVAL("imvpm", first_run);
            GETVAL("imvpm", vol_thres, VolThresMin, VolThresMax);
            GETVAL("imvpm", x_zoom, PlotXZoomMin, PlotXZoomMax);
            GETVAL("imvpm", y_zoom, PlotYZoomMin, PlotYZoomMax);
            GETVAL("imvpm", c_pos, PlotRangeMin, PlotRangeMax);
            GETVAL("imvpm", rul_right);
            GETVAL("imvpm", semi_lines);
            GETVAL("imvpm", semi_lbls);
            GETVAL("imvpm", oct_offset, OctOffsetMin, OctOffsetMax);
            GETVAL("imvpm", calibration, PitchCalibMin, PitchCalibMax);
            GETVAL("imvpm", transpose, TransposeMin, TransposeMax);
            GETVAL("imvpm", autoscroll);
            GETVAL("imvpm", y_ascrl_vel, PlotAScrlVelMin, PlotAScrlVelMax);
            GETVAL("imvpm", show_freq);
            GETVAL("imvpm", show_tuner);
            GETVAL("imvpm", note_names, 0, NoteNamesCount - 1);
            GETVAL("imvpm", metronome);
            GETVAL("imvpm", tempo_val, TempoValueMin, TempoValueMax);
            GETVAL("imvpm", tempo_grid);
            GETVAL("imvpm", tempo_meter, TempoMeterMin, TempoMeterMax);
            GETVAL("imvpm", but_hold);
            GETVAL("imvpm", but_scale);
            GETVAL("imvpm", but_tempo);
            GETVAL("imvpm", but_devices);
            GETVAL("imvpm", click_hold);
            GETVAL("imvpm", seek_step, SeekStepMin, SeekStepMax);
            GETVAL("imvpm", custom_scaling);
            GETVAL("imvpm", custom_scale, CustomScaleMin, CustomScaleMax);
            GETVAL("imvpm", play_volume, 0.0f, 1.0f);
            GETVAL("imvpm", mute);
            GETVAL("imvpm", scale_str, (const char**)scale_list, IM_ARRAYSIZE(scale_list));
            {
                int size;
                if (GetIniValue(ini, "imvpm", "f_peak_smooth", size, TunerSmoothMin, TunerSmoothMax))
                    UpdatePeakBuf(size);
            }
            {
                char key[64];
                for (size_t i = 0; i < plot_colors.size(); i++)
                {
                    snprintf(key, IM_ARRAYSIZE(key), "plot_colors_%zu", i);
                    GetIniValue(ini, "imvpm", key, (int&)plot_colors[i], std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
                }
            }
            {
                int ival;
                if (GetIniValue(ini, "imvpm", "wnd_bg_color", ival, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()))
                    ImGui::SysWndBgColor = ImColor((ImU32)ival);
                const char *pv = ini.GetValue("imvpm", "wnd_pos");
                if (pv)
                {
                    ImRect winpos;
                    char *end;
                    int i;
                    for (i = 0; i < 4; i++, pv = end)
                    {
                        ImS32 val = strtol(pv, &end, 0);
                        if (*end != ' ' && *end != '\0')
                            break;
                        reinterpret_cast<ImS32*>(&winpos.x)[i] = val;
                    }
                    if (i == 4)
                        ImGui::SysWndPos = winpos;
                }
                int wstate = ImGui::WSNormal;
                if (GetIniValue(ini, "imvpm", "wnd_state", wstate, ImGui::WSNormal, ImGui::WSMaximized))
                    ImGui::SysWndState = (ImGui::WindowState)wstate;
            }
            GETVAL("imvpm", record_dir, IM_ARRAYSIZE(record_dir));
            {
                const char *pv = ini.GetValue("imvpm", "open_dir");
                if (pv)
                    open_dir = pv;
            }
        }
    }

    UpdateCalibration();
    UpdateScale();
    AdjustVolume();

    if (first_run && !record_dir[0])
    {
        msg_log.LogMsg(LOG_WARN, "Consider setting the record directory in the settings before start recording.");
        msg_log.LogMsg(LOG_WARN, "Default path is %s", pfd::path::home().c_str());
    }
}

#define  SETVAL(sec, var, format) do { snprintf(sval, IM_ARRAYSIZE(sval), format, var); ini.SetValue(sec, #var, sval); } while(0)
#define SETBOOL(sec, var) do { ini.SetValue(sec, #var, (var) ? "true" : "false"); } while(0)
static void SaveSettings()
{
    if (config_file.empty() || ro_config)
        return;

    CSimpleIniA ini(true /* Unicode */);
    char sval[64];

    first_run = false;
    SETBOOL("imvpm", first_run);
    SETVAL ("imvpm", vol_thres, "%0.3f");
    SETVAL ("imvpm", x_zoom, "%0.3f");
    SETVAL ("imvpm", y_zoom, "%0.3f");
    SETVAL ("imvpm", c_pos, "%0.3f");
    SETBOOL("imvpm", rul_right);
    SETBOOL("imvpm", semi_lines);
    SETBOOL("imvpm", semi_lbls);
    SETVAL ("imvpm", oct_offset, "%d");
    SETVAL ("imvpm", calibration, "%0.3f");
    SETVAL ("imvpm", transpose, "%d");
    SETBOOL("imvpm", autoscroll);
    SETVAL ("imvpm", y_ascrl_vel, "%d");
    SETBOOL("imvpm", show_freq);
    SETBOOL("imvpm", show_tuner);
    SETVAL ("imvpm", note_names, "%d");
    SETBOOL("imvpm", metronome);
    SETVAL ("imvpm", tempo_val, "%d");
    SETBOOL("imvpm", tempo_grid);
    SETVAL ("imvpm", tempo_meter, "%d");
    SETBOOL("imvpm", but_hold);
    SETBOOL("imvpm", but_scale);
    SETBOOL("imvpm", but_tempo);
    SETBOOL("imvpm", but_devices);
    SETBOOL("imvpm", click_hold);
    SETVAL ("imvpm", seek_step, "%0.1f");
    SETBOOL("imvpm", custom_scaling);
    SETVAL ("imvpm", custom_scale, "%0.1f");
    SETVAL ("imvpm", play_volume, "%0.3f");
    SETBOOL("imvpm", mute);
    ini.SetValue("imvpm", "scale_str", scale_str.c_str());
    {
        size_t f_peak_smooth = f_peak_buf.size();
        SETVAL ("imvpm", f_peak_smooth, "%zu");
    }
    {
        char key[64];

        for (size_t i = 0; i < plot_colors.size(); i++)
        {
            if (i == PlotIdxReserved) continue;
            snprintf(key, IM_ARRAYSIZE(key), "plot_colors_%zu", i);
            snprintf(sval, IM_ARRAYSIZE(sval), "0x%X", plot_colors[i]);
            ini.SetValue("imvpm", key, sval);
        }
    }
    // window state
    snprintf(sval, IM_ARRAYSIZE(sval), "0x%X", (ImU32)ImColor(ImGui::SysWndBgColor));
    ini.SetValue("imvpm", "wnd_bg_color", sval);
    snprintf(sval, IM_ARRAYSIZE(sval), "%d %d %d %d", ImGui::SysWndPos.x, ImGui::SysWndPos.y, ImGui::SysWndPos.w, ImGui::SysWndPos.h);
    ini.SetValue("imvpm", "wnd_pos", sval);
    snprintf(sval, IM_ARRAYSIZE(sval), "%d", (int)ImGui::SysWndState);
    ini.SetValue("imvpm", "wnd_state", sval);
    if (record_dir[0])
        ini.SetValue("imvpm", "record_dir", record_dir);
    if (!open_dir.empty())
        ini.SetValue("imvpm", "open_dir", open_dir.c_str());

    ini.SaveFile(config_file.c_str());
}

static void ResetSettings()
{
    vol_thres = VolThresDef;
    x_zoom = PlotXZoomDef;
    y_zoom = PlotYZoomDef;
    c_pos = PlotPosDef;
    rul_right = PlotRulRightDef;
    semi_lines = PlotSemiLinesDef;
    semi_lbls = PlotSemiLblsDef;
    oct_offset = OctOffsetDef;
    calibration = PitchCalibDef;
    transpose = TransposeDef;
    autoscroll = PlotAScrlDef;
    y_ascrl_vel = PlotAScrlVelDef;
    show_freq = ShowFreqDef;
    show_tuner = ShowTunerDef;
    note_names = NoteNamesDef;
    metronome = MetronomeDef;
    tempo_val = TempoValueDef;
    tempo_grid = TempoGridDef;
    tempo_meter = TempoMeterDef;
    but_hold = ButtonHoldDef;
    but_scale = ButtonScaleDef;
    but_tempo = ButtonTempoDef;
    but_devices = ButtonDevsDef;
    click_hold = ClickToHoldDef;
    seek_step = SeekStepDef;
    if (custom_scaling || custom_scale != 1.0f)
        ImGui::AppReconfigure = true;
    custom_scaling = false;
    custom_scale = 1.0f;
    scale_str = scale_list[0];
    ImGui::SysWndBgColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
    plot_colors = DefaultPlotColors;
    play_volume = 1.0f;
    mute = false;

    UpdatePeakBuf(TunerSmoothDef);
    UpdateCalibration();
    UpdateScale();
    AdjustVolume();
}

static bool ButtonWidget(const char* text, ImU32 color = UI_colors[UIIdxWidgetText], bool disabled = false)
{
    bool ret;

    if (disabled)
        ImGui::BeginDisabled();
    ImGui::PushFont(font_widget);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushStyleColor(ImGuiCol_Button, UI_colors[UIIdxWidget]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI_colors[UIIdxWidgetHovered]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI_colors[UIIdxWidgetActive]);
    ret = ImGui::Button(text, widget_sz);
    ImGui::PopStyleColor(4);
    ImGui::PopFont();
    if (disabled)
        ImGui::EndDisabled();

    return ret;
}

static bool HandlePopupState(const char *id, WndState &state, const ImVec2 &pos, const ImVec2 &pivot, ImGuiWindowFlags flags = 0)
{
    if (state == wsOpen)
        ImGui::OpenPopup(id);
    else if (state == wsClosing)
        state = wsClosed;

    ImGui::SetNextWindowPos(pos, ImGuiCond_Appearing, pivot);
    if (!ImGui::BeginPopup(id, flags))
    {
        if (state == wsOpened)
            state = wsClosing;
        else
            state = wsClosed;
        return false;
    }
    state = wsOpened;

    return true;
}

static inline bool ShowWindow(WndState &state)
{
    if (state != wsClosed)
        return false;

    state = wsOpen;
    return true;
}

//-----------------------------------------------------------------------------
// [SECTION] Backend callbacks
//-----------------------------------------------------------------------------

// AudioHandler
void sampleCb(_UNUSED_ AudioHandler::Format format, uint32_t channels, const void *pData, uint32_t frameCount, _UNUSED_ void *userData)
{
    std::lock_guard<std::mutex> lock(analyzer_mtx);
    const float *bufptr = (const float*)pData;
    while (frameCount--)
    {
        float sample = 0.0f;
        for (uint32_t ch = 0; ch < channels; ch++) // downmix to mono
            sample += *bufptr++;
        analyzer.addData(sample / channels);
    }
}

void eventCb(const AudioHandler::Notification &notification, _UNUSED_ void *userData)
{
    std::stringstream title;
    size_t sep;

    switch(notification.event)
    {
        case AudioHandler::EventPlayFile:
            sep = notification.dataStr.rfind(pfd::path::separator());
            title << WINDOW_TITLE ": Playing " << (sep == std::string::npos ? notification.dataStr : notification.dataStr.c_str() + sep + 1);
            ImGui::SysSetWindowTitle(title.str().c_str());
            AlignTempo();
            break;
        case AudioHandler::EventRecordFile:
            sep = notification.dataStr.rfind(pfd::path::separator());
            title << WINDOW_TITLE ": Recording " << (sep == std::string::npos ? notification.dataStr : notification.dataStr.c_str() + sep + 1);
            ImGui::SysSetWindowTitle(title.str().c_str());
            break;
        case AudioHandler::EventSeek:
            AlignTempo(notification.dataU64 / Analyzer::ANALYZE_INTERVAL);
            break;
        case AudioHandler::EventResume:
            if (notification.dataU64 != AudioHandler::EventOpCapture)
                break;
            // fall through
        default:
        case AudioHandler::EventStop:
            ImGui::SysSetWindowTitle(WINDOW_TITLE);
            if (notification.dataU64 == AudioHandler::EventOpRecord)
                msg_log.LogMsg(LOG_INFO, "File recorded: %s", last_file.c_str());
    }
}

// ImGui
void DragAndDropCb(const char *file)
{
    static double blanking = 0.0;
    double time = ImGui::GetTime();
    if (blanking < time)
    {
        AudioHandler::State state;
        audiohandler.getState(state);

        if (!state.isRecording())
            Play(file);
        blanking = time + 3.0; // accept only one file in x seconds
    }
}

int ImGui::AppInit(int argc, char const *const* argv)
{
    msg_log.SetLevel(LOG_INFO);

    LoadSettings();

    AlignTempo();
    audiohandler.attachFrameDataCb(sampleCb);
    audiohandler.attachNotificationCb(AudioHandler::EventPlayFile
                                    | AudioHandler::EventRecordFile
                                    | AudioHandler::EventSeek
                                    | AudioHandler::EventResume
                                    | AudioHandler::EventStop,
                                      eventCb);
    audiohandler.setPlaybackEOFaction(AudioHandler::CmdCapture);
    audiohandler.setUpdatePlaybackFileName(true);
    audiohandler.enumerate();

    if (!pfd::settings::available())
    {
        msg_log.LogMsg(LOG_WARN, "portable-file-dialogs is unavailable on this platform, file dialogs ceased");
        msg_log.LogMsg(LOG_WARN, "On *nix systems try to install kdialog / zenity");
    }

    popl::OptionParser op("opts");
    auto capture_option     = op.add<popl::Value<std::string>>("i", "capture", "capture device");
    auto playback_option    = op.add<popl::Value<std::string>>("o", "playback", "playback device");
    auto record_option      = op.add<popl::Switch>("r", "record", "start recording\n(overwrite an existing\nfile without asking)");
    auto verbose_option     = op.add<popl::Switch>("v", "verbose", "enable debug log");
    // save the help text for the About window
    {
        std::stringstream ss;
        ss << "imvpm [opts] [file]\n" << op;
        cmd_options = ss.str();
    }

    try
    {
        op.parse(argc, argv);

        if (capture_option->is_set())
            audiohandler.setPreferredCaptureDevice(capture_option->value().c_str());
        if (playback_option->is_set())
            audiohandler.setPreferredPlaybackDevice(capture_option->value().c_str());
        if (verbose_option->is_set())
            msg_log.SetLevel(LOG_DBG);

        if (record_option->is_set())
            Record(op.non_option_args().size() && !op.non_option_args()[0].empty() ? op.non_option_args()[0].c_str() : nullptr);
        else if (op.non_option_args().size() && !op.non_option_args()[0].empty())
            Play(op.non_option_args()[0].c_str());
        else
            Capture();
    }
    catch(const popl::invalid_option& e)
    {
        msg_log.LogMsg(LOG_ERR, "Invalid option: %s", e.what());
    }
    catch (const std::exception& e) {}

    return 0;
}

#define FONT_DATA(n) n ## _compressed_data
#define FONT_SIZE(n) n ## _compressed_size
#define ADD_FONT(var, ranges, name) do {\
  snprintf(font_config.Name, IM_ARRAYSIZE(font_config.Name), "%s, %0.1fpx", #name, var ## _sz); \
  var = io.Fonts->AddFontFromMemoryCompressedTTF(FONT_DATA(name) , FONT_SIZE(name), var ## _sz * ui_scale, &font_config, ranges); \
} while(0)
#define MERGE_FONT(var, ranges, name) do {\
  font_config.MergeMode = true; \
  font_config.GlyphMinAdvanceX = var ## _sz * ui_scale; \
  io.Fonts->AddFontFromMemoryCompressedTTF(FONT_DATA(name) , FONT_SIZE(name), font_config.GlyphMinAdvanceX, &font_config, ranges); \
  font_config.MergeMode = false; \
  font_config.GlyphMinAdvanceX = 0; \
} while(0)
bool ImGui::AppConfig(bool startup)
{
    if (custom_scaling)
        ui_scale = custom_scale;
    else
        ui_scale = ImGui::SysWndScaling;

    ImGuiIO &io = ImGui::GetIO();

    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ImFontConfig font_config; 
    font_config.OversampleH = 1;
    font_config.OversampleV = 1; 
    font_config.PixelSnapH = 1;
    static const ImWchar ranges_ui[] =
    {
        0x0020, 0x00FF, // Basic Latin + Latin Supplement
        0x0400, 0x052F, // Cyrillic + Cyrillic Supplement
        0x2DE0, 0x2DFF, // Cyrillic Extended-A
        0xA640, 0xA69F, // Cyrillic Extended-B
        0x266D, 0x266F, // ♭, ♯
        0x1D2E, 0x1D3E, // Superscript capital
        0x2026, 0x2026, // Horizontal Ellipsis
        0
    };
    static const ImWchar ranges_icons_regular[] =
    {
        0xF07C, 0xF07C, // folder-open
        0xF111, 0xF111, // circle
        0xF144, 0xF144, // circle-play
        0xF256, 0xF256, // hand
        0xF28B, 0xF28B, // circle-pause
        0xF28D, 0xF28D, // circle-stop
        0
    };
    static const ImWchar ranges_icons_solid[] =
    {
        0xE473, 0xE473, // chart-simple
        0xF026, 0xF028, // volume-off, volume-low, volume-high
        0xF065, 0xF066, // expand, compress
        0xF0c9, 0xF0c9, // bars
        0xF101, 0xF101, // angles-right
        0xF129, 0xF131, // info, microphone, microphone-slash
        0xF1DE, 0xF1DE, // sliders
        0xF52B, 0xF52B, // door-open
        0xF6A9, 0xF6A9, // volume-xmark
        0
    };
    static const ImWchar ranges_notes[] =
    {
        0x0020, 0x007F, // Basic Latin
        0x266D, 0x266F, // ♭, ♯
        0
    };

    io.Fonts->Clear();
    ADD_FONT(font_def, ranges_ui, FONT);
    MERGE_FONT(font_icon, ranges_icons_regular, FONT_ICONS_REGULAR);
    MERGE_FONT(font_icon, ranges_icons_solid, FONT_ICONS_SOLID);
    ADD_FONT(font_widget, ranges_icons_regular, FONT_ICONS_REGULAR);
    MERGE_FONT(font_widget, ranges_icons_solid, FONT_ICONS_SOLID);
    ADD_FONT(font_tuner, ranges_notes, FONT_NOTES);
    ADD_FONT(font_pitch, ranges_notes, FONT_NOTES);
    ADD_FONT(font_grid, ranges_notes, FONT_NOTES);
    io.Fonts->Build();
    io.FontDefault = font_def;
    fonts_reloaded = true;

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowPadding = ImVec2(font_def_sz / 2 * ui_scale, font_def_sz / 2 * ui_scale);
    style.ItemSpacing = ImVec2(font_def_sz / 2 * ui_scale, font_def_sz / 2 * ui_scale);
    style.ItemInnerSpacing = ImVec2(font_def_sz / 4 * ui_scale, font_def_sz / 4 * ui_scale);
    style.WindowRounding = 5.0f * ui_scale;
    style.FrameRounding = 5.0f * ui_scale;
    style.PopupRounding = 5.0f * ui_scale;
    style.GrabRounding  = 4.0f * ui_scale;
    style.GrabMinSize   = 15.0f * ui_scale;
    style.SeparatorTextAlign = ImVec2(0.5f, 0.75f);
    style.ColorButtonPosition = ImGuiDir_Left;
    style.Colors[ImGuiCol_Text] = ImColor(UI_colors[UIIdxText]);
    style.Colors[ImGuiCol_WindowBg] = ImColor(UI_colors[UIIdxWndBg]);
    style.Colors[ImGuiCol_PopupBg] = ImColor(UI_colors[UIIdxPopBg]);

    widget_padding = font_widget_sz * ui_scale / 5;
    widget_sz = ImVec2(font_widget_sz * ui_scale + widget_padding * 2, font_widget_sz * ui_scale + widget_padding * 2);
    widget_margin = font_widget_sz * ui_scale / 2.5;
    menu_spacing = 8.0f * ui_scale;

    ImGui::SysWndMinMax.x = (widget_sz.x + widget_margin) * 10;
    ImGui::SysWndMinMax.y = (widget_sz.x + widget_margin) * 10;

    if (startup)
    {
        ImGui::SysSetWindowTitle(WINDOW_TITLE);
        ImGui::SysAcceptFiles(DragAndDropCb);
    }

    return true;
}

void ImGui::AppNewFrame()
{
    // Exceptionally add an extra assert here for people confused about initial Dear ImGui setup
    // Most functions would normally just assert/crash if the context is missing.
    IM_ASSERT(ImGui::GetCurrentContext() != NULL && "Missing Dear ImGui context. Refer to examples app!");

    if (fonts_reloaded)
    {
        fonts_reloaded = false;

        // determine ruller width by the longest note name
        ImGui::PushFont(font_grid);
        const char **names = (const char**)lut_note[note_names];
        rullbl_sz.x = 0.0f;
        for(int i = 0; i < 12; ++i)
        {
            ImVec2 size = ImGui::CalcTextSize(names[i]);
            if (size.x > rullbl_sz.x)
                rullbl_sz = size;
        }
        if (semi_lbls)
            rullbl_sz.x += ImGui::CalcTextSize("♯").x;
        rullbl_sz.x += ImGui::CalcTextSize("8").x;
        rullbl_sz.x += rul_margin * 2.0f * ui_scale;
        ImGui::PopFont();
        ImGui::PushFont(font_pitch);
        x_peak_off = ImGui::CalcTextSize("8").x * 1.5f;
        ImGui::PopFont();
        ImGui::PushFont(font_def);
        tempo_sz = ImVec2(ImGui::CalcTextSize("888").x + widget_padding * 2, widget_sz.y);
        scale_sel_wdt = ImGui::CalcTextSize(scale_list[IM_ARRAYSIZE(scale_list) - 1]).x + menu_spacing * 4;
        hold_btn_sz = ImGui::CalcTextSize("HOLD"); hold_btn_sz.x += widget_padding * 2; hold_btn_sz.y += widget_padding * 2;
        ImGui::PopFont();
    }

    // process dialog operations
    if (open_file_dlg && open_file_dlg->ready(0))
    {
        auto files = open_file_dlg->result();
        if (files.size() > 0)
        {
            Play(files[0].c_str());
            auto sep = files[0].rfind(pfd::path::separator());
            if (sep != std::string::npos)
                open_dir = files[0].substr(0, sep);
        }
        open_file_dlg = nullptr;
    }
    if (select_folder_dlg && select_folder_dlg->ready(0))
    {
        auto dir = select_folder_dlg->result();
        if (!dir.empty() && dir.length() < IM_ARRAYSIZE(record_dir))
        {
            memcpy(record_dir, dir.c_str(), dir.length());
            record_dir[dir.length()] = '\0';
        }
        select_folder_dlg = nullptr;
    }

    audiohandler.getError();                           // discard any errors
    audiohandler.getState(ah_state, &ah_len, &ah_pos); // cache handler state for the frame

    // if handler is idling try to restart after a grace period for some number of tries
    static double restart_at = 0.0;
    static int tries = 0;
    if (ah_state.isIdle())
    {
        if (tries < 2)
        {
            if (restart_at == 0.0)
                restart_at = ImGui::GetTime() + 1.0;
            else if (ImGui::GetTime() > restart_at)
            {
                restart_at = 0.0;
                tries++;
                Capture();
            }
        }
    }
    else
    {
        restart_at = 0.0;
        tries = 0;
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->Pos);
    ImGui::SetNextWindowSize(ImGui::GetMainViewport()->Size);
    // Body of the main window starts here.
    if (ImGui::Begin(WINDOW_TITLE, nullptr,
        ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        ))
    {
        Draw();

        // widgets
        //  menu
        ImVec2 wsize = ImGui::GetWindowSize();
        ImVec2 center = wsize / 2;
        ImVec2 pos = wsize;
        pos.x -= widget_sz.x + widget_margin + rullbl_sz.x * rul_right;
        pos.y -= widget_sz.y + widget_margin;
        ImGui::SetCursorPos(pos);
        Menu();

        //  device selector
        if (but_devices)
        {
            pos.x -= widget_sz.x + widget_margin;
            ImGui::SetCursorPos(pos);
            if (ah_state.isPlaying())
                PlaybackDevices();
            else
                CaptureDevices();
        }

        // manual scroll indicator
        if (!autoscroll)
        {
            pos.x -= widget_sz.x + widget_margin;
            ImGui::SetCursorPos(pos);
            ButtonWidget(ICON_FA_HAND, GetColorU32(UI_colors[UIIdxWidgetText], 0.5f), true);
        }

        // audio control
        pos.x = center.x - widget_sz.x * 1.5f - widget_margin;
        ImGui::SetCursorPos(pos);
        AudioControl();

        pos.x = widget_margin + rullbl_sz.x * !rul_right;

        // tempo control
        if (but_tempo)
        {
            ImGui::SetCursorPos(pos);
            TempoControl();
        }

        pos.y = widget_margin;

        // scale selector
        if (but_scale)
        {
            ImGui::SetCursorPos(pos);
            ScaleSelector();
        }

        // HOLD button
        if (but_hold)
        {
            pos.y += (widget_sz.y - hold_btn_sz.y) / 2;
            pos.x = wsize.x - widget_margin - rullbl_sz.x * rul_right - hold_btn_sz.x;
            ImGui::SetCursorPos(pos);
            HoldButton();
        }

        // playback progress
        if (ah_state.isPlaying() && ah_len > 0)
            PlaybackProgress();

        // panning reset button
        if (x_offset != 0.0f)
        {
            float x_sz = font_def_sz * ui_scale + ImGui::GetStyle().FramePadding.x;
            ImGui::SetCursorPos(ImVec2(wsize.x - x_sz - rullbl_sz.x * rul_right, center.y - wsize.y / 4));
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetColorU32(UI_colors[UIIdxWidgetHovered], 0.75f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI_colors[UIIdxWidgetHovered]);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI_colors[UIIdxWidgetActive]);
            if (ImGui::Button(ICON_FA_ANGLES_RIGHT "##PanReset", ImVec2(x_sz, wsize.y / 2)))
                x_off_reset = true;
            ImGui::PopStyleColor(3);
        }

        InputControl();
        ProcessLog();

        // End of MainWindow
        ImGui::End();
    }

    SettingsWindow();
    ImGui::ShowAboutWindow(nullptr);

    if (wnd_spectrum)
        SpectrumWindow(&wnd_spectrum);

    //ImGui::ShowIDStackToolWindow(nullptr);
}

void ImGui::AppDestroy()
{
    SaveSettings();
}

//-----------------------------------------------------------------------------
// [SECTION] Child windows
//-----------------------------------------------------------------------------

static void Menu()
{
    if (ButtonWidget(ICON_FA_BARS))
        ImGui::OpenPopup("##MenuPopup");

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(menu_spacing, menu_spacing));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(menu_spacing, menu_spacing));
    if (ImGui::BeginPopup("##MenuPopup"))
    {
        if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open file", ""))
            OpenAudioFile();
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_CHART_SIMPLE " Spectrum", "", wnd_spectrum))
            wnd_spectrum = !wnd_spectrum;
        if (ImGui::MenuItem(ICON_FA_SLIDERS " Settings", ""))
            ShowWindow(wnd_settings);
        if (ImGui::MenuItem(ICON_FA_INFO " About", ""))
            ShowWindow(wnd_about);
        ImGui::Separator();
        if (ImGui::MenuItem(ICON_FA_DOOR_OPEN " Exit", ""))
            ImGui::AppExit = true;

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}

static void CaptureDevices()
{
    if (ButtonWidget(ICON_FA_MICROPHONE, ah_state.isCapOrRec() ? UI_colors[UIIdxCapture] : UI_colors[UIIdxWidgetText]))
    {
        audiohandler.enumerate();
        ImGui::OpenPopup("##CaptureDevicesPopup");
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(menu_spacing, menu_spacing));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(menu_spacing, menu_spacing));
    if (ImGui::BeginPopup("##CaptureDevicesPopup"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, ImVec2(0.5f, 0.75f));
        ImGui::SeparatorText("Capture devices");
        ImGui::PopStyleVar();
        auto devices = audiohandler.getCaptureDevices();
        for (size_t n = 0; n < devices.list.size(); n++)
        {
            bool is_selected = devices.list[n].name == devices.selectedName;
            if (ImGui::MenuItem(TruncateUTF8String(devices.list[n].name, 60).c_str(), "", is_selected) && (!is_selected || ah_state.isIdle()))
            {
                audiohandler.setPreferredCaptureDevice(devices.list[n].name.c_str());
                if (ah_state.isIdle())
                    audiohandler.capture();
            }
        }
        audiohandler.unlockDevices();

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}

static void PlaybackDevices()
{
    if (ButtonWidget(mute ? ICON_FA_VOLUME_XMARK : ICON_FA_VOLUME_OFF))
    {
        audiohandler.enumerate();
        ImGui::OpenPopup("##PlaybackDevicesPopup");
    }

    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right, false))
        ToggleMute();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(menu_spacing, menu_spacing));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(menu_spacing, menu_spacing));
    if (ImGui::BeginPopup("##PlaybackDevicesPopup"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, ImVec2(0.5f, 0.75f));
        ImGui::SeparatorText("Playback devices");
        ImGui::PopStyleVar();
        auto devices = audiohandler.getPlaybackDevices();
        for (size_t n = 0; n < devices.list.size(); n++)
        {
            bool is_selected = devices.list[n].name == devices.selectedName;
            if (ImGui::MenuItem(TruncateUTF8String(devices.list[n].name, 60).c_str(), "", is_selected) && !is_selected)
                audiohandler.setPreferredPlaybackDevice(devices.list[n].name.c_str());
        }
        audiohandler.unlockDevices();

        float vol;
        audiohandler.getPlaybackVolumeFactor(vol);
        vol *= 100.0f;
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::SliderFloat("##VolumeControl", &vol, 0.0f, 100.0f, "Volume %.0f%%", ImGuiSliderFlags_AlwaysClamp))
        {
            play_volume = vol / 100.0f;
            mute = false;
            AdjustVolume();
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}

static void AudioControl()
{
    // align cursor to pixel boundary for the record dot to align properly
    ImVec2 pos = ImGui::GetCursorPos();
    pos.x = std::roundf(pos.x);
    pos.y = std::roundf(pos.y);
    ImGui::SetCursorPos(pos);

    // stop button
    bool can_stop = ah_state.isPlaying() || ah_state.isRecording();
    if (!can_stop)
        ImGui::BeginDisabled();
    if (ButtonWidget(ICON_FA_CIRCLE_STOP))
        Capture();
    if (!can_stop)
        ImGui::EndDisabled();

    // record button
    bool can_record = !ah_state.isPlaying();
    ImGui::SameLine(0, widget_margin);
    // custom render the dot
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImColor color = UI_colors[UIIdxRecord];
        float mul = 0.5f + 0.5f * can_record;
        if (ah_state.isRecording())
            mul *= std::sin((float)ImGui::GetTime() * (float)M_PI * 2.0f) * 0.2f + 0.8f;
        color.Value.x *= mul;
        color.Value.y *= mul;
        color.Value.z *= mul;
        draw_list->AddCircleFilled(ImGui::GetCursorPos() + widget_sz / 2, widget_sz.x * 0.15f, color);
    }
    if (!can_record)
        ImGui::BeginDisabled();
    if (ButtonWidget(ICON_FA_CIRCLE))
        Record();
    if (!can_record)
        ImGui::EndDisabled();

    // play / pause button
    ImGui::SameLine(0, widget_margin);
    if (ah_state.isPlaying() && ah_state.canPause())
    {
        if (ButtonWidget(ICON_FA_CIRCLE_PAUSE))
            Pause();
    }
    else
    {
        bool can_play = ah_state.hasPlaybackFile && !ah_state.isRecording();
        if (!can_play)
            ImGui::BeginDisabled();
        if (ButtonWidget(ICON_FA_CIRCLE_PLAY))
        {
            if (ah_state.canResume())
                Resume();
            else
                Play();
        }
        if (!can_play)
            ImGui::EndDisabled();
    }
}

static void TempoSettings()
{
    ImGui::Checkbox("Tempo grid", &tempo_grid);
    ImGui::SameLine(); ImGui::Checkbox("Metronome", &metronome);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(font_def_sz / 8 * ui_scale, font_def_sz / 8 * ui_scale));
    ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
    if (ImGui::ArrowButton("##TempoSlower", ImGuiDir_Left)) { tempo_val > TempoValueMin && tempo_val--; }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##TempoFaster", ImGuiDir_Right)) { tempo_val < TempoValueMax && tempo_val++; }
    ImGui::PopItemFlag();
    ImGui::SameLine();
    ImGui::PopStyleVar();
    ImGui::SliderInt("##TempoValue", &tempo_val, TempoValueMin, TempoValueMax, "BPM = %d", ImGuiSliderFlags_AlwaysClamp);

    ImGui::BeginGroup();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Meter");
    ImGui::SameLine(); ImGui::RadioButton("4/4", &tempo_meter, TempoMeter4_4);
    ImGui::SameLine(); ImGui::RadioButton("3/4", &tempo_meter, TempoMeter3_4);
    ImGui::SameLine(); ImGui::RadioButton("None", &tempo_meter, TempoMeterNone);
    ImGui::EndGroup();
}

static void TempoControl()
{
    static char label[] = "120\n ᴮᴾᴹ##TempoControl";

    snprintf(label, 4, "%3d", tempo_val);
    label[3] = '\n';
    ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(UI_colors[UIIdxWidgetText], 0.5f + 0.5f * (metronome || tempo_grid)));
    ImGui::PushStyleColor(ImGuiCol_Button, UI_colors[UIIdxWidget]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI_colors[UIIdxWidgetHovered]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI_colors[UIIdxWidgetActive]);
    if (ImGui::Button(label, tempo_sz))
    {
        ImGui::OpenPopup("##TempoPopup");
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing, ImVec2(0, 1.0f));
    }
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();

    if (ImGui::BeginPopup("##TempoPopup"))
    {
        ImGui::SeparatorText("Tempo");
        TempoSettings();
        ImGui::EndPopup();
    }
}

static void ScaleSelector(bool from_settings)
{
    ImGuiComboFlags flags = ImGuiComboFlags_None;
    if (!from_settings)
        flags |= ImGuiComboFlags_NoArrowButton;

    if (!from_settings)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(menu_spacing, menu_spacing));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(menu_spacing, menu_spacing));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(UI_colors[UIIdxWidgetText], 0.8f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, UI_colors[UIIdxWidget]);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, UI_colors[UIIdxWidgetHovered]);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, UI_colors[UIIdxWidgetActive]);
        ImGui::SetNextItemWidth(scale_sel_wdt);
    }
    if (ImGui::BeginCombo("##ScaleSelector", scale_str.c_str(), flags))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, UI_colors[UIIdxText]);
        for (int n = 0; n < IM_ARRAYSIZE(scale_list); n++)
        {
            const bool is_selected = scale_str.compare(0, std::string::npos, scale_list[n]) == 0;
            if (ImGui::Selectable(scale_list[n], is_selected))
            {
                scale_str = scale_list[n];
                UpdateScale();
            }

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::PopStyleColor();
        ImGui::EndCombo();
    }
    if (!from_settings)
    {
        ImGui::PopStyleColor(4);
        ImGui::PopStyleVar(2);
    }
}

static void HoldButton()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(UI_colors[UIIdxWidgetText], 0.5f + 0.5f * analyzer.on_hold()));
    ImGui::PushStyleColor(ImGuiCol_Button, UI_colors[UIIdxWidget]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UI_colors[UIIdxWidgetHovered]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, UI_colors[UIIdxWidgetActive]);
    if (ImGui::Button("HOLD", hold_btn_sz))
        ToggleHold();
    ImGui::PopStyleColor(4);
}

static void PlaybackProgress()
{
    progress_hgt = widget_margin * (0.4f + progress_hover * 0.5f);
    ImVec2 size = ImGui::GetWindowSize();
    ImVec2 pos = { 0, size.y - progress_hgt };
    size.y = progress_hgt;
    ImGui::SetCursorPos(pos);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, UI_colors[UIIdxWidgetHovered]);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, UI_colors[UIIdxProgress]);
    ImGui::ProgressBar((float)ah_pos / ah_len, size, "");
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    ImGui::SetCursorPos(pos); // revert cursor
    bool seek = ImGui::InvisibleButton("##Seek", size);

    bool hover = ImGui::IsItemHovered();
    static int grace = 0;
    if (hover != progress_hover)
    {
        // some magic number filtering
        grace += hover * 9 + 1; // 100 ms heatup, 1 second cooldown
        if (grace >= ImGui::GetIO().Framerate)
            progress_hover = hover;
    }
    else
        grace = 0;
    if (hover && progress_hover)
    {
        ImVec2 mouse_pos = ImGui::GetMousePos() - ImGui::GetWindowPos();
        uint64_t seek_to = std::min((uint64_t)(mouse_pos.x / size.x * ah_len), ah_len);
        ImGui::SetTooltip("%s", audiohandler.framesToTime(seek_to).c_str());
        if (seek)
            Seek(seek_to);
    }
}

static bool ColorPicker(const char *label, ImU32 &color, float split, bool alpha)
{
    float button_width(100.0f * ui_scale);
    static ImColor backup_color;
    bool ret = false;
    ImGuiColorEditFlags flags = alpha ? (ImGuiColorEditFlags)0 : ImGuiColorEditFlags_NoAlpha;
    ImColor col(color);

    ImGui::PushID(label);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    ImGui::SameLine(split >= 0.0f ? split : ImGui::GetContentRegionAvail().x + ImGui::GetCursorPos().x - button_width);
    if (ImGui::ColorButton("##ColorButton", col, flags, ImVec2(button_width, ImGui::GetFrameHeight())))
    {
        ImGui::OpenPopup("##PaletePicker");
        backup_color = color;
    }
    if (ImGui::BeginPopup("##PaletePicker"))
    {
        ImVec2 item_sz = ImVec2(20, 20) * ui_scale;
        constexpr size_t row_sz(4);

        float spacing = ImGui::GetStyle().ItemSpacing.y;
        ImVec2 preview_sz = (item_sz + ImGui::GetStyle().ItemSpacing) * row_sz;
        preview_sz.y *= font_def_sz / 32.0f; // adjust for best fit

        ImGui::TextUnformatted("Pick a color");
        ImGui::Separator();
        if (ImGui::ColorPicker4("##ColorPicker", (float*)&col, flags | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoSmallPreview))
        {
            color = col;
            ret = true;
        }
        ImGui::SameLine();

        ImGui::BeginGroup(); // Lock X position
        ImGui::TextUnformatted("Current");
        ImGui::ColorButton("##CurrentColor", col, flags | ImGuiColorEditFlags_NoPicker, preview_sz);
        ImGui::TextUnformatted("Previous");
        if (ImGui::ColorButton("##PreviousColor", backup_color, flags | ImGuiColorEditFlags_NoPicker, preview_sz))
        {
            color = backup_color;
            ret = true;
        }
        ImGui::Separator();
        ImGui::TextUnformatted("Palette");
        for (int n = 0; n < ColorCount; n++)
        {
            ImGui::PushID(n);
            if ((n % row_sz) != 0)
                ImGui::SameLine(0.0f, spacing);

            ImGuiColorEditFlags palette_button_flags = ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoSmallPreview;
            if (ImGui::ColorButton("##PaletteItem", ImColor(palette[n]), palette_button_flags, item_sz))
            {
                color = palette[n];
                ret = true;
            }
            ImGui::SetItemTooltip("%s", palette_names[n]);
            ImGui::PopID();
        }
        ImGui::EndGroup();

        ImGui::EndPopup();
    }
    ImGui::PopID();

    return ret;
}

static void InputControl()
{
    static bool mlblock = false;
    static bool psysfocus = true;

    bool hover = ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered();

    // Mouse
    if (hover)
    {
        ImGuiIO &io  = ImGui::GetIO();
        ImVec2 wsize = ImGui::GetWindowSize();

        // vertical zoom
        if (io.KeyMods & ImGuiMod_Ctrl && io.MouseWheel != 0.0f)
        {
            float y_zoomp = y_zoom;

            y_zoom += io.MouseWheel * PlotYZoomSpd;
            y_zoom  = FCLAMP(y_zoom, PlotYZoomMin, PlotYZoomMax);

            // zoom around mouse pos
            c_pos += (c_top - c_pos) * (1.0f - y_zoomp / y_zoom) * (1.0f - io.MousePos.y / wsize.y);
        }

        // horizontal zoom
        if (io.KeyMods & ImGuiMod_Shift && io.MouseWheel != 0.0f)
        {
            float x_zoomp = x_zoom;

            x_zoom += io.MouseWheel * PlotXZoomSpd;
            x_zoom  = FCLAMP(x_zoom, x_zoom_min, PlotXZoomMax);

            // zoom around mouse pos
            if (x_offset)
                x_offset += wsize.x / (x_zoomp * ui_scale) * (1.0f - x_zoomp / x_zoom) * (1.0f - io.MousePos.x / wsize.x);
        }

        // vertical scroll
        if (!mlblock && io.KeyMods == ImGuiMod_None && io.MouseWheel != 0.0f)
        {
            c_pos += io.MouseWheel * PlotYScrlSpd;
            y_ascrl_grace = ImGui::GetTime() + PlotAScrlGrace;
        }

        // left button
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            c_pos += ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).y / (y_zoom * font_grid_sz * ui_scale / c_dist);
            x_offset += ImGui::GetMouseDragDelta(ImGuiMouseButton_Left).x / (x_zoom * ui_scale);
            x_off_reset = false;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            y_ascrl_grace = ImGui::GetTime() + PlotAScrlGrace;
            mlblock = true;
        }

        if (!mlblock && click_hold && ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            TogglePause();
    }
    // handle nc activity
    if (ImGui::SysWndFocus != psysfocus || (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && (!hover || !ImGui::IsWindowFocused())))
        mlblock = true;
    else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
        mlblock = false;

    // Keyboard
    if (ImGui::IsWindowFocused())
    {
        // Pause
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
            TogglePause();

        // Autoscroll
        if (ImGui::IsKeyPressed(ImGuiKey_A, false))
            autoscroll = !autoscroll;

        // Fullscreen
        if (ImGui::IsKeyPressed(ImGuiKey_F, false))
            ToggleFullscreen();

        // Mute
        if (ImGui::IsKeyPressed(ImGuiKey_M, false))
            ToggleMute();

        // Always on top
        if (ImGui::IsKeyPressed(ImGuiKey_T, false))
            ImGui::SysSetAlwaysOnTop(!ImGui::SysIsAlwaysOnTop());

        // Stop
        if (ImGui::IsKeyPressed(ImGuiKey_S, false) && (ah_state.isPlaying() || ah_state.isRecording()))
            Capture();

        // Play
        if (ImGui::IsKeyPressed(ImGuiKey_P, false) && ah_state.hasPlaybackFile && !ah_state.isRecording())
            Play();

        // Record
        if (ImGui::IsKeyPressed(ImGuiKey_R, false) && !ah_state.isPlaying())
            Record();

        // Rewind
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
            Seek(-seek_step, true);

        // Fast forward
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
            Seek(seek_step, true);
    }

    psysfocus = ImGui::SysWndFocus;
}

static void Draw()
{
    double f_peak;
    size_t total_analyze_cnt, pitch_buf_pos;

    {
        // cache analyzer state
        std::lock_guard<std::mutex> lock(analyzer_mtx);
        f_peak = analyzer.get_peak_freq();
        total_analyze_cnt = analyzer.get_total_analyze_cnt();
        pitch_buf_pos = analyzer.get_pitch_buf_pos();
    }

    float c_peak = Analyzer::freq_to_cent(f_peak);
    if (c_peak >= 0.0f)
        c_peak += c_calib;

    ImVec2 wsize = ImGui::GetWindowSize();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // adjust plot limits
    float step = y_zoom * font_grid_sz * ui_scale;
    float c2y_mul = step / c_dist;
    float c2y_off = wsize.y + c_pos * c2y_mul;

    c_max = (float)PlotRangeMax - (wsize.y - rullbl_sz.y / 2.0f) / c2y_mul;
    if (c_pos > c_max)
    {
        c2y_off += (c_max - c_pos) * c2y_mul;
        c_pos = c_max;
    }
    c_min = (float)PlotRangeMin - rullbl_sz.y / c2y_mul / 2.0f;
    if (c_pos < c_min)
    {
        c2y_off += (c_min - c_pos) * c2y_mul;
        c_pos = c_min;
    }
    c_top = c_pos + wsize.y / c2y_mul;

    // do autoscrolling
    if (autoscroll && !analyzer.on_hold() && ah_state.isActive() && c_peak >= 0.0 && y_ascrl_grace < ImGui::GetTime() && x_offset == 0.0f)
    {
        static int velocity = 0;

        if (c_peak < c_pos + (float)PlotAScrlMar)
            velocity -= y_ascrl_vel;
        else if (c_peak > c_top - (float)PlotAScrlMar)
            velocity += y_ascrl_vel;
        else
            velocity = 0;
        c_pos += (float)velocity;
    }

    // set plot boundaries and feature positions
    // plot x near ruler; x far of ruler; x center, considering ruler position
    float x_near, x_far, x_center;
    // pitch data last position, first position
    float x_left, x_right;
    // octave boundary notch size and direction
    float notch = scale_chroma ? 0.0f : 4.0f * ui_scale;
    // ruler label position and align
    float x_rullbl; int align_rullbl;
    if (rul_right)
    {
        x_near       = wsize.x - rullbl_sz.x;
        x_far        = 0.0f;
        x_left       = x_far + lut_linew[PlotIdxPitch] * ui_scale / 2; // + pitch line width
        x_right      = x_near - lut_linew[PlotIdxTonic] * ui_scale;    // - split line width
        x_rullbl     = x_near + rul_margin * ui_scale;
        align_rullbl = TextAlignLeft;
    }
    else
    {
        x_near       = rullbl_sz.x;
        x_far        = wsize.x;
        x_left       = x_near + lut_linew[PlotIdxTonic] * ui_scale;
        x_right      = x_far - lut_linew[PlotIdxPitch] * ui_scale / 2;
        x_rullbl     = x_near - rul_margin * ui_scale;
        align_rullbl = TextAlignRight;
        notch *= -1.0f;
    }
    x_center = std::roundf((x_near + x_far) / 2.0f);

    float x_span = (x_right - x_left) / ui_scale;
    // limit offset
    if (x_off_reset)
    {
        x_offset = (int)(x_offset - std::fmin(50.0f, x_offset / 4.0f)); // just arbitrary easing
        x_off_reset = x_offset > 0;
    }
    x_offset = FCLAMP(x_offset, 0.0f, (float)(Analyzer::PITCH_BUF_SIZE - 2 - (int)(x_span / x_zoom)));
    // adjust horizontal zoom
    x_zoom_min = std::fmax(PlotXZoomMin, x_span / (float)(Analyzer::PITCH_BUF_SIZE - 2)); // limit zoom by pitch buffer size; ignore current pitch buffer element
    x_zoom = std::fmax(x_zoom, x_zoom_min);
    float x_zoom_scaled = x_zoom * ui_scale;

    // horizontal grid / metronome
    if (tempo_grid || metronome)
    {
        const float intervals_per_bpm = 60.0f /* seconds */ / (float)tempo_val / Analyzer::get_interval_sec();
        const size_t total_count_adjusted = total_analyze_cnt - 1; // ignore current pitch buffer element

        if (metronome)
        {
            float trig_dist = 7.0f - (float)tempo_val / 60.0f;
            float trig = (std::fmod(std::fmod((double)total_count_adjusted, intervals_per_bpm) + trig_dist, intervals_per_bpm) - trig_dist) / trig_dist;
            if (trig <= 1.0f)
                draw_list->AddRect(ImVec2(0.0f, 0.0f), ImVec2(wsize.x, wsize.y), plot_colors[PlotIdxMetronome], 0.0f, ImDrawFlags_None, (1.0f - std::fabs(trig)) * 20.0f * ui_scale);
        }

        if (tempo_grid)
        {
            size_t beat_cnt = ((double)total_count_adjusted - (int)x_offset) / intervals_per_bpm;
            float offset = x_right - std::fmod((double)(total_count_adjusted - (int)x_offset), intervals_per_bpm) * x_zoom_scaled;
            while(offset > x_left)
            {
                draw_list->AddLine(ImVec2(offset, 0.0f), ImVec2(offset, wsize.y), plot_colors[PlotIdxTempo],
                    lut_linew[PlotIdxTempo + (tempo_meter > 0 && beat_cnt % tempo_meter == 0)] * ui_scale);
                offset -= x_zoom_scaled * intervals_per_bpm;
                beat_cnt--;
            }
        }
    }

    // draw vertical grid
    ImGui::PushFont(font_grid);
    int key_bot = (int)(c_pos / c_dist);
    int key_top = key_bot + (int)(wsize.y / step) + 1;
    for (int key = key_bot; key <= key_top; ++key)
    {
        float y = std::roundf(c2y_off - c_dist * key * c2y_mul);
        int note =   (key + 24) % 12;
        int note_scaled = (note - scale_key + 12) % 12;
        int octave = (key + 24) / 12 - 2 + oct_offset; // minus negative truncation compensation ( - 24/12)
        int line_idx = scale_chroma ? note + 8 : lut_number[scale_major][note_scaled];
        int label_idx = lut_number[1][note]; // Major scale

        // line
        if (semi_lines || line_idx)
            draw_list->AddLine(ImVec2(x_far, y), ImVec2(x_near + notch * !(line_idx - 1), y), plot_colors[line_idx], lut_linew[line_idx] * ui_scale);

        // label
        if (semi_lbls || label_idx)
            AddNoteLabel(ImVec2(x_rullbl, y), align_rullbl, TextAlignMiddle, note, semi_lbls << (int)!!label_idx, octave, plot_colors[line_idx], draw_list);
    }
    ImGui::PopFont();

    // draw split line
    draw_list->AddLine(ImVec2(x_near, 0), ImVec2(x_near, wsize.y), plot_colors[PlotIdxTonic], lut_linew[PlotIdxTonic] * ui_scale);

    // plot the data
    {
        int max_cnt = (int)((x_right - x_left) / x_zoom_scaled);
        float line_w = lut_linew[PlotIdxPitch] * ui_scale;
        ImU32 color = plot_colors[PlotIdxPitch];
        float pp = -1.0f;
        ImVec2 pv;

        auto pitch_buf = analyzer.get_pitch_buf();
        size_t pitch_buf_offset = pitch_buf_pos + Analyzer::PITCH_BUF_SIZE - 1 - (int)x_offset; // ignore current pitch buffer element
        for(int i = 0; i <= max_cnt; ++i) // inclusive
        {
            float p = pitch_buf[(pitch_buf_offset - i) % Analyzer::PITCH_BUF_SIZE];
            if (p >= 0.0f)
            {
                p += c_calib;
                ImVec2 v(x_right - x_zoom_scaled * i, std::roundf(c2y_off - p * c2y_mul));
                if (pp >= 0.0f && std::fabs(p - pp) <= dc_max)
                    draw_list->AddLine(v, pv, color, line_w);
                pv = v;
            }
            pp = p;
        }
    }

    // update mean frequency buffer
    f_peak_buf[f_peak_buf_pos++] = f_peak;
    f_peak_buf_pos = f_peak_buf_pos % f_peak_buf.size();
    if (c_peak >= 0.0f)
    {
        int meandiv = 0;
        double f_mean = 0.0;
        for(size_t i = 0; i < f_peak_buf.size(); ++i)
        {
            double freq = f_peak_buf[i];
            if (freq > 0.0)
            {
                f_mean += freq;
                ++meandiv;
            }
        }

        // calculate note and octave of current mean frequency
        f_mean /= meandiv;
        float c_mean = Analyzer::freq_to_cent(f_mean) + c_calib;
        int note = (int)((double)c_mean + 0.5) + 50; // round up
        int octave = note / 1200 + oct_offset;
        note = note % 1200 / 100;

        // display note
        ImGui::PushFont(font_pitch);
        AddNoteLabel(ImVec2(x_center + x_peak_off, top_feat_pos * ui_scale), TextAlignRight, TextAlignBottom,
                         note, 1 << (int)!!lut_number[1][note], octave, plot_colors[PlotIdxNote], draw_list);
        ImGui::PopFont();
        // draw tuner
        if (show_tuner)
        {
            float y_tuner = (top_feat_pos + 6.0f) * ui_scale;
            float y_long  = y_tuner + 7.0f * ui_scale;
            float y_short = y_tuner + 4.0f * ui_scale;
            float zoom = 1.75f * ui_scale;
            // triangle mark
            draw_list->AddTriangleFilled(ImVec2(x_center - 6.0f * ui_scale, top_feat_pos * ui_scale),
                                         ImVec2(x_center + 6.0f * ui_scale, top_feat_pos * ui_scale),
                                         ImVec2(x_center, y_tuner - 1.0f * ui_scale),
                                         plot_colors[PlotIdxTuner]);

            ImGui::PushFont(font_tuner);
            float stop = c_mean + 100.0f;
            int pos = (int)((c_mean - 100.0f) / 10.0f) * 10;
            ImU32 color = plot_colors[PlotIdxTuner];
            for ( ; (float)pos <= stop; pos += 10)
            {
                float x_pos = std::roundf(x_center + ((float)pos - c_mean) * zoom);
                float linew = lut_linew[PlotIdxSemitone] * ui_scale;
                switch (pos % 100)
                {
                case 0:
                {
                    int note = pos % 1200 / 100;

                    // tuner note
                    AddNoteLabel(ImVec2(x_pos, y_long), TextAlignCenter, TextAlignTop,
                         note, 1 - !!lut_number[1][note], -1, color, draw_list);

                    // long thick tick
                    linew = lut_linew[PlotIdxSupertonic] * ui_scale;
                } // fall through
                case 50:
                    // long thin tick
                    draw_list->AddLine(ImVec2(x_pos, y_tuner), ImVec2(x_pos, y_long), color, linew);
                    break;
                default:
                    // short thin tick
                    draw_list->AddLine(ImVec2(x_pos, y_tuner), ImVec2(x_pos, y_short), color, linew);
                }
            }
            ImGui::PopFont();
        }

        if (show_freq)
            AddFmtTextAligned(ImVec2(x_center + x_peak_off * 2.5f, top_feat_pos * ui_scale), TextAlignLeft, TextAlignBottom,
                plot_colors[PlotIdxTuner], draw_list, "%4.0fHz", f_mean);
    }
}

static void ProcessLog()
{
    constexpr size_t maxMsgs = 3;
    constexpr float msgTimeoutSec = 5.0f;
    constexpr float faderate = msgTimeoutSec / 0.3f /* duration, seconds */ / 2 /* fadein + fadeout */;
    constexpr float alpha_step = maxMsgs <= 1 ? 0.0f : 0.4f / (maxMsgs - 1);

    static unsigned long long nextN = maxMsgs;
    static double MsgTimeout[maxMsgs] = {};
    static const ImU32* const msg_colors[LOG_MAXLVL + 1] = { &UI_colors[UIIdxMsgErr], &UI_colors[UIIdxMsgWarn], &UI_colors[UIIdxWidgetText], &UI_colors[UIIdxMsgDbg] };

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowSize();
    pos.x /= 2;
    pos.y -= widget_sz.y + widget_margin * 1.5f + font_def_sz;

    float alpha = 1.0f;
    size_t timedOut = 0;
    auto& entries = msg_log.LockEntries();
    for ( auto &entry : entries )
    {
        if (entry.N > nextN)
            continue;
        if (entry.N <= nextN - maxMsgs)
            break;

        double time = ImGui::GetTime();
        size_t curMsg = entry.N % maxMsgs;
        if (MsgTimeout[curMsg] == 0.0)
            MsgTimeout[curMsg] = time + msgTimeoutSec;
        else if (time >= MsgTimeout[curMsg])
        {
            timedOut++;
            MsgTimeout[curMsg] = 0.0;
            continue;
        }

        ImU32 color = ImGui::GetColorU32(*msg_colors[entry.Lvl],
                alpha * std::fmin(faderate - std::fabs(std::fmod((float)(MsgTimeout[curMsg] - time) * faderate * 2.0f / msgTimeoutSec, faderate * 2.0f) - faderate), 1.0f));
        AddTextAligned(pos, TextAlignCenter, TextAlignMiddle, color, draw_list, entry.Msg.get());

        pos.y -= font_def_sz * 1.5f;
        alpha -= alpha_step;
    }
    msg_log.UnlockEntries();
    nextN += timedOut;
}

// from src/plug.c:fft_analyze() @ https://github.com/tsoding/musializer
static void SpectrumWindow(bool *show)
{
    static const double fft_binw = Analyzer::SAMPLE_FREQ / Analyzer::FFTSIZE;
    static const double step = std::pow(2.0, 1.0/12.0);
    static const double fbot = floor(Analyzer::Analyzer::FREQ_C2 / step) * step;
    static const double ftop = std::fmin(Analyzer::SAMPLE_FREQ / 2.0, ceil(Analyzer::FREQ_C7 / step) * step);
    static const size_t keycnt = (size_t)((std::log2(ftop) - std::log2(fbot)) * 12.0 + 1);
    static std::unique_ptr<float[]> out_log(new float[keycnt]());
    static std::unique_ptr<float[]> out_smooth(new float[keycnt]());
    static size_t p_total_cnt = 0;
    static double f_peak = -1.0;

    ImVec2 wsize = ImVec2(std::ceil(8.0f * ui_scale) * keycnt + ImGui::GetStyle().WindowPadding.x * 2, 200.0f * ui_scale);
    ImGui::SetNextWindowSizeConstraints(ImVec2(wsize.x, wsize.y), ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()));
    ImGui::SetNextWindowSize(wsize, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(
        ImVec2(rullbl_sz.x * !rul_right + widget_margin, (top_feat_pos + 11.0f /* tuner offset + tuner long tick */ + font_tuner_sz) * ui_scale + widget_margin),
        ImGuiCond_Appearing);
    if (!ImGui::Begin("Spectrum C2..C7", show, ImGuiWindowFlags_NoFocusOnAppearing))
    {
        ImGui::End();
        return;
    }

    // "Squash" into keys
    float max_amp = 1.0f;
    {
        std::lock_guard<std::mutex> lock(analyzer_mtx);
        size_t total_cnt = analyzer.get_total_analyze_cnt();
        if (total_cnt != p_total_cnt)
        {
            f_peak = analyzer.get_peak_freq();
            auto fft_buf = analyzer.get_fft_buf();
            size_t cnt = 0;
            for (double f = fbot; f <= ftop; f *= step)
            {
                size_t q = (size_t)round(f / fft_binw), qtop = (size_t)round(f * step / fft_binw);
                float a = 0.0f;
                do
                {
                    float b = (float)std::log(Analyzer::power(fft_buf[q * 2], fft_buf[q * 2 + 1]));
                    if (b > a) a = b;
                } while (++q < qtop);
                if (max_amp < a) max_amp = a;
                IM_ASSERT(cnt < keycnt);
                out_log[cnt++] = a;
            }

            p_total_cnt = total_cnt;
        }
    }

    // Normalize Frequencies to 0..1 range
    if (max_amp > 1.0f)
    {
        for (size_t i = 0; i < keycnt; ++i)
            out_log[i] /= max_amp;
    }

    // Smooth out and plot the values
    auto *draw_list = ImGui::GetWindowDrawList();
    ImVec2 rmin = ImGui::GetWindowPos() + ImGui::GetCursorPos();
    ImVec2 rmax = rmin + ImGui::GetContentRegionAvail();
    float adv = floor((rmax.x - rmin.x) / keycnt);
    rmax.x = rmin.x + adv * keycnt; // adjust for rounded cell size
    float margin = 2.0f * ui_scale;
    float width = adv - margin;
    float height = rmax.y - rmin.y;
    int n_peak = f_peak >= 0.0 ? (int)std::round((std::log2(f_peak) - std::log2(fbot)) * 12.0) : -1;
    rmax.x = rmin.x + width;
    double dt = 1.0 / ImGui::GetIO().Framerate;
    constexpr float smoothness = 10;
    for (size_t i = 0; i < keycnt; ++i)
    {
        out_smooth[i] += (out_log[i] - out_smooth[i]) * smoothness * dt;
        rmin.y = rmax.y - std::fmax(margin, out_smooth[i] * height);
        draw_list->AddRectFilled(rmin, rmax, (int)i == n_peak ? plot_colors[PlotIdxPitch] : UI_colors[UIIdxText]);
        rmin.x += adv;
        rmax.x += adv;
    }

    ImGui::End();
}

static void SettingsWindow()
{
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImGui::GetMainViewport()->Size * 0.8f);
    if (!HandlePopupState("Settings", wnd_settings, ImGui::GetMainViewport()->GetCenter(), ImVec2(0.5f, 0.5f), ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (wnd_settings == wsClosing)
            SaveSettings();
        return;
    }

    // clip the separator
    ImVec2 btn_sz = ImGui::CalcTextSize("defaults") + ImGui::GetStyle().FramePadding * 2;
    ImVec2 b_pos = ImGui::GetCursorPos();
    b_pos.x += ImGui::GetContentRegionAvail().x - btn_sz.x;
    ImVec2 cl_min = ImGui::GetCursorScreenPos();
    ImVec2 cl_max = ImGui::GetWindowPos() + b_pos;
    cl_max.x -= ImGui::GetStyle().ItemSpacing.x;
    cl_max.y += btn_sz.y;
    ImGui::PushClipRect(cl_min, cl_max, true);
    ImGui::SeparatorText("Settings");
    ImGui::PopClipRect();
    ImGui::SetCursorPos(b_pos);

    static double timeout = 0.0;
    double time = ImGui::GetTime();
    if (time >= timeout)
        timeout = 0.0;
    bool confirm = timeout > 0.0;
    if (confirm)
    {
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32(200,   0,  40, 200));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(255,  40,  40, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive , IM_COL32(220,   0,   0, 255));
    }
    if (ImGui::Button(confirm ? "confirm" : "defaults", btn_sz))
    {
        if (time < timeout)
        {
            timeout = 0.0;
            ResetSettings();
        }
        else
            timeout = time + 3.0;
    }
    if (confirm)
        ImGui::PopStyleColor(3);

    if (ro_config)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, UI_colors[UIIdxMsgWarn]);
        ImGui::TextUnformatted("Config is read-only, settings are not persistent");
        ImGui::PopStyleColor();
    }

    ImGui::PushItemWidth(-FLT_MIN);

    // Analyzer control
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Volume threshold");
        ImGui::SameLine();
        if (ImGui::SliderFloat("##VolumeThreshold", &vol_thres, VolThresMin, VolThresMax, "%.2f", ImGuiSliderFlags_AlwaysClamp))
            analyzer.set_threshold((double)vol_thres);
    }

    // grid control
    {
        ImGui::Checkbox("Semitone lines", &semi_lines);
        static bool p_semi_lbls = semi_lbls;
        ImGui::SameLine(); ImGui::Checkbox("Semitone labels", &semi_lbls);
        if (p_semi_lbls != semi_lbls)
        {
            p_semi_lbls = semi_lbls;
            fonts_reloaded = true;
        }
        ImGui::Checkbox("Ruller on the right side", &rul_right);
    }

    // octave reference
    {
        ImGui::BeginGroup();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Octave reference: 440Hz =");
        ImGui::SameLine(); ImGui::RadioButton("A4", &oct_offset, OctOffsetA4);
        ImGui::SameLine(); ImGui::RadioButton("A3", &oct_offset, OctOffsetA3);
        ImGui::EndGroup();
    }

    // calibration
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(oct_offset == OctOffsetA4 ? "Calibration A4 =" : "Calibration A3 =");
        ImGui::SameLine();
        if (ImGui::SliderFloat("##Calibration", &calibration, PitchCalibMin, PitchCalibMax, "%.1f Hz", ImGuiSliderFlags_AlwaysClamp))
            UpdateCalibration();
    }

    // transpose
    {
        bool update = false;
        ImGui::TextUnformatted("Transpose");
        ImGui::Indent();
        ImGui::BeginGroup();
        update |= ImGui::RadioButton("C Inst.", &transpose, TransposeC);
        ImGui::SameLine(); update |= ImGui::RadioButton("B♭ Inst.", &transpose, TransposeBb);
        ImGui::SameLine(); update |= ImGui::RadioButton("E♭ Inst.", &transpose, TransposeEb);
        ImGui::SameLine(); update |= ImGui::RadioButton("F Inst.", &transpose, TransposeF);
        ImGui::EndGroup();
        ImGui::Unindent();
        if (update)
            UpdateCalibration();
    }

    // autoscroll
    {
        ImGui::AlignTextToFramePadding();
        ImGui::Checkbox("Enable autoscroll", &autoscroll);
        if (!autoscroll)
            ImGui::BeginDisabled();
        ImGui::SameLine(); ImGui::SliderInt("##ScrollVelocity", &y_ascrl_vel, PlotAScrlVelMin, PlotAScrlVelMax, "velocity = %d", ImGuiSliderFlags_AlwaysClamp);
        if (!autoscroll)
            ImGui::EndDisabled();
    }

    // pitch and tuner control
    {
        ImGui::TextUnformatted("Note display");
        ImGui::Indent();
        ImGui::Checkbox("Tuner", &show_tuner);
        ImGui::SameLine(); ImGui::Checkbox("Frequency", &show_freq);
        ImGui::SameLine();
        int samples = (int)f_peak_buf.size();
        if (ImGui::SliderInt("##Smoothing", &samples, TunerSmoothMin, TunerSmoothMax, "smoothing = %d", ImGuiSliderFlags_AlwaysClamp))
            UpdatePeakBuf(samples);
        ImGui::Unindent();
    }

    // note naming scheme
    {
        ImGui::BeginGroup();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Note names");
        ImGui::Indent();
        fonts_reloaded |= ImGui::RadioButton("English:     C, D, E, F, G, A, B", &note_names, NoteNamesEnglish);
        fonts_reloaded |= ImGui::RadioButton("Romance:  Do, Re, Mi, Fa, Sol, La, Si", &note_names, NoteNamesRomance);
        ImGui::Unindent();
        ImGui::EndGroup();
    }

    // tempo control
    {
        ImGui::TextUnformatted("Tempo");
        ImGui::Indent();
        TempoSettings();
        ImGui::Unindent();
    }

    // scale selector
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Scale");
        ImGui::SameLine(); ScaleSelector(true /* from_settings */);
    }

    // UI behavior
    {
        ImGui::TextUnformatted("UI behavior");
        ImGui::Indent();
        ImGui::Checkbox("HOLD", &but_hold);
        ImGui::SameLine(); ImGui::Checkbox("Scale", &but_scale);
        ImGui::SameLine(); ImGui::Checkbox("Tempo", &but_tempo);
        ImGui::Checkbox("Device selector", &but_devices);
        ImGui::SameLine(); ImGui::Checkbox("Click2Hold", &click_hold);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Seek step, seconds");
        ImGui::SameLine(); ImGui::SliderFloat("##SeekStep", &seek_step, SeekStepMin, SeekStepMax, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        if (ImGui::Checkbox("Custom scaling", &custom_scaling))
            ImGui::AppReconfigure = true;
        if (!custom_scaling)
            ImGui::BeginDisabled();
        ImGui::SameLine(); ImGui::SliderFloat("##CustomScale", &custom_scale, CustomScaleMin, CustomScaleMax, "%.1f", ImGuiSliderFlags_AlwaysClamp);
        // update configuration only after user is done with the setting
        if (ImGui::IsItemDeactivatedAfterEdit())
            ImGui::AppReconfigure = true;
        if (!custom_scaling)
            ImGui::EndDisabled();
        ImGui::Unindent();
    }

    // record files directory
    {
        ImGui::TextUnformatted("Record directory");
        ImGui::Indent();
        ImGui::SetNextItemWidth(0.0f - ImGui::GetFrameHeight() - font_def_sz / 8 * ui_scale);
        ImGui::InputText("##RecordDir", record_dir, IM_ARRAYSIZE(record_dir), ImGuiInputTextFlags_ElideLeft);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(font_def_sz / 8 * ui_scale, font_def_sz / 8 * ui_scale));
        ImGui::SameLine();
        ImGui::PopStyleVar();
        if (ImGui::Button("...##RecordDirBrowse", ImVec2(ImGui::GetFrameHeight(), 0.0f)))
            BrowseDirectory();
        ImGui::Unindent();
    }

    // color settings
    {
        ImGui::TextUnformatted("Colors");

        ImGui::Indent();
        static double tooltiptime = 0.0;
        ImU32 bgcolor = ImColor(ImGui::SysWndBgColor);
        if (ColorPicker("Background", bgcolor, -FLT_MIN, true))
        {
            ImVec4 newcol = ImColor(bgcolor);
            if (newcol.w < 1.0f && ImGui::SysWndBgColor.w == 1.0f)
                tooltiptime = time + 2.0;
            ImGui::SysWndBgColor = newcol;
        }
        if (tooltiptime > time)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, UI_colors[UIIdxMsgWarn]);
            ImGui::SetTooltip("Enabling transparency\nrequires app restart");
            ImGui::PopStyleColor();
        }
        ColorPicker("Pitch", plot_colors[PlotIdxPitch], -FLT_MIN);
        ColorPicker("Tempo", plot_colors[PlotIdxTempo], -FLT_MIN);
        ColorPicker("Metronome", plot_colors[PlotIdxMetronome], -FLT_MIN);
        ColorPicker("Note", plot_colors[PlotIdxNote], -FLT_MIN);
        ColorPicker("Tuner", plot_colors[PlotIdxTuner], -FLT_MIN);
        ImGui::Unindent();

        enum { CloseNone = 0, CloseScale, CloseChromatic };
        static int closenode = CloseNone;
        if (closenode == CloseScale)
            ImGui::SetNextItemOpen(false);
        if (ImGui::TreeNodeEx("Scale##Colors", scale_chroma ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_DefaultOpen))
        {
            ColorPicker("1st Tonic", plot_colors[PlotIdxTonic], -FLT_MIN);
            ColorPicker("2nd Supertonic", plot_colors[PlotIdxSupertonic], -FLT_MIN);
            ColorPicker("3rd Mediant", plot_colors[PlotIdxMediant], -FLT_MIN);
            ColorPicker("4th Subdominant", plot_colors[PlotIdxSubdominant], -FLT_MIN);
            ColorPicker("5th Dominant", plot_colors[PlotIdxDominant], -FLT_MIN);
            ColorPicker("6th Subtonic", plot_colors[PlotIdxSubtonic], -FLT_MIN);
            ColorPicker("7th Leading", plot_colors[PlotIdxLeading], -FLT_MIN);
            ColorPicker("Semitone", plot_colors[PlotIdxSemitone], -FLT_MIN);

            closenode = CloseChromatic;
            ImGui::TreePop();
        }

        if (closenode == CloseChromatic)
            ImGui::SetNextItemOpen(false);
        if (ImGui::TreeNodeEx("Chromatic##Colors", scale_chroma ? ImGuiTreeNodeFlags_DefaultOpen : ImGuiTreeNodeFlags_None))
        {
            ColorPicker("C", plot_colors[PlotIdxChromaC], -FLT_MIN);
            ColorPicker("C♯/D♭", plot_colors[PlotIdxChromaCsBb], -FLT_MIN);
            ColorPicker("D", plot_colors[PlotIdxChromaD], -FLT_MIN);
            ColorPicker("D♯/E♭", plot_colors[PlotIdxChromaDsEb], -FLT_MIN);
            ColorPicker("E", plot_colors[PlotIdxChromaE], -FLT_MIN);
            ColorPicker("F", plot_colors[PlotIdxChromaF], -FLT_MIN);
            ColorPicker("F♯/G♭", plot_colors[PlotIdxChromaFsGb], -FLT_MIN);
            ColorPicker("G", plot_colors[PlotIdxChromaG], -FLT_MIN);
            ColorPicker("G♯/A♭", plot_colors[PlotIdxChromaGsAb], -FLT_MIN);
            ColorPicker("A", plot_colors[PlotIdxChromaA], -FLT_MIN);
            ColorPicker("A♯/B♭", plot_colors[PlotIdxChromaAsBb], -FLT_MIN);
            ColorPicker("B", plot_colors[PlotIdxChromaB], -FLT_MIN);

            closenode = CloseScale;
            ImGui::TreePop();
        }
    }

    ImGui::PopItemWidth();

    ImGui::EndPopup();
}

//-----------------------------------------------------------------------------
// [SECTION] About Window / ShowAboutWindow()
// Access from Dear ImGui Demo -> Tools -> About
//-----------------------------------------------------------------------------

#define LINK(url, text, descr) do { ImGui::TextLinkOpenURL(text, url); ImGui::SameLine(); ImGui::TextUnformatted(descr); } while(0)
void ImGui::ShowAboutWindow(_UNUSED_ bool* p_open)
{
    if (!HandlePopupState("About imVocalPitchMonitor", wnd_about, ImGui::GetMainViewport()->GetCenter(), ImVec2(0.5f, 0.5f)))
        return;

    constexpr const char* VocalPithMonitorURL = "https://play.google.com/store/apps/details?id=com.tadaoyamaoka.vocalpitchmonitor";
    constexpr const char* ImGuiURL = "https://github.com/ocornut/imgui";
    constexpr const char* miniaudioURL = "https://miniaud.io";
    constexpr const char* xiphURL = "https://xiph.org";
    constexpr const char* fft4gURL = "https://github.com/YSRKEN/Ooura-FFT-Library-by-Other-Language";
    constexpr const char* simpleiniURL = "https://github.com/brofield/simpleini";
    constexpr const char* portable_file_dialogsURL = "https://github.com/samhocevar/portable-file-dialogs" ;
    constexpr const char* poplURL = "https://github.com/badaix/popl";
    constexpr const char* DejaVuURL = "https://dejavu-fonts.github.io";
    constexpr const char* Font_AwesomeURL = "https://fontawesome.com";

    ImGui::Text("imVocalPitchMonitor %s", VER_VERSION_DISPLAY);
    ImGui::Text("%s", VER_DATE_AUTHOR);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, -1.0f));
    ImGui::TextUnformatted("Port*");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("All core features code was stolen from the decompiled original APK.");
    ImGui::SameLine(); ImGui::TextUnformatted(" to PC of ");
    ImGui::SameLine();
    ImGui::PopStyleVar();
    ImGui::TextLinkOpenURL("VocalPitchMonitor", VocalPithMonitorURL);
    ImGui::SameLine();
    ImGui::TextUnformatted("Android app");
    ImGui::TextUnformatted("by Tadao Yamaoka.");
    ImGui::TextUnformatted("Without his hard work this project wouldn't be possible.");

    ImGui::TextUnformatted("Other software used by this project:");
    ImGui::Indent();
    LINK(ImGuiURL, "ImGui",  "© Omar Cornut" );
    LINK(miniaudioURL, "miniaudio",  "© David Reid" );
 #if defined(HAVE_OPUS)
    LINK(xiphURL, "libogg, opus, opusfile",  "© Xiph.Org Foundation" );
 #endif // HAVE_OPUS
    LINK(fft4gURL, "fft4g",  "© Takuya OOURA" );
    LINK(simpleiniURL, "simpleini",  "© Brodie Thiesfield" );
    LINK(portable_file_dialogsURL, "portable-file-dialogs",  "© Sam Hocevar" );
    LINK(poplURL, "Program Options Parser Library",  "© Johannes Pohl" );
    LINK(DejaVuURL, "DejaVu fonts",  "© DejaVu fonts team" );
    LINK(Font_AwesomeURL, "Font Awesome",  "© Fonticons, Inc." );
    ImGui::Unindent();
    ImGui::TextUnformatted("Command line options:");
    ImGui::Indent();
    ImGui::PushFont(font_grid);
    ImGui::TextUnformatted(cmd_options.c_str());
    ImGui::PopFont();
    ImGui::Unindent();

    static bool show_config_info = false;
    ImGui::Checkbox("Config/Build Information", &show_config_info);
    if (show_config_info)
    {
        ImGuiIO& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();

        bool copy_to_clipboard = ImGui::Button("Copy to clipboard");
        ImVec2 child_size = ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 10);
        ImGui::BeginChild(ImGui::GetID("cfg_infos"), child_size, ImGuiChildFlags_FrameStyle);
        if (copy_to_clipboard)
        {
            ImGui::LogToClipboard();
            ImGui::LogText("```\n"); // Back quotes will make text appears without formatting when pasting on GitHub
        }

        ImGui::Text("GUI framework is Dear ImGui %s (%d)", IMGUI_VERSION, IMGUI_VERSION_NUM);
        ImGui::Separator();
        ImGui::Text("sizeof(size_t): %d, sizeof(ImDrawIdx): %d, sizeof(ImDrawVert): %d", (int)sizeof(size_t), (int)sizeof(ImDrawIdx), (int)sizeof(ImDrawVert));
        ImGui::Text("define: __cplusplus=%d", (int)__cplusplus);
 #ifdef IMGUI_DISABLE_OBSOLETE_FUNCTIONS
        ImGui::Text("define: IMGUI_DISABLE_OBSOLETE_FUNCTIONS");
 #endif
 #ifdef IMGUI_DISABLE_OBSOLETE_KEYIO
        ImGui::Text("define: IMGUI_DISABLE_OBSOLETE_KEYIO");
 #endif
 #ifdef IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS
        ImGui::Text("define: IMGUI_DISABLE_WIN32_DEFAULT_CLIPBOARD_FUNCTIONS");
 #endif
 #ifdef IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS
        ImGui::Text("define: IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS");
 #endif
 #ifdef IMGUI_DISABLE_WIN32_FUNCTIONS
        ImGui::Text("define: IMGUI_DISABLE_WIN32_FUNCTIONS");
 #endif
 #ifdef IMGUI_DISABLE_DEFAULT_FORMAT_FUNCTIONS
        ImGui::Text("define: IMGUI_DISABLE_DEFAULT_FORMAT_FUNCTIONS");
 #endif
 #ifdef IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS
        ImGui::Text("define: IMGUI_DISABLE_DEFAULT_MATH_FUNCTIONS");
 #endif
 #ifdef IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS
        ImGui::Text("define: IMGUI_DISABLE_DEFAULT_FILE_FUNCTIONS");
 #endif
 #ifdef IMGUI_DISABLE_FILE_FUNCTIONS
        ImGui::Text("define: IMGUI_DISABLE_FILE_FUNCTIONS");
 #endif
 #ifdef IMGUI_DISABLE_DEFAULT_ALLOCATORS
        ImGui::Text("define: IMGUI_DISABLE_DEFAULT_ALLOCATORS");
 #endif
 #ifdef IMGUI_USE_BGRA_PACKED_COLOR
        ImGui::Text("define: IMGUI_USE_BGRA_PACKED_COLOR");
 #endif
 #ifdef _WIN32
        ImGui::Text("define: _WIN32");
 #endif
 #ifdef _WIN64
        ImGui::Text("define: _WIN64");
 #endif
 #ifdef __linux__
        ImGui::Text("define: __linux__");
 #endif
 #ifdef __APPLE__
        ImGui::Text("define: __APPLE__");
 #endif
 #ifdef _MSC_VER
        ImGui::Text("define: _MSC_VER=%d", _MSC_VER);
 #endif
 #ifdef _MSVC_LANG
        ImGui::Text("define: _MSVC_LANG=%d", (int)_MSVC_LANG);
 #endif
 #ifdef __MINGW32__
        ImGui::Text("define: __MINGW32__");
 #endif
 #ifdef __MINGW64__
        ImGui::Text("define: __MINGW64__");
 #endif
 #ifdef __GNUC__
        ImGui::Text("define: __GNUC__=%d", (int)__GNUC__);
 #endif
 #ifdef __clang_version__
        ImGui::Text("define: __clang_version__=%s", __clang_version__);
 #endif
 #ifdef __EMSCRIPTEN__
        ImGui::Text("define: __EMSCRIPTEN__");
 #endif
        ImGui::Separator();
        ImGui::Text("io.BackendPlatformName: %s", io.BackendPlatformName ? io.BackendPlatformName : "NULL");
        ImGui::Text("io.BackendRendererName: %s", io.BackendRendererName ? io.BackendRendererName : "NULL");
        ImGui::Text("io.ConfigFlags: 0x%08X", io.ConfigFlags);
        if (io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard)        ImGui::Text(" NavEnableKeyboard");
        if (io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad)         ImGui::Text(" NavEnableGamepad");
        if (io.ConfigFlags & ImGuiConfigFlags_NavEnableSetMousePos)     ImGui::Text(" NavEnableSetMousePos");
        if (io.ConfigFlags & ImGuiConfigFlags_NavNoCaptureKeyboard)     ImGui::Text(" NavNoCaptureKeyboard");
        if (io.ConfigFlags & ImGuiConfigFlags_NoMouse)                  ImGui::Text(" NoMouse");
        if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange)      ImGui::Text(" NoMouseCursorChange");
        if (io.MouseDrawCursor)                                         ImGui::Text("io.MouseDrawCursor");
        if (io.ConfigMacOSXBehaviors)                                   ImGui::Text("io.ConfigMacOSXBehaviors");
        if (io.ConfigInputTextCursorBlink)                              ImGui::Text("io.ConfigInputTextCursorBlink");
        if (io.ConfigWindowsResizeFromEdges)                            ImGui::Text("io.ConfigWindowsResizeFromEdges");
        if (io.ConfigWindowsMoveFromTitleBarOnly)                       ImGui::Text("io.ConfigWindowsMoveFromTitleBarOnly");
        if (io.ConfigMemoryCompactTimer >= 0.0f)                        ImGui::Text("io.ConfigMemoryCompactTimer = %.1f", io.ConfigMemoryCompactTimer);
        ImGui::Text("io.BackendFlags: 0x%08X", io.BackendFlags);
        if (io.BackendFlags & ImGuiBackendFlags_HasGamepad)             ImGui::Text(" HasGamepad");
        if (io.BackendFlags & ImGuiBackendFlags_HasMouseCursors)        ImGui::Text(" HasMouseCursors");
        if (io.BackendFlags & ImGuiBackendFlags_HasSetMousePos)         ImGui::Text(" HasSetMousePos");
        if (io.BackendFlags & ImGuiBackendFlags_RendererHasVtxOffset)   ImGui::Text(" RendererHasVtxOffset");
        ImGui::Separator();
        ImGui::Text("io.Fonts: %d fonts, Flags: 0x%08X, TexSize: %d,%d", io.Fonts->Fonts.Size, io.Fonts->Flags, io.Fonts->TexWidth, io.Fonts->TexHeight);
        ImGui::Text("io.DisplaySize: %.2f,%.2f", io.DisplaySize.x, io.DisplaySize.y);
        ImGui::Text("io.DisplayFramebufferScale: %.2f,%.2f", io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        ImGui::Separator();
        ImGui::Text("style.WindowPadding: %.2f,%.2f", style.WindowPadding.x, style.WindowPadding.y);
        ImGui::Text("style.WindowBorderSize: %.2f", style.WindowBorderSize);
        ImGui::Text("style.FramePadding: %.2f,%.2f", style.FramePadding.x, style.FramePadding.y);
        ImGui::Text("style.FrameRounding: %.2f", style.FrameRounding);
        ImGui::Text("style.FrameBorderSize: %.2f", style.FrameBorderSize);
        ImGui::Text("style.ItemSpacing: %.2f,%.2f", style.ItemSpacing.x, style.ItemSpacing.y);
        ImGui::Text("style.ItemInnerSpacing: %.2f,%.2f", style.ItemInnerSpacing.x, style.ItemInnerSpacing.y);

        if (copy_to_clipboard)
        {
            ImGui::LogText("\n```\n");
            ImGui::LogFinish();
        }
        ImGui::EndChild();
    }

    ImGui::EndPopup();
}
