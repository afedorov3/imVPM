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

// [SECTION] App state
// [SECTION] Forward Declarations
// [SECTION] Helpers
//
// [SECTION] Backend callbacks
//   App Init function           / AppInit()
//   App Config function         / AppConfig()
//   App Draw Frame              / AppDrawFrame()
//   App Destroy                 / AppDestroy()
//
// [SECTION] Child windows
//   Toolbar Window              / ShowToolbarWindow()
//
// [SECTION] Standard windows
//   About Window                / ShowAboutWindow()

*/

#if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <IconsFontAwesome6.h>
#include "Analyzer.hpp"
#include "AudioHandler.h"
#include "fonts.h"

#define NOMINMAX
#include <portable-file-dialogs.h>

#define IMGUI_APP
#include "imgui_local.h"
#include "imgui_widgets.h"
#include "version.h"

// System includes
#include <cstdio>           // vsnprintf, sscanf, printf
#include <algorithm>        // min, max
#include <vector>
#if !defined(_MSC_VER) || _MSC_VER >= 1800
#include <inttypes.h>       // PRId64/PRIu64, not avail in some MinGW headers.
#endif

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
#pragma clang diagnostic ignored "-Wold-style-cast"                 // warning: use of old-style cast                           // yes, they are more terse.
#pragma clang diagnostic ignored "-Wdeprecated-declarations"        // warning: 'xx' is deprecated: The POSIX name for this..   // for strdup used in demo code (so user can copy & paste the code)
#pragma clang diagnostic ignored "-Wint-to-void-pointer-cast"       // warning: cast to 'void *' from smaller integer type
#pragma clang diagnostic ignored "-Wformat-security"                // warning: format string is not a string literal
#pragma clang diagnostic ignored "-Wexit-time-destructors"          // warning: declaration requires an exit-time destructor    // exit-time destruction order is undefined. if MemFree() leads to users code that has been disabled before exit it might cause problems. ImGui coding style welcomes static/globals.
#pragma clang diagnostic ignored "-Wunused-macros"                  // warning: macro is not used                               // we define snprintf/vsnprintf on Windows so they are available, but not always used.
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"  // warning: zero as null pointer constant                   // some standard header variations use #define NULL 0
#pragma clang diagnostic ignored "-Wdouble-promotion"               // warning: implicit conversion from 'float' to 'double' when passing argument to function  // using printf() is a misery with this as C++ va_arg ellipsis changes float to double.
#pragma clang diagnostic ignored "-Wreserved-id-macro"              // warning: macro name is a reserved identifier
#pragma clang diagnostic ignored "-Wimplicit-int-float-conversion"  // warning: implicit conversion from 'xxx' to 'float' may lose precision
#elif defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wpragmas"                  // warning: unknown option after '#pragma GCC diagnostic' kind
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"      // warning: cast to pointer from integer of different size
#pragma GCC diagnostic ignored "-Wformat-security"          // warning: format string is not a string literal (potentially insecure)
#pragma GCC diagnostic ignored "-Wdouble-promotion"         // warning: implicit conversion from 'float' to 'double' when passing argument to function
#pragma GCC diagnostic ignored "-Wconversion"               // warning: conversion to 'xxxx' from 'xxxx' may alter its value
#pragma GCC diagnostic ignored "-Wmisleading-indentation"   // [__GNUC__ >= 6] warning: this 'if' clause does not guard this statement      // GCC 6.0+ only. See #883 on GitHub.
#endif

// Play it nice with Windows users (Update: May 2018, Notepad now supports Unix-style carriage returns!)
#ifdef _WIN32
#define IM_NEWLINE  "\r\n"
#else
#define IM_NEWLINE  "\n"
#endif

// Helpers
#if defined(_MSC_VER) && !defined(snprintf)
#define snprintf    _snprintf
#endif
#if defined(_MSC_VER) && !defined(vsnprintf)
#define vsnprintf   _vsnprintf
#endif

// Format specifiers for 64-bit values (hasn't been decently standardized before VS2013)
#if !defined(PRId64) && defined(_MSC_VER)
#define PRId64 "I64d"
#define PRIu64 "I64u"
#elif !defined(PRId64)
#define PRId64 "lld"
#define PRIu64 "llu"
#endif

// Helpers macros
// We normally try to not use many helpers in imgui_demo.cpp in order to make code easier to copy and paste,
// but making an exception here as those are largely simplifying code...
// In other imgui sources we can use nicer internal functions from imgui_internal.h (ImMin/ImMax) but not in the demo.
#define IM_MIN(A, B)            (((A) < (B)) ? (A) : (B))
#define IM_MAX(A, B)            (((A) >= (B)) ? (A) : (B))
#define IM_CLAMP(V, MN, MX)     ((V) < (MN) ? (MN) : (V) > (MX) ? (MX) : (V))

// Enforce cdecl calling convention for functions called by the standard library,
// in case compilation settings changed the default to e.g. __vectorcall
#ifndef IMGUI_CDECL
#ifdef _MSC_VER
#define IMGUI_CDECL __cdecl
#else
#define IMGUI_CDECL
#endif
#endif

//-----------------------------------------------------------------------------
// [SECTION] App state
//-----------------------------------------------------------------------------

// CONSTANTS
#define FONT       Font      // symbol names that was given to binary_to_compressed_c when fonts.h being created
#define FONT_ICONS FontIcons
#define FONT_NOTES FontMono
static constexpr float    font_def_sz = 20.0f;  // default font size, px
static constexpr float   font_icon_sz = 16.0f;  // default font icon size, px
static constexpr float font_widget_sz = 28.0f;  // widget icons size, px
static constexpr float   font_grid_sz = 18.0f;  // grid font size, px
static constexpr float  font_pitch_sz = 36.0f;  // pitch font size, px
static constexpr float  font_tuner_sz = 16.0f;  // tuner font size, px
static constexpr float     rul_margin = 4.0f;   // ruler label margin, px
static constexpr float   top_feat_pos = 42.0f;  // top features vertical position, px: pitch note indicator, tuner, frequency
static constexpr double        c_dist = 100.0;  // interval width, Cents
static constexpr float         dc_max = 400.0f; // plot: max diff between data points, Cents

