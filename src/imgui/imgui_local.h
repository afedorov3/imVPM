#pragma once

struct ImRect
{
    ImS32                                                     x, y, w, h;
    constexpr ImRect()                                        : x(0), y(0), w(0), h(0) { }
    constexpr ImRect(ImS32 _x, ImS32 _y, ImS32 _w, ImS32 _h)  : x(_x), y(_y), w(_w), h(_h) { }
};

namespace ImGui
{
    // Backend variables
    extern ImRect SysWinPos;                         // initial window position provider x, y, w, h
    extern ImVec4 SysBgColor;                        // Client window background color r, g, b, a
    extern bool AppExit;                             // Exit signal to backend
    extern bool AppReconfigure;                      // request backend to call AppConfig()
                                                     // backend can also set this flag on system configuration events
    extern ImU32 DPI;                                // current DPI of the window (or system if N/A)
    extern const ImU32 defaultDPI;                   // default DPI of the window (or system if N/A)

    // App functions
    int  AppInit(int argc, char const *const* argv); // called once at the start of main() function
    bool AppConfig();                                // called before NewFrame at startup and after system settings has changed
                                                     // if true is returned, fonts texture will be updated
                                                     // Do not call directrly from app, set AppReconfigure flag instead
    void AppNewFrame();                              // called After NewFrame at every frame
    void AppDestroy();                               // called once just after the main loop

    // Backend functions
    void SysSetWindowTitle(const char* title);       // set native window title
    void SysMaximize();                              // maximize system window
    bool SysIsMaximized();                           // returns true if system window is maximized
    void SysRestore();                               // restore system window position

    ImS32 SysOpen(const char* file);

}
