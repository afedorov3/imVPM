#pragma once

struct ImRect
{
    ImS32                                                     x, y, w, h;
    constexpr ImRect()                                        : x(0), y(0), w(0), h(0) { }
    constexpr ImRect(ImS32 _x, ImS32 _y, ImS32 _w, ImS32 _h)  : x(_x), y(_y), w(_w), h(_h) { }
};

namespace ImGui
{
    enum WindowState {
        WSNormal = 0,
        WSMinimized,
        WSMaximized,
    };

    // Backend variables
    extern ImRect SysWndPos;                         // restored window position x, y, w, h
    extern ImRect SysWndMinMax;                      // desired min (x,y) and max(w,h) window client area
    extern ImVec4 SysWndBgColor;                     // client window background color r, g, b, a
    extern bool SysWndFocus;                         // system window has focus
    extern WindowState SysWndState;                  // system window state
    extern bool AppExit;                             // exit signal to backend
    extern bool AppReconfigure;                      // request backend to call AppConfig()
                                                     // backend can also set this flag on system configuration events
    extern ImU32 DPI;                                // current DPI of the window (or system if N/A)
    extern const ImU32 defaultDPI;                   // default DPI of the window (or system if N/A)

    // App functions
    int  AppInit(int argc, char const *const* argv); // called once at the start of main() function
    bool AppConfig();                                // called before NewFrame at startup and after system settings has changed
                                                     // if true is returned, fonts texture will be updated
                                                     // do not call directrly from app, set AppReconfigure flag instead
    void AppNewFrame();                              // called After NewFrame at every frame
    void AppDestroy();                               // called once just after the main loop

    // Backend functions
    void SysSetWindowTitle(const char* title);       // set native window title
    void SysMinimize();                              // minimize system window
    void SysMaximize();                              // maximize system window
    void SysRestore();                               // restore system window position
    ImS32 SysOpen(const char* resource);             // open object
}
