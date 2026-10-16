#include "core/SettingsRegistry.h"

#include <algorithm>
#include <cctype>
#include <tuple>
#include <cstddef>
#include <string_view>

namespace b21ui::core {
    void SettingsRegistry::Add(B21UI_ClientId id, const B21UI_ClientDesc& desc) {
        if (!id || desc.size < B21UI_CLIENTDESC_MIN_SIZE || desc.kind != B21UI_KIND_MODAL ||
            !(desc.flags & B21UI_FLAG_SETTINGS) || Contains(id)) return;
        const char* label = desc.name;
        if (desc.size >= offsetof(B21UI_ClientDesc, settingsLabel) + sizeof(desc.settingsLabel) &&
            desc.settingsLabel && desc.settingsLabel[0]) label = desc.settingsLabel;
        const char* icon = "\xEF\x80\x93";
        if (desc.size >= offsetof(B21UI_ClientDesc, settingsIcon) + sizeof(desc.settingsIcon) &&
            desc.settingsIcon && desc.settingsIcon[0]) icon = desc.settingsIcon;
        const char* category = "Script Extender";
        if (desc.size >= offsetof(B21UI_ClientDesc, settingsCategory) + sizeof(desc.settingsCategory) &&
            desc.settingsCategory && desc.settingsCategory[0]) category = desc.settingsCategory;
        if (std::string_view(category) == "UI 21") category = "Script Extender";
        panels_.push_back({id, label ? label : "", icon, category});
    }

    bool SettingsRegistry::Contains(B21UI_ClientId id) const {
        return std::ranges::any_of(panels_, [id](const Panel& panel) { return panel.id == id; });
    }

    const char* SettingsRegistry::Category(B21UI_ClientId id) const {
        for (const auto& panel : panels_) if (panel.id == id) return panel.category.c_str();
        return "Script Extender";
    }

    B21UI_ClientId SettingsRegistry::Resolve(B21UI_ClientId id) const {
        if (id) return Contains(id) ? id : 0;
        if (selected_) return selected_;
        if (const auto it = std::ranges::find(panels_, preferred_, &Panel::label); it != panels_.end()) return it->id;
        return First();
    }

    // The panel the sidebar lists first: Script Extender, then other categories A-Z, B21UI last, each by label.
    B21UI_ClientId SettingsRegistry::First() const {
        const auto lower = [](std::string text) {
            std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        };
        const auto rank = [](const Panel& panel) {
            return panel.category == "B21UI" ? 2 : panel.category == "Script Extender" ? 0 : 1;
        };
        const auto first = std::ranges::min_element(panels_, [&](const Panel& a, const Panel& b) {
            return std::tuple(rank(a), lower(a.category), lower(a.label)) < std::tuple(rank(b), lower(b.category), lower(b.label));
        });
        return first == panels_.end() ? 0 : first->id;
    }

    const char* SettingsRegistry::Label(B21UI_ClientId id) const {
        for (const auto& panel : panels_) if (panel.id == id) return panel.label.c_str();
        return "";
    }

    void SettingsRegistry::Select(B21UI_ClientId id) {
        if (Contains(id)) selected_ = id;
    }

    std::uint32_t SettingsRegistry::Panels(B21UI_SettingsPanel* panels, std::uint32_t capacity) const {
        if (panels)
            for (std::size_t i = 0; i < std::min<std::size_t>(capacity, panels_.size()); ++i)
                panels[i] = {panels_[i].id, panels_[i].label.c_str(), panels_[i].id == selected_ ? 1u : 0u,
                             panels_[i].icon.c_str()};
        return static_cast<std::uint32_t>(panels_.size());
    }
}
