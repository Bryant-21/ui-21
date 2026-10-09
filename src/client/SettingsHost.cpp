#include "client/SettingsHost.h"
#include "b21ui/Settings.h"
#include "b21ui/modern/Theme.h"
#include "b21ui/modern/Widgets.h"

#include <imgui_internal.h>

#include <algorithm>
#include <string>
#include <vector>

namespace b21ui::client {
    void SettingsHost::Draw(Client& client, const FrameContext& frame) {
        namespace theme = modern::theme;
        namespace w = modern::w;
        theme::SetAppearanceClient("UI21_Settings");
        theme::Apply(std::max(1.0F, frame.height / 1080.0F));
        fullscreen_ = theme::SavedFullscreen("UI21_Settings");
        const auto popupOpen = [] {
            return ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        };
        const bool backBlocked = ImGui::GetIO().WantTextInput || popupOpen();
        bool close{};
        const bool armed = closeArmed_;
        if (!ImGui::IsKeyDown(ImGuiKey_Escape) && !ImGui::IsKeyDown(ImGuiKey_GamepadFaceRight)) closeArmed_ = true;
        ImGui::PushFont(theme::CurrentFonts().body, theme::Px(theme::BodySize));
        ImVec2 size;
        if (w::BeginAppWindow("##ui21Settings", {frame.width, frame.height}, fullscreen_, {theme::Px(1500), theme::Px(900)}, size)) {
            ImGui::BeginChild("##settingsHeader", {0, theme::Px(68)}, ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoScrollbar);
            ImGui::SetCursorPos({theme::Px(24), theme::Px(20)});
            {
                w::FontScope title(theme::CurrentFonts().bold, 22);
                ImGui::PushStyleColor(ImGuiCol_Text, theme::Accent);
                ImGui::TextUnformatted(modern::icon::Gear);
                ImGui::PopStyleColor();
                ImGui::SameLine(0, theme::Px(12));
                ImGui::TextUnformatted("UI 21 Settings");
            }
            ImGui::SameLine();
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - theme::Px(24) - 4 * ImGui::GetFrameHeight() - 3 * ImGui::GetStyle().ItemSpacing.x);
            w::QuickSettings("settingsAppearance", appearance_, {}, "Settings appearance");
            ImGui::SameLine();
            w::WindowLauncher(client, "ui21Settings");
            ImGui::SameLine();
            if (w::FullscreenToggle(fullscreen_)) theme::SaveFullscreen("UI21_Settings", fullscreen_);
            ImGui::SameLine();
            close = w::Icon("closeSettings", modern::icon::Close, "Close (Esc / B)");
            ImGui::EndChild();
            ImGui::Separator();

            const float sidebarWidth = std::min(theme::Px(270), ImGui::GetContentRegionAvail().x * 0.3F);
            ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::Translucent(theme::Sidebar));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {theme::Px(14), theme::Px(20)});
            ImGui::BeginChild("##settingsMods", {sidebarWidth, 0}, ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_NavFlattened);
            ImGui::PopStyleVar();
            const auto panels = SettingsPanels();
            std::vector<std::string> categories;
            for (const auto& panel : panels) {
                const std::string category = SettingsCategory(panel.id);
                if (std::ranges::find(categories, category) == categories.end()) categories.push_back(category);
            }
            for (const auto& category : categories) {
                w::Heading(category.c_str());
                for (const auto& panel : panels) {
                    if (category != SettingsCategory(panel.id)) continue;
                    ImGui::PushID(static_cast<int>(panel.id));
                    if (w::NavigationItem("##mod", panel.label, panel.icon, panel.selected != 0)) OpenSettings(panel.id);
                    if (panel.selected) {
                        ImGui::Indent(theme::Px(14));
                        client.DrawSettingsNavigation(frame);
                        ImGui::Unindent(theme::Px(14));
                    }
                    ImGui::PopID();
                }
                ImGui::Dummy({0, theme::Px(12)});
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
            const auto sidebarMax = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddLine({sidebarMax.x, ImGui::GetItemRectMin().y}, sidebarMax, theme::Border);
            ImGui::SameLine(0, 0);
            ImGui::BeginChild("##settingsBody", {0, 0}, ImGuiChildFlags_NavFlattened);
            client.Draw(frame);
            ImGui::EndChild();
        }
        ImGui::End();
        const auto back = [](ImGuiKey key) {
            return ImGui::TestKeyOwner(key, ImGuiKeyOwner_NoOwner) && ImGui::IsKeyPressed(key, false);
        };
        if (armed && !backBlocked && !popupOpen() && !ImGui::GetIO().WantTextInput &&
            (back(ImGuiKey_Escape) || back(ImGuiKey_GamepadFaceRight))) close = true;
        ImGui::PopFont();
        if (close) b21ui::Close(client);
    }
}
