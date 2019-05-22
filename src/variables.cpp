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

#include <sse-hooks/sse-hooks.h>

#include <array>
#include <vector>
#include <string>
#include <functional>
#include <ctime>
#include <cmath>

#include <windows.h>

//--------------------------------------------------------------------------------------------------

/// Defined in skse.cpp
extern sseh_api sseh;

/// To turn relative addresses into absolute so that the Skyrim watch points can be set.
static std::uintptr_t base_address = 0;

/// Something to use as POC
template<class T>
struct pointer {
    std::uintptr_t pointer, offset;
    inline T get () {
        return (T) (offset + *(std::uintptr_t*) (base_address + pointer));
    }
};

/**
 * Current in-game time since
 *
 * Integer part: Day (starting from zero)
 * Floating part: Hours as % of 24,
 *                Minutes as % of 60
 *                Seconds as % of 60
 *                and so on...
 * In the main menu, the number may vary. 1 at start, 1.333 after "Quit to Main Menu" and maybe
 * other values, depending on the situation.
 *
 * The game starts at Morndas, the 17th of Last Seed, 4E201, approximately 9:30.
 */

static pointer<float*> game_epoch = {};

//--------------------------------------------------------------------------------------------------

std::string
local_time (const char* format)
{
    std::array<char, 64> buff = {};
    std::time_t t = std::time (nullptr);
    std::strftime (buff.data (), buff.size (), format, std::localtime (&t));
    return buff.data ();
}

//--------------------------------------------------------------------------------------------------

static std::string
game_time ()
{
    auto source = game_epoch.get ();
    if (!source || !std::isnormal (*source) || *source < 0)
        return "(n/a)";

    float hms = *source - int (*source);
    int h = int (hms *= 24);
    hms  -= int (hms);
    int m = int (hms *= 60);
    hms  -= int (hms);
    int s = int (hms *= 60);

    return std::to_string (h) + ":" + std::to_string (m) + ":" + std::to_string (s);
}

//--------------------------------------------------------------------------------------------------

auto
make_variables ()
{
    std::vector<std::pair<std::string, std::function<std::string ()>>> vars;

    base_address = reinterpret_cast<std::uintptr_t> (::GetModuleHandle (nullptr));
    /*
    Found five consecitive pointers with offsets which seems to reside somewhere in the Papyrus
    virtual machine object (0x1ec3b78) according to SKSE. Weirdly, it is inside the eventSink array
    as specified there. No clue what is it, but on this machine and runtime it seems stable
    reference.
    *(*0x1ec3ba8 + 0x114)
    *(*0x1ec3bb0 +  0xdc)
    *(*0x1ec3bb8 +  0xa4)
    *(*0x1ec3bc0 +  0x6c)
    *(*0x1ec3bc8 +  0x34)
    */
    game_epoch.pointer = 0x1ec3bc8, game_epoch.offset = 0x34;
    if (sseh.find_target)
    {
        sseh.find_target ("GameTime", &game_epoch.pointer);
        sseh.find_target ("GameTime.Offset", &game_epoch.offset);
    }

    vars.emplace_back ("Game time", [] () { return game_time (); });
    vars.emplace_back ("Local time", [] () { return local_time ("%X"); });
    vars.emplace_back ("Local date", [] () { return local_time ("%x"); });
    vars.emplace_back ("Local time & date", [] () { return local_time ("%c"); });

    return vars;
}

//--------------------------------------------------------------------------------------------------