// LUTs
static constexpr const char *lut_note[][12] = {
                                             {  "C",  "C",  "D",  "D",  "E",  "F",  "F",  "G",  "G",  "A",  "A",  "B" }, // NoteNamesEnglish
                                             {  "Do", "Do", "Re", "Re", "Mi", "Fa", "Fa", "Sol","Sol","La", "La", "Si"}  // NoteNamesRomance
                                              };
static constexpr const char *lut_semisym[] = {   "",  "♯",  " " }; // semitone symbol selector, [0] elem for labels with no semitone indicator
static constexpr const char lut_number[][12] = {  // gives note number or 0 if semitone
                                             {   1 ,   0 ,   2 ,   3 ,   0 ,   4 ,   0 ,   5 ,   6 ,   0 ,   7 ,   0  }, // Minor scale
                                             {   1 ,   0 ,   2 ,   0 ,   3 ,   4 ,   0 ,   5 ,   0 ,   6 ,   0 ,   7  }  // Major scale
                                               };
static constexpr const float   lut_linew[] = { // plot features line widths
                                // Normal: Semitone,  C,    D,    E,    F,    G,    A,    B
                                               1.5f, 2.5f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f,
                                // Chromatic:   C,  C♯/D♭,  D,  D♯/E♭,  E,    F,  F♯/G♭,  G,  G♯/A♭,  A,  A♯/B♭,  B
                                               2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f,
                                // tempo:    normal  meter
                                               0.5f, 2.0f,
                                // plot data: pitch
                                               1.2f
                                             };

