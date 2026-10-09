#include "b21ui/Windows.h"

#include <array>

namespace b21ui {
    namespace {
        constexpr std::array kWindows{
            Window{"devTools", "Dev Tools", "\xEF\x97\xBD", L"B21_DevTools.dll", "B21_DevTools"},
            Window{"ui21Settings", "UI 21 Settings", "\xEF\x80\x93", nullptr, nullptr},
            Window{"talesConfig", "TFA Config", "\xEF\x85\x8E", L"B21_TalesFromAppalachia.dll", "B21_TalesFromAppalachia"},
            Window{"autoConflictResolver", "Auto Conflict Resolver", "\xEF\x8E\x87", L"AutoConflictResolver.dll",
                   "AutoConflictResolver"},
            Window{"fullScreenMap", "Full Screen Map", "\xEF\x89\xB9", L"B21_FullScreenMap.dll", "B21_FullScreenMap"},
            Window{"musicPlayer", "Music Player", "\xEF\x80\x81", L"B21_MusicPlayer.dll", "B21_MusicPlayer"},
        };
    }

    std::span<const Window> Windows() { return kWindows; }

    std::vector<const Window*> InstalledWindows(std::string_view self, const std::function<bool(const Window&)>& loaded) {
        std::vector<const Window*> installed;
        for (const auto& window : kWindows)
            if (window.id != self && loaded(window)) installed.push_back(&window);
        return installed;
    }
}
