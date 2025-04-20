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

    typedef void (*AppDragAndDropCb)(const char *file);

    // Backend variables
    extern ImRect SysWndPos;                         // restored window position x, y, w, h
    extern ImRect SysWndMinMax;                      // desired min (x,y) and max(w,h) window client area
    extern ImVec4 SysWndBgColor;                     // client window background color r, g, b, a
    extern bool SysWndFocus;                         // system window has focus
    extern WindowState SysWndState;                  // system window state
    extern float SysWndScaling;                      // current scaling of the window
    extern bool AppExit;                             // exit signal to backend
    extern bool AppReconfigure;                      // request backend to call AppConfig()
                                                     // backend can also set this flag on system configuration events

    // App functions
    int  AppInit(int argc, char const *const* argv); // called once at the start of main() function, non-zero return aborts the app
    bool AppConfig(bool startup);                    // called before NewFrame at startup and after system settings has changed
                                                     // if true is returned, fonts texture will be updated
                                                     // do not call directly from the app, set AppReconfigure flag instead
    void AppNewFrame();                              // called after NewFrame every frame
    void AppDestroy();                               // called once just after the main loop

    // Backend functions
    // window related functions (including drag and drop) should not be called prior AppConfig()
    void SysSetWindowTitle(const char* title);       // set native window title
    void SysMinimize();                              // minimize system window
    void SysMaximize();                              // maximize system window
    void SysRestore();                               // restore system window position
    bool SysIsAlwaysOnTop();                         // window is always on top
    void SysSetAlwaysOnTop(bool on_top);             // set/reset window topmost style
    ImS32 SysOpen(const char* resource);             // open resource in system shell,
                                                     // returns 0 on success or error code on failure (if supported)
                                                     // custom open routine provided due to Platform_OpenInShellFn runs
                                                     // ShellExecuteA and so requires UTF8 support shenanigans (1903 + manifest)
    void SysAcceptFiles(AppDragAndDropCb cb);        // start accepting drag and drop files
    void SysRejectFiles();                           // stop accepting drag and drop files
}
