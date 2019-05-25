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
#include <utils/winutils.hpp>

#include <d3d11.h>

#include <memory>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <utility>
#include <functional>

//--------------------------------------------------------------------------------------------------

// skse.cpp

void journal_version (int* maj, int* min, int* patch, const char** timestamp);

extern std::ofstream& log ();
extern std::string logfile_path;

extern imgui_api imgui;
extern sseimgui_api sseimgui;

//--------------------------------------------------------------------------------------------------

// fileio.cpp

bool save_text (std::string const& destination);
bool save_book (std::string const& destination);
bool load_book (std::string const& source);
bool load_takenotes (std::string const& source);
bool save_settings ();
bool load_settings ();
bool save_variables ();
bool load_variables ();

extern std::string journal_directory;
extern std::string books_directory;
extern std::string default_book;
extern std::string settings_location;
extern std::string images_directory;

//--------------------------------------------------------------------------------------------------

// variables.cpp

struct variable_t
{
    bool deletable;
    int fuid;   ///< Unique identifier of functions, allows loading of custom vars
    std::string name, params, info;
    std::function<std::string (variable_t*)> apply;   ///< Avoids inheritance, dynamic mem & etc.
    inline std::string operator () () { return apply (this); }
};

/// @see https://en.cppreference.com/w/cpp/chrono/c/strftime
std::string local_time (const char* format);

std::vector<variable_t> make_variables ();

//--------------------------------------------------------------------------------------------------

// render.cpp

/// Wraps up common logic for drawing a button
class button_t
{
    ImVec2 tl, sz, align;
    const char *label, *label_end;
    std::uint32_t hover_tint;

public:
    static ImVec2 wpos, wsz;

    void init (const char* label,
            float tlx, float tly, float szx, float szy,
            std::uint32_t hover, float ax = .5f, float ay = .5f);

    bool draw ();
};

struct image_t
{
    bool background;    ///< Will be there text above it?
    std::uint32_t tint = IM_COL32_WHITE;
    /// top left & bottom right points for texture and position
    std::array<float, 4> uv = {{ 0, 0, 1, 1 }}, xy = {{ 0, 0, 1, 1 }};
    ID3D11ShaderResourceView* ref;
};

struct page_t
{
    std::string title, content;
    image_t image;
};

struct font_t
{
    std::string name;
    float scale;
    float size;
    std::uint32_t color;    ///< Only this is tuned by the UI, rest are default init only
    std::string file;
    std::string glyphs;
    std::vector<ImWchar> ranges;
    const char* default_data;
    ImFont* imfont; ///< Actual font with its settings (apart from #color)
};

extern bool obtain_image (std::string const& file, image_t& img);

//--------------------------------------------------------------------------------------------------

/// Most important stuff for the current running instance
struct journal_t
{
    std::string background_file;
    ID3D11ShaderResourceView* background;

    font_t button_font, chapter_font, text_font, default_font;

    button_t button_prev, button_next,
             button_settings, button_elements, button_chapters,
             button_save, button_saveas, button_load;
    bool show_settings, show_elements, show_chapters, show_saveas, show_load;

    std::vector<variable_t> variables;

    struct image_source_t {
        unsigned refcount;
        std::string file;
    };
    /// Kinda garbage collection, allows sharing of textures across the book
    std::map<ID3D11ShaderResourceView*, image_source_t> images;

    std::vector<page_t> pages;
    unsigned current_page;
};

extern journal_t journal;

//--------------------------------------------------------------------------------------------------

#endif

