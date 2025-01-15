// Dear ImGui: standalone example application for DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <tchar.h>
#include <memory>
#include <errno.h>

#define IMGUI_BACKEND
#include "imgui_local.h"
// FIXME
// https://github.com/ocornut/imgui/issues/2311
#include "imgui_dx11_font_stub.h"

// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
static HWND                     g_Window = nullptr;
static UINT_PTR                 g_idGlobalRefreshTimer = 0;

       ImRect                   ImGui::SysWndPos  = ImRect(100, 100, 1280, 800);
       ImRect                   ImGui::SysWndMinMax  = ImRect(0, 0, 0, 0);
       ImVec4                   ImGui::SysWndBgColor = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
       bool                     ImGui::SysWndFocus = false;
       ImGui::WindowState       ImGui::SysWndState = ImGui::WSNormal;
       bool                     ImGui::AppExit = false;
       bool                     ImGui::AppReconfigure = false;
       ImU32                    ImGui::DPI = USER_DEFAULT_SCREEN_DPI;
 const ImU32                    ImGui::defaultDPI = USER_DEFAULT_SCREEN_DPI;

// system function pointers
static BOOL (WINAPI * ptrEnableNonClientDpiScaling) (HWND) = nullptr;
static DPI_AWARENESS_CONTEXT (WINAPI * pfnSetThreadDpiAwarenessContext) (DPI_AWARENESS_CONTEXT) = nullptr;
static UINT (WINAPI * pfnGetDpiForSystem) () = nullptr;
static UINT (WINAPI * pfnGetDpiForWindow) (HWND) = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static UINT GetDPI (HWND hWnd);
static void CALLBACK GuiChangesCoalescingTimer (HWND hWnd, UINT, UINT_PTR id, DWORD);
static LRESULT OnDpiChange(WPARAM dpi, const RECT * r);
static std::unique_ptr<wchar_t[]> utf8_to_wchar(const char *utf8str);
static std::unique_ptr<char[]> argv_wchar_to_utf8(int argc, wchar_t const *const *wargv);

// Convenient loading function, see WinMain
//  - simplified version of https://github.com/tringi/emphasize/blob/master/Windows/Windows_Symbol.hpp

template <typename P>
bool Symbol (HMODULE h, P & pointer, const char * name) 
{
    if (P p = reinterpret_cast <P> (GetProcAddress (h, name)))
    {
        pointer = p;
        return true;
    } else
        return false;
}
template <typename P>
bool Symbol (HMODULE h, P & pointer, USHORT index)
{
    return Symbol (h, pointer, MAKEINTRESOURCEA (index));
}

// Main code
int wmain(int argc, wchar_t** wargv)
{
    auto argv = argv_wchar_to_utf8(argc, wargv); // should be guaranteed to live until exit
    if (argv == nullptr) return -ENOMEM;

    // Application init
    int ret = ImGui::AppInit(argc, (char**)argv.get());
    if (ret != 0) return ret;

    if (HMODULE hUser32 = GetModuleHandleW (L"USER32"))
    {
        Symbol (hUser32, ptrEnableNonClientDpiScaling, "EnableNonClientDpiScaling");
        Symbol (hUser32, pfnSetThreadDpiAwarenessContext, "SetThreadDpiAwarenessContext");
        Symbol (hUser32, pfnGetDpiForSystem, "GetDpiForSystem");
        Symbol (hUser32, pfnGetDpiForWindow, "GetDpiForWindow");
    }
    if (pfnSetThreadDpiAwarenessContext)
    {
        pfnSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    }

    // Create application window
    //ImGui_ImplWin32_EnableDpiAwareness();
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui Main Window", nullptr };
    ::RegisterClassExW(&wc);
    g_Window = ::CreateWindowW(wc.lpszClassName, L"Dear ImGui main window", WS_OVERLAPPEDWINDOW, ImGui::SysWndPos.x, ImGui::SysWndPos.y, ImGui::SysWndPos.w, ImGui::SysWndPos.h, nullptr, nullptr, wc.hInstance, nullptr);

    // Initialize Direct3D
    if (!CreateDeviceD3D(g_Window))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    int cmdshow;
    switch (ImGui::SysWndState)
    {
        case ImGui::WSMinimized: cmdshow = SW_SHOWMINIMIZED; break;
        case ImGui::WSMaximized: cmdshow = SW_SHOWMAXIMIZED; break;
        default: cmdshow = SW_SHOWDEFAULT;
    }
    ::ShowWindow(g_Window, cmdshow);
    ::UpdateWindow(g_Window);

    ImGui::DPI = (ImU32) GetDPI(g_Window);

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
    ImGui_ImplWin32_Init(g_Window);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    //IM_ASSERT(font != nullptr);

    // Initial config
    ImGui::AppConfig();

    // Main loop
    while (!ImGui::AppExit)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                ImGui::AppExit = true;
        }
        if (ImGui::AppExit)
            break;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Signal to App on config change
        if (ImGui::AppReconfigure)
        {
            if (ImGui::AppConfig())
                ImGui_ImplDX11_ReCreateFontsTexture();
            ImGui::AppReconfigure = false;
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Delegate frame drawing to the app
        ImGui::AppNewFrame();

        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = {
                                                    ImGui::SysWndBgColor.x * ImGui::SysWndBgColor.w,
                                                    ImGui::SysWndBgColor.y * ImGui::SysWndBgColor.w,
                                                    ImGui::SysWndBgColor.z * ImGui::SysWndBgColor.w,
                                                    ImGui::SysWndBgColor.w
                                                };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        g_pSwapChain->Present(1, 0); // Present with vsync
        //g_pSwapChain->Present(0, 0); // Present without vsync
    }

    // Finalize App
    ImGui::AppDestroy();

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(g_Window);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    return ret;
}

