#include "core/SettingsRegistry.h"

#include <algorithm>
#include <cstddef>

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
        const char* category = "UI 21";
        if (desc.size >= offsetof(B21UI_ClientDesc, settingsCategory) + sizeof(desc.settingsCategory) &&
            desc.settingsCategory && desc.settingsCategory[0]) category = desc.settingsCategory;
        panels_.push_back({id, label ? label : "", icon, category});
    }

    bool SettingsRegistry::Contains(B21UI_ClientId id) const {
        return std::ranges::any_of(panels_, [id](const Panel& panel) { return panel.id == id; });
    }

    const char* SettingsRegistry::Category(B21UI_ClientId id) const {
        for (const auto& panel : panels_) if (panel.id == id) return panel.category.c_str();
        return "UI 21";
    }

    B21UI_ClientId SettingsRegistry::Resolve(B21UI_ClientId id) const {
        if (id) return Contains(id) ? id : 0;
        return selected_ ? selected_ : panels_.empty() ? 0 : panels_.front().id;
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
