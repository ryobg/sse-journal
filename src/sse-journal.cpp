/**
 * @file sse-journal.cpp
 * @brief Main functionality for SSE Journal
 * @internal
 *
 * This file is part of Skyrim SE Journal mod (aka Journal).
 *
 *   Journal is free software: you can redistribute it and/or modify it
 *   under the terms of the GNU Lesser General Public License as published
 *   by the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Journal is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with Journal. If not, see <http://www.gnu.org/licenses/>.
 *
 * @endinternal
 *
 * @ingroup Core
 *
 * @details
 */

#include <sse-imgui/sse-imgui.h>
#include <sse-gui/sse-gui.h>

#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include <cstring>
#include <functional>
#include <iostream>
#include <ctime>

#include <d3d11.h>
#include <DDSTextureLoader/DDSTextureLoader.h>

//--------------------------------------------------------------------------------------------------

// Also for SSE-ImGui
#define IM_COL32_R_SHIFT    0
#define IM_COL32_G_SHIFT    8
#define IM_COL32_B_SHIFT    16
#define IM_COL32_A_SHIFT    24
#define IM_COL32_A_MASK     0xFF000000
#define IM_COL32(R,G,B,A) (((ImU32)(A)<<IM_COL32_A_SHIFT) | ((ImU32)(B)<<IM_COL32_B_SHIFT) | ((ImU32)(G)<<IM_COL32_G_SHIFT) | ((ImU32)(R)<<IM_COL32_R_SHIFT))
#define IM_COL32_WHITE       IM_COL32(255,255,255,255)  // Opaque white = 0xFFFFFFFF
#define IM_COL32_BLACK       IM_COL32(0,0,0,255)        // Opaque black
#define IM_COL32_BLACK_TRANS IM_COL32(0,0,0,0)          // Transparent black = 0x00000000

//--------------------------------------------------------------------------------------------------

/// Defined in skse.cpp
extern std::ofstream& log ();

/// Defined in skse.cpp
extern imgui_api imgui;

/// Defined in skse.cpp
extern std::unique_ptr<ssegui_api> ssegui;

auto constexpr lite_tint = IM_COL32 (191, 157, 111,  64);
auto constexpr dark_tint = IM_COL32 (191, 157, 111,  96);
auto constexpr frame_col = IM_COL32 (192, 157, 111, 192);

//--------------------------------------------------------------------------------------------------

/// Wraps up common logic for drawing a button, uses shared state across the instances
class button_t
{
    ImVec2 tl, sz, align;
    const char *label, *label_end;
    std::uint32_t hover_tint;
public:
    static ImFont* font;
    static std::uint32_t* color;
    static ID3D11ShaderResourceView* background;

    static ImVec2 wpos, wsz; ///< Updated on each frame

    button_t () : label (nullptr), label_end (nullptr) {}
    button_t (const char* label,
            float tlx, float tly, float szx, float szy,
            std::uint32_t hover, float ax = .5f, float ay = .5f)
    {
        this->align = ImVec2 { ax, ay };
        this->label = label;
        label_end = std::strchr (label, '#');
        tl.x = tlx, tl.y = tly, sz.x = szx, sz.y = szy;
        hover_tint = hover;
    }

    bool draw ()
    {
        imgui.igPushFont (font);
        imgui.igPushStyleColorU32 (ImGuiCol_Text, *color);
        ImVec2 ptl { wsz.x * tl.x, wsz.y * tl.y },
               psz { wsz.x * sz.x, wsz.y * sz.y };
        imgui.igSetCursorPos (ptl);
        bool pressed = imgui.igInvisibleButton (label, psz);
        bool hovered = imgui.igIsItemHovered (0);
        if (hovered)
        {
            constexpr float vmax = .7226f; // The Background Y pixels reach ~72% of a 2k texture
            imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), background,
                ImVec2 { wpos.x + ptl.x,         wpos.y + ptl.y         },
                ImVec2 { wpos.x + ptl.x + psz.x, wpos.y + ptl.y + psz.y },
                ImVec2 { tl.x, tl.y*vmax }, ImVec2 { tl.x + sz.x, (tl.y + sz.y)*vmax }, hover_tint);
        }
        auto txtsz = imgui.igCalcTextSize (label, label_end, false, -1.f);
        imgui.igSetCursorPos (ImVec2 { ptl.x + align.x * (psz.x - txtsz.x),
                                       ptl.y + align.y * (psz.y - txtsz.y) });
        imgui.igTextUnformatted (label, label_end);
        imgui.igPopFont ();
        imgui.igPopStyleColor (1);
        return pressed;
    }
};