// orig palette
constexpr const ImU32 palette[] = {
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
    IM_COL32( 32,  32,  32, 255),  // DARK GRAY
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

// SETTINGS
// range values
static constexpr float VolThresMax =     50.0f;  // Analyzer: volume threshold max value
static constexpr float VolThresMin =      0.0f;  // Analyzer: volume threshold min value
static constexpr float VolThresDef =      2.0f;  // Analyzer: volume threshold default value [2.0f]
static constexpr int   PitchCalibMax =     450;  // pitch calibration max value, Hz
static constexpr int   PitchCalibMin =     430;  // pitch calibration min value, Hz
static constexpr int   PitchCalibDef =     440;  // pitch calibration, Hz, default value [440]
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
static constexpr float PlotYZoomSpd =     1.0f;  // plot: vertical mouse zoom factor, ruler font heights
static constexpr float PlotYScrlSpd =    50.0f;  // plot: vertical mouse scroll factor, Cents
static constexpr bool  PlotAScrlDef =     true;  // plot: vertical autoscroll enabled by default
static constexpr int   PlotAScrlVelMax =    10;  // plot: vertical autoscroll max value, Cents
static constexpr int   PlotAScrlVelMin =     1;  // plot: vertical autoscroll min value, Cents
static constexpr int   PlotAScrlVelDef =     3;  // plot: vertical autoscroll, Cents, default value [3]
static constexpr int   PlotAScrlMar =      100;  // plot: vertical autoscroll margin, Cents
static constexpr bool  PlotSemiLinesDef = true;  // plot: draw semitone lines if not on Chromatic scale, default value
static constexpr bool  PlotSemiLblsDef = false;  // plot: draw semitone label, default value
static constexpr bool  ShowFreqDef =     false;  // show average pitch frequency by default [false]
static constexpr bool  ShowTunerDef =     true;  // show tuner by default [true]
static constexpr bool  MetronomeDef =    false;  // metronome pulse by default [false]
static constexpr int   TempoValueMax =     250;  // tempo max value, beats per minute
static constexpr int   TempoValueMin =      20;  // tempo min value, beats per minute
static constexpr int   TempoValueDef =     120;  // tempo, beats per minute, default value [120]
static constexpr bool  TempoGridDef =    false;  // plot: draw tempo grid by default [false]
static constexpr bool  ButtonHoldDef =    true;  // interface: show hold button by default [true]
static constexpr bool  ButtonScaleDef =   true;  // interface: show scale selector by default [true]
static constexpr bool  ButtonTempoDef =   true;  // interface: show tempo selector by default [true]

// selections
enum {                                         // settings: note naming scheme
    NoteNamesEnglish = 0,                        //   English
    NoteNamesRomance = 1,                        //   Romance
    NoteNamesCount   = IM_ARRAYSIZE(lut_note), // scheme count
    NoteNamesDef     = NoteNamesEnglish        // default value
};
enum {                                         // plot: octave offset
    OctOffsetA3  = 0,                            // A3
    OctOffsetA4  = 1,                            // A4
    OctOffsetMax = OctOffsetA4,                // max value
    OctOffsetMin = OctOffsetA3,                // min value
    OctOffsetDef = OctOffsetA4                 // default
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
    TempoMeterMax = TempoMeter4_4,  // max_value
    TempoMeterMin = 0,              // min value
    TempoMeterDef = TempoMeter4_4   // default value
};

static float      vol_thres = VolThresDef;                                                        //  N/I
static float         x_zoom = PlotXZoomDef;
static float         y_zoom = PlotYZoomDef;
static float          c_pos = PlotPosDef;         // plot current bottom position, Cents
static bool      semi_lines = PlotSemiLinesDef;
static bool       semi_lbls = PlotSemiLblsDef;
static int       oct_offset = OctOffsetDef;
static int      calibration = PitchCalibDef;
static int        transpose = TransposeDef;
static bool      autoscroll = PlotAScrlDef;
static int       y_ascr_vel = PlotAScrlVelDef;
static bool       show_freq = ShowFreqDef;
static bool      show_tuner = ShowTunerDef;
static int       note_names = NoteNamesDef;
static bool       metronome = MetronomeDef;
static int        tempo_val = TempoValueDef;
static bool      tempo_grid = TempoGridDef;
static int      tempo_meter = TempoMeterDef;
static bool        but_hold = ButtonHoldDef;                                                      //  N/I
static bool       but_scale = ButtonScaleDef;                                                     //  N/I
static bool       but_tempo = ButtonTempoDef;                                                     //  N/I
/*
    palette[ColorRed],
    palette[ColorOrange],
    palette[ColorYellow],
    palette[ColorGreen],
    palette[ColorBlue],
    palette[ColorPurple],
*/
static ImU32 plot_colors[] = {  // Plot palete, fixed order up to and including pitch
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
    PlotIdxMeter,
    PlotIdxPitch,
    PlotIdxMetronome,
    PlotIdxNote,
    PlotIdxTuner,
    PlotLineWidthCount = IM_ARRAYSIZE(lut_linew),
    PlotColorCount     = IM_ARRAYSIZE(plot_colors)
};

static ImU32 UI_colors[] = {
    IM_COL32(180, 180, 180, 255),
    IM_COL32(  0,   0,   0,   0),
    IM_COL32(255, 255, 255,  30),
    IM_COL32(255, 255, 255,  60),
    IM_COL32(255,   0,  50, 200),
    IM_COL32(180,  80,  80, 255),
    IM_COL32(255,  60,  60, 255),
    IM_COL32(255, 255,  70, 255)
};
enum {
    UIIdxDefault = 0,
    UIIdxButton,
    UIIdxButtonHovered,
    UIIdxButtonActive,
    UIIdxProgress,
    UIIdxRecord,
    UIIdxMsgErr,
    UIIdxMsgWarn,
    UIColorCount       = IM_ARRAYSIZE(UI_colors)
};

// STATE
static ImFont      *font_def =  nullptr;  // default font ptr
static ImFont   *font_widget =  nullptr;  // widget icons font ptr
static ImFont     *font_grid =  nullptr;  // grid font ptr
static ImFont    *font_pitch =  nullptr;  // pitch font ptr
static ImFont    *font_tuner =  nullptr;  // tuner font ptr
static bool   fonts_reloaded =    false;  // Fonts reloaded signal
static bool         aboutwnd =    false;  // About popup trigger
static bool             hold =    false;  // hold/pause is active
static float           scale =      1.0;  // DPI scaling factor
static float      x_zoom_min =     0.0f;  // plot horizontal zoom min limit based on window size,
                                          //           DPI scale and Analyzer's pitch array size
static float           c_top =     0.0f;  // current top plot position, Cents
static float           c_max =  9500.0f;  // current max plot position, Cents
static float           c_min = -1200.0f;  // current min plot position, Cents
static float         c_calib =     0.0f;  // calibrated and transposed offset, Cents
static int         scale_key =        0;  // plot: scale key                                           N/I [settings/loadscale]
static bool      scale_major =     true;  // plot: scale is Major                                      N/I [settings/loadscale]
static bool     scale_chroma =    false;  // plot: scale is Cromatic                                   N/I [settings/loadscale]
static bool        rul_right =     true;  // vertical axis ruler position
static float      x_peak_off =     0.0f;  // peak note position offset
static ImVec2      rullbl_sz;             // vertical axis ruler size
static ImVec2      widget_sz;             // UI widget size
static float   widget_margin;             // UI widget margin
static float  widget_padding;             // UI widget padding
static float    menu_spacing;             // UI menu spacing
static float    progress_hgt;             // UI playback progress height
static bool   progress_hover =    false;  // UI playback progress is hovered
static std::vector<double> f_peak_buf;    // peak frequency averaging buffer
static size_t f_peak_buf_pos = 0;         // peak frequency averaging buffer position

typedef std::unique_ptr<pfd::open_file> unique_open_file;
static unique_open_file open_file_dlg = nullptr; // open file dialog operation

static Analyzer analyzer;
static std::mutex analyzer_mtx;
static AudioHandler audiohandler(44100 /* Fsample */, 2 /* channels */, AudioHandler::FormatF32 /* sample format */, AudioHandler::FormatS16 /* record format */, 1470 /* cb interval */);
static AudioHandler::State ah_state;      // frame-locked handler state
static uint64_t ah_len = 0, ah_pos = 0;   // handler length and position, applicable only to playback and record

//-----------------------------------------------------------------------------
// [SECTION] Forward Declarations, Helpers
//-----------------------------------------------------------------------------

// UI routines
static void ShowToolbarWindow(bool* p_open);
//  widgets
static void Menu();                       // menu widget
static void CaptureDevices();             // capture device selection widget
static void PlaybackProgress();           // playback progress bar

// main window routines
static void InputControl();               // handle controls
static void Draw();                       // main drawing routine
static void ProcessLog();                 // AudioHandler log messages

enum {
    TextAlignLeft = 0,
    TextAlignCenter = -1,
    TextAlignRight = -2,
    TextAlignBottom = -2,
    TextAlignMiddle = -1,
    TextAlignTop = 0,
};
static void AddTextAligned(ImVec2 at, int alignH, int alignV, ImU32 color, ImDrawList* draw_list, const char *str);
static void AddFmtTextAligned(ImVec2 at, int alignH, int alignV, ImU32 color, ImDrawList* draw_list, const char *fmt, ...);

static void AddNoteLabel(ImVec2 at, int align, int note, int sharpidx, int octave, ImU32 color, ImDrawList* draw_list);

//-----------------------------------------------------------------------------
// [SECTION] Helpers
//-----------------------------------------------------------------------------

#ifndef _WIN32
#include <errno.h>
typedef int errno_t;
errno_t strcpy_s(char* dst, size_t size, const char* src)
{ 
    if(!size || !dst || !src) return EINVAL;

    for(;size > 1;--size)
    {
        if(!(*dst++ = *src++)) return 0;
    }
    *dst = '\0';

    return ERANGE;
}
int sprintf_s(char *dst, size_t dst_sz, const char *fmt, ...)
{
    int ret;
    va_list args;
    va_start(args, fmt);
    ret = vsnprintf(dst, dst_sz - 1, fmt, args);
    va_end(args);
    dst[ret] = '\0';
    return ret;
}
#endif // !_WIN32

void AddNoteLabel(ImVec2 at, int alignH, int alignV, int note, int sharpidx, int octave, ImU32 color, ImDrawList* draw_list)
{
    static char label[16];

    if (octave >= 0)
        sprintf_s(label, IM_ARRAYSIZE(label), "%s%s%d", lut_note[note_names][note], lut_semisym[sharpidx], octave);
    else
        sprintf_s(label, IM_ARRAYSIZE(label), "%s%s", lut_note[note_names][note], lut_semisym[sharpidx]);
    ImVec2 size = ImGui::CalcTextSize(label);
    at.x += size.x / 2.0f * alignH;
    at.y += size.y / 2.0f * alignV;
    draw_list->AddText(at, color, label);
}

void AddTextAligned(ImVec2 at, int alignH, int alignV, ImU32 color, ImDrawList* draw_list, const char *str)
{
    ImVec2 size = ImGui::CalcTextSize(str);
    at.x += size.x / 2.0f * alignH;
    at.y += size.y / 2.0f * alignV;
    draw_list->AddText(at, color, str);    
}

void AddFmtTextAligned(ImVec2 at, int alignH, int alignV, ImU32 color, ImDrawList* draw_list, const char *fmt, ...)
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

void togglePause()
{
    if (ah_state.canPause())
        audiohandler.pause();
    else if (ah_state.canResume())
        audiohandler.resume();
}

void toggleFullscreen()
{
    if (ImGui::SysIsMaximized())
        ImGui::SysRestore();
    else
        ImGui::SysMaximize();
}

void OpenAudioFile()
{
    open_file_dlg = unique_open_file(new pfd::open_file("Open file for playback", pfd::path::home(),
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

//-----------------------------------------------------------------------------
// [SECTION] Backend callbacks
//-----------------------------------------------------------------------------

void sampleCb(AudioHandler::Format format, uint32_t channels, const void *pData, uint32_t frameCount, void *userData)
{
    std::lock_guard<std::mutex> lock(analyzer_mtx);
    const float *bufptr = (const float*)pData;
    const uint32_t sampleCount = frameCount * channels;
    for (uint32_t i = 0; i < sampleCount; i += channels)
    {
        float sample = bufptr[i];
        for (uint32_t ch = 1; ch < channels; ++ch) // downmix to mono
            sample += bufptr[i + ch]; // will overflow on integral formats
        analyzer.addData(sample / channels);
    }
}

int ImGui::AppInit(int argc, char const *const* argv)
{
    audiohandler.attachFrameDataCb(sampleCb, nullptr);
    audiohandler.setPlaybackEOFaction(AudioHandler::CmdCapture);
    audiohandler.log().SetLevel(logger::LOG_WARN);
    audiohandler.enumerate();

    if (!pfd::settings::available())
        audiohandler.log().LogMsg(logger::LOG_WARN, "portable-file-dialogs is unavailable on this platform, file operations ceased");

    if (argc > 1)
    {
        audiohandler.play(argv[1]);
    }
    else
    {
        audiohandler.setPreferredCaptureDevice("mic");
        audiohandler.capture();
    }

    return 0;
}

#define FONT_DATA(n) n ## _compressed_data
#define FONT_SIZE(n) n ## _compressed_size
#define ADD_FONT(var, ranges, name) do {\
  strcpy_s(font_config.Name, IM_ARRAYSIZE(font_config.Name), #name ", " #var); \
  var = io.Fonts->AddFontFromMemoryCompressedTTF(FONT_DATA(name) , FONT_SIZE(name), var ## _sz * scale, &font_config, ranges); \
} while(0)
#define MERGE_FONT(var, ranges, name) do {\
  font_config.MergeMode = true; \
  font_config.GlyphMinAdvanceX = var ## _sz * scale; \
  io.Fonts->AddFontFromMemoryCompressedTTF(FONT_DATA(name) , FONT_SIZE(name), font_config.GlyphMinAdvanceX, &font_config, ranges); \
  font_config.MergeMode = false; \
  font_config.GlyphMinAdvanceX = 0; \
} while(0)
bool ImGui::AppConfig()
{
    ImGui::SysSetWindowTitle("imVocalPitchMonitor");

    scale = (float)ImGui::DPI / (float)ImGui::defaultDPI;

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
        0,
    };
    static const ImWchar ranges_icons[] =
    {
        0xF07C, 0xF07C, // folder-open
        0xF0c9, 0xF0c9, // bars
        0xF130, 0xF130, // microphone
        0xF1DE, 0xF1DE, // sliders
        0,
    };
    static const ImWchar ranges_notes[] =
    {
        0x0020, 0x0020, // space
        0x0030, 0x0039, // 0~9
        0x0041, 0x005A, // A~Z
        0x0061, 0x007A, // a~z
        0x266D, 0x266F, // ♭, ♯
        0,
    };

    io.Fonts->Clear();

    ADD_FONT(font_def, ranges_ui, FONT);
    MERGE_FONT(font_icon, ranges_icons, FONT_ICONS);
    ADD_FONT(font_widget, ranges_icons, FONT_ICONS);
    ADD_FONT(font_tuner, ranges_notes, FONT_NOTES);
    ADD_FONT(font_pitch, ranges_notes, FONT_NOTES);
    ADD_FONT(font_grid, ranges_notes, FONT_NOTES);

    io.Fonts->Build();
    io.FontDefault = font_def;
    fonts_reloaded = true;

    ImGuiStyle& style = ImGui::GetStyle();
    style.FrameRounding = 5.0f * scale;
    style.PopupRounding = 5.0f * scale;
    style.GrabRounding  = 4.0f * scale;
    style.Colors[ImGuiCol_Text] = ImColor(UI_colors[UIIdxDefault]);

    widget_padding = font_widget_sz * scale / 10;
    widget_sz = ImVec2(font_widget_sz * scale + widget_padding * 2, font_widget_sz * scale + widget_padding * 2);
    widget_margin = font_widget_sz * scale / 2;
    menu_spacing = 8.0f * scale;

    return true;
}

namespace ImGui { IMGUI_API void ShowFontAtlas(ImFontAtlas* atlas); }

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
        for(int i = 0; i < 12; ++i) {
            ImVec2 size = ImGui::CalcTextSize(names[i]);
            if (size.x > rullbl_sz.x)
                rullbl_sz = size;
        }
        if (semi_lbls)
            rullbl_sz.x += ImGui::CalcTextSize("♯").x;
        rullbl_sz.x += ImGui::CalcTextSize("8").x;
        rullbl_sz.x += rul_margin * 2.0f * scale;
        ImGui::PopFont();
        ImGui::PushFont(font_pitch);
        x_peak_off = ImGui::CalcTextSize("8").x * 1.5f;
        ImGui::PopFont();

        // FIXME move the following to the settings load routine
        size_t tuner_smooth_len = 3;
        f_peak_buf.resize(tuner_smooth_len, -1.0);
        f_peak_buf_pos = 0;
        c_calib = (float)((std::log2(440.0) - std::log2((double)calibration)) * 12.0 * 100.0) + (float)(transpose * 100);
        //////////
    }

    // process file operations
    if (open_file_dlg && open_file_dlg->ready(0))
    {
        auto files = open_file_dlg->result();
        if (files.size() > 0)
        {
            audiohandler.stop();
            audiohandler.play(files[0].c_str());
        }
        open_file_dlg = nullptr;
    }

    audiohandler.getState(ah_state, &ah_len, &ah_pos); // get handler state for a frame

    const ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::ShowAboutWindow(nullptr);

    ShowToolbarWindow(nullptr);

    // Widgets
    //  Menu
    ImVec2 pos = viewport->Pos + viewport->Size;
    pos.x -= widget_sz.x + widget_margin + rullbl_sz.x;
    pos.y -= widget_sz.y + widget_margin;
    ImGui::SetNextWindowPos(pos);
    Menu();

    //  Capture device selector
    pos.x -= widget_sz.x + widget_margin;
    ImGui::SetNextWindowPos(pos);
    CaptureDevices();

    // playback progress
    if (ah_state.isPlaying())
    {
        progress_hgt = widget_margin * (0.4f + progress_hover * 0.5f);
        pos.x = 0;
        pos.y = viewport->Size.y - progress_hgt;
        ImGui::SetNextWindowPos(pos);
        ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, progress_hgt));
        PlaybackProgress();
    }

    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    // Body of the main window starts here.
    if (ImGui::Begin("imVocalPitchMonitor", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_NoScrollWithMouse
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        ))
    {
        InputControl();
        Draw();
        ProcessLog();

        // End of MainWindow
        ImGui::End();
    }
}

