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

    // This is good candidate to offload to SSE ImGui
    if (FAILED (DirectX::CreateDDSTextureFromFile (journal.device, journal.context,
                    L"Data\\interface\\sse-journal\\book.dds", nullptr, &journal.background)))
    {
        log () << "Unable to load DDS." << std::endl;
        return false;
    }

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
    if (!active || !journal.background)
        return;

    imgui.igBegin ("SSE Journal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);

    auto wpos = imgui.igGetWindowPos ();
    auto wbr = imgui.igGetWindowSize ();
    wbr.x += wpos.x; wbr.y += wpos.y;
    imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
            wpos, wbr, ImVec2 {0,0}, ImVec2 {1,.7226f},
            imgui.igGetColorU32Vec4 (ImVec4 {1.f,1.f,1.f,1.f}));

    imgui.igCheckbox ("Options", &journal.show_options);
    imgui.igCheckbox ("Chapters", &journal.show_chapters);
    imgui.igEnd ();

    if (journal.show_options)
    {
        imgui.igBegin ("SSE Journal: Options", &journal.show_options, 0);
        imgui.igEnd ();
    }

    if (journal.show_chapters)
    {
        imgui.igBegin ("SSE Journal: Chapters", &journal.show_chapters, 0);
        imgui.igListBoxFnPtr ("Table of Content", &journal.selected_chapter, print_chapter,
                journal.chapters.data (), static_cast<int> (journal.chapters.size ()), -1);
        imgui.igEnd ();
    }
}

//--------------------------------------------------------------------------------------------------