ImFont* button_t::font = nullptr;
std::uint32_t* button_t::color = nullptr;
ID3D11ShaderResourceView* button_t::background = nullptr;
ImVec2 button_t::wpos = {};
ImVec2 button_t::wsz = {};

//--------------------------------------------------------------------------------------------------

/// State of the current Journal run
struct {
    ID3D11Device*           device;
    ID3D11DeviceContext*    context;
    IDXGISwapChain*         chain;
    HWND                    window;

    ID3D11ShaderResourceView* background;
    std::string left_title, left_text, right_title, right_text;

    ImFont *button_font, *chapter_font, *text_font, *system_font;
    std::uint32_t button_color, chapter_color, text_color;

    button_t button_prev, button_next,
             button_settings, button_variables, button_chapters,
             button_save, button_saveas, button_load;
    bool show_settings, show_variables, show_chapters;

    std::vector<std::pair<std::string, std::function<std::string ()>>> variables;
}
journal = {};

//--------------------------------------------------------------------------------------------------

bool
setup ()
{
    if (!ssegui->parameter ("ID3D11Device", &journal.device)
            || !ssegui->parameter ("ID3D11DeviceContext", &journal.context)
            || !ssegui->parameter ("IDXGISwapChain", &journal.chain)
            || !ssegui->parameter ("window", &journal.window)
            )
    {
        log () << "Unable to fetch SSE GUI parameters." << std::endl;
        return false;
    }

    // This is good candidate to offload to SSE ImGui or SSE-GUI
    if (FAILED (DirectX::CreateDDSTextureFromFile (journal.device, journal.context,
                    L"Data\\interface\\sse-journal\\book.dds", nullptr, &journal.background)))
    {
        log () << "Unable to load DDS." << std::endl;
        return false;
    }

    auto fa = imgui.igGetIO ()->Fonts;
    // This MUST go to SSE ImGui
    imgui.ImFontAtlas_AddFontDefault (fa, nullptr);

    extern ImFont* inconsolata_font (float, const ImFontConfig*, const ImWchar*);
    extern ImFont* viner_font (float, const ImFontConfig*, const ImWchar*);
    journal.system_font  = inconsolata_font (24.f, nullptr, nullptr);
    journal.text_font    = viner_font (48.f, nullptr, nullptr);//merge later so != button_font
    journal.chapter_font = viner_font (64.f, nullptr, nullptr);
    journal.button_font  = viner_font (48.f, nullptr, nullptr);
    if (!journal.system_font || !journal.text_font || !journal.button_font || !journal.chapter_font)
    {
        log () << "Unable to load fonts!" << std::endl;
        return false;
    }

    journal.button_color = IM_COL32_WHITE;
    journal.chapter_color = IM_COL32_BLACK;
    journal.text_color = IM_COL32 (42, 34, 24, 192);

    button_t::font = journal.button_font;
    button_t::color = &journal.button_color;
    button_t::background = journal.background;
    auto& j = journal;
    j.button_prev      = button_t ("Prev##B"     ,   0.f, 0, .050f,   1.f, lite_tint);
    j.button_settings  = button_t ("Settings##B" , .070f, 0, .128f, .060f, dark_tint, .5f, .85f);
    j.button_variables = button_t ("Variables##B", .212f, 0, .128f, .060f, dark_tint, .5f, .85f);
    j.button_chapters  = button_t ("Chapters##B" , .354f, 0, .128f, .060f, dark_tint, .5f, .85f);
    j.button_save      = button_t ("Save##B"     , .528f, 0, .128f, .060f, dark_tint, .5f, .85f);
    j.button_saveas    = button_t ("Save As##B"  , .670f, 0, .128f, .060f, dark_tint, .5f, .85f);
    j.button_load      = button_t ("Load##B"     , .812f, 0, .128f, .060f, dark_tint, .5f, .85f);
    j.button_next      = button_t ("Next##B"     ,  .95f, 0, .050f,   1.f, lite_tint);

    journal.variables.emplace_back ("Local time", [] () -> std::string
        {
            std::array<char, 100> buff = {};
            std::time_t t = std::time (nullptr);
            std::strftime (buff.data (), buff.size (), "%X", std::localtime (&t));
            return buff.data ();
        });
    return true;
}

//--------------------------------------------------------------------------------------------------

/// Resizing one by one causes FPS stutters and CDTs, hence minimal SSO size + power of 2

static int
imgui_text_resize (ImGuiInputTextCallbackData* data)
{
    auto better_size = [] (std::size_t n)
    {
        std::size_t p = 16;
        while (p < n) p <<= 1;
        return p;
    };
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        auto str = reinterpret_cast<std::string*> (data->UserData);
        str->resize (better_size (data->BufSize));
        data->Buf = const_cast<char*> (str->c_str ());
    }
    return 0;
}

