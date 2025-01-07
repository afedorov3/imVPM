#pragma once

/*
Adds ability to recreate and upload only the font texture
See https://github.com/ocornut/imgui/issues/2311

imgui_impl_dx11.cpp, just before #endif // #ifndef IMGUI_DISABLE
  #define DX11_FONT_STUB_IMPLEMENTATION
  #include "imgui_dx11_font_stub.h"

win32_dx11.cpp, before ImGui_ImplDX11_NewFrame();
  #include "imgui_dx11_font_stub.h"
  ...
  if (atlas_needs_upload) {
      ImGui_ImplDX11_ReCreateFontsTexture();
      atlas_needs_upload = false;
  }
  ...
*/

void ImGui_ImplDX11_ReCreateFontsTexture();

#ifdef DX11_FONT_STUB_IMPLEMENTATION
void ImGui_ImplDX11_ReCreateFontsTexture()
{
    ImGui_ImplDX11_Data* bd = ImGui_ImplDX11_GetBackendData();
    if (!bd->pd3dDevice)
        return;

    if (bd->pFontSampler)           { bd->pFontSampler->Release(); bd->pFontSampler = nullptr; }
    if (bd->pFontTextureView)       { bd->pFontTextureView->Release(); bd->pFontTextureView = nullptr; ImGui::GetIO().Fonts->SetTexID(0); } // We copied data->pFontTextureView to io.Fonts->TexID so let's clear that as well.
    ImGui_ImplDX11_CreateFontsTexture();
}
#endif // DX11_FONT_STUB_IMPLEMENTATION
