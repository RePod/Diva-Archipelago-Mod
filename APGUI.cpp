#include "APGUI.h"
#include "APDeathLink.h"
#include "APTraps.h"

namespace APGUI
{
    bool showImGuiDemo = false;
    bool g_ImGuiInitialized = false;

    bool autohide = true; // Hide Client during gameplay

    ID3D11Device* g_Device = nullptr;
    ID3D11DeviceContext* g_Context = nullptr;
    HWND g_hWnd = nullptr;
    WNDPROC g_OriginalWndProc = nullptr;

    void init(IDXGISwapChain* swapChain)
    {
        if (g_ImGuiInitialized)
            return;

        ImGui_ImplWin32_EnableDpiAwareness();
        float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

        // Get device + context
        swapChain->GetDevice(__uuidof(ID3D11Device), (void**)&g_Device);
        g_Device->GetImmediateContext(&g_Context);

        // Get window handle
        DXGI_SWAP_CHAIN_DESC desc;
        swapChain->GetDesc(&desc);
        g_hWnd = desc.OutputWindow;

        // Init ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

        ImGuiStyle& style = ImGui::GetStyle();
        style.ScaleAllSizes(main_scale);
        style.FontScaleDpi = main_scale;

        ImGui_ImplWin32_Init(g_hWnd);
        ImGui_ImplDX11_Init(g_Device, g_Context);

        g_ImGuiInitialized = true;
    }

    void onFrame()
    {
        // autohide, in game, not paused, not on results
        auto PvPlayData = 0x1412C2330;
        if (autohide && *(bool*)PvPlayData && !*(bool*)(PvPlayData + 0x1) && !*(bool*)(PvPlayData + 0x2D17D)) {
            ImGui::GetIO().WantCaptureKeyboard = false;
            ImGui::GetIO().WantCaptureMouse = false;
            return;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (showImGuiDemo)
            ImGui::ShowDemoWindow();

        ImGui::Begin("Archipelago Mod");

        if (ImGui::BeginTabBar("APTabs")) {
            APGUI::ImGuiTab();
            APDeathLink::ImGuiTab();
            APTraps::ImGuiTab();

            //APLogger::ImGuiTab();

            ImGui::EndTabBar();
        }

        ImGui::End();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    void ImGuiTab()
    {
        if (ImGui::BeginTabItem("Client")) {
            ImGui::Checkbox("Hide during gameplay", &autohide);
            ImGui::Checkbox("Show ImGui demo", &showImGuiDemo);

            ImGui::EndTabItem();
        }
    }
}