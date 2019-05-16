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

//--------------------------------------------------------------------------------------------------

/// State of the current Journal run
struct {
    bool show_options;
    bool show_chapters;
    int selected_chapter;
    std::vector<std::string> chapters;

    ID3D11Device*           device;
    ID3D11DeviceContext*    context;
    IDXGISwapChain*         chain;
    HWND                    window;

    ID3D11ShaderResourceView* background;
    std::string left_title, left_text, right_title, right_text;

    ImFont *button_font, *chapter_font, *text_font;
    std::uint32_t button_color, chapter_color, text_color;
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

    extern ImFont* viner_font (float, const ImFontConfig*, const ImWchar*);
    journal.text_font    = viner_font (48.f, nullptr, nullptr);//to be merged later ie !=button_font
    journal.chapter_font = viner_font (64.f, nullptr, nullptr);
    journal.button_font  = viner_font (48.f, nullptr, nullptr);
    if (!journal.text_font || !journal.button_font || !journal.chapter_font)
    {
        log () << "Unable to load font Viner Hand" << std::endl;
        return false;
    }

    journal.button_color = IM_COL32_WHITE;
    journal.chapter_color = IM_COL32_BLACK;
    journal.text_color = IM_COL32_BLACK;
    return true;
}

//--------------------------------------------------------------------------------------------------