// Helper functions

bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_NCCREATE:
        if (ptrEnableNonClientDpiScaling)
        {
            ptrEnableNonClientDpiScaling (hWnd); // required for v1 per-monitor scaling
        }
        break;
    case WM_ACTIVATE:
        ImGui::SysWndFocus = (bool)wParam;
        break;
    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
        RECT wr = { };
        DWORD ret = AdjustWindowRectEx(&wr, WS_OVERLAPPEDWINDOW, FALSE, 0);
        wr.right -= wr.left;
        wr.bottom -= wr.top;
        if (ImGui::SysWndMinMax.x)
            lpMMI->ptMinTrackSize.x = ImGui::SysWndMinMax.x + wr.right;
        if (ImGui::SysWndMinMax.y)
            lpMMI->ptMinTrackSize.y = ImGui::SysWndMinMax.y + wr.bottom;
        if (ImGui::SysWndMinMax.w)
            lpMMI->ptMaxTrackSize.x = ImGui::SysWndMinMax.w + wr.right;
        if (ImGui::SysWndMinMax.h)
            lpMMI->ptMaxTrackSize.y = ImGui::SysWndMinMax.h + wr.bottom;
        return 0;
    }
    case WM_WINDOWPOSCHANGED:
    {
        WINDOWPOS *wp = (WINDOWPOS*)lParam;
        LONG style = GetWindowLong(hWnd, GWL_STYLE);
        if (style & WS_MAXIMIZE)
            ImGui::SysWndState = ImGui::WSMaximized;
        else if (style & WS_MINIMIZE)
            ImGui::SysWndState = ImGui::WSMinimized;
        else
            ImGui::SysWndState = ImGui::WSNormal;
        if (!(wp->flags & SWP_NOSIZE) && ImGui::SysWndState != ImGui::WSMinimized)
        {
            RECT rect;
            GetClientRect(hWnd, &rect);
            g_ResizeWidth = (UINT)rect.right; // Queue resize
            g_ResizeHeight = (UINT)rect.bottom;
        }
        if (!(wp->flags & SWP_NOMOVE) || !(wp->flags & SWP_NOSIZE))
        {
            WINDOWPLACEMENT wpl;
            GetWindowPlacement(hWnd, &wpl);
            ImGui::SysWndPos.x = wpl.rcNormalPosition.left;
            ImGui::SysWndPos.y = wpl.rcNormalPosition.top;
            ImGui::SysWndPos.w = wpl.rcNormalPosition.right - wpl.rcNormalPosition.left;
            ImGui::SysWndPos.h = wpl.rcNormalPosition.bottom - wpl.rcNormalPosition.top;
        }
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_DPICHANGED:
        return OnDpiChange (wParam, reinterpret_cast <const RECT *> (lParam));
    case WM_THEMECHANGED:
    case WM_SETTINGCHANGE:
    case WM_DWMCOMPOSITIONCHANGED:
        g_idGlobalRefreshTimer = SetTimer (nullptr, g_idGlobalRefreshTimer, 500, GuiChangesCoalescingTimer);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Generalized DPI retrieval
//  - GetDpiFor(System/Window) available since 1607 / LTSB2016 / Server 2016
//  - GetDeviceCaps is classic way, working way back to XP
//
ImU32 GetDPI(HWND hWnd)
{
    if (hWnd != nullptr)
    {
        if (pfnGetDpiForWindow)
            return (ImU32) pfnGetDpiForWindow (hWnd);
    }
    else
    {
        if (pfnGetDpiForSystem)
            return (ImU32) pfnGetDpiForSystem ();
    }
    if (HDC hDC = GetDC (hWnd))
    {
        auto dpi = GetDeviceCaps (hDC, LOGPIXELSX);
        ReleaseDC (hWnd, hDC);
        return (ImU32) dpi;
    } else
        return USER_DEFAULT_SCREEN_DPI;
}

void CALLBACK GuiChangesCoalescingTimer(HWND hWnd, UINT, UINT_PTR id, DWORD)
{
    g_idGlobalRefreshTimer = 0;
    KillTimer (hWnd, id);

    // ignore if DPI awareness API is available
    if (pfnSetThreadDpiAwarenessContext == nullptr)
    {
        ImU32 dpi = GetDPI(hWnd);
        if (ImGui::DPI != dpi)
        {
            ImGui::DPI = dpi;
            ImGui::AppReconfigure = true;
        }
    }
}

LRESULT OnDpiChange(WPARAM dpi, const RECT * r)
{
    dpi = LOWORD (dpi);

    if (ImGui::DPI != dpi)
    {
        ImGui::DPI = (ImU32) dpi;
        ImGui::AppReconfigure = true;
    }

    SetWindowPos(g_Window, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, 0);
    return 0;
}

static std::unique_ptr<wchar_t[]> utf8_to_wchar(const char *utf8str)
{
    int bufsz = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8str, -1, nullptr, 0);
    if (bufsz == 0)
        return nullptr;
    auto buf = std::unique_ptr<wchar_t[]>(new wchar_t[bufsz]());
    if (buf == nullptr)
        return nullptr;
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8str, -1, buf.get(), bufsz);
    return buf;
}

