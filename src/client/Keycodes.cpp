#include "b21ui/Keybindings.h"
#include <Windows.h>

namespace b21ui::keys {
    int FromVirtualKey(unsigned int key) {
        if (!key) return 0;
        switch (key) {
        case VK_LBUTTON: return 256; case VK_RBUTTON: return 257; case VK_MBUTTON: return 258;
        case VK_XBUTTON1: return 259; case VK_XBUTTON2: return 260; case VK_PAUSE: return 197;
        case VK_HOME: return 199; case VK_UP: return 200; case VK_PRIOR: return 201;
        case VK_LEFT: return 203; case VK_RIGHT: return 205; case VK_END: return 207;
        case VK_DOWN: return 208; case VK_NEXT: return 209; case VK_INSERT: return 210;
        case VK_DELETE: return 211; case VK_DIVIDE: return 181; case VK_SNAPSHOT: return 183;
        default: break;
        }
        const auto scan = ::MapVirtualKeyW(key, MAPVK_VK_TO_VSC_EX);
        return static_cast<int>((scan & 0xFF) | (scan & 0xFF00 ? 0x80 : 0));
    }
}
