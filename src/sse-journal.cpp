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
    ImFont* font_viner;
    std::string left_title, left_text, right_title, right_text;
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

    journal.font_viner = imgui.ImFontAtlas_AddFontFromFileTTF (
                fa, "Data\\interface\\sse-journal\\VinerHandITC.ttf", 30.f, nullptr, nullptr);
    if (!journal.font_viner)
    {
        log () << "Unable to load font VinerHandITC.tff" << std::endl;
        return false;
    }

    journal.left_title.resize (256, ' ');
    journal.right_title = journal.left_title;
    journal.left_text.resize (4096, ' ');
    journal.right_text = journal.left_text;
    return true;
}

//--------------------------------------------------------------------------------------------------

static bool
print_chapter (void* data, int idx, const char** out_txt)
{
    auto vec = reinterpret_cast<std::string*> (data);
    *out_txt = vec[idx].c_str ();
    return true;
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
    imgui.igPushFont (journal.font_viner);

    auto wpos = imgui.igGetWindowPos ();
    auto wsz = imgui.igGetWindowSize ();

    imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
            wpos, ImVec2 {wsz.x+wpos.x, wsz.y+wpos.y}, ImVec2 {0,0}, ImVec2 {1,.7226f},
            imgui.igGetColorU32Vec4 (ImVec4 {1,1,1,1}));

    const float page_width    = .055f * wsz.x;
    const float menu_width    = .128f * wsz.x;
    const float menu_height   = .044f * wsz.y;
    const float title_height  = .044f * wsz.y;
    const float text_width    = .412f * wsz.x;
    const float text_height   = .784f * wsz.y;
    const float status_height = .025f * wsz.y;

    const float left_page  = .070f * wsz.x;
    const float right_page = .518f * wsz.x;
    const float menu_top   = .038f * wsz.y;
    const float title_top  = .103f * wsz.y;
    const float text_top   = .159f * wsz.y;
    const float status_top = .950f * wsz.y;

    const float menu_gap = (text_width - menu_width*3) * .5f;

    // Port/larboard/ladebord

    imgui.igSetCursorPos (ImVec2 { 0, 0 });
    bool prev_pressed = imgui.igButton ("Prev", ImVec2 { page_width, wsz.y });

    imgui.igSetCursorPos (ImVec2 { left_page, menu_top });
    bool options_pressed = imgui.igButton ("Options", ImVec2 { menu_width, menu_height });
    imgui.igSetCursorPos (ImVec2 { left_page + menu_width + menu_gap, menu_top });
    bool variables_pressed = imgui.igButton ("Variables", ImVec2 { menu_width, menu_height });
    imgui.igSetCursorPos (ImVec2 { left_page + 2*(menu_width + menu_gap), menu_top });
    bool chapters_pressed = imgui.igButton ("Chapters", ImVec2 { menu_width, menu_height });

    imgui.igSetCursorPos (ImVec2 { left_page, title_top });
    imgui.igInputTextMultiline ("##Left title",
            &journal.left_title[0], journal.left_title.capacity (),
            ImVec2 { text_width, title_height }, 0, nullptr, nullptr);
    imgui.igSetCursorPos (ImVec2 { left_page, text_top });
    imgui.igInputTextMultiline ("##Left text",
            &journal.left_text[0], journal.left_text.capacity (),
            ImVec2 { text_width, text_height }, 0, nullptr, nullptr);
    imgui.igSetCursorPos (ImVec2 { left_page, status_top });
    imgui.igText ("Port status bar goes here");

    // Starboard/steobord

    imgui.igSetCursorPos (ImVec2 { right_page, menu_top });
    bool overwrite_pressed = imgui.igButton ("Overwrite", ImVec2 { menu_width, menu_height });
    imgui.igSetCursorPos (ImVec2 { right_page + menu_width + menu_gap, menu_top });
    bool saveas_pressed = imgui.igButton ("Save as", ImVec2 { menu_width, menu_height });
    imgui.igSetCursorPos (ImVec2 { right_page + 2*(menu_width + menu_gap), menu_top });
    bool load_pressed = imgui.igButton ("Load", ImVec2 { menu_width, menu_height });

    imgui.igSetCursorPos (ImVec2 { right_page, title_top });
    imgui.igInputTextMultiline ("##Right title",
            &journal.left_title[0], journal.left_title.capacity (),
            ImVec2 { text_width, title_height }, 0, nullptr, nullptr);
    imgui.igSetCursorPos (ImVec2 { right_page, text_top });
    imgui.igInputTextMultiline ("##Right text",
            &journal.left_text[0], journal.left_text.capacity (),
            ImVec2 { text_width, text_height }, 0, nullptr, nullptr);
    imgui.igSetCursorPos (ImVec2 { right_page, status_top });
    imgui.igText ("Starboard status bar goes here");

    imgui.igSetCursorPos (ImVec2 { wsz.x-page_width, 0 });
    bool next_pressed = imgui.igButton ("Next", ImVec2 { page_width, wsz.y });

    imgui.igPopFont ();
    imgui.igEnd ();
}

//--------------------------------------------------------------------------------------------------

