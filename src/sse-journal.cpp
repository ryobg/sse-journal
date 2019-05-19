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
#include <utils/winutils.hpp>

#include <fstream>
#include <map>
#include <vector>
#include <memory>
#include <string>
#include <cstring>
#include <functional>

#include <d3d11.h>
#include <DDSTextureLoader/DDSTextureLoader.h>

// Warning come in a BSON parser, which is not used, and probably shouldn't be
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat="
#  pragma GCC diagnostic ignored "-Wformat-extra-args"
#  include <nlohmann/json.hpp>
#  pragma GCC diagnostic pop
#endif

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
extern void journal_version (int* maj, int* min, int* patch, const char** timestamp);

/// Defined in skse.cpp
extern imgui_api imgui;

/// Defined in skse.cpp
extern std::unique_ptr<ssegui_api> ssegui;

auto constexpr lite_tint = IM_COL32 (191, 157, 111,  64);
auto constexpr dark_tint = IM_COL32 (191, 157, 111,  96);
auto constexpr frame_col = IM_COL32 (192, 157, 111, 192);

static const char* plugin_directory  = "Data\\SKSE\\Plugins\\sse-journal";
static const wchar_t* background_dds =L"Data\\SKSE\\Plugins\\sse-journal\\book.dds";
static const char* settings_location = "Data\\SKSE\\Plugins\\sse-journal\\settings.json";
static const char* books_directory   = "Data\\SKSE\\Plugins\\sse-journal\\books\\";
static const char* default_book      = "Data\\SKSE\\Plugins\\sse-journal\\books\\default_book.json";

/// Defined in skse.cpp
extern std::string logfile_path;

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

struct page_t
{
    std::string title, content;
};

//--------------------------------------------------------------------------------------------------

/// State of the current Journal run
struct {
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
}
journal = {};

//--------------------------------------------------------------------------------------------------

