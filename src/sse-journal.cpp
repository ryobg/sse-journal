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
#include <fstream>

//--------------------------------------------------------------------------------------------------

/// Defined in skse.cpp
extern std::ofstream logfile;

/// Defined in skse.cpp
extern imgui_api imgui;

//--------------------------------------------------------------------------------------------------

void SSEIMGUI_CCONV
render (int active)
{
    if (!active)
        return;

    static bool show_demo_window = true;
    static bool show_another_window = false;

    if (show_demo_window)
        imgui.igShowDemoWindow (&show_demo_window);
    {
        static float f = 0.0f;
        static int counter = 0;
        imgui.igBegin ("Hello, world!", nullptr, 0);
        imgui.igText ("This is some useful text.");
        imgui.igCheckbox ("Demo Window", &show_demo_window);
        imgui.igCheckbox ("Another Window", &show_another_window);
        imgui.igSliderFloat ("float", &f, 0.0f, 1.0f, "%.3f", 1.f);
        if (imgui.igButton ("Button", ImVec2 {}))
            counter++;
        imgui.igSameLine (0.f, -1.f);
        imgui.igText ("counter = %d", counter);
        imgui.igText ("Application average %.3f ms/frame (%.1f FPS)",
                1000.0f / imgui.igGetIO ()->Framerate, imgui.igGetIO ()->Framerate);
        imgui.igEnd ();
    }
    if (show_another_window)
    {
        imgui.igBegin ("Another Window", &show_another_window, 0);
        imgui.igText ("Hello from another window!");
        if (imgui.igButton ("Close Me", ImVec2 {}))
            show_another_window = false;
        imgui.igEnd ();
    }
}

//--------------------------------------------------------------------------------------------------