static std::unique_ptr<char[]> argv_wchar_to_utf8(int argc, wchar_t const *const *wargv)
{
    size_t argsz = 0;
    for (int n = 0; n < argc; ++n) {
        int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wargv[n], -1, NULL, 0, NULL, NULL);
        argsz += bytes > 0 ? bytes : 1; // preserve arg as empty on conversion failure
    }

    size_t ptrsz = (argc + 1 /* +NULL */) * sizeof(char*);
    auto argv = std::unique_ptr<char[]>(new char[ptrsz + argsz]);
    char **ptrs = (char**)argv.get();
    char  *argsptr = argv.get() + ptrsz;
    for (int n = 0; n < argc; ++n)
    {
        ptrs[n] = argsptr;
        int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wargv[n], -1, argsptr, (int)argsz, NULL, NULL);
        if (bytes < 1) bytes = 1;
        argsptr[bytes - 1] = '\0'; // just in case
        argsptr += bytes;
        argsz   -= bytes;
    }
    ptrs[argc] = (char*)nullptr; // last ptr is NULL

    return argv;
}

static int ShellExecute2errno(HINSTANCE hcode)
{
    // https://learn.microsoft.com/en-us/windows/win32/api/shellapi/nf-shellapi-shellexecutew
    // The return value is cast as an HINSTANCE for backward compatibility with 16-bit Windows applications.
    // It is not a true HINSTANCE, however. It can be cast only to an INT_PTR and compared to either 32 or the following error codes below.
    auto code = reinterpret_cast<INT_PTR>(hcode);
    if (code > 32) return 0;
    switch(code)
    {
    case 0:
    case SE_ERR_OOM:                return ENOMEM;
    case SE_ERR_FNF:
    case SE_ERR_PNF:                return ENOENT;
    case ERROR_BAD_FORMAT:          return ENOEXEC;
    case SE_ERR_ACCESSDENIED:       return EACCES;
    case SE_ERR_ASSOCINCOMPLETE:
    case SE_ERR_NOASSOC:            return ESRCH;
    case SE_ERR_DDEBUSY:            return EBUSY;
    case SE_ERR_SHARE:              return EPERM;
    }
    return EFAULT;
}

// Backend specific functions
// set native window title
void ImGui::SysSetWindowTitle(const char *title)
{
    if (g_Window == nullptr) return;

    auto newtitle = utf8_to_wchar(title);
    if (newtitle == nullptr) return;

    ::SetWindowTextW(g_Window, newtitle.get());
}

void ImGui::SysMinimize()
{
    ::PostMessage(g_Window, WM_SYSCOMMAND, SC_MINIMIZE, 0);
}

void ImGui::SysMaximize()
{
    ::PostMessage(g_Window, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
}

void ImGui::SysRestore()
{
    ::PostMessage(g_Window, WM_SYSCOMMAND, SC_RESTORE, 0);
}

ImS32 ImGui::SysOpen(const char *resource)
{
    auto wresource = utf8_to_wchar(resource);
    if (wresource == nullptr) return EINVAL;

    return ShellExecute2errno(::ShellExecuteW(g_Window, L"open", wresource.get(), nullptr, nullptr, SW_SHOWNORMAL));
}