static bool
save_text (std::string const& destination)
{
    int maj, min, patch;
    const char* timestamp;
    journal_version (&maj, &min, &patch, &timestamp);

    extern std::string local_time (const char* format);
    try
    {
        std::ofstream of (destination);
        if (!of.is_open ())
        {
            log () << "Unable to open " << destination << " for writting." << std::endl;
            return false;
        }

        of << "SSE-Journal "<< maj<<'.'<< min <<'.'<< patch <<" ("<< timestamp << ")\n"
           << journal.pages.size () << " pages exported on " << local_time ("%c") << '\n'
           << std::endl;

        int i = 0;
        for (auto const& p: journal.pages)
        {
            of << "Page #" << std::to_string (i++) << '\n'
               << p.title.c_str () << '\n'
               << p.content.c_str () << '\n'
               << std::endl;
        }
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to save book: " << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

static bool
save_book (std::string const& destination)
{
    int maj, min, patch;
    const char* timestamp;
    journal_version (&maj, &min, &patch, &timestamp);

    try
    {
        nlohmann::json json = {
            { "version", {
                { "major", maj },
                { "minor", min },
                { "patch", patch },
                { "timestamp", timestamp }
            }},
            { "size", journal.pages.size () },
            { "current", journal.current_page },
            { "pages", nlohmann::json::object () }
        };

        int i = 0;
        for (auto const& p: journal.pages)
            json["pages"][std::to_string (i++)] = {
                { "title", p.title.c_str () },
                { "content", p.content.c_str () }
            };

        std::ofstream of (destination);
        if (!of.is_open ())
        {
            log () << "Unable to open " << destination << " for writting." << std::endl;
            return false;
        }

        of << json.dump (4);
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to save book: " << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

static bool
load_book (std::string const& source)
{
    int maj;
    journal_version (&maj, nullptr, nullptr, nullptr);

    try
    {
        std::ifstream fi (source);
        if (!fi.is_open ())
        {
            log () << "Unable to open " << source << " for reading." << std::endl;
            return false;
        }

        nlohmann::json json;
        fi >> json;

        if (json["version"]["major"].get<int> () != maj)
        {
            log () << "Incompatible book version." << std::endl;
            return false;
        }

        auto current = json["current"].get<unsigned> ();

        std::map<int, page_t> pages; // a map for page sorting and gaps fixing
        for (auto const& kv: json["pages"].items ())
        {
            page_t p;
            int ndx = std::stoull (kv.key ());
            auto& v = kv.value ();
            p.title = v["title"].get<std::string> ();
            p.content = v["content"].get<std::string> ();
            pages.emplace (ndx, std::move (p));
        }

        journal.pages.clear ();
        journal.pages.reserve (pages.size ());
        for (auto const& kv: pages)
            journal.pages.emplace_back (page_t {
                    std::move (kv.second.title), std::move (kv.second.content) });

        while (journal.pages.size () < 3)
        {
            log () << "Less than two pages. Inserting empty one." << std::endl;
            journal.pages.emplace_back (page_t { "", "" });
        }

        if (current >= journal.pages.size ())
        {
            log () << "Current page seems off. Setting it to the first one." << std::endl;
            current = 0;
        }
        journal.current_page = current;
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to load book: " << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

static bool
load_takenotes (std::string const& source)
{
    return false;
}

//--------------------------------------------------------------------------------------------------

static bool
save_settings ()
{
    int maj, min, patch;
    const char* timestamp;
    journal_version (&maj, &min, &patch, &timestamp);

    try
    {
        nlohmann::json json = {
            { "version", {
                { "major", maj },
                { "minor", min },
                { "patch", patch },
                { "timestamp", timestamp }
            }},
            { "text font", {
                { "scale", journal.text_font->Scale },
                { "color", hex_string (journal.text_color) }
            }},
            { "chapter font", {
                { "scale", journal.chapter_font->Scale },
                { "color", hex_string (journal.chapter_color) }
            }},
            { "button font", {
                { "scale", journal.button_font->Scale },
                { "color", hex_string (journal.button_color) }
            }},
            { "system font", {
                { "scale", journal.system_font->Scale }
            }}
        };

        std::ofstream of (settings_location);
        if (!of.is_open ())
        {
            log () << "Unable to open " << settings_location << " for writting." << std::endl;
            return false;
        }

        of << json.dump (4);
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to save settings file: " << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

static bool
load_settings ()
{
    int maj;
    journal_version (&maj, nullptr, nullptr, nullptr);

    try
    {
        std::ifstream fi (settings_location);
        if (!fi.is_open ())
        {
            log () << "Unable to open " << settings_location << " for reading." << std::endl;
            return false;
        }

        nlohmann::json json;
        fi >> json;

        if (json["version"]["major"].get<int> () != maj)
        {
            log () << "Incompatible settings file." << std::endl;
            return false;
        }

        journal.text_font->Scale = json["text font"]["scale"].get<float> ();
        journal.chapter_font->Scale = json["chapter font"]["scale"].get<float> ();
        journal.button_font->Scale = json["button font"]["scale"].get<float> ();
        journal.system_font->Scale = json["system font"]["scale"].get<float> ();

        std::string color;
        color = json["text font"]["color"].get<std::string> ();
        journal.text_color = std::stoull (color, nullptr, 0);
        color = json["chapter font"]["color"].get<std::string> ();
        journal.chapter_color = std::stoull (color, nullptr, 0);
        color = json["button font"]["color"].get<std::string> ();
        journal.button_color = std::stoull (color, nullptr, 0);
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to save settings file: " << ex.what () << std::endl;
        return false;
    }
    return true;
}

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
    if (FAILED (DirectX::CreateDDSTextureFromFile (
                    journal.device, journal.context, background_dds, nullptr, &journal.background)))
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
    journal.text_color = IM_COL32 (21, 17, 12, 255);

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

    extern std::vector<std::pair<std::string, std::function<std::string ()>>> make_variables ();
    journal.variables = make_variables ();

    load_settings (); // File may not exist yet

    // Fun experiment: ~half a second to load/save 1000 pages with 40k symbols each.
    // This is like ~40MB file, or something like 40 fat books of 500 pages each one. Should be
    // bearable in practice for lower spec machines. The ImGui is well responsive btw.

    load_book (default_book); // This one also may not exist
    if (journal.pages.size () < 3)
        journal.pages.resize (2);
    if (journal.current_page >= journal.pages.size ())
        journal.current_page = 0;

    return true;
}

//--------------------------------------------------------------------------------------------------

/// Resizing one by one causes FPS stutters and CDTs, hence minimal SSO size + power of 2

static inline std::size_t
next_pow2 (std::size_t n)
{
    std::size_t p = 16;
    while (p < n) p <<= 1;
    return p;
};

static void
append_input (std::string& text, std::string const& suffix)
{
    auto sz = std::strlen (text.c_str ());
    if (sz + suffix.size () > text.size ())
        text.resize (next_pow2 (sz + suffix.size () + text.size ()));
    text.insert (sz, suffix);
}

static int
imgui_text_resize (ImGuiInputTextCallbackData* data)
{
    if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
    {
        auto str = reinterpret_cast<std::string*> (data->UserData);
        str->resize (next_pow2 (data->BufSize) - 1); // likely to avoid the internal pow2 of resize
        data->Buf = const_cast<char*> (str->c_str ());
    }
    return 0;
}

/// Shared
bool
imgui_input_text (const char* label, std::string& text)
{
    return imgui.igInputText (
            label, const_cast<char*> (text.c_str ()), text.size (),
            ImGuiInputTextFlags_CallbackResize, imgui_text_resize, &text);
}

/// Shared
bool
imgui_input_multiline (const char* label, std::string& text, ImVec2 const& size)
{
    return imgui.igInputTextMultiline (
            label, const_cast<char*> (text.c_str ()), text.size (),
            size, ImGuiInputTextFlags_CallbackResize, imgui_text_resize, &text);
}

//--------------------------------------------------------------------------------------------------

static void
popup_error (bool begin, const char* name)
{
    if (begin && !imgui.igIsPopupOpen (name))
        imgui.igOpenPopup (name);
    if (imgui.igBeginPopupModal (name, nullptr, 0))
    {
        imgui.igText ("An error has occured, see %s", logfile_path.c_str ());
        if (imgui.igButton ("Close", ImVec2 {} ))
            imgui.igCloseCurrentPopup ();
        imgui.igSetItemDefaultFocus ();
        imgui.igEndPopup ();
    }
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
    if (journal.button_saveas.draw ())
        journal.show_saveas = !journal.show_saveas;
    if (journal.button_load.draw ())
        journal.show_load = !journal.show_load;

    bool action_ok = true;
    if (journal.button_save.draw ())
        action_ok = save_book (default_book);
    popup_error (!action_ok, "Saving book failed");

    bool prev = journal.button_prev.draw ();
    bool next = journal.button_next.draw ();

    imgui.igPushFont (journal.chapter_font);
    imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.chapter_color);

    imgui.igSetNextItemWidth (text_width);
    imgui.igSetCursorPos (ImVec2 { left_page, title_top });
    imgui_input_text ("##Left title", journal.pages[journal.current_page].title);
    if (imgui.igIsItemHovered (0) && !imgui.igIsItemActive ())
        imgui.ImDrawList_AddRect (imgui.igGetWindowDrawList (),
                ImVec2 { wpos.x+left_page, wpos.y+title_top },
                ImVec2 { wpos.x+left_page+text_width, wpos.y+title_top+imgui.igGetFrameHeight () },
                frame_col, 0, ImDrawCornerFlags_All, 2.f);

    imgui.igSetCursorPos (ImVec2 { right_page, title_top });
    imgui.igSetNextItemWidth (text_width);
    imgui_input_text ("##Right title", journal.pages[journal.current_page+1].title);
    if (imgui.igIsItemHovered (0) && !imgui.igIsItemActive ())
        imgui.ImDrawList_AddRect (imgui.igGetWindowDrawList (),
                ImVec2 { wpos.x+right_page, wpos.y+title_top },
                ImVec2 { wpos.x+right_page+text_width, wpos.y+title_top+imgui.igGetFrameHeight () },
                frame_col, 0, ImDrawCornerFlags_All, 2.f);

    imgui.igPopFont ();
    imgui.igPopStyleColor (1);
    imgui.igPushFont (journal.text_font);
    imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.text_color);
    // Awkward, but there is no sane way to disable it
    imgui.igPushStyleColorU32 (ImGuiCol_ScrollbarBg, IM_COL32_BLACK_TRANS);
    imgui.igPushStyleColorU32 (ImGuiCol_ScrollbarGrab, IM_COL32_BLACK_TRANS);
    imgui.igPushStyleColorU32 (ImGuiCol_ScrollbarGrabHovered, IM_COL32_BLACK_TRANS);
    imgui.igPushStyleColorU32 (ImGuiCol_ScrollbarGrabActive, IM_COL32_BLACK_TRANS);

    imgui.igSetCursorPos (ImVec2 { left_page, text_top });
    imgui_input_multiline ("##Left text",
            journal.pages[journal.current_page].content, ImVec2 { text_width, text_height });
    if (imgui.igIsItemHovered (0) && !imgui.igIsItemActive ())
        imgui.ImDrawList_AddRect (imgui.igGetWindowDrawList (),
                ImVec2 { wpos.x+left_page, wpos.y+text_top },
                ImVec2 { wpos.x+left_page+text_width, wpos.y+text_top+text_height },
                frame_col, 0, ImDrawCornerFlags_All, 2.f);

    imgui.igSetCursorPos (ImVec2 { right_page, text_top });
    imgui_input_multiline ("##Right text",
            journal.pages[journal.current_page+1].content, ImVec2 { text_width, text_height });
    if (imgui.igIsItemHovered (0) && !imgui.igIsItemActive ())
        imgui.ImDrawList_AddRect (imgui.igGetWindowDrawList (),
                ImVec2 { wpos.x+right_page, wpos.y+text_top },
                ImVec2 { wpos.x+right_page+text_width, wpos.y+text_top+text_height },
                frame_col, 0, ImDrawCornerFlags_All, 2.f);

    imgui.igPopFont ();
    imgui.igPopStyleColor (5);
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
    extern void draw_saveas ();
    if (journal.show_saveas)
        draw_saveas ();
    extern void draw_load ();
    if (journal.show_load)
        draw_load ();
}

//--------------------------------------------------------------------------------------------------

void
draw_settings ()
{
    imgui.igPushFont (journal.system_font);
    if (imgui.igBegin ("SSE Journal: Settings", &journal.show_settings, 0))
    {
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

        imgui.igDummy (ImVec2 { 1, imgui.igGetFrameHeight () });

        bool save_ok = true;
        if (imgui.igButton ("Save settings", ImVec2 {}))
            save_ok = save_settings ();
        popup_error (!save_ok, "Saving settings failed");

        imgui.igSameLine (0, -1);

        bool load_ok = true;
        if (imgui.igButton ("Load settings", ImVec2 {}))
            load_ok = load_settings ();
        popup_error (!load_ok, "Loading settings failed");
    }
    imgui.igEnd ();
    imgui.igPopFont ();
}

//--------------------------------------------------------------------------------------------------

bool
extract_variable_text (void* data, int idx, const char** out_text)
{
    auto vars = reinterpret_cast<decltype (journal.variables)*> (data);
    *out_text = vars->at (idx).first.c_str ();
    return true;
}

void
draw_variables ()
{
    imgui.igPushFont (journal.system_font);
    if (imgui.igBegin ("SSE Journal: Variables", &journal.show_variables, 0))
    {
        static int selection = -1;
        static std::string output;

        if (imgui.igListBoxFnPtr ("Variables", &selection, extract_variable_text,
                &journal.variables, static_cast<int> (journal.variables.size ()), -1))
        {
            if (unsigned (selection) < journal.variables.size ())
                output = journal.variables[selection].second ();
        }
        imgui_input_text ("Output", output);
        if (imgui.igButton ("Append left", ImVec2 {}))
            append_input (journal.pages[journal.current_page].content, output);
        imgui.igSameLine (0, -1);
        if (imgui.igButton ("Append right", ImVec2 {}))
            append_input (journal.pages[journal.current_page+1].content, output);
        if (imgui.igButton ("Copy to Clipboard", ImVec2 {}))
            imgui.igSetClipboardText (output.c_str ());
    }
    imgui.igEnd ();
    imgui.igPopFont ();
}

//--------------------------------------------------------------------------------------------------

void
draw_chapters ()
{
    imgui.igPushFont (journal.system_font);
    if (imgui.igBegin ("SSE Journal: Chapters", &journal.show_chapters, 0))
    {
    }
    imgui.igEnd ();
    imgui.igPopFont ();
}

//--------------------------------------------------------------------------------------------------

void
draw_saveas ()
{
    static std::string name;
    static int typesel = 0;
    static std::array<const char*, 2> types = { "Journal book (*.json)", "Plain text (*.txt)" };

    imgui.igPushFont (journal.system_font);
    if (imgui.igBegin ("SSE Journal: Save as file", &journal.show_saveas, 0))
    {
        imgui.igText ("Storage folder: %s", books_directory);
        imgui_input_text ("Name", name);
        imgui.igCombo ("Type", &typesel, types.data (), int (types.size ()), -1);
        if (imgui.igButton ("Cancel", ImVec2 {}))
            journal.show_saveas = false;
        imgui.igSameLine (0, -1);
        if (imgui.igButton ("Save", ImVec2 {}))
        {
            bool ok = true;
            auto root = std::string (books_directory) + name.c_str ();
            if (typesel == 0) ok = save_book (root + ".json");
            if (typesel == 1) ok = save_text (root + ".txt");
            popup_error (!ok, "Save As failed");
            if (ok) journal.show_saveas = false;
        }
    }
    imgui.igEnd ();
    imgui.igPopFont ();
}

//--------------------------------------------------------------------------------------------------

template<class T>
bool
enumerate_files (T wildcard, std::vector<std::string>& out)
{
    std::wstring w;
    if (!utf8_to_utf16 (wildcard, w))
        return false;
    out.clear ();
    WIN32_FIND_DATA fd;
    auto h = ::FindFirstFile (w.c_str (), &fd);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    do
    {
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        std::string s;
        if (!utf16_to_utf8 (fd.cFileName, s))
            break;
        out.emplace_back (std::move (s));
    }
    while (::FindNextFile (h, &fd));
    auto e = ::GetLastError ();
    ::FindClose (h);
    return e == ERROR_NO_MORE_FILES;
}

void
enumerate_books (const char* extension, std::vector<std::string>& out)
{
    auto wildcard = std::string (books_directory) + extension;
    enumerate_files (wildcard.c_str (), out);
    for (auto& name: out)
        name.erase (name.find_last_of ('.'));
}

//--------------------------------------------------------------------------------------------------

bool
extract_vector_string (void* data, int idx, const char** out_text)
{
    auto vars = reinterpret_cast<std::vector<std::string>*> (data);
    *out_text = vars->at (idx).c_str ();
    return true;
}

void
draw_load ()
{
    static int typesel = 0;
    static int namesel = -1;
    static std::array<const char*, 2> types = { "Journal book (*.json)", "Take Notes (*.xml)" };
    static std::array<const char*, 2> filters = { "*.json", "*.xml" };
    static std::vector<std::string> names;
    static bool reload_names = false;

    if (journal.show_load != reload_names)
    {
        reload_names = journal.show_load;
        enumerate_books (filters[typesel], names);
    }

    imgui.igPushFont (journal.system_font);
    if (imgui.igBegin ("SSE Journal: Load", &journal.show_load, 0))
    {
        imgui.igText ("Storage folder: %s", books_directory);
        imgui.igListBoxFnPtr (
                "Names", &namesel, extract_vector_string, &names, int (names.size ()), -1);
        if (imgui.igCombo ("Type", &typesel, types.data (), int (types.size ()), -1))
            enumerate_books (filters[typesel], names);
        if (imgui.igButton ("Cancel", ImVec2 {}))
            journal.show_load = false;
        imgui.igSameLine (0, -1);
        if (imgui.igButton ("Load", ImVec2 {}) && unsigned (namesel) < names.size ())
        {
            bool ok = true;
            auto target = books_directory + names[namesel];
            if (typesel == 0) ok = load_book (target + ".json");
            if (typesel == 1) ok = load_takenotes (target + ".xml");
            popup_error (!ok, "Load book failed");
            if (ok) journal.show_load = false;
        }
    }
    imgui.igEnd ();
    imgui.igPopFont ();
}

//--------------------------------------------------------------------------------------------------