void ImGui::AppDestroy()
{

}

//-----------------------------------------------------------------------------
// [SECTION] Child windows
//-----------------------------------------------------------------------------

void ShowToolbarWindow(bool* p_open)
{
    static ImVec2 pos = ImVec2(10.0f, 10.0f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Appearing);

    if (!ImGui::Begin("##ToolbarWnd", p_open,
        ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        ))
    {
        goto defer;
    }

    if (ImGui::CollapsingHeader("Toolbar", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::Button("Quit"))
        {
            ImGui::AppExit = true;
        }

        if (ImGui::Button("About"))
        {
            aboutwnd = true;
        }

        ImGui::Checkbox("autoscroll", &autoscroll);
        ImGui::Checkbox("label on right", &rul_right);
        ImGui::TextUnformatted("Octave number");
        ImGui::BeginGroup();
        ImGui::RadioButton("A4", &oct_offset, OctOffsetA4);
        ImGui::SameLine(); ImGui::RadioButton("A3", &oct_offset, OctOffsetA3);
        ImGui::EndGroup();
        static int p_note_names = note_names;
        ImGui::TextUnformatted("Note names");
        ImGui::BeginGroup();
        ImGui::RadioButton("English", &note_names, NoteNamesEnglish);
        ImGui::SameLine(); ImGui::RadioButton("Romance", &note_names, NoteNamesRomance);
        ImGui::EndGroup();
        if (p_note_names != note_names) { fonts_reloaded = true; p_note_names = note_names; }
        ImGui::Checkbox("semi lines", &semi_lines);
        static bool p_semi_lbls = semi_lbls;
        ImGui::SameLine(); ImGui::Checkbox("semi lbls", &semi_lbls);
        if (p_semi_lbls != semi_lbls) { fonts_reloaded = true; p_semi_lbls = semi_lbls; }
        ImGui::Checkbox("scale chroma", &scale_chroma);

        /*{
            const AudioHandler::Devices &devices = audiohandler.getCaptureDevices();
            if (ImGui::BeginCombo(ICON_FA_MICROPHONE, devices.selectedName.c_str())) {
                for (int n = 0; n < devices.list.size(); n++) {
                    bool is_selected = devices.list[n].name == devices.selectedName;
                    if (ImGui::Selectable(devices.list[n].name.c_str(), is_selected) && !is_selected)
                    {
                        audiohandler.setPreferredCaptureDevice(devices.list[n].name.c_str());
                        if (ah_state.isIdle())
                            audiohandler.capture();
                    }
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
                ImGui::SetItemTooltip("Capture / Record device");
            audiohandler.unlockDevices();
            ImGui::SameLine();
            if (ImGui::Button(ICON_FA_ROTATE))
                audiohandler.enumerate();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_NoSharedDelay))
                ImGui::SetItemTooltip("Re-enumerate audio devices");
        }*/
     }

defer:
    ImGui::End();
}