static int
imgui_text_resize (ImGuiInputTextCallbackData* data)
{
    auto better_size = [] (std::size_t n) // next power of 2 basically
    {
        std::size_t p = 16;
        while (p < n) p <<= 1;
        return p;
    };
    // generally data->BufSize == data->BufTextLen + 1
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

class button
{
public:
	void draw ()
	{
    	imgui.igSetCursorPos (ImVec2 { 0, 0 });
    	imgui.igSetCursorPos (ImVec2 { left_page + menu_width + menu_gap, menu_top });

    bool prev_pressed = imgui.igInvisibleButton ("Prev##Button", ImVec2 { page_width, wsz.y });
    if (imgui.igIsItemHovered (0))
        imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
                wpos, ImVec2 {wpos.x+page_width, wpos.y+wsz.y}, ImVec2 {0,0},
                ImVec2 {.050f, .7226f}, page_hover_tint);
	float txtsz = imgui.igCalcTextSize ("Prev", nullptr, false, -1.f);
    imgui.igSetCursorPos (ImVec2 { 0+.5f*(page_width-txtsz.x), 0+.5f(wsz.y-txtsz.y) });
	imgui.igTextUnformatted ("Prev", nullptr);
};

//--------------------------------------------------------------------------------------------------

void SSEIMGUI_CCONV
render (int active)
{
    if (!active)
        return;

    imgui.igBegin ("SSE Journal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);
    int push_col = 0;//IM_COL32 (191,157,111,64)
	imgui.igPushStyleColorU32 (ImGuiCol_ButtonActive, 0); ++push_col;
	imgui.igPushStyleColorU32 (ImGuiCol_ButtonHovered, 0); ++push_col;
	imgui.igPushStyleColorU32 (ImGuiCol_Button, 0); ++push_col;
	imgui.igPushStyleColorU32 (ImGuiCol_FrameBg, 0); ++push_col;
	imgui.igPushStyleVarFloat (ImGuiStyleVar_FrameBorderSize, 0);

    auto wpos = imgui.igGetWindowPos ();
    auto wsz = imgui.igGetWindowSize ();

    imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
            wpos, ImVec2 {wpos.x+wsz.x, wpos.y+wsz.y}, ImVec2 {0,0}, ImVec2 {1,.7226f},
            imgui.igGetColorU32Vec4 (ImVec4 {1,1,1,1}));

	// Ratio, ratio multiplied by pixel size and the absolute positions summed with these
	// are used all below. It may be pulled off as more capsulated and less dublication.
    const float page_width    = .050f * wsz.x;
    const float menu_width    = .128f * wsz.x;
    const float menu_height   = .044f * wsz.y;
    const float text_width    = .412f * wsz.x;
    const float text_height   = .784f * wsz.y;

    const float left_page  = .070f * wsz.x;
    const float right_page = .518f * wsz.x;
    const float menu_top   = .038f * wsz.y;
    const float title_top  = .103f * wsz.y;
    const float text_top   = .159f * wsz.y;
    const float status_top = .950f * wsz.y;

    const float menu_gap = (text_width - menu_width*3) * .5f;
    auto constexpr page_hover_tint = IM_COL32 (191, 157, 111, 64);

    // Port/larboard/ladebord

    imgui.igPushFont (journal.button_font);
	imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.button_color);

    imgui.igSetCursorPos (ImVec2 { 0, 0 });
    bool prev_pressed = imgui.igInvisibleButton ("Prev##Button", ImVec2 { page_width, wsz.y });
    if (imgui.igIsItemHovered (0))
        imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
                wpos, ImVec2 {wpos.x+page_width, wpos.y+wsz.y}, ImVec2 {0,0},
                ImVec2 {.050f, .7226f}, page_hover_tint);
	float txtsz = imgui.igCalcTextSize ("Prev", nullptr, false, -1.f);
    imgui.igSetCursorPos (ImVec2 { 0+.5f*(page_width-txtsz.x), 0+.5f(wsz.y-txtsz.y) });
	imgui.igTextUnformatted ("Prev", nullptr);

    imgui.igSetCursorPos (ImVec2 { left_page, menu_top });
    bool options_pressed = imgui.igButton ("Options", ImVec2 { menu_width, menu_height });
    if (imgui.igIsItemHovered (0))
        imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
                ImVec2 { wpos.x+left_page, wpos.y },
                ImVec2 { wpos.x+left_page+menu_width, wpos.y+menu_top+menu_height },
                ImVec2 { .070f, 0 }, ImVec2 {.070f+.128f, .038f+.044f }, page_hover_tint);

    imgui.igSetCursorPos (ImVec2 { left_page + menu_width + menu_gap, menu_top });
    bool variables_pressed = imgui.igButton ("Variables", ImVec2 { menu_width, menu_height });
    if (imgui.igIsItemHovered (0))
        imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
                ImVec2 { wpos.x+left_page+menu_gap+menu_width, wpos.y },
                ImVec2 { wpos.x+left_page+menu_gap+menu_width*2, wpos.y+menu_top+menu_height },
                ImVec2 { .070f+.128f+.014f, 0 }, ImVec2 {.070f+2*.128f+.014f, .038f+.044f },
                page_hover_tint);

    imgui.igSetCursorPos (ImVec2 { left_page + 2*(menu_width + menu_gap), menu_top });
    bool chapters_pressed = imgui.igButton ("Chapters", ImVec2 { menu_width, menu_height });
    if (imgui.igIsItemHovered (0))
        imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
                ImVec2 { wpos.x+left_page+2*(menu_gap+menu_width), wpos.y },
                ImVec2 { wpos.x+left_page+2*menu_gap+3*menu_width, wpos.y+menu_top+menu_height },
                ImVec2 { .070f+2*(.128f+.014f), 0 }, ImVec2 {.070f+3*.128f+2*.014f, .038f+.044f },
                page_hover_tint);

    imgui.igPopFont ();
	imgui.igPopStyleColor (1);

    imgui.igPushFont (journal.chapter_font);
	imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.chapter_color);
    imgui.igSetNextItemWidth (text_width);
    imgui.igSetCursorPos (ImVec2 { left_page, title_top });
    imgui_input_text ("##Left title", journal.left_title);
    if (imgui.igIsItemHovered (0) && !imgui.igIsItemActive ())
        imgui.ImDrawList_AddRectFilled (imgui.igGetWindowDrawList (),
                ImVec2 { wpos.x+left_page, wpos.y+title_top },
                ImVec2 { wpos.x+left_page+text_width, wpos.y+title_top+imgui.igGetFrameHeight () },
                page_hover_tint, 0, ImDrawCornerFlags_All);
    imgui.igPopFont ();
	imgui.igPopStyleColor (1);

    imgui.igPushFont (journal.text_font);
	imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.text_color);
    imgui.igSetCursorPos (ImVec2 { left_page, text_top });
    imgui_input_multiline ("##Left text", journal.left_text, ImVec2 { text_width, text_height });
    if (imgui.igIsItemHovered (0) && !imgui.igIsItemActive ())
        imgui.ImDrawList_AddRectFilled (imgui.igGetWindowDrawList (),
                ImVec2 { wpos.x+left_page, wpos.y+text_top },
                ImVec2 { wpos.x+left_page+text_width, wpos.y+text_top+text_height },
                page_hover_tint, 0, ImDrawCornerFlags_All);
    imgui.igPopFont ();
	imgui.igPopStyleColor (1);

    imgui.igPushFont (journal.text_font);
	imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.button_color);
    imgui.igSetCursorPos (ImVec2 { left_page, status_top });
    imgui.igText ("Port status bar goes here");
    imgui.igPopFont ();
	imgui.igPopStyleColor (1);

    // Starboard/steobord

    imgui.igPushFont (journal.button_font);
	imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.button_color);

    imgui.igSetCursorPos (ImVec2 { wsz.x-page_width, 0 });
    bool next_pressed = imgui.igButton ("Next", ImVec2 { page_width, wsz.y });
    if (imgui.igIsItemHovered (0))
        imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
                ImVec2 { wpos.x+wsz.x-page_width, wpos.y }, ImVec2 { wpos.x+wsz.x, wpos.y+wsz.y },
                ImVec2 { .950f, 0 }, ImVec2 { 1, .7226f }, page_hover_tint);

    imgui.igSetCursorPos (ImVec2 { right_page, menu_top });
    bool overwrite_pressed = imgui.igButton ("Overwrite", ImVec2 { menu_width, menu_height });
    if (imgui.igIsItemHovered (0))
        imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
                ImVec2 { wpos.x+right_page, wpos.y },
                ImVec2 { wpos.x+right_page+menu_width, wpos.y+menu_top+menu_height },
                ImVec2 { .518f, 0 }, ImVec2 {.518f+.128f, .038f+.044f }, page_hover_tint);

    imgui.igSetCursorPos (ImVec2 { right_page + menu_width + menu_gap, menu_top });
    bool saveas_pressed = imgui.igButton ("Save as", ImVec2 { menu_width, menu_height });
    if (imgui.igIsItemHovered (0))
        imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
                ImVec2 { wpos.x+right_page+menu_gap+menu_width, wpos.y },
                ImVec2 { wpos.x+right_page+menu_gap+menu_width*2, wpos.y+menu_top+menu_height },
                ImVec2 { .518f+.128f+.014f, 0 }, ImVec2 {.518f+2*.128f+.014f, .038f+.044f },
                page_hover_tint);

    imgui.igSetCursorPos (ImVec2 { right_page + 2*(menu_width + menu_gap), menu_top });
    bool load_pressed = imgui.igButton ("Load", ImVec2 { menu_width, menu_height });
    if (imgui.igIsItemHovered (0))
        imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
                ImVec2 { wpos.x+right_page+2*(menu_gap+menu_width), wpos.y },
                ImVec2 { wpos.x+right_page+2*menu_gap+3*menu_width, wpos.y+menu_top+menu_height },
                ImVec2 { .518f+2*(.128f+.014f), 0 }, ImVec2 {.518f+3*.128f+2*.014f, .038f+.044f },
                page_hover_tint);

    imgui.igPopFont ();
	imgui.igPopStyleColor (1);

    imgui.igPushFont (journal.chapter_font);
	imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.chapter_color);
    imgui.igSetCursorPos (ImVec2 { right_page, title_top });
    imgui.igSetNextItemWidth (text_width);
    imgui_input_text ("##Right title", journal.right_title);
    if (imgui.igIsItemHovered (0) && !imgui.igIsItemActive ())
        imgui.ImDrawList_AddRectFilled (imgui.igGetWindowDrawList (),
                ImVec2 { wpos.x+right_page, wpos.y+title_top },
                ImVec2 { wpos.x+right_page+text_width, wpos.y+title_top+imgui.igGetFrameHeight () },
                page_hover_tint, 0, ImDrawCornerFlags_All);
    imgui.igPopFont ();
	imgui.igPopStyleColor (1);

    imgui.igPushFont (journal.text_font);
	imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.text_color);
    imgui.igSetCursorPos (ImVec2 { right_page, text_top });
    imgui_input_multiline ("##Right text", journal.right_text, ImVec2 { text_width, text_height });
    if (imgui.igIsItemHovered (0) && !imgui.igIsItemActive ())
        imgui.ImDrawList_AddRectFilled (imgui.igGetWindowDrawList (),
                ImVec2 { wpos.x+right_page, wpos.y+text_top },
                ImVec2 { wpos.x+right_page+text_width, wpos.y+text_top+text_height },
                page_hover_tint, 0, ImDrawCornerFlags_All);
    imgui.igPopFont ();
	imgui.igPopStyleColor (1);

    imgui.igPushFont (journal.text_font);
	imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.button_color);
    imgui.igSetCursorPos (ImVec2 { right_page, status_top });
    imgui.igText ("Starboard status bar goes here");
    imgui.igPopFont ();
	imgui.igPopStyleColor (1);

	imgui.igPopStyleVar (1);
	imgui.igPopStyleColor (push_col);
    imgui.igEnd ();
}

//--------------------------------------------------------------------------------------------------

