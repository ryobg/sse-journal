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

#include <array>
#include <vector>
#include <string>
#include <functional>
#include <ctime>

//--------------------------------------------------------------------------------------------------

static inline std::string
local_time (const char* format)
{
    std::array<char, 64> buff = {};
    std::time_t t = std::time (nullptr);
    std::strftime (buff.data (), buff.size (), format, std::localtime (&t));
    return buff.data ();
}

//--------------------------------------------------------------------------------------------------

auto
make_variables ()
{
    std::vector<std::pair<std::string, std::function<std::string ()>>> vars;

    vars.emplace_back ("Local time", [] () { return local_time ("%X"); });
    vars.emplace_back ("Local date", [] () { return local_time ("%x"); });
    vars.emplace_back ("Local time & date", [] () { return local_time ("%c"); });

    return vars;
}

//--------------------------------------------------------------------------------------------------