void Menu()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##Menu", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        );
    ImGui::PopStyleVar(2);

    ImGui::PushFont(font_widget);
    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(UI_colors[UIIdxButton]));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(UI_colors[UIIdxButtonHovered]));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(UI_colors[UIIdxButtonActive]));
    if (ImGui::Button(ICON_FA_BARS, widget_sz))
        ImGui::OpenPopup("##MenuPopup");
    ImGui::PopStyleColor(3);
    ImGui::PopFont();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(menu_spacing, menu_spacing));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(menu_spacing, menu_spacing));
    if (ImGui::BeginPopup("##MenuPopup"))
    {
        if (ImGui::Selectable(ICON_FA_FOLDER_OPEN " Open file"))
            OpenAudioFile();
        ImGui::Separator();
        ImGui::Selectable(ICON_FA_SLIDERS " Settings");

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);

    // End of Menu window
    ImGui::End();
}

void CaptureDevices()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##CaptureDevices", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        );
    ImGui::PopStyleVar(2);

    ImGui::PushFont(font_widget);
    ImGui::PushStyleColor(ImGuiCol_Text, (ImVec4)ImColor(ah_state.isCapOrRec() ? UI_colors[UIIdxRecord] : UI_colors[UIIdxDefault]));
    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor(UI_colors[UIIdxButton]));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor(UI_colors[UIIdxButtonHovered]));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor(UI_colors[UIIdxButtonActive]));
    if (ImGui::Button(ICON_FA_MICROPHONE, widget_sz))
    {
        audiohandler.enumerate();
        ImGui::OpenPopup("##CaptureDevicesPopup");
    }
    ImGui::PopStyleColor(4);
    ImGui::PopFont();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(menu_spacing, menu_spacing));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(menu_spacing, menu_spacing));
    if (ImGui::BeginPopup("##CaptureDevicesPopup"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextAlign, ImVec2(0.5f, 0.75f));
        ImGui::SeparatorText("Capture devices");
        ImGui::PopStyleVar();
        const AudioHandler::Devices &devices = audiohandler.getCaptureDevices();
        for (int n = 0; n < devices.list.size(); n++) {
            bool is_selected = devices.list[n].name == devices.selectedName;
            if (ImGui::MenuItem(devices.list[n].name.c_str(), "", is_selected) && !is_selected)
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

    // End of CaptureDevice window
    ImGui::End();
}

void PlaybackProgress()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##PlaybackProgress", nullptr,
        ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoDecoration
        | ImGuiWindowFlags_NoBackground
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoFocusOnAppearing
        );
    ImGui::PopStyleVar(2);

    ImVec2 wsize = ImGui::GetWindowSize();
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, (ImVec4)ImColor(UI_colors[UIIdxButtonHovered]));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, (ImVec4)ImColor(UI_colors[UIIdxProgress]));
    ImGui::ProgressBar((float)ah_pos / ah_len, wsize, "");
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNone))
    {
        ImVec2 mpos = ImGui::GetMousePos();
        ImGui::SetTooltip(audiohandler.framesToTime(uint64_t(mpos.x / wsize.x * ah_len)).c_str());
    }

    bool hover = ImGui::IsWindowHovered();
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

    //ImGui::SetItemTooltip("Capture / Record device");

    // End of PlaybackProgress window
    ImGui::End();
}

