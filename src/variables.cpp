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
 * The game starts at Morndas, the 17th of Last Seed, 4E201, approximately 9:30.
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
    std::tm t;
    float hms = *source - int (*source);
    t.tm_hour = int (hms *= 24);
    hms  -= int (hms);
    t.tm_min  = int (hms *= 60);
    hms  -= int (hms);
    t.tm_sec  = int (hms * 60);

    int d = int (*source);
    t.tm_year = d / 366 - 1900 + 201;
    t.tm_yday = d % 366;
    t.tm_wday = (d+1) % 7;
    t.tm_isdst = 0;

    std::array<int, 12> md = { 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };
    auto mit = std::lower_bound (md.cbegin (), md.cend (), t.tm_yday);
    t.tm_mon = mit - md.cbegin ();
    t.tm_mday = t.tm_yday - (t.tm_mon ? *(mit-1) : 0);

    // Replace years
    auto ys = "4E" + std::to_string (t.tm_year);
    replace_all (format, "%EY", ys);
    replace_all (format, "%Ey", ys);
    replace_all (format, "%EC", ys);
    replace_all (format, "%G" , ys);
    replace_all (format, "%g" , ys);

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
    replace_all (format, "%b", longmon[t.tm_mon]);
    replace_all (format, "%B", birtmon[t.tm_mon]);
    replace_all (format, "%h", argomon[t.tm_mon]);

    // Replace days
    static std::array<std::string, 7> longwday = {
        "Sundas", "Morndas", "Tirdas", "Middas", "Turdas", "Fredas", "Loredas"
    };
    static std::array<std::string, 7> shrtwday = {
        "Sun", "Mor", "Tir", "Mid", "Tur", "Fre", "Lor"
    };
    replace_all (format, "%a", shrtwday[t.tm_wday]);
    replace_all (format, "%A", longwday[t.tm_wday]);

    // Custom sets are ignored
    replace_all (format, "%c" , "");
    replace_all (format, "%Ec", "");
    replace_all (format, "%x" , "");
    replace_all (format, "%Ex", "");
    replace_all (format, "%X" , "");
    replace_all (format, "%EX", "");

    return local_time (format.c_str (), t);
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
        gtime.name = "Game time";
        gtime.info = "Similar to Local Time, but with few quirks:\n"
            "EY, Ey, EC, G and g are replaced with 4E%Y (e.g. 4E201)\n"
            "b is long month name (e.g. First Seed)\n"
            "B is the birth sign for that month (e.g. The Mage)\n"
            "h is the Argonian name (e.g. Hist-Dooka (Mature Hist))\n"
            "a is short day name, the 1st three letters (e.g. Tir)\n"
            "A is the long day name (e.g. Middas)\n"
            "c, Ec, x, Ex, X and EX are removed";
        gtime.params = "%r %p %A, day %e of %b, %g";
        gtime.apply = [] (variable_t* self) { return game_time (self->params); };
        vars.emplace_back (std::move (gtime));
    }

    variable_t ltime;
    ltime.name = "Local time";
    ltime.info = "Look the format specification on "
        "https://en.cppreference.com/w/cpp/chrono/c/strftime";
    ltime.params = "%X %x";
    ltime.apply = [] (variable_t* self) { return local_time (self->params.c_str ()); };
    vars.emplace_back (std::move (ltime));

    return vars;
}

//--------------------------------------------------------------------------------------------------

