#pragma once
#include "pch.h"
#include <d3d11.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>

namespace APGUI
{
    extern HWND g_hWnd;
    extern WNDPROC g_OriginalWndProc;

    void init(IDXGISwapChain* swapChain, ID3D11Device* device, ID3D11DeviceContext* deviceContext);
    void onFrame(IDXGISwapChain* swapChain);
    void warning();
    void ImGuiTab();

    void config(const toml::table& settings);
    void save(toml::table &settings);
}