inline void InputControl()
{
    // Mouse
    if (ImGui::IsWindowHovered())
    {
        ImGuiIO &io  = ImGui::GetIO();
        ImVec2 wsize = ImGui::GetWindowSize();

        // vertical zoom
        if (io.KeyMods == ImGuiMod_Ctrl && io.MouseWheel != 0.0f)
        {
            float y_zoomp = y_zoom;

            y_zoom += io.MouseWheel * PlotYZoomSpd;
            y_zoom  = std::max(PlotYZoomMin, std::min(y_zoom, PlotYZoomMax));
                
            // zoom around mouse pos
            c_pos += (c_top - c_pos) * (1.0f - y_zoomp / y_zoom) * (1.0f - io.MousePos.y / wsize.y);
        }

        // horizontal zoom
        if (io.KeyMods == ImGuiMod_Shift && io.MouseWheel != 0.0f)
        {
            x_zoom += io.MouseWheel * PlotXZoomSpd;
            x_zoom  = std::max(x_zoom_min, std::min(x_zoom, PlotXZoomMax));
        }

        // vertical scroll
        if (io.KeyMods == ImGuiMod_None && io.MouseWheel != 0.0f)
        {
            c_pos += io.MouseWheel * PlotYScrlSpd;
        }

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left, false))
            togglePause();
    }

    // Keyboard
    if (ImGui::IsWindowFocused())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false))
            togglePause();

        if (ImGui::IsKeyPressed(ImGuiKey_F, false))
            toggleFullscreen();
    }
}

