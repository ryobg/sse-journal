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


#include <fstream>
#include <vector>

// Warning come in a BSON parser, which is not used, and probably shouldn't be
#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wformat="
#  pragma GCC diagnostic ignored "-Wformat-extra-args"
#  include <nlohmann/json.hpp>
#  pragma GCC diagnostic pop
#endif

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

bool
save_settings (std::string const& destination)
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
        log () << "Unable to save settings file: " << ex.what () << std::endl;
        return false;
    }
    return true;
}

//--------------------------------------------------------------------------------------------------

bool
load_settings (std::string const& source)
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
load_takenotes (std::string const& source)
{
    return false;
}

//--------------------------------------------------------------------------------------------------

