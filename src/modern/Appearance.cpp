#include "b21ui/Paths.h"
#include "b21ui/Common.h"
#include "b21ui/modern/AppearanceCodec.h"
#include "b21ui/modern/Theme.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <string>
#include <system_error>

namespace b21ui::modern::theme {
    namespace {
        constexpr auto kSection = L"Modern";
        constexpr auto kWindowsSection = L"Windows";

        struct Store {
            Appearance appearance;
            Appearance defaults;
            std::wstring section = kSection;
            std::filesystem::file_time_type loadedWrite{};
            std::chrono::steady_clock::time_point nextCheck{};
            bool loaded = false;
        };

        Store& Current() {
            return common::ContextSlot<Store>();
        }

        const std::filesystem::path& File() {
            static const auto path = PluginAssetDir().parent_path() / "B21UI.ini";
            return path;
        }

        std::wstring Wide(std::string_view text) { return {text.begin(), text.end()}; }

        appearance::Lookup Reader(std::wstring section) {
            return [section = std::move(section)](std::string_view key) -> std::optional<std::string> {
                wchar_t buffer[32]{};
                ::GetPrivateProfileStringW(section.c_str(), Wide(key).c_str(), L"", buffer, static_cast<DWORD>(std::size(buffer)),
                                           File().c_str());
                if (buffer[0] == 0) return std::nullopt;
                std::string out;
                for (const wchar_t* c = buffer; *c; ++c) out.push_back(static_cast<char>(*c));
                return out;
            };
        }

        std::filesystem::file_time_type LastWrite() {
            std::error_code error;
            const auto time = std::filesystem::last_write_time(File(), error);
            return error ? std::filesystem::file_time_type{} : time;
        }

        void Load(Store& store) {
            const auto client = Reader(store.section);
            const auto shared = store.section == kSection ? appearance::Lookup{} : Reader(kSection);
            store.appearance = appearance::Resolve(client, shared, store.defaults);
            store.loadedWrite = LastWrite();
            store.loaded = true;
        }
    }

    void SetAppearanceClient(std::string_view client, const Appearance& defaults) {
        auto& store = Current();
        const auto section = Wide(appearance::Section(client));
        if (store.section == section && store.loaded && store.defaults.backgroundOpacity == defaults.backgroundOpacity &&
            store.defaults.textScale == defaults.textScale && store.defaults.tableTextSize == defaults.tableTextSize &&
            store.defaults.family == defaults.family) return;
        store.section = section;
        store.defaults = defaults;
        store.loaded = false;
    }

    const Appearance& DefaultAppearance() { return Current().defaults; }

    const Appearance& CurrentAppearance() {
        auto& store = Current();
        const auto now = std::chrono::steady_clock::now();
        if (!store.loaded) {
            Load(store);
        } else if (now >= store.nextCheck) {
            if (LastWrite() != store.loadedWrite) Load(store);
        }
        if (now >= store.nextCheck) store.nextCheck = now + std::chrono::seconds(1);
        return store.appearance;
    }

    bool SavedFullscreen(std::string_view window) {
        const std::wstring key(window.begin(), window.end());
        return ::GetPrivateProfileIntW(kWindowsSection, (key + L"Fullscreen").c_str(), 0, File().c_str()) != 0;
    }

    void SaveFullscreen(std::string_view window, bool fullscreen) {
        const std::wstring key(window.begin(), window.end());
        ::WritePrivateProfileStringW(kWindowsSection, (key + L"Fullscreen").c_str(), fullscreen ? L"1" : L"0", File().c_str());
    }

    bool SavedWindowFlag(std::string_view window, std::string_view name, bool fallback) {
        const auto key = Wide(window) + Wide(name);
        return ::GetPrivateProfileIntW(kWindowsSection, key.c_str(), fallback ? 1 : 0, File().c_str()) != 0;
    }

    void SaveWindowFlag(std::string_view window, std::string_view name, bool value) {
        const auto key = Wide(window) + Wide(name);
        ::WritePrivateProfileStringW(kWindowsSection, key.c_str(), value ? L"1" : L"0", File().c_str());
    }

    std::string SavedWindowText(std::string_view window, std::string_view name) {
        const auto key = Wide(window) + Wide(name);
        wchar_t buffer[128]{};
        ::GetPrivateProfileStringW(kWindowsSection, key.c_str(), L"", buffer, static_cast<DWORD>(std::size(buffer)), File().c_str());
        std::string out;
        for (const wchar_t* c = buffer; *c; ++c) out.push_back(static_cast<char>(*c));
        return out;
    }

    void SaveWindowText(std::string_view window, std::string_view name, std::string_view value) {
        const auto key = Wide(window) + Wide(name);
        ::WritePrivateProfileStringW(kWindowsSection, key.c_str(), Wide(value).c_str(), File().c_str());
    }

    void SetAppearance(const Appearance& appearance, bool save) {
        auto& store = Current();
        store.appearance = {std::clamp(appearance.backgroundOpacity, MinBackgroundOpacity, 1.0F),
                            std::clamp(appearance.textScale, MinTextScale, MaxTextScale),
                            std::clamp(appearance.tableTextSize, MinTableTextSize, MaxTableTextSize), appearance.family};
        store.loaded = true;
        if (!save) return;
        for (const auto& [key, value] : appearance::Format(store.appearance))
            ::WritePrivateProfileStringW(store.section.c_str(), Wide(key).c_str(), Wide(value).c_str(), File().c_str());
        store.loadedWrite = LastWrite();
    }

    void SetFamily(Family family, bool allWindows) {
        auto& store = Current();
        CurrentAppearance();
        store.appearance.family = family;
        const auto name = Wide(appearance::FamilyName(family));
        if (!allWindows) {
            ::WritePrivateProfileStringW(store.section.c_str(), L"Theme", name.c_str(), File().c_str());
        } else {
            ::WritePrivateProfileStringW(kSection, L"Theme", name.c_str(), File().c_str());
            std::wstring names(4096, L'\0');
            const auto length = ::GetPrivateProfileSectionNamesW(names.data(), static_cast<DWORD>(names.size()), File().c_str());
            names.resize(length);
            const std::wstring prefix = std::wstring(kSection) + L".";
            for (std::size_t at = 0; at < names.size();) {
                const std::wstring section(names.c_str() + at);
                at += section.size() + 1;
                if (section.starts_with(prefix)) ::WritePrivateProfileStringW(section.c_str(), L"Theme", nullptr, File().c_str());
            }
        }
        store.loadedWrite = LastWrite();
    }
}