static bool
imgui_input_text (const char* label, std::string& text)
{
    if (text.size () < 16) text.resize (16);
    return imgui.igInputText (
            label, &text[0], text.size (),
            ImGuiInputTextFlags_CallbackResize, imgui_text_resize, &text);
}

static bool
imgui_input_multiline (const char* label, std::string& text, ImVec2 const& size)
{
    if (text.size () < 16) text.resize (16);
    return imgui.igInputTextMultiline (
            label, &text[0], text.size (), size,
            ImGuiInputTextFlags_CallbackResize, imgui_text_resize, &text);
}

//--------------------------------------------------------------------------------------------------

void SSEIMGUI_CCONV
render (int active)
{
    if (!active)
        return;

    imgui.igBegin ("SSE Journal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);
    int push_col = 0;
    imgui.igPushStyleColorU32 (ImGuiCol_FrameBg, 0); ++push_col;
    imgui.igPushStyleVarFloat (ImGuiStyleVar_FrameBorderSize, 0);

    auto wpos = button_t::wpos = imgui.igGetWindowPos ();
    auto wsz  = button_t::wsz  = imgui.igGetWindowSize ();

    imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
            wpos, ImVec2 {wpos.x+wsz.x, wpos.y+wsz.y}, ImVec2 {0,0}, ImVec2 {1,.7226f},
            imgui.igGetColorU32Vec4 (ImVec4 {1,1,1,1}));

    // Ratio, ratio multiplied by pixel size and the absolute positions summed with these
    // are used all below. It may be pulled off as more capsulated and less dublication.
    const float text_width    = .412f * wsz.x;
    const float text_height   = .784f * wsz.y;

    const float left_page  = .070f * wsz.x;
    const float right_page = .528f * wsz.x;
    const float title_top  = .095f * wsz.y;
    const float text_top   = .159f * wsz.y;
    const float status_top = .950f * wsz.y;

    // Port/larboard/ladebord
    // Starboard/steobord

    if (journal.button_settings.draw ())
        journal.show_settings = !journal.show_settings;
    if (journal.button_variables.draw ())
        journal.show_variables = !journal.show_variables;
    if (journal.button_chapters.draw ())
        journal.show_chapters = !journal.show_chapters;

    bool save = journal.button_save.draw ();
    bool saveas = journal.button_saveas.draw ();
    bool load = journal.button_load.draw ();
    bool prev = journal.button_prev.draw ();
    bool next = journal.button_next.draw ();

    imgui.igPushFont (journal.chapter_font);
    imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.chapter_color);

    imgui.igSetNextItemWidth (text_width);
    imgui.igSetCursorPos (ImVec2 { left_page, title_top });
    imgui_input_text ("##Left title", journal.left_title);
    if (imgui.igIsItemHovered (0) && !imgui.igIsItemActive ())
        imgui.ImDrawList_AddRect (imgui.igGetWindowDrawList (),
                ImVec2 { wpos.x+left_page, wpos.y+title_top },
                ImVec2 { wpos.x+left_page+text_width, wpos.y+title_top+imgui.igGetFrameHeight () },
                frame_col, 0, ImDrawCornerFlags_All, 2.f);

    imgui.igSetCursorPos (ImVec2 { right_page, title_top });
    imgui.igSetNextItemWidth (text_width);
    imgui_input_text ("##Right title", journal.right_title);
    if (imgui.igIsItemHovered (0) && !imgui.igIsItemActive ())
        imgui.ImDrawList_AddRect (imgui.igGetWindowDrawList (),
                ImVec2 { wpos.x+right_page, wpos.y+title_top },
                ImVec2 { wpos.x+right_page+text_width, wpos.y+title_top+imgui.igGetFrameHeight () },
                frame_col, 0, ImDrawCornerFlags_All, 2.f);

    imgui.igPopFont ();
    imgui.igPopStyleColor (1);
    imgui.igPushFont (journal.text_font);
    imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.text_color);

    imgui.igSetCursorPos (ImVec2 { left_page, text_top });
    imgui_input_multiline ("##Left text", journal.left_text, ImVec2 { text_width, text_height });
    if (imgui.igIsItemHovered (0) && !imgui.igIsItemActive ())
        imgui.ImDrawList_AddRect (imgui.igGetWindowDrawList (),
                ImVec2 { wpos.x+left_page, wpos.y+text_top },
                ImVec2 { wpos.x+left_page+text_width, wpos.y+text_top+text_height },
                frame_col, 0, ImDrawCornerFlags_All, 2.f);

    imgui.igSetCursorPos (ImVec2 { right_page, text_top });
    imgui_input_multiline ("##Right text", journal.right_text, ImVec2 { text_width, text_height });
    if (imgui.igIsItemHovered (0) && !imgui.igIsItemActive ())
        imgui.ImDrawList_AddRect (imgui.igGetWindowDrawList (),
                ImVec2 { wpos.x+right_page, wpos.y+text_top },
                ImVec2 { wpos.x+right_page+text_width, wpos.y+text_top+text_height },
                frame_col, 0, ImDrawCornerFlags_All, 2.f);

    imgui.igPopFont ();
    imgui.igPopStyleColor (1);
    imgui.igPushFont (journal.text_font);
    imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.button_color);

    imgui.igSetCursorPos (ImVec2 { left_page, status_top });
    imgui.igText ("Port status bar goes here");
    imgui.igSetCursorPos (ImVec2 { right_page, status_top });
    imgui.igText ("Starboard status bar goes here");

    imgui.igPopFont ();
    imgui.igPopStyleColor (1);

    imgui.igPopStyleVar (1);
    imgui.igPopStyleColor (push_col);
    imgui.igEnd ();

    extern void draw_settings ();
    if (journal.show_settings)
        draw_settings ();
    extern void draw_variables ();
    if (journal.show_variables)
        draw_variables ();
    extern void draw_chapters ();
    if (journal.show_chapters)
        draw_chapters ();
}