void Draw()
{
    double f_peak = analyzer.get_peak_freq();
    float c_peak = Analyzer::freq_to_cent(f_peak);
    if (c_peak >= 0.0f)
        c_peak += c_calib;

    ImVec2 wsize = ImGui::GetWindowSize();
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

    // adjust plot limits
    double step = y_zoom * font_grid_sz * scale;
    double c2y_mul = step / c_dist;
    double c2y_off = wsize.y + c_pos * c2y_mul;

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
    if (autoscroll && !ah_state.isIdle() && c_peak >= 0.0) {
        static int velocity = 0;

        if (c_peak < c_pos + (float)PlotAScrlMar)
            velocity -= y_ascr_vel;
        else if (c_peak > c_top - (float)PlotAScrlMar)
            velocity += y_ascr_vel;
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
    float notch = scale_chroma ? 0.0f : 4.0f * scale;
    // ruler label position and alight
    float x_rullbl; int align_rullbl;
    if (rul_right)
    {
        x_near       = wsize.x - rullbl_sz.x;
        x_far        = 0.0f;
        x_left       = x_far + lut_linew[PlotIdxPitch] * scale / 2; // + pitch line width
        x_right      = x_near - lut_linew[PlotIdxTonic] * scale;    // - split line width
        x_rullbl     = x_near + rul_margin * scale;
        align_rullbl = TextAlignLeft;
    }
    else
    {
        x_near       = rullbl_sz.x;
        x_far        = wsize.x;
        x_left       = x_near + lut_linew[PlotIdxTonic] * scale;
        x_right      = x_far - lut_linew[PlotIdxPitch] * scale / 2;
        x_rullbl     = x_near - rul_margin * scale;
        align_rullbl = TextAlignRight;
        notch *= -1.0f;
    }
    x_center = std::roundf((x_near + x_far) / 2.0f);

    // adjust horizontal zoom
    x_zoom_min = std::max(PlotXZoomMin, (x_right - x_left) / scale / (float)(Analyzer::PITCH_BUF_SIZE - 2)); // limit zoom by pitch buffer size; ignore current pitch buffer element
    if (x_zoom < x_zoom_min)
        x_zoom = x_zoom_min;
    float x_zoom_scaled = x_zoom * scale;

    // horizontal grid / metronome
    if (tempo_grid || metronome) {
        float intervals_per_bpm = 60.0f/* sec */ / (float)tempo_val / Analyzer::get_interval_sec();
        float total_analyze_cnt = (float)analyzer.get_total_analyze_cnt();
        int beat_cnt = (int)(total_analyze_cnt / intervals_per_bpm);
        float offset = total_analyze_cnt - (float)beat_cnt * intervals_per_bpm;

        if (metronome) {
            float trig_dist = 7.0f - (float)tempo_val / 60.0f;
            float trig = (std::fmod(offset + trig_dist, intervals_per_bpm) - trig_dist) / trig_dist;
            if (trig <= 1.0f)
                draw_list->AddRect(ImVec2(0.0f, 0.0f), ImVec2(wsize.x, wsize.y), plot_colors[PlotIdxMetronome], 0.0f, ImDrawFlags_None, (1.0f - std::fabs(trig)) * 20.0f * scale);
        }

        if (tempo_grid) {
            offset = x_right - offset * x_zoom_scaled;
            while(offset > x_left) {
                draw_list->AddLine(ImVec2(offset, 0.0f), ImVec2(offset, wsize.y), plot_colors[PlotIdxTempo],
                    lut_linew[PlotIdxTempo + (tempo_meter > 0 && beat_cnt % tempo_meter == 0)] * scale);
                offset -=  x_zoom_scaled * intervals_per_bpm;
                --beat_cnt;
            }
        }
    }

    // draw vertical grid
    ImGui::PushFont(font_grid);
    int key_bot = (int)(c_pos / c_dist);
    int key_top = key_bot + (int)(wsize.y / step) + 1;
    for (int key = key_bot; key <= key_top; ++key) {
        float y = std::roundf(c2y_off - c_dist * key * c2y_mul);
        int note =   (key + 24) % 12;
        int note_scaled = (note - scale_key + 12) % 12;
        int octave = (key + 24) / 12 - 2 + oct_offset; // minus negative truncation compensation ( - 24/12)
        int line_idx = scale_chroma ? note + 8 : lut_number[scale_major][note_scaled];
        int label_idx = lut_number[1][note]; // Major scale

        // line
        if (semi_lines || line_idx)
            draw_list->AddLine(ImVec2(x_far, y), ImVec2(x_near + notch * !(line_idx - 1), y), plot_colors[line_idx], lut_linew[line_idx] * scale);

        // label
        if (semi_lbls || label_idx)
            AddNoteLabel(ImVec2(x_rullbl, y), align_rullbl, TextAlignMiddle, note, semi_lbls << !!label_idx, octave, plot_colors[line_idx], draw_list);
    }
    ImGui::PopFont();

    // draw split line
    draw_list->AddLine(ImVec2(x_near, 0), ImVec2(x_near, wsize.y), plot_colors[PlotIdxTonic], lut_linew[PlotIdxTonic] * scale);

    // plot the data
    {
        int max_cnt = (int)((x_right - x_left) / x_zoom_scaled) + 1; // include the start point
        float line_w =  lut_linew[PlotIdxPitch] * scale;
        ImU32 color = plot_colors[PlotIdxPitch];
        float pp = -1.0f;
        ImVec2 pv;

        std::lock_guard<std::mutex> lock(analyzer_mtx);
        std::shared_ptr<const float[]> pitch_buf = analyzer.get_pitch_buf();
        const size_t pitch_buf_pos = analyzer.get_pitch_buf_pos();
        const size_t pitch_buf_sz = Analyzer::PITCH_BUF_SIZE;
        for(int i = 0; i < max_cnt; ++i)
        {
            float p = pitch_buf[(pitch_buf_pos + pitch_buf_sz - 1 - i) % pitch_buf_sz];
            if (p >= 0.0f)
            {
                ImVec2 v(x_right - x_zoom_scaled * i, std::roundf(c2y_off - (p + c_calib) * c2y_mul));
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
    if (c_peak >= 0.0f) {
        int meandiv = 0;
        double f_mean = 0.0;
        for(int i = 0; i < f_peak_buf.size(); ++i) {
            double freq = f_peak_buf[i];
            if (freq > 0.0) {
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
        AddNoteLabel(ImVec2(x_center + x_peak_off, top_feat_pos * scale), TextAlignRight, TextAlignBottom,
                         note, 1 << !!lut_number[1][note], octave, plot_colors[PlotIdxNote], draw_list);
        ImGui::PopFont();
        // draw tuner
        if (show_tuner) {
            float y_tuner = (top_feat_pos + 6.0f) * scale;
            float y_long  = y_tuner + 7.0f * scale;
            float y_short = y_tuner + 4.0f * scale;
            float zoom = 1.75f * scale;
            // triangle mark
            draw_list->AddTriangleFilled(ImVec2(x_center - 6.0f * scale, top_feat_pos * scale),
                                         ImVec2(x_center + 6.0f * scale, top_feat_pos * scale),
                                         ImVec2(x_center, y_tuner - 1.0f * scale),
                                         plot_colors[PlotIdxTuner]);

            ImGui::PushFont(font_tuner);
            float stop = c_mean + 100.0f;
            int pos = (int)((c_mean - 100.0f) / 10.0f) * 10;
            ImU32 color = plot_colors[PlotIdxTuner];
            for ( ; (float)pos <= stop; pos += 10) {
                float x_pos = std::roundf(x_center + ((float)pos - c_mean) * zoom);
                float linew = lut_linew[PlotIdxSemitone] * scale;
                switch (pos % 100) {
                case 0: {
                    int note = pos % 1200 / 100;

                    // tuner note
                    AddNoteLabel(ImVec2(x_pos, y_long), TextAlignCenter, TextAlignTop,
                         note, 1 - !!lut_number[1][note], -1, color, draw_list);

                    // long thick tick
                    linew = lut_linew[PlotIdxSupertonic] * scale;
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
            AddFmtTextAligned(ImVec2(x_center + x_peak_off * 2.5f, top_feat_pos * scale), TextAlignLeft, TextAlignBottom,
                plot_colors[PlotIdxTuner], draw_list, "%4.0fHz", f_mean);
    }
}

void ProcessLog()
{
    constexpr size_t maxMsgs = 3;
    constexpr float msgTimeoutSec = 5.0f;
    constexpr float faderate = msgTimeoutSec / 0.3f /* duration, seconds */ / 2 /* fadein + fadeout */;

    static unsigned long long nextN = maxMsgs;
    static float MsgTimeout[maxMsgs] = {};

    ImVec2 wsize = ImGui::GetWindowSize();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    size_t timedOut = 0;
    size_t pos = 0;
    auto& entries = audiohandler.log().LockEntries();
    for ( auto &entry : entries )
    {
        if (entry.N > nextN)
            continue;
        if (entry.N <= nextN - maxMsgs)
            break;

        float time = ImGui::GetTime();
        size_t curMsg = entry.N % maxMsgs;
        if (MsgTimeout[curMsg] == 0.0f)
            MsgTimeout[curMsg] = time + msgTimeoutSec;
        else if (time >= MsgTimeout[curMsg])
        {
            timedOut++;
            MsgTimeout[curMsg] = 0.0f;
            continue;
        }
        float alpha = 1.0f - 0.3f * pos;
        // fadein/fadeout
        alpha *= std::fmin(faderate - std::fabs(std::fmod((MsgTimeout[curMsg] - time) * faderate * 2.0f / msgTimeoutSec, faderate * 2.0f) - faderate), 1.0f);
        ImU32 color = ImGui::GetColorU32(entry.Lvl == logger::LOG_ERR ? UI_colors[UIIdxMsgErr] : UI_colors[UIIdxMsgWarn], alpha);
        AddTextAligned(ImVec2(wsize.x / 2, wsize.y - font_def_sz * 1.5f * maxMsgs + font_def_sz * 1.5f * pos), TextAlignCenter, TextAlignMiddle,
                color, draw_list, entry.Msg.get());
        pos++;
    }
    audiohandler.log().UnlockEntries();
    nextN += timedOut;
}

//-----------------------------------------------------------------------------
// [SECTION] About Window / ShowAboutWindow()
// Access from Dear ImGui Demo -> Tools -> About
//-----------------------------------------------------------------------------

void ImGui::ShowAboutWindow(bool* p_open)
{
    if (aboutwnd)
    {
        ImGui::OpenPopup("About imVocalPitchMonitor");
        aboutwnd = false;
    }
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopup("About imVocalPitchMonitor"))
        return;

    static const char* VocalPithMonitorURL = "https://play.google.com/store/apps/details?id=com.tadaoyamaoka.vocalpitchmonitor";
    ImGui::Text("imVocalPitchMonitor %s", IMVPM_VERSION_DISPLAY);
    ImGui::Text("%s", IMVPM_DATE_AUTHOR);
    ImGui::TextUnformatted("Ported from*"); ImGui::SameLine();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("All core features code is stolen from decompiled original apk.");
    if (ImGui::Link("VocalPitchMonitor", VocalPithMonitorURL))
    {
        ImGui::SysOpen(VocalPithMonitorURL);
    }; ImGui::SameLine();
    ImGui::TextUnformatted("Android App");
    ImGui::TextUnformatted("by Tadao Yamaoka.");
    ImGui::TextUnformatted("Without his hard work this project wouldn't be possible.");

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
