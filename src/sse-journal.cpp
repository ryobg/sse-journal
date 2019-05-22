/**
 * @file sse-journal.cpp
 * @brief User interface management
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

#include "sse-journal.hpp"
#include <cstring>
#include <cctype>

//--------------------------------------------------------------------------------------------------

auto constexpr lite_tint = IM_COL32 (191, 157, 111,  64);
auto constexpr dark_tint = IM_COL32 (191, 157, 111,  96);
auto constexpr frame_col = IM_COL32 (192, 157, 111, 192);

journal_t journal = {};

//--------------------------------------------------------------------------------------------------

ImVec2 button_t::wpos = {};
ImVec2 button_t::wsz = {};

void button_t::init (
        const char* label,
        float tlx, float tly, float szx, float szy,
        std::uint32_t hover, float ax, float ay)
{
    this->align = ImVec2 { ax, ay };
    this->label = label;
    label_end = std::strchr (label, '#');
    tl.x = tlx, tl.y = tly, sz.x = szx, sz.y = szy;
    hover_tint = hover;
}

bool button_t::draw ()
{
    imgui.igPushFont (journal.button_font.imfont);
    imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.button_font.color);
    ImVec2 ptl { wsz.x * tl.x, wsz.y * tl.y },
           psz { wsz.x * sz.x, wsz.y * sz.y };
    imgui.igSetCursorPos (ptl);
    bool pressed = imgui.igInvisibleButton (label, psz);
    bool hovered = imgui.igIsItemHovered (0);
    if (hovered)
    {
        constexpr float vmax = .7226f; // The Background Y pixels reach ~72% of a 2k texture
        imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
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

//--------------------------------------------------------------------------------------------------

bool
setup ()
{
    load_settings (); // File may not exist yet

    if (!sseimgui->ddsfile_texture (journal.background_file.c_str (), nullptr, &journal.background))
    {
        log () << "Unable to load DDS." << std::endl;
        return false;
    }

    auto& j = journal;
    j.button_prev     .init ("Prev##B"     ,   0.f, 0, .050f,   1.f, lite_tint);
    j.button_settings .init ("Settings##B" , .070f, 0, .128f, .060f, dark_tint, .5f, .85f);
    j.button_variables.init ("Variables##B", .212f, 0, .128f, .060f, dark_tint, .5f, .85f);
    j.button_chapters .init ("Chapters##B" , .354f, 0, .128f, .060f, dark_tint, .5f, .85f);
    j.button_save     .init ("Save##B"     , .528f, 0, .128f, .060f, dark_tint, .5f, .85f);
    j.button_saveas   .init ("Save As##B"  , .670f, 0, .128f, .060f, dark_tint, .5f, .85f);
    j.button_load     .init ("Load##B"     , .812f, 0, .128f, .060f, dark_tint, .5f, .85f);
    j.button_next     .init ("Next##B"     ,  .95f, 0, .050f,   1.f, lite_tint);

    journal.variables = make_variables ();

    // Fun experiment: ~half a second to load/save 1000 pages with 40k symbols each.
    // This is like ~40MB file, or something like 40 fat books of 500 pages each one. Should be
    // bearable in practice for lower spec machines. The ImGui is well responsive btw.

    load_book (default_book); // This one also may not exist
    if (journal.pages.size () < 3)
        journal.pages.resize (2);
    if (journal.current_page+2 >= journal.pages.size ())
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
            label, const_cast<char*> (text.c_str ()), text.size () + 1,
            ImGuiInputTextFlags_CallbackResize, imgui_text_resize, &text);
}

/// Shared
bool
imgui_input_multiline (const char* label, std::string& text, ImVec2 const& size)
{
    return imgui.igInputTextMultiline (
            label, const_cast<char*> (text.c_str ()), text.size () + 1,
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

    imgui.igSetNextWindowSize (ImVec2 { 800, 600 }, ImGuiCond_FirstUseEver);

    imgui.igBegin ("SSE Journal", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground);
    imgui.igPushStyleColorU32 (ImGuiCol_FrameBg, 0);
    imgui.igPushStyleVarFloat (ImGuiStyleVar_FrameBorderSize, 0);

    auto wpos = button_t::wpos = imgui.igGetWindowPos ();
    auto wsz  = button_t::wsz  = imgui.igGetWindowSize ();

    imgui.ImDrawList_AddImage (imgui.igGetWindowDrawList (), journal.background,
            wpos, ImVec2 {wpos.x+wsz.x, wpos.y+wsz.y}, ImVec2 {0,0}, ImVec2 {1,.7226f},
            imgui.igGetColorU32Vec4 (ImVec4 {1,1,1,1}));

    // Ratio, ratio multiplied by pixel size and the absolute positions summed with these
    // are used all below. It may be pulled off as more capsulated and less dublication.
    const float text_width    = .412f * wsz.x;
    const float text_height   = .800f * wsz.y;

    const float left_page  = .070f * wsz.x;
    const float right_page = .528f * wsz.x;
    const float title_top  = .090f * wsz.y;
    const float text_top   = .159f * wsz.y;

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

    extern void previous_page ();
    if (journal.button_prev.draw ())
        previous_page ();
    extern void next_page ();
    if (journal.button_next.draw ())
        next_page ();

    imgui.igPushFont (journal.chapter_font.imfont);
    imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.chapter_font.color);

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
    imgui.igPushFont (journal.text_font.imfont);
    imgui.igPushStyleColorU32 (ImGuiCol_Text, journal.text_font.color);
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
    imgui.igPopStyleVar (1);
    imgui.igPopStyleColor (1);
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

static std::string greedy_word_wrap (std::string const& source, unsigned width);

void
draw_settings ()
{
    imgui.igPushFont (journal.default_font.imfont);
    if (imgui.igBegin ("SSE Journal: Settings", &journal.show_settings, 0))
    {
        static ImVec4 button_c  = imgui.igColorConvertU32ToFloat4 (journal.button_font.color),
                      chapter_c = imgui.igColorConvertU32ToFloat4 (journal.chapter_font.color),
                      text_c    = imgui.igColorConvertU32ToFloat4 (journal.text_font.color);
        constexpr int cflags = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_DisplayHSV
            | ImGuiColorEditFlags_InputRGB | ImGuiColorEditFlags_PickerHueBar
            | ImGuiColorEditFlags_AlphaBar;

        imgui.igText ("Buttons font:");
        if (imgui.igColorEdit4 ("Color##Buttons", (float*) &button_c, cflags))
            journal.button_font.color = imgui.igGetColorU32Vec4 (button_c);
        imgui.igSliderFloat ("Scale##Buttons", &journal.button_font.imfont->Scale,.5f,2.f,"%.2f",1);

        imgui.igText ("Titles font:");
        if (imgui.igColorEdit4 ("Color##Titles", (float*) &chapter_c, cflags))
            journal.chapter_font.color = imgui.igGetColorU32Vec4 (chapter_c);
        imgui.igSliderFloat ("Scale##Titles", &journal.chapter_font.imfont->Scale,.5f,2.f,"%.2f",1);

        imgui.igText ("Text font:");
        if (imgui.igColorEdit4 ("Color##Text", (float*) &text_c, cflags))
            journal.text_font.color = imgui.igGetColorU32Vec4 (text_c);
        imgui.igSliderFloat ("Scale##Text", &journal.text_font.imfont->Scale, .5f, 2.f, "%.2f", 1);

        imgui.igText ("Default font:");
        imgui.igSliderFloat ("Scale", &journal.default_font.imfont->Scale, .5f, 2.f, "%.2f", 1);

        static int wrap_width = 60;
        imgui.igDummy (ImVec2 { 1, imgui.igGetFrameHeight () });
        imgui.igText ("Word wrap:");
        imgui.igDragInt ("Line width", &wrap_width, 1, 40, 160, "%d");
        if (imgui.igButton ("Wrap", ImVec2 {}))
        {
            for (auto& p: journal.pages)
                p.content = greedy_word_wrap (p.content, wrap_width);
        }

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
    imgui.igPushFont (journal.default_font.imfont);
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

static bool
visible_symbols (std::string const& s)
{
    if (!s.empty ()) for (auto p = s.c_str (); *p; ++p)
        if (*p != ' ' && !std::iscntrl (*p))
            return true;
    return false;
}

//--------------------------------------------------------------------------------------------------

bool
extract_chapter_title (void* data, int idx, const char** out_text)
{
    auto const& title = journal.pages[idx].title;
    if (visible_symbols (title))
        *out_text = title.c_str ();
    else
        *out_text = "(n/a)";
    return true;
}

void
draw_chapters ()
{
    static float items = 7.25f;
    static int selection = -1;

    imgui.igPushFont (journal.default_font.imfont);
    if (imgui.igBegin ("SSE Journal: Chapters", &journal.show_chapters, 0))
    {
        if (imgui.igListBoxFnPtr ("##Chapters", &selection, extract_chapter_title, nullptr,
                int (journal.pages.size ()), items))
        {
            int ndx = selection;
            if (ndx + 1 == int (journal.pages.size ()))
                ndx--;
            journal.current_page = ndx;
        }

        imgui.igSameLine (0, -1);
        imgui.igBeginGroup ();
        bool adjust = false;

        if (imgui.igButton ("Insert before", ImVec2 {-1, 0}))
        {
            if (selection >= 0 && selection < int (journal.pages.size ()))
                adjust = true,
                journal.pages.insert (journal.pages.begin () + selection, page_t {});
        }
        if (imgui.igButton ("Insert after", ImVec2 {-1, 0}))
        {
            if (selection >= 0 && selection < int (journal.pages.size ()))
                adjust = true,
                journal.pages.insert (journal.pages.begin () + selection + 1, page_t {});
        }
        if (imgui.igButton ("Delete", ImVec2 {-1, 0}))
        {
            if (selection >= 0 && selection < int (journal.pages.size ()))
                adjust = true,
                journal.pages.erase (journal.pages.begin () + selection);
        }
        imgui.igEndGroup ();

        if (adjust)
        {
            if (journal.pages.size () < 2)
                journal.pages.resize (2);
            while (journal.current_page+2 > journal.pages.size ())
                journal.current_page--;
        }

        items = (imgui.igGetWindowHeight () / imgui.igGetTextLineHeightWithSpacing ()) - 2;
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

    imgui.igPushFont (journal.default_font.imfont);
    if (imgui.igBegin ("SSE Journal: Save as file", &journal.show_saveas, 0))
    {
        imgui.igText (books_directory.c_str ());
        imgui_input_text ("Name", name);
        imgui.igCombo ("Type", &typesel, types.data (), int (types.size ()), -1);
        if (imgui.igButton ("Cancel", ImVec2 {}))
            journal.show_saveas = false;
        imgui.igSameLine (0, -1);
        if (imgui.igButton ("Save", ImVec2 {}))
        {
            bool ok = true;
            auto root = books_directory + name.c_str ();
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
    auto wildcard = books_directory + extension;
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
    static float items = -1;

    if (journal.show_load != reload_names)
    {
        reload_names = journal.show_load;
        enumerate_books (filters[typesel], names);
    }

    imgui.igPushFont (journal.default_font.imfont);
    if (imgui.igBegin ("SSE Journal: Load", &journal.show_load, 0))
    {
        imgui.igText (books_directory.c_str ());
        imgui.igBeginGroup ();
        if (imgui.igCombo ("##Type", &typesel, types.data (), int (types.size ()), -1))
            enumerate_books (filters[typesel], names);
        imgui.igListBoxFnPtr ("##Names",
                &namesel, extract_vector_string, &names, int (names.size ()), items);
        imgui.igEndGroup ();
        imgui.igSameLine (0, -1);
        imgui.igBeginGroup ();
        if (imgui.igButton ("Load", ImVec2 {-1, 0}) && unsigned (namesel) < names.size ())
        {
            bool ok = true;
            auto target = books_directory + names[namesel];
            if (typesel == 0) ok = load_book (target + ".json");
            if (typesel == 1) ok = load_takenotes (target + ".xml");
            popup_error (!ok, "Load book failed");
            if (ok) journal.show_load = false;
        }
        if (imgui.igButton ("Cancel", ImVec2 {-1, 0}))
            journal.show_load = false;
        imgui.igEndGroup ();
        items = (imgui.igGetWindowHeight () / imgui.igGetTextLineHeightWithSpacing ()) - 4;
    }
    imgui.igEnd ();
    imgui.igPopFont ();
}

//--------------------------------------------------------------------------------------------------

void
previous_page ()
{
    if (journal.current_page > 0)
        journal.current_page--;
}

//--------------------------------------------------------------------------------------------------

void
next_page ()
{
    if (journal.current_page+2 < journal.pages.size ())
        journal.current_page++;
    // New page if not whitespaces only. It is a bit heurestic and must be careful with regard to
    // the UTF-8 symbols, hence bare safe assumptions were made in the ASCII range. It helps avoid
    // including some sophisticated library for handling unicode. While the Viner font provides
    // very few symbols, merging within an icon font or other utf-8 rich one, will cause issues if
    // not cautious.
    else if (journal.current_page + 2 == journal.pages.size ())
    {
        if (visible_symbols (journal.pages.back ().title)
                || visible_symbols (journal.pages.back ().content))
        {
            journal.pages.push_back (page_t {});
            journal.current_page++;
        }
    }
}

//--------------------------------------------------------------------------------------------------

static std::string
greedy_word_wrap (std::string const& source, unsigned width)
{
    auto n = std::strlen (source.c_str ());
    std::string out (n, 0);

    for (unsigned i = 0; i < n; )
    {
        // copy string until the end of the line is reached
        for (unsigned c = 1; c <= width; ++c, ++i)
        {
            if (i == n)
                return out;
            out[i] = source[i];
            if (source[i] == '\n')
                c = 1;
        }

        if (std::isspace (source[i]))
            out[i++] = '\n';
        // check for nearest whitespace back in string
        else for (unsigned k = i; k > 0; --k)
            if (std::isspace (source[k]))
            {
                out[k] = '\n';
                i = k + 1;
                break;
            }
    }
    return out;
}

//--------------------------------------------------------------------------------------------------

