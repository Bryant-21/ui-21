#include "b21ui/modern/AppearanceCodec.h"

#include <algorithm>
#include <charconv>
#include <format>

namespace b21ui::modern::appearance {
    namespace {
        std::optional<float> Number(const std::optional<std::string>& text) {
            if (!text || text->empty()) return std::nullopt;
            float value = 0.0F;
            const auto* begin = text->data();
            const auto result = std::from_chars(begin, begin + text->size(), value);
            if (result.ec != std::errc{} || result.ptr == begin) return std::nullopt;
            return value;
        }

        float Read(const Lookup& client, const Lookup& shared, std::string_view key, float fallback, float low, float high) {
            auto value = Number(client ? client(key) : std::nullopt);
            if (!value) value = Number(shared ? shared(key) : std::nullopt);
            return std::clamp(value.value_or(fallback), low, high);
        }
    }

    std::string_view FamilyName(theme::Family family) {
        switch (family) {
        case theme::Family::Fallout4: return "fo4";
        case theme::Family::Fallout76: return "fo76";
        default: return "modern";
        }
    }

    theme::Family ParseFamily(std::string_view text) {
        if (text == "fo4") return theme::Family::Fallout4;
        if (text == "fo76") return theme::Family::Fallout76;
        return theme::Family::Modern;
    }

    std::string Section(std::string_view client) {
        return client.empty() ? std::string("Modern") : std::format("Modern.{}", client);
    }

    theme::Appearance Resolve(const Lookup& client, const Lookup& shared, const theme::Appearance& defaults) {
        theme::Appearance out;
        out.backgroundOpacity = Read(client, shared, "BackgroundOpacity", defaults.backgroundOpacity, theme::MinBackgroundOpacity, 1.0F);
        out.textScale = Read(client, shared, "TextScale", defaults.textScale, theme::MinTextScale, theme::MaxTextScale);
        out.tableTextSize =
            Read(client, shared, "TableTextSize", defaults.tableTextSize, theme::MinTableTextSize, theme::MaxTableTextSize);
        auto family = client ? client("Theme") : std::nullopt;
        if (!family && shared) family = shared("Theme");
        out.family = family ? ParseFamily(*family) : defaults.family;
        return out;
    }

    std::vector<std::pair<std::string, std::string>> Format(const theme::Appearance& appearance) {
        return {{"BackgroundOpacity", std::format("{:.2f}", appearance.backgroundOpacity)},
                {"TextScale", std::format("{:.2f}", appearance.textScale)},
                {"TableTextSize", std::format("{:.2f}", appearance.tableTextSize)}};
    }
}
