/**
 * @file fileio.cpp
 * @brief File saving and loading and etc. I/O related operations
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

#include <rapidxml/rapidxml.hpp>
#include <gsl/gsl_util>

#include <fstream>
#include <vector>
#include <iterator>

// Warning come in a BSON parser, which is not used, and probably shouldn't be
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat="
#  pragma GCC diagnostic ignored "-Wformat-extra-args"
#  include <nlohmann/json.hpp>
#  pragma GCC diagnostic pop
#endif

//--------------------------------------------------------------------------------------------------

std::string journal_directory = "Data\\SKSE\\Plugins\\sse-journal\\";
std::string books_directory   = journal_directory + "books\\";
std::string default_book      = books_directory   + "default_book.json";
std::string settings_location = journal_directory + "settings.json";
std::string variables_location= journal_directory + "variables.json";
std::string images_directory  = journal_directory + "images\\";

//--------------------------------------------------------------------------------------------------

bool
save_text (std::string const& destination)
{
    int maj, min, patch;
    const char* timestamp;
    journal_version (&maj, &min, &patch, &timestamp);

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

bool
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
        {
            auto it = journal.images.find (p.image.ref);
            json["pages"][std::to_string (i++)] = {
                { "title", p.title.c_str () },
                { "content", p.content.c_str () },
                { "image",  {
                    { "file", it == journal.images.end () ? "" : it->second.file.c_str () },
                    { "background", p.image.background },
                    { "tint", hex_string (p.image.tint) },
                    { "uv", { p.image.uv[0], p.image.uv[1], p.image.uv[2], p.image.uv[3] }},
                    { "xy", { p.image.xy[0], p.image.xy[1], p.image.xy[2], p.image.xy[3] }}
                }}
            };
        }

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

bool
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
            page_t p = {};
            int ndx = std::stoull (kv.key ());
            auto& v = kv.value ();
            p.title = v["title"].get<std::string> ();
            p.content = v["content"].get<std::string> ();
            if (v.contains ("image"))
            {
                auto& vi = v["image"];
                auto it = vi["uv"].begin ();
                for (float& uv: p.image.uv) uv = *it++;
                it = vi["xy"].begin ();
                for (float& xy: p.image.xy) xy = *it++;
                p.image.tint = std::stoull (vi["tint"].get<std::string> (), nullptr, 0);
                p.image.background = vi["background"];
                obtain_image (vi["file"], p.image); // resets p.image on success
            }
            pages.emplace (ndx, std::move (p));
        }

        journal.pages.clear ();
        journal.pages.reserve (pages.size ());
        for (auto const& kv: pages)
            journal.pages.emplace_back (std::move (kv.second));

        while (journal.pages.size () < 2)
        {
            log () << "Less than two pages. Inserting empty one." << std::endl;
            journal.pages.emplace_back (page_t {});
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

static void
save_font (nlohmann::json& json, font_t const& font)
{
    auto& jf = json[font.name + " font"];
    jf["scale"] = font.imfont->Scale;
    jf["color"] = hex_string (font.color);
    jf["size"] = font.imfont->FontSize;
    jf["file"] = font.file;
    jf["glyphs"] = font.glyphs;
    jf["ranges"] = font.ranges;
}

//--------------------------------------------------------------------------------------------------

bool
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
        };

        json["titlebar"] = journal.show_titlebar;
        json["background"]["file"] = journal.background_file;
        save_font (json, journal.text_font);
        save_font (json, journal.chapter_font);
        save_font (json, journal.button_font);
        save_font (json, journal.default_font);

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

static void
load_font (nlohmann::json const& json, font_t& font)
{
    std::string section = font.name + " font";
    nlohmann::json jf;
    if (json.contains (section))
        jf = json[section];
    else jf = nlohmann::json::object ();

    font.color = std::stoull (jf.value ("color", hex_string (font.color)), nullptr, 0);
    font.scale = jf.value ("scale", font.scale);
    auto set_scale = gsl::finally ([&font] { font.imfont->Scale = font.scale; });

    // The UI load button, otherwise we have to recreate all fonts outside the rendering loop
    // and thats too much of hassle if not tuning all other params through the UI too.
    if (font.imfont)
        return;

    font.size = jf.value ("size", font.size);
    font.glyphs = jf.value ("glyphs", font.glyphs);
    font.file = jf.value ("file", journal_directory + font.name + ".ttf");
    if (font.file.empty ())
        font.file = journal_directory + font.name + ".ttf";
    font.ranges = jf.value ("ranges", std::vector<ImWchar> {});

    auto font_atlas = imgui.igGetIO ()->Fonts;
    ImWchar const* ranges = nullptr;
    if (font.ranges.size ())
    {
        font.ranges.push_back (0);
        font.glyphs.clear ();
    }
    else if (font.glyphs.size ())
    {
        if (font.glyphs == "all")
        {
            static const ImWchar buff[] = { 0x0020, 0xFFEF, 0 }; // This one is tricky to avoid CDT
            ranges = buff;
        }
        if (font.glyphs == "korean")
            ranges = imgui.ImFontAtlas_GetGlyphRangesKorean (font_atlas);
        if (font.glyphs == "japanase")
            ranges = imgui.ImFontAtlas_GetGlyphRangesJapanese (font_atlas);
        if (font.glyphs == "chinese full")
            ranges = imgui.ImFontAtlas_GetGlyphRangesChineseFull (font_atlas);
        if (font.glyphs == "chinese common")
            ranges = imgui.ImFontAtlas_GetGlyphRangesChineseSimplifiedCommon (font_atlas);
        if (font.glyphs == "cyrillic")
            ranges = imgui.ImFontAtlas_GetGlyphRangesCyrillic (font_atlas);
        if (font.glyphs == "thai")
            ranges = imgui.ImFontAtlas_GetGlyphRangesThai (font_atlas);
        if (font.glyphs == "vietnamese")
            ranges = imgui.ImFontAtlas_GetGlyphRangesVietnamese (font_atlas);
    }

    if (file_exists (font.file))
        font.imfont = imgui.ImFontAtlas_AddFontFromFileTTF (
                font_atlas, font.file.c_str (), font.size, nullptr, ranges);
    if (!font.imfont)
    {
        font.imfont = imgui.ImFontAtlas_AddFontFromMemoryCompressedBase85TTF (
            font_atlas, font.default_data, font.size, nullptr, ranges);
        font.file.clear ();
    }
}

//--------------------------------------------------------------------------------------------------

bool
load_settings ()
{
    int maj;
    journal_version (&maj, nullptr, nullptr, nullptr);

    try
    {
        nlohmann::json json;

        std::ifstream fi (settings_location);
        if (!fi.is_open ())
        {
            log () << "Unable to open " << settings_location << " for reading." << std::endl;
        }
        else
        {
            fi >> json;
            if (json["version"]["major"].get<int> () != maj)
            {
                log () << "Incompatible settings file." << std::endl;
                return false;
            }
        }

        extern const char* font_viner_hand;
        extern const char* font_inconsolata;

        journal.button_font.name = "button";
        journal.button_font.scale = 1.f;
        journal.button_font.size = 36.f;
        journal.button_font.color = IM_COL32_WHITE;
        journal.button_font.file = "";
        journal.button_font.glyphs = "all";
        journal.button_font.ranges = {};
        journal.button_font.default_data = font_viner_hand;
        load_font (json, journal.button_font);

        journal.chapter_font.name = "chapter";
        journal.chapter_font.glyphs = "all";
        journal.chapter_font.scale = 1.f;
        journal.chapter_font.size = 54.f;
        journal.chapter_font.color = IM_COL32_BLACK;
        journal.chapter_font.file = "";
        journal.chapter_font.glyphs = "all";
        journal.chapter_font.ranges = {};
        journal.chapter_font.default_data = font_viner_hand;
        load_font (json, journal.chapter_font);

        journal.text_font.name = "text";
        journal.text_font.glyphs = "all";
        journal.text_font.scale = 1.f;
        journal.text_font.size = 36.f;
        journal.text_font.color = IM_COL32 (21, 17, 12, 255);
        journal.text_font.file = "";
        journal.text_font.glyphs = "all";
        journal.text_font.ranges = {};
        journal.text_font.default_data = font_viner_hand;
        load_font (json, journal.text_font);

        journal.default_font.name = "system";
        journal.default_font.scale = 1.f;
        journal.default_font.size = 18.f;
        journal.default_font.color = IM_COL32_WHITE;
        journal.default_font.file = "";
        journal.default_font.glyphs = "all";
        journal.default_font.ranges = {};
        journal.default_font.default_data = font_inconsolata;
        load_font (json, journal.default_font);

        journal.background_file = journal_directory + "book.dds";
        if (json.contains ("background"))
            journal.background_file = json["background"].value ("file", journal.background_file);

        journal.show_titlebar = json.value ("titlebar", false);
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to load settings file: " << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

bool
load_takenotes (std::string const& source)
{
    try
    {
        std::ifstream fi (source);
        if (!fi.is_open ())
        {
            log () << "Unable to open " << source << " for reading." << std::endl;
            return false;
        }

        std::string content {std::istreambuf_iterator<char> (fi),
                             std::istreambuf_iterator<char> ()};

        using namespace rapidxml;
        xml_document<> doc;
        doc.parse<0> (&content[0]);
        auto fiss = doc.first_node ("fiss");
        if (!fiss) throw std::runtime_error ("No /fiss node");
        auto data = fiss->first_node ("Data");
        if (!data) throw std::runtime_error ("No /fiss/Data node");
        auto noe = data->first_node ("NumberOfEntries");
        if (!noe) throw std::runtime_error ("No /fiss/Data/NumberOfEntries node");
        auto n = (int) std::stoul (noe->value ());

        std::vector<page_t> pages (std::max (n, 2));
        for (int i = 0; i < n; ++i)
        {
            auto num = std::to_string (i+1);
            auto title = data->first_node (("date" + num).c_str ());
            if (!title) continue; // Turns out there can be holes
            auto entry = data->first_node (("entry" + num).c_str ());
            if (!entry) continue;
            pages[i].title = title->value ();
            pages[i].content = entry->value ();
        }

        while (pages.size () < 2)
        {
            log () << "Less than two pages. Inserting empty one." << std::endl;
            pages.emplace_back (page_t {});
        }

        journal.pages = std::move (pages);
        journal.current_page = 0;
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to load Take Notes XML file: " << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

bool
save_variables ()
{
    try
    {
        nlohmann::json json;

        for (auto const& v: journal.variables)
            if (v.deletable) json["variables"].push_back ({
                { "fuid", v.fuid },
                { "name", v.name.c_str () },
                { "params", v.params.c_str () }
            });

        std::ofstream of (variables_location);
        if (!of.is_open ())
        {
            log () << "Unable to open " << variables_location << " for writting." << std::endl;
            return false;
        }
        of << json.dump (4);
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to save variables file: " << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

bool
load_variables ()
{
    try
    {
        nlohmann::json json;

        std::ifstream fi (variables_location);
        if (!fi.is_open ())
            log () << "Unable to open " << variables_location << " for reading." << std::endl;
        else
            fi >> json;

        // It is a bit more complex as at least the order of custom elements is to be preserved.
        journal.variables.erase (std::remove_if (
                    journal.variables.begin (), journal.variables.end (), [] (auto const& v)
                    { return v.deletable; }), journal.variables.end ());

        if (!json.contains ("variables"))
            return true;

        std::vector<variable_t> vars;
        for (auto const& jv: json["variables"])
        {
            int fuid = jv["fuid"].get<int> ();
            for (auto const& src: journal.variables)
                if (src.fuid == fuid)
                {
                    variable_t v = src;
                    v.name = jv["name"].get<std::string> ();
                    v.params = jv["params"].get<std::string> ();
                    v.deletable = true;
                    vars.emplace_back (std::move (v));
                    break;
                }
        }

        journal.variables.insert (journal.variables.begin (),
                std::make_move_iterator (vars.begin ()),
                std::make_move_iterator (vars.end ()));
    }
    catch (std::exception const& ex)
    {
        log () << "Unable to load variables file: " << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

