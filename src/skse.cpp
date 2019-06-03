/**
 * @file skse.cpp
 * @brief Implements SKSE plugin for SSE Journal
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
 * @ingroup Public API
 *
 * @details
 */

#include <sse-imgui/sse-imgui.h>
#include <sse-gui/sse-gui.h>
#include <sse-hooks/sse-hooks.h>
#include <utils/winutils.hpp>

#include <fstream>
#include <iomanip>
#include <chrono>
#include <array>
#include <cstdint>
typedef std::uint32_t UInt32;
typedef std::uint64_t UInt64;
#include <skse/PluginAPI.h>

//--------------------------------------------------------------------------------------------------

/// Given by SKSE to uniquely identify this DLL
static PluginHandle plugin = 0;

/// To communicate with the other SKSE plugins.
static SKSEMessagingInterface* messages = nullptr;

/// Log file in pre-defined location
static std::ofstream logfile;

/// [shared] Local initialization
sseimgui_api sseimgui = {};

/// [shared] Local initialization
sseh_api sseh = {};

/// [shared] Table with pointers
imgui_api imgui = {};

/// [shared] Reports current log file path (for user friendly messages)
std::string logfile_path;

/// [shared] Kind of message queue to be polled from the renderer for custom commands execution
std::string journal_message;

//--------------------------------------------------------------------------------------------------

static void
open_log ()
{
    logfile_path = "";
    if (known_folder_path (FOLDERID_Documents, logfile_path))
    {
        // Before plugins are loaded, SKSE takes care to create the directiories
        logfile_path += "\\My Games\\Skyrim Special Edition\\SKSE\\";
    }
    logfile_path += "sse-journal.log";
    logfile.open (logfile_path);
}

//--------------------------------------------------------------------------------------------------

decltype(logfile)&
log ()
{
    // MinGW 4.9.1 have no std::put_time()
    using std::chrono::system_clock;
    auto now_c = system_clock::to_time_t (system_clock::now ());
    auto loc_c = std::localtime (&now_c);
    logfile << '['
            << 1900 + loc_c->tm_year
            << '-' << std::setw (2) << std::setfill ('0') << loc_c->tm_mon
            << '-' << std::setw (2) << std::setfill ('0') << loc_c->tm_mday
            << ' ' << std::setw (2) << std::setfill ('0') << loc_c->tm_hour
            << ':' << std::setw (2) << std::setfill ('0') << loc_c->tm_min
            << ':' << std::setw (2) << std::setfill ('0') << loc_c->tm_sec
        << "] ";
    return logfile;
}

//--------------------------------------------------------------------------------------------------

void
journal_version (int* maj, int* min, int* patch, const char** timestamp)
{
    constexpr std::array<int, 3> ver = {
#include "../VERSION"
    };
    if (maj) *maj = ver[0];
    if (min) *min = ver[1];
    if (patch) *patch = ver[2];
    if (timestamp) *timestamp = JOURNAL_TIMESTAMP; //"2019-04-15T08:37:11.419416+00:00"
}

//--------------------------------------------------------------------------------------------------

/// SSE-MapTrack may send message with a command to execute, from within its rendering loop

static void
handle_journal_message (SKSEMessagingInterface::Message* m)
{
    if (m->type != 1 || m->dataLen < 1)
        return;
    journal_message = reinterpret_cast<const char*> (m->data);
}

//--------------------------------------------------------------------------------------------------

static void
handle_sseimgui_message (SKSEMessagingInterface::Message* m)
{
    if (m->type != SSEIMGUI_API_VERSION)
    {
        log () << "Unsupported SSEIMGUI interface v" << m->type
               << " (it is not v" << SSEIMGUI_API_VERSION
                << "). Bailing out." << std::endl;
        return;
    }

    sseimgui = *reinterpret_cast<sseimgui_api*> (m->data);

    int maj;
    sseimgui.version (nullptr, &maj, nullptr, nullptr);
    if (maj < 1)
    {
        log () << "SSE-Journal needs SSE-ImGui 1.1 or later." << std::endl;
        return;
    }

    imgui = sseimgui.make_imgui_api ();
    log () << "Accepted SSEIMGUI interface v" << SSEIMGUI_API_VERSION << std::endl;

    extern bool setup ();
    if (!setup ())
    {
        log () << "Unable to initialize SSE Journal" << std::endl;
        return;
    }

    extern void render (int); sseimgui.render_listener (&render, 0);
    log () << "All done." << std::endl;
}

//--------------------------------------------------------------------------------------------------

static void
handle_sseh_message (SKSEMessagingInterface::Message* m)
{
    if (m->type != SSEH_API_VERSION)
    {
        log () << "Unsupported SSEH interface v" << m->type
               << " (it is not v" << SSEH_API_VERSION
                << "). Bailing out." << std::endl;
        return;
    }

    sseh = *reinterpret_cast<sseh_api*> (m->data);
}

//--------------------------------------------------------------------------------------------------

/// Post Load ensure SSE-ImGui is loaded and can accept listeners

static void
handle_skse_message (SKSEMessagingInterface::Message* m)
{
    if (m->type != SKSEMessagingInterface::kMessage_PostLoad)
        return;
    log () << "SKSE Post Load." << std::endl;
    messages->RegisterListener (plugin, "SSEH", handle_sseh_message);
    messages->RegisterListener (plugin, "SSEIMGUI", handle_sseimgui_message);
}

//--------------------------------------------------------------------------------------------------

/// @see SKSE.PluginAPI.h

extern "C" __declspec(dllexport) bool SSEIMGUI_CCONV
SKSEPlugin_Query (SKSEInterface const* skse, PluginInfo* info)
{
    info->infoVersion = PluginInfo::kInfoVersion;
    info->name = "sse-journal";
    journal_version ((int*) &info->version, nullptr, nullptr, nullptr);

    plugin = skse->GetPluginHandle ();

    if (skse->isEditor)
        return false;

    return true;
}

//--------------------------------------------------------------------------------------------------

/// @see SKSE.PluginAPI.h

extern "C" __declspec(dllexport) bool SSEIMGUI_CCONV
SKSEPlugin_Load (SKSEInterface const* skse)
{
    open_log ();

    messages = (SKSEMessagingInterface*) skse->QueryInterface (kInterface_Messaging);
    messages->RegisterListener (plugin, "SKSE", handle_skse_message);
    messages->RegisterListener (plugin, "sse-journal", handle_journal_message);

    int a, m, p;
    const char* b;
    journal_version (&a, &m, &p, &b);
    log () << "SSE-Journal "<< a <<'.'<< m <<'.'<< p <<" ("<< b <<')' << std::endl;
    return true;
}

//--------------------------------------------------------------------------------------------------