//--------------------------------------------------------------------------------------------------

void draw_settings ()
{
    imgui.igBegin ("SSE Journal: Settings", nullptr, 0);
    imgui.igPushFont (journal.system_font);

    static ImVec4 button_c  = imgui.igColorConvertU32ToFloat4 (journal.button_color),
                  chapter_c = imgui.igColorConvertU32ToFloat4 (journal.chapter_color),
                  text_c    = imgui.igColorConvertU32ToFloat4 (journal.text_color);
    constexpr int cflags = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayHSV
        | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar
        | ImGuiColorEditFlags_AlphaBar;

    imgui.igText ("Buttons font:");
    if (imgui.igColorEdit4 ("Color##Buttons", (float*) &button_c, cflags))
        journal.button_color = imgui.igGetColorU32Vec4 (button_c);
    imgui.igSliderFloat ("Scale##Buttons", &journal.button_font->Scale, .5f, 2.f, "%.2f", 1);

    imgui.igText ("Titles font:");
    if (imgui.igColorEdit4 ("Color##Titles", (float*) &chapter_c, cflags))
        journal.chapter_color = imgui.igGetColorU32Vec4 (chapter_c);
    imgui.igSliderFloat ("Scale##Titles", &journal.chapter_font->Scale, .5f, 2.f, "%.2f", 1);

    imgui.igText ("Text font:");
    if (imgui.igColorEdit4 ("Color##Text", (float*) &text_c, cflags))
        journal.text_color = imgui.igGetColorU32Vec4 (text_c);
    imgui.igSliderFloat ("Scale##Text", &journal.text_font->Scale, .5f, 2.f, "%.2f", 1);

    imgui.igText ("Default font:");
    imgui.igSliderFloat ("Scale", &journal.system_font->Scale, .5f, 2.f, "%.2f", 1);

    imgui.igPopFont ();
    imgui.igEnd ();
}

//--------------------------------------------------------------------------------------------------

static bool
extract_variable_text (void* data, int idx, const char** out_text)
{
    auto vars = reinterpret_cast<decltype (journal.variables)*> (data);
    *out_text = vars->at (idx).first.c_str ();
    return true;
}

void draw_variables ()
{
    imgui.igBegin ("SSE Journal: Variables", nullptr, 0);
    imgui.igPushFont (journal.system_font);

    static int selection = -1;
    static std::string output;

    if (imgui.igListBoxFnPtr ("Available variables", &selection, extract_variable_text,
            &journal.variables, static_cast<int> (journal.variables.size ()), -1))
    {
        if (unsigned (selection) < journal.variables.size ())
            output = journal.variables[selection].second ();
    }
    imgui_input_text ("Output", output);
    if (imgui.igButton ("Copy to Clipboard", ImVec2 {}))
        imgui.igSetClipboardText (output.c_str ());

    imgui.igPopFont ();
    imgui.igEnd ();
}

//--------------------------------------------------------------------------------------------------

void draw_chapters ()
{
    imgui.igBegin ("SSE Journal: Chapters", nullptr, 0);
    imgui.igEnd ();
}

//--------------------------------------------------------------------------------------------------

