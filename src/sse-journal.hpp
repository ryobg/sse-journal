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

//--------------------------------------------------------------------------------------------------

// fileio.cpp

bool save_text (std::string const& destination);
bool save_book (std::string const& destination);
bool load_book (std::string const& source);
bool load_takenotes (std::string const& source);
bool save_settings ();
bool load_settings ();

//--------------------------------------------------------------------------------------------------

// variables.cpp

std::string local_time (const char* format);
std::vector<std::pair<std::string, std::function<std::string ()>>> make_variables ();

//--------------------------------------------------------------------------------------------------

#endif

