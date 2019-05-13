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
#include <fstream>
#include <vector>

//--------------------------------------------------------------------------------------------------

/// Defined in skse.cpp
extern std::ofstream logfile;

/// Defined in skse.cpp
extern imgui_api imgui;

//--------------------------------------------------------------------------------------------------

/// State of the current Journal run
struct {
    bool show_options;
    bool show_chapters;
    int selected_chapter;
    std::vector<std::string> chapters;
}
journal = {};

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


    imgui.igBegin ("SSE Journal", nullptr, 0);
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

