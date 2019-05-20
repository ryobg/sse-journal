/**
 * @file sse-journal.hpp
 * @brief Shared interface between files in SSE-Journal
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

#ifndef SSEJOURNAL_HPP
#define SSEJOURNAL_HPP

#include <sse-imgui/sse-imgui.h>
#include <sse-gui/sse-gui.h>
#include <utils/winutils.hpp>

#include <d3d11.h>

#include <memory>
#include <fstream>
#include <string>
#include <vector>
#include <utility>
#include <functional>

//--------------------------------------------------------------------------------------------------

// skse.cpp

void journal_version (int* maj, int* min, int* patch, const char** timestamp);

extern std::ofstream& log ();
extern std::string logfile_path;

extern imgui_api imgui;
extern std::unique_ptr<ssegui_api> ssegui;

//--------------------------------------------------------------------------------------------------

// fileio.cpp

bool save_text (std::string const& destination);
bool save_book (std::string const& destination);
bool load_book (std::string const& source);
bool save_settings (std::string const& destination);
bool load_settings (std::string const& source);
bool load_takenotes (std::string const& source);

//--------------------------------------------------------------------------------------------------

// variables.cpp

std::string local_time (const char* format);
std::vector<std::pair<std::string, std::function<std::string ()>>> make_variables ();

//--------------------------------------------------------------------------------------------------

// sse-journal.cpp

/// Wraps up common logic for drawing a button
class button_t
{
    ImVec2 tl, sz, align;
    const char *label, *label_end;
    std::uint32_t hover_tint;

public:
    static ImFont* font;
    static std::uint32_t* color;
    static ID3D11ShaderResourceView* background;
    static ImVec2 wpos, wsz;

    void init (const char* label,
            float tlx, float tly, float szx, float szy,
            std::uint32_t hover, float ax = .5f, float ay = .5f);

    bool draw ();
};

//--------------------------------------------------------------------------------------------------

struct page_t
{
    std::string title, content;
};

//--------------------------------------------------------------------------------------------------

/// Most important stuff for the current running instance
struct journal_t
{
    ID3D11Device*           device;
    ID3D11DeviceContext*    context;
    IDXGISwapChain*         chain;
    HWND                    window;

    ID3D11ShaderResourceView* background;

    ImFont *button_font, *chapter_font, *text_font, *system_font;
    std::uint32_t button_color, chapter_color, text_color;

    button_t button_prev, button_next,
             button_settings, button_variables, button_chapters,
             button_save, button_saveas, button_load;
    bool show_settings, show_variables, show_chapters, show_saveas, show_load;

    std::vector<std::pair<std::string, std::function<std::string ()>>> variables;

    std::vector<page_t> pages;
    unsigned current_page;
};

extern journal_t journal;

//--------------------------------------------------------------------------------------------------

#endif
