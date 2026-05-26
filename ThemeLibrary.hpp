#pragma once
#include <imgui.h>

namespace ThemeLibrary {
    inline void ApplyDeepSpace() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding     = ImVec2(15, 15);
        style.FramePadding      = ImVec2(10, 8);
        style.ItemSpacing       = ImVec2(12, 10);
        style.WindowRounding    = 12.0f; 
        style.FrameRounding     = 6.0f;
        style.PopupRounding     = 8.0f;
        style.ScrollbarRounding = 12.0f;
        style.GrabRounding      = 6.0f;
        style.WindowBorderSize  = 1.0f; 
        style.ChildBorderSize   = 1.0f;
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.92f, 0.96f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.05f, 0.05f, 0.08f, 1.00f); // Opaque for performance
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.06f, 0.07f, 0.10f, 0.98f);
        colors[ImGuiCol_Border]                 = ImVec4(0.30f, 0.40f, 0.60f, 0.50f); // Reflective Border
        colors[ImGuiCol_FrameBg]                = ImVec4(0.12f, 0.14f, 0.22f, 1.00f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.25f, 0.35f, 1.00f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.25f, 0.32f, 0.45f, 1.00f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.03f, 0.03f, 0.05f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.10f, 0.12f, 0.20f, 1.00f); // Glassy Title
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.05f, 0.05f, 0.07f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.18f, 0.22f, 0.35f, 1.00f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.10f, 0.30f, 0.50f, 1.00f); // Reflective Hover
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.00f, 0.60f, 0.85f, 0.80f); 
        colors[ImGuiCol_Button]                 = ImVec4(0.18f, 0.22f, 0.32f, 1.00f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.00f, 0.55f, 0.80f, 0.70f); 
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.00f, 0.45f, 0.70f, 1.00f);
        colors[ImGuiCol_Separator]              = ImVec4(0.25f, 0.30f, 0.45f, 0.50f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.00f, 0.70f, 0.90f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.00f, 0.50f, 0.80f, 0.35f);
    }
    
    inline void ApplyGlassmorphism() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(20, 20);
        style.FramePadding = ImVec2(12, 10);
        style.ItemSpacing = ImVec2(12, 12);
        style.WindowRounding = 18.0f;
        style.FrameRounding = 10.0f;
        style.PopupRounding = 12.0f;
        style.ScrollbarRounding = 15.0f;
        style.WindowBorderSize = 1.0f;
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.08f);
        colors[ImGuiCol_ChildBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.03f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.05f, 0.08f, 0.12f, 0.92f);
        colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.10f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.30f);
        colors[ImGuiCol_Button] = ImVec4(0.40f, 0.60f, 1.00f, 0.25f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.40f, 0.60f, 1.00f, 0.45f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.40f, 0.60f, 1.00f, 0.65f);
        colors[ImGuiCol_Header] = ImVec4(1.00f, 1.00f, 1.00f, 0.15f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(1.00f, 1.00f, 1.00f, 0.25f);
        colors[ImGuiCol_HeaderActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.35f);
        colors[ImGuiCol_Separator] = ImVec4(1.00f, 1.00f, 1.00f, 0.15f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.20f);
    }
    
    inline void ApplyCyberpunk() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding = ImVec2(10, 10);
        style.FramePadding = ImVec2(8, 6);
        style.ItemSpacing = ImVec2(10, 10);
        style.WindowRounding = 0.0f;
        style.FrameRounding = 0.0f;
        style.PopupRounding = 0.0f;
        style.ScrollbarRounding = 0.0f;
        style.GrabRounding = 0.0f;
        style.WindowBorderSize = 1.0f;
        style.ChildBorderSize = 1.0f;
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.08f, 0.95f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
        colors[ImGuiCol_Border] = ImVec4(1.00f, 0.00f, 0.80f, 0.80f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.15f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(1.00f, 0.00f, 0.80f, 0.20f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(1.00f, 0.00f, 0.80f, 0.40f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.05f, 0.05f, 0.08f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.05f, 0.05f, 0.08f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.00f, 1.00f, 1.00f, 0.20f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 1.00f, 1.00f, 0.40f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.00f, 1.00f, 1.00f, 0.60f);
        colors[ImGuiCol_Button] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(1.00f, 0.00f, 0.80f, 0.20f);
        colors[ImGuiCol_ButtonActive] = ImVec4(1.00f, 0.00f, 0.80f, 0.50f);
        colors[ImGuiCol_Separator] = ImVec4(1.00f, 0.00f, 0.80f, 0.50f);
    }
    
    inline void ApplySolarLight() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 8.0f;
        style.FrameRounding = 4.0f;
        style.WindowBorderSize = 1.0f;
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
        colors[ImGuiCol_WindowBg] = ImVec4(0.94f, 0.96f, 1.00f, 1.00f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.98f);
        colors[ImGuiCol_Border] = ImVec4(0.70f, 0.75f, 0.85f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.85f, 0.90f, 1.00f, 1.00f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.75f, 0.85f, 1.00f, 1.00f);
        colors[ImGuiCol_TitleBg] = ImVec4(0.85f, 0.90f, 0.95f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.80f, 0.85f, 0.90f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.90f, 0.92f, 0.96f, 1.00f);
        colors[ImGuiCol_Header] = ImVec4(0.80f, 0.85f, 1.00f, 1.00f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.70f, 0.80f, 1.00f, 1.00f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.60f, 0.75f, 1.00f, 1.00f);
        colors[ImGuiCol_Button] = ImVec4(0.85f, 0.90f, 0.95f, 1.00f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.75f, 0.85f, 1.00f, 1.00f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.65f, 0.75f, 0.95f, 1.00f);
    }

    inline void ApplyCustomBackground() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowRounding = 12.0f;
        style.FrameRounding = 6.0f;
        style.WindowBorderSize = 1.0f;
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.07f, 0.60f); 
        colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.10f);
        colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
        colors[ImGuiCol_Border] = ImVec4(1.00f, 1.00f, 1.00f, 0.20f);
        colors[ImGuiCol_FrameBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.40f);
    }

    inline void ApplyHolographicGlass() {
        ImGuiStyle& style = ImGui::GetStyle();
        style.WindowPadding     = ImVec2(20, 20);
        style.FramePadding      = ImVec2(10, 8);
        style.ItemSpacing       = ImVec2(10, 10);
        style.WindowRounding    = 10.0f; // Tech look: moderately rounded
        style.FrameRounding     = 4.0f;
        style.PopupRounding     = 8.0f;
        style.ScrollbarRounding = 10.0f;
        style.GrabRounding      = 4.0f;
        style.WindowBorderSize  = 1.0f;
        style.ChildBorderSize   = 1.0f;
        ImVec4* colors = style.Colors;
        colors[ImGuiCol_Text]                   = ImVec4(0.90f, 0.95f, 1.00f, 1.00f); // Ice White
        colors[ImGuiCol_WindowBg]               = ImVec4(0.02f, 0.02f, 0.05f, 0.75f); // Deep Void Glass
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.05f, 0.05f, 0.10f, 0.90f);
        colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.80f, 0.90f, 0.35f); // Cyan Glow Border
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.00f, 0.20f, 0.30f, 0.20f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.00f, 0.40f, 0.50f, 0.30f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.00f, 0.60f, 0.70f, 0.40f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.02f, 0.02f, 0.05f, 0.80f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.02f, 0.02f, 0.05f, 0.80f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.02f, 0.02f, 0.05f, 0.80f);
        colors[ImGuiCol_Header]                 = ImVec4(0.00f, 0.40f, 0.50f, 0.20f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.00f, 0.60f, 0.70f, 0.30f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.00f, 0.80f, 0.90f, 0.40f); // Bright Cyan
        colors[ImGuiCol_Button]                 = ImVec4(0.00f, 0.30f, 0.40f, 0.20f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.00f, 0.60f, 0.70f, 0.30f); 
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.00f, 0.80f, 0.90f, 0.50f);
        colors[ImGuiCol_Separator]              = ImVec4(0.00f, 0.80f, 0.90f, 0.30f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.00f, 0.90f, 1.00f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.00f, 0.60f, 0.80f, 0.35f);
    }

    inline void ApplyThemeByIndex(int index) {
        switch(index) {
            case 0: ApplyDeepSpace(); break;
            case 1: ApplyGlassmorphism(); break;
            case 2: ApplyCyberpunk(); break;
            case 3: ApplySolarLight(); break;
            case 4: ApplyCustomBackground(); break;
            case 5: ApplyHolographicGlass(); break;
            default: ApplyDeepSpace(); break;
        }
    }
}