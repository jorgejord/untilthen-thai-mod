// sao_theme.hpp — Sword Art Online (Aincrad menu) styling + animated decorations for Dear ImGui.
// Dark translucent panels, cyan glow, angular corner brackets, scanlines, hover pulse.
#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <cmath>

namespace sao {

// ---- live theme state (an accent colour drives EVERYTHING incl. the decorations) ----
inline ImVec4 g_accent = ImVec4(0.18f, 0.83f, 0.92f, 1.f);  // default: SAO cyan
inline bool   g_light  = false;                              // false=dark, true=light background

// ---- palette (all derived from g_accent / g_light so themes retint the whole UI) ----
inline ImU32 Cyan(float a=1.f)     { return ImColor(g_accent.x, g_accent.y, g_accent.z, a); }
inline ImU32 CyanDim(float a=1.f)  { return ImColor(g_accent.x*0.45f, g_accent.y*0.50f, g_accent.z*0.52f, a); }
inline ImU32 Ink(float a=1.f)      { return g_light? ImColor(0.86f,0.90f,0.93f,a) : ImColor(0.04f, 0.06f, 0.08f, a); }
inline ImU32 Panel(float a=1.f)    { return g_light? ImColor(0.93f,0.95f,0.97f,a) : ImColor(0.07f, 0.10f, 0.13f, a); }
inline ImU32 TextC(float a=1.f)    { return g_light? ImColor(0.10f,0.14f,0.18f,a) : ImColor(0.85f, 0.92f, 0.95f, a); }

inline float Pulse(float speed=2.f, float lo=0.4f, float hi=1.f) {
    float t = (float)ImGui::GetTime();
    float s = 0.5f + 0.5f * sinf(t * speed);
    return lo + (hi - lo) * s;
}
inline float EaseOut(float x){ return 1.f - powf(1.f - x, 3.f); }

// SAO signature: four corner brackets `[  ]` around a rect, with optional animated draw-in.
inline void CornerBrackets(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float len=14.f, float th=2.f) {
    dl->AddLine(ImVec2(a.x, a.y), ImVec2(a.x+len, a.y), col, th);
    dl->AddLine(ImVec2(a.x, a.y), ImVec2(a.x, a.y+len), col, th);
    dl->AddLine(ImVec2(b.x, a.y), ImVec2(b.x-len, a.y), col, th);
    dl->AddLine(ImVec2(b.x, a.y), ImVec2(b.x, a.y+len), col, th);
    dl->AddLine(ImVec2(a.x, b.y), ImVec2(a.x+len, b.y), col, th);
    dl->AddLine(ImVec2(a.x, b.y), ImVec2(a.x, b.y-len), col, th);
    dl->AddLine(ImVec2(b.x, b.y), ImVec2(b.x-len, b.y), col, th);
    dl->AddLine(ImVec2(b.x, b.y), ImVec2(b.x, b.y-len), col, th);
}

// soft outer glow border (a few expanding translucent rects)
inline void GlowBorder(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 base, int layers=4, float rounding=2.f) {
    for (int i = layers; i >= 1; --i) {
        float e = (float)i * 1.6f;
        float alpha = 0.10f * (float)(layers - i + 1) / layers;
        dl->AddRect(ImVec2(a.x-e, a.y-e), ImVec2(b.x+e, b.y+e),
                    (base & 0x00FFFFFF) | ((ImU32)(alpha*255) << 24), rounding+e, 0, 1.5f);
    }
}

// animated horizontal scanline sweeping down a rect
inline void Scanlines(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float gap=4.f) {
    for (float y = a.y; y < b.y; y += gap)
        dl->AddLine(ImVec2(a.x, y), ImVec2(b.x, y), (col & 0x00FFFFFF) | 0x14000000, 1.f);
    float h = b.y - a.y;
    float t = fmodf((float)ImGui::GetTime() * 0.35f, 1.f);
    float sy = a.y + t * h;
    dl->AddRectFilledMultiColor(ImVec2(a.x, sy-10), ImVec2(b.x, sy+10),
        (col&0x00FFFFFF), (col&0x00FFFFFF), (col&0x00FFFFFF)|0x30000000, (col&0x00FFFFFF)|0x30000000);
}

// smooth highlight band that slides across a rect and pauses off-screen (clean, no flicker)
inline void Sheen(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float speed=0.16f) {
    float w = b.x - a.x;
    float t = fmodf((float)ImGui::GetTime() * speed, 1.7f);   // 1.0..1.7 = off-screen pause
    float cx = a.x + t * w;
    float bw = 70.f;
    ImU32 c0 = (col & 0x00FFFFFF);
    ImU32 cM = (col & 0x00FFFFFF) | 0x1E000000;
    dl->PushClipRect(a, b, true);
    dl->AddRectFilledMultiColor(ImVec2(cx-bw,a.y), ImVec2(cx,b.y), c0, cM, cM, c0);
    dl->AddRectFilledMultiColor(ImVec2(cx,a.y), ImVec2(cx+bw,b.y), cM, c0, c0, cM);
    dl->PopClipRect();
}

// SAO skill-menu selection bar: bright cyan left edge + gradient fading right + glow
inline void SkillBar(ImDrawList* dl, ImVec2 a, ImVec2 b, float p=1.f) {
    ImU32 cBright = Cyan(0.55f*p);
    ImU32 cFade   = Cyan(0.0f);
    dl->AddRectFilledMultiColor(a, b, cBright, cFade, cFade, cBright); // L bright -> R transparent
    dl->AddRectFilled(ImVec2(a.x,a.y), ImVec2(a.x+3.f,b.y), Cyan(0.95f*p)); // left accent line
    dl->AddLine(ImVec2(a.x,b.y-1), ImVec2(b.x,b.y-1), Cyan(0.25f*p), 1.f);
}

// glowing horizontal accent bar (used under headers)
inline void AccentBar(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col) {
    dl->AddRectFilledMultiColor(a, b, col, (col&0x00FFFFFF)|0x10000000,
                                (col&0x00FFFFFF)|0x10000000, col);
}

inline void ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 3.f; s.FrameRounding = 2.f; s.ChildRounding = 3.f;
    s.GrabRounding = 2.f; s.PopupRounding = 3.f; s.ScrollbarRounding = 2.f;
    s.WindowBorderSize = 1.f; s.FrameBorderSize = 1.f; s.ChildBorderSize = 1.f;
    s.WindowPadding = ImVec2(22,12); s.FramePadding = ImVec2(11,8); s.ItemSpacing = ImVec2(9,9);
    s.TabRounding = 2.f; s.TabBarBorderSize = 2.f;
    s.ScrollbarSize = 12.f; s.GrabMinSize = 10.f;
    ImVec4* c = s.Colors;
    const ImVec4 ac = g_accent;                                  // accent (theme colour)
    // accent at brightness scale `m`, alpha `a`
    auto A=[&](float m,float a){ float k=g_light? (1.f-(1.f-m)) : m; (void)k; return ImVec4(ac.x*m, ac.y*m, ac.z*m, a); };
    auto C=[&](float r,float g,float b,float a){ return ImVec4(r,g,b,a); };
    if(g_light){
        c[ImGuiCol_Text]             = C(0.10f,0.14f,0.18f,1.f);
        c[ImGuiCol_TextDisabled]     = C(0.45f,0.50f,0.55f,1.f);
        c[ImGuiCol_WindowBg]         = C(0.93f,0.95f,0.97f,1.f);
        c[ImGuiCol_ChildBg]          = C(0.97f,0.98f,0.99f,0.65f);
        c[ImGuiCol_PopupBg]          = C(0.96f,0.97f,0.98f,0.99f);
        c[ImGuiCol_FrameBg]          = C(0.88f,0.91f,0.94f,0.95f);
        c[ImGuiCol_FrameBgHovered]   = A(0.85f,0.30f);
        c[ImGuiCol_FrameBgActive]    = A(0.80f,0.45f);
        c[ImGuiCol_TitleBg]          = C(0.85f,0.88f,0.91f,1.f);
        c[ImGuiCol_TitleBgActive]    = A(0.85f,0.55f);
        c[ImGuiCol_MenuBarBg]        = C(0.88f,0.90f,0.93f,1.f);
        c[ImGuiCol_ScrollbarBg]      = C(0.88f,0.90f,0.92f,0.6f);
        c[ImGuiCol_TableHeaderBg]    = C(0.84f,0.88f,0.92f,1.f);
        c[ImGuiCol_TableRowBg]       = C(1.f,1.f,1.f,0.35f);
        c[ImGuiCol_TableRowBgAlt]    = C(0.90f,0.93f,0.96f,0.55f);
    } else {
        c[ImGuiCol_Text]             = C(0.85f,0.92f,0.95f,1.f);
        c[ImGuiCol_TextDisabled]     = C(0.40f,0.50f,0.55f,1.f);
        c[ImGuiCol_WindowBg]         = C(0.035f,0.055f,0.075f,0.94f);
        c[ImGuiCol_ChildBg]          = C(0.05f,0.075f,0.10f,0.55f);
        c[ImGuiCol_PopupBg]          = C(0.045f,0.075f,0.10f,0.99f);
        c[ImGuiCol_FrameBg]          = C(0.08f,0.13f,0.16f,0.85f);
        c[ImGuiCol_FrameBgHovered]   = A(0.42f,0.90f);
        c[ImGuiCol_FrameBgActive]    = A(0.58f,0.95f);
        c[ImGuiCol_TitleBg]          = C(0.04f,0.07f,0.09f,1.f);
        c[ImGuiCol_TitleBgActive]    = A(0.20f,1.f);
        c[ImGuiCol_MenuBarBg]        = C(0.05f,0.08f,0.10f,1.f);
        c[ImGuiCol_ScrollbarBg]      = C(0.03f,0.05f,0.07f,0.6f);
        c[ImGuiCol_TableHeaderBg]    = C(0.06f,0.13f,0.16f,1.f);
        c[ImGuiCol_TableRowBg]       = C(0.05f,0.08f,0.10f,0.35f);
        c[ImGuiCol_TableRowBgAlt]    = C(0.07f,0.11f,0.14f,0.45f);
    }
    // accent-driven slots (identical formula in both modes -> theme colour shows through)
    c[ImGuiCol_Border]               = ImVec4(ac.x,ac.y,ac.z,0.85f);
    c[ImGuiCol_Header]               = A(0.42f,0.70f);
    c[ImGuiCol_HeaderHovered]        = A(0.68f,0.80f);
    c[ImGuiCol_HeaderActive]         = A(0.85f,0.90f);
    c[ImGuiCol_Button]               = A(0.28f,0.85f);
    c[ImGuiCol_ButtonHovered]        = A(0.68f,0.95f);
    c[ImGuiCol_ButtonActive]         = ImVec4(ac.x,ac.y,ac.z,1.f);
    c[ImGuiCol_CheckMark]            = ImVec4(ac.x,ac.y,ac.z,1.f);
    c[ImGuiCol_SliderGrab]           = A(0.95f,1.f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(ac.x,ac.y,ac.z,1.f);
    c[ImGuiCol_Separator]            = A(0.88f,0.7f);
    c[ImGuiCol_ScrollbarGrab]        = A(0.50f,0.8f);
    c[ImGuiCol_ScrollbarGrabHovered] = A(0.72f,0.9f);
    c[ImGuiCol_TableBorderStrong]    = A(0.80f,0.72f);
    c[ImGuiCol_TableBorderLight]     = A(0.55f,0.5f);
    c[ImGuiCol_TextSelectedBg]       = A(0.72f,0.45f);
    c[ImGuiCol_Tab]                  = A(0.16f,0.95f);
    c[ImGuiCol_TabHovered]           = A(0.68f,0.85f);
    c[ImGuiCol_TabSelected]          = A(0.42f,1.f);
    c[ImGuiCol_TabSelectedOverline]  = ImVec4(ac.x,ac.y,ac.z,1.f);
    c[ImGuiCol_TabDimmed]            = A(0.12f,1.f);
    c[ImGuiCol_TabDimmedSelected]    = A(0.26f,1.f);
}

} // namespace sao
