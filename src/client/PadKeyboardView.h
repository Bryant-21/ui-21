#pragma once
#include "core/PadKeyboard.h"

namespace b21ui::client {
    // Draws the keyboard on the foreground draw list in the player's HUD color, the same on every
    // client. It never creates a window, so it cannot take focus from the text field it types into.
    void DrawPadKeyboard(const core::PadKeyboard& keyboard, float scale);
}
