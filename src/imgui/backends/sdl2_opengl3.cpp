// Dear ImGui: standalone example application for SDL2 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL_opengles2.h>
#else
#include <SDL_opengl.h>
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

#define IMGUI_BACKEND
#include "imgui_local.h"

// Data
static SDL_Window*              g_Window = nullptr;
static ImGui::AppDragAndDropCb  g_DropCb = nullptr;

       ImRect                   ImGui::SysWndPos  = ImRect(100, 100, 1280, 800);
       ImRect                   ImGui::SysWndMinMax  = ImRect(0, 0, 0, 0);
       ImVec4                   ImGui::SysWndBgColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
       bool                     ImGui::SysWndFocus = false;
       ImGui::WindowState       ImGui::SysWndState = ImGui::WSNormal;
       float                    ImGui::SysWndScaling = 1.0f;
       bool                     ImGui::AppExit = false;
       bool                     ImGui::AppReconfigure = false;

// Main code
int main(int argc, char* argv[])
{
    // Application init
    int ret = ImGui::AppInit(argc, argv);
    if (ret != 0) return ret;

    // Setup SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    const char* glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    const char* glsl_version = "#version 300 es";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#elif defined(__APPLE__)
    // GL 3.2 Core + GLSL 150
    const char* glsl_version = "#version 150";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // From 2.0.18: Enable native IME.
#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    switch (ImGui::SysWndState)
    {
        case ImGui::WSMinimized: window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_MINIMIZED); break;
        case ImGui::WSMaximized: window_flags = (SDL_WindowFlags)(window_flags | SDL_WINDOW_MAXIMIZED); break;
        default: break;
    }
    g_Window = SDL_CreateWindow("Dear ImGui SDL2+OpenGL3 main window", ImGui::SysWndPos.x, ImGui::SysWndPos.y, ImGui::SysWndPos.w, ImGui::SysWndPos.h, window_flags);
    if (g_Window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(g_Window);
    if (gl_context == nullptr)
    {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return -1;
    }

    SDL_GL_MakeCurrent(g_Window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForOpenGL(g_Window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Initial config
    ImGui::AppConfig(true);
 
    ImRect pMinMax = {};
    // Main loop
    bool done = false;
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!ImGui::AppExit)
#endif
    {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                ImGui::AppExit = true;
            else if (event.type == SDL_WINDOWEVENT && event.window.windowID == SDL_GetWindowID(g_Window))
            {
                switch (event.window.event)
                {
                    case SDL_WINDOWEVENT_CLOSE:
                        ImGui::AppExit = true;
                        break;
                    case SDL_WINDOWEVENT_RESIZED:
                        if ((SDL_GetWindowFlags(g_Window) & (SDL_WINDOW_FULLSCREEN
                                                          | SDL_WINDOW_MINIMIZED
                                                          | SDL_WINDOW_MAXIMIZED
                                                          | SDL_WINDOW_FULLSCREEN_DESKTOP)) == 0)
                        {
                            ImGui::SysWndPos.w = event.window.data1;
                            ImGui::SysWndPos.h = event.window.data2;
                        }
                        break;
                    case SDL_WINDOWEVENT_MOVED:
                        if ((SDL_GetWindowFlags(g_Window) & (SDL_WINDOW_FULLSCREEN
                                                          | SDL_WINDOW_MINIMIZED
                                                          | SDL_WINDOW_MAXIMIZED
                                                          | SDL_WINDOW_FULLSCREEN_DESKTOP)) == 0)
                        {
                            ImGui::SysWndPos.x = event.window.data1;
                            ImGui::SysWndPos.y = event.window.data2;
                        }
                        break;
                    case SDL_WINDOWEVENT_MINIMIZED:
                        ImGui::SysWndState = ImGui::WSMinimized;
                        break;
                    case SDL_WINDOWEVENT_MAXIMIZED:
                        ImGui::SysWndState = ImGui::WSMaximized;
                        break;
                    case SDL_WINDOWEVENT_RESTORED:
                        ImGui::SysWndState = ImGui::WSNormal;
                        break;
                    case SDL_WINDOWEVENT_FOCUS_GAINED:
                        ImGui::SysWndFocus = true;
                        break;
                    case SDL_WINDOWEVENT_FOCUS_LOST:
                        ImGui::SysWndFocus = false;
                        break;
                }
            }
            else if (event.type == SDL_DROPFILE)
            {
                if (g_DropCb)
                    g_DropCb(event.drop.file);
                SDL_free(event.drop.file);
            }
        }
        if (SDL_GetWindowFlags(g_Window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // Signal to App on config change
        if (ImGui::AppReconfigure)
        {
            if (ImGui::AppConfig(false))
            {
                ImGui_ImplOpenGL3_DestroyFontsTexture();
                ImGui_ImplOpenGL3_CreateFontsTexture();
            }
            ImGui::AppReconfigure = false;
        }

        if (SDL_memcmp(&ImGui::SysWndMinMax, &pMinMax, sizeof(ImGui::SysWndMinMax)))
        {
            SDL_SetWindowMinimumSize(g_Window, ImGui::SysWndMinMax.x, ImGui::SysWndMinMax.y);
            SDL_SetWindowMaximumSize(g_Window, ImGui::SysWndMinMax.w, ImGui::SysWndMinMax.h);
            SDL_memcpy(&pMinMax, &ImGui::SysWndMinMax, sizeof(ImGui::SysWndMinMax));
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Delegate frame drawing to the app
        ImGui::AppNewFrame();

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(ImGui::SysWndBgColor.x * ImGui::SysWndBgColor.w,
                     ImGui::SysWndBgColor.y * ImGui::SysWndBgColor.w,
                     ImGui::SysWndBgColor.z * ImGui::SysWndBgColor.w,
                     ImGui::SysWndBgColor.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(g_Window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Finalize App
    ImGui::AppDestroy();

    ImGui::SysRejectFiles();

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(g_Window);
    SDL_Quit();

    return 0;
}

// Backend specific functions
// set native window title
void ImGui::SysSetWindowTitle(const char *title)
{
    SDL_SetWindowTitle(g_Window, title);
}

void ImGui::SysMinimize()
{
    SDL_MinimizeWindow(g_Window);
}

void ImGui::SysMaximize()
{
    SDL_MaximizeWindow(g_Window);
}

void ImGui::SysRestore()
{
    SDL_RestoreWindow(g_Window);
}

bool ImGui::SysIsAlwaysOnTop()
{
    return !!(SDL_GetWindowFlags(g_Window) & SDL_WINDOW_ALWAYS_ON_TOP);
}

void ImGui::SysSetAlwaysOnTop(bool on_top)
{
    SDL_SetWindowAlwaysOnTop(g_Window, (SDL_bool)on_top);
}

ImS32 ImGui::SysOpen(const char *resource)
{
    return (ImS32)!ImGui::GetPlatformIO().Platform_OpenInShellFn(ImGui::GetIO().Ctx, resource);
}

void ImGui::SysAcceptFiles(AppDragAndDropCb cb)
{
    if (cb == nullptr)
        return SysRejectFiles();

    g_DropCb = cb;
    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
}

void ImGui::SysRejectFiles()
{
    SDL_EventState(SDL_DROPFILE, SDL_DISABLE);
    g_DropCb = nullptr;
}
