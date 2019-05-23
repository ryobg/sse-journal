/**
 * @file variables.cpp
 * @brief Methods for obtaining the so called Journal Variables
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
#include <sse-hooks/sse-hooks.h>

#include <array>
#include <vector>
#include <string>
#include <functional>
#include <ctime>
#include <cmath>
#include <algorithm>

#include <windows.h>

//--------------------------------------------------------------------------------------------------

/// Defined in skse.cpp
extern sseh_api sseh;

/// To turn relative addresses into absolute so that the Skyrim watch points can be set.
static std::uintptr_t skyrim_base = 0;

/// Wrap general logic of obtaining an address to a relative object
template<class T>
struct pointer
{
    std::uintptr_t pointer, offset;
    inline T* obtain () const
    {
        auto that = reinterpret_cast<std::uintptr_t*> (skyrim_base + pointer);
        if (*that) return reinterpret_cast<T*> (*that + offset);
        return nullptr;
    }
};

/**
 * Current in-game time since...
 *
 * Integer part: Day (starting from zero)
 * Floating part: Hours as % of 24,
 *                Minutes as % of 60
 *                Seconds as % of 60
 *                and so on...
 * In the main menu, the number may vary. 1 at start, 1.333 after "Quit to Main Menu" and maybe
 * other values, depending on the situation. At start of the game, the pointer reference is null,
 * hence no way to obtain the value.
 *
 * The game starts at Morndas, the 17th of Last Seed, 4E201, near 09:30.
 *
 * Found five consecitive pointers with offsets which seems to reside somewhere in the Papyrus
 * virtual machine object (0x1ec3b78) according to SKSE. Weirdly, it is inside the eventSink array
 * as specified there. No clue what is it, but on this machine and runtime it seems stable
 * reference:
 *
 * *0x1ec3ba8 + 0x114
 * *0x1ec3bb0 +  0xdc
 * *0x1ec3bb8 +  0xa4
 * *0x1ec3bc0 +  0x6c
 * *0x1ec3bc8 +  0x34
 */

struct pointer<float> game_epoch = {};

//--------------------------------------------------------------------------------------------------

/// Small utility function
static void
replace_all (std::string& data, std::string const& search, std::string const& replace)
{
    std::size_t n = data.find (search);
    while (n != std::string::npos)
    {
        data.replace (n, search.size (), replace);
        n = data.find (search, n + replace.size ());
    }
}

//--------------------------------------------------------------------------------------------------

static std::string
local_time (const char* format, std::tm& lt)
{
    std::string s;
    std::size_t n = 16;
    do
    {
        s.resize (n-1);
        if (auto r = std::strftime (&s[0], n-1, format, &lt))
        {
            s.resize (r);
            break;
        }
        n *= 2;
    }
    while (n < 512);
    return s;
}

//--------------------------------------------------------------------------------------------------

/**
 * Very simple custom formatted time printing for the Skyrim calendar.
 *
 * Preparses some stuff before calling back strftime()
 */

static std::string
game_time (std::string format)
{
    float* source = game_epoch.obtain ();
    if (!source || !std::isnormal (*source) || *source < 0)
        return "(n/a)";

    // Compute the format input
    float hms = *source - int (*source);
    int h = int (hms *= 24);
    hms  -= int (hms);
    int m = int (hms *= 60);
    hms  -= int (hms);
    int s = int (hms * 60);

    // Adjusts for starting date: Mon 17 Jul 201
    int d = int (*source);
    int y = d / 366 + 201;
    int yd = d % 366 + 229;
    int wd = (d+1) % 7;

    std::array<int, 12> months = { 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
    auto mit = std::lower_bound (months.cbegin (), months.cend (), yd);
    int mo = mit - months.cbegin ();
    int md = (mo ? yd-*(mit-1) : 1+yd);

    // Replace years
    auto sy = std::to_string (y);
    auto sY = "4E" + sy;
    replace_all (format, "%y", sy);
    replace_all (format, "%Y", sY);

    // Replace months
    static std::array<std::string, 12> longmon = {
        "Morning Star", "Sun's Dawn", "First Seed", "Rain's Hand", "Second Seed", "Midyear",
        "Sun's Height", "Last Seed", "Hearthfire", "Frostfall", "Sun's Dusk", "Evening Star"
    };
    static std::array<std::string, 12> birtmon = {
        "The Ritual", "The Lover", "The Lord", "The Mage", "The Shadow", "The Steed",
        "The Apprentice", "The Warrior", "The Lady", "The Tower", "The Atronach", "The Thief"
    };
    static std::array<std::string, 12> argomon = {
        "Vakka (Sun)", "Xeech (Nut)", "Sisei (Sprout)", "Hist-Deek (Hist Sapling)",
        "Hist-Dooka (Mature Hist)", "Hist-Tsoko (Elder Hist)", "Thtithil-Gah (Egg-Basket)",
        "Thtithil (Egg)", "Nushmeeko (Lizard)", "Shaja-Nushmeeko (Semi-Humanoid Lizard)",
        "Saxhleel (Argonian)", "Xulomaht (The Deceased)"
    };
    replace_all (format, "%lm", longmon[mo]);
    replace_all (format, "%bm", birtmon[mo]);
    replace_all (format, "%am", argomon[mo]);
    replace_all (format, "%mo", std::to_string (mo+1));
    replace_all (format, "%md", std::to_string (md));

    // Weekdays
    static std::array<std::string, 7> longwday = {
        "Sundas", "Morndas", "Tirdas", "Middas", "Turdas", "Fredas", "Loredas"
    };
    static std::array<std::string, 7> shrtwday = {
        "Sun", "Mor", "Tir", "Mid", "Tur", "Fre", "Lor"
    };
    replace_all (format, "%sd", shrtwday[wd]);
    replace_all (format, "%ld", longwday[wd]);
    replace_all (format, "%wd", std::to_string (wd+1));

    // Time
    replace_all (format, "%h", std::to_string (h));
    replace_all (format, "%m", std::to_string (m));
    replace_all (format, "%s", std::to_string (s));

    // Raw
    replace_all (format, "%r", std::to_string (*source));
    replace_all (format, "%ri", std::to_string (d));

    return format;
}

//--------------------------------------------------------------------------------------------------

std::string
local_time (const char* format)
{
    std::time_t t = std::time (nullptr);
    std::tm* lt = std::localtime (&t);
    return local_time (format, *lt);
}

//--------------------------------------------------------------------------------------------------

std::vector<variable_t>
make_variables ()
{
    skyrim_base = reinterpret_cast<std::uintptr_t> (::GetModuleHandle (nullptr));
    std::vector<variable_t> vars;

    game_epoch.pointer = 0x1ec3bc8, game_epoch.offset = 0x34;
    if (sseh.find_target)
    {
        sseh.find_target ("GameTime", &game_epoch.pointer);
        sseh.find_target ("GameTime.Offset", &game_epoch.offset);
    }
    if (game_epoch.pointer)
    {
        variable_t gtime;
        gtime.fuid = 1;
        gtime.deletable = false;
        gtime.name = "Game time (fixed)";
        gtime.info = "Following substitions starts with %:\n"
            "y is the year number (e.g. 201)\n"
            "Y is the year with the epoch in front (e.g. 4E201)\n"
            "lm is long month name (e.g. First Seed)\n"
            "bm is the birth sign for that month (e.g. The Mage)\n"
            "am is the Argonian month (e.g. Hist-Dooka (Mature Hist))\n"
            "mo is the month number (from 1 to 12)\n"
            "md is the month day numer (from 1 to 28,30 or 31)\n"
            "sd is short day name, the 1st three letters (e.g. Tir)\n"
            "ld is the long day name (e.g. Middas)\n"
            "wd is the week day numer (from 1 to 7)\n"
            "h is the hour (from 0 to 23)\n"
            "m are the minutes (from 0 to 59)\n"
            "s are the seconds (from 0 to 59)\n"
            "r is the raw input (aka Papyrus.GetCurrentGameTime ())\n"
            "ri is the integer part of %r (i.e. game days since start)";
        gtime.params = "%h:%m %ld, day %md of %lm, %Y";
        gtime.apply = [] (variable_t* self) { return game_time (self->params); };
        vars.emplace_back (std::move (gtime));
    }

    variable_t ltime;
    ltime.fuid = 2;
    ltime.deletable = false;
    ltime.name = "Local time (fixed)";
    ltime.info = "Look the format specification on\n"
        "https://en.cppreference.com/w/cpp/chrono/c/strftime";
    ltime.params = "%X %x";
    ltime.apply = [] (variable_t* self) { return local_time (self->params.c_str ()); };
    vars.emplace_back (std::move (ltime));

    return vars;
}

//--------------------------------------------------------------------------------------------------

