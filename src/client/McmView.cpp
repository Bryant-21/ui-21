#include "client/McmView.h"
#include "client/InputMap.h"
#include "b21ui/modern/Theme.h"
#include "b21ui/modern/Widgets.h"

#include <Windows.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <format>

namespace b21ui::mcm {
    namespace {
        double Number(const Json& value) {
            return value.is_number() ? value.get<double>() : value.is_boolean() && value.get<bool>() ? 1.0 : 0.0;
        }
        std::string Caption(const Json& value) { return value.is_string() ? value.get<std::string>() : value.dump(); }
        int ScanCode(ImGuiKey key) {
            for (int vk = 8; vk < 256; ++vk)
                if (client::KeyFromVk(static_cast<std::uint32_t>(vk)) == key) {
                    const auto scan = ::MapVirtualKeyW(static_cast<UINT>(vk), MAPVK_VK_TO_VSC_EX);
                    return static_cast<int>((scan & 0xFF) | ((scan & 0xFF00) ? 0x80 : 0));
                }
            return 0;
        }
        std::string KeyCaption(const Json& value) {
            int code{}, modifiers{};
            if (value.is_array() && value.size() == 2) { code = value[0].get<int>(); modifiers = value[1].get<int>(); }
            else if (value.is_string()) ::sscanf_s(value.get<std::string>().c_str(), "%d,%d", &code, &modifiers);
            if (!code) return "Unbound";
            std::string label;
            if (modifiers & 2) label += "Ctrl + ";
            if (modifiers & 1) label += "Shift + ";
            if (modifiers & 4) label += "Alt + ";
            static constexpr const char* mouse[]{"Left mouse", "Right mouse", "Middle mouse", "Mouse 4", "Mouse 5"};
            static constexpr const char* pad[]{"D-pad up", "D-pad down", "D-pad left", "D-pad right", "Start", "Back",
                "Left stick", "Right stick", "LB", "RB", "A", "B", "X", "Y", "LT", "RT"};
            if (code >= 256 && code < 261) return label + mouse[code - 256];
            if (code >= 266 && code < 282) return label + pad[code - 266];
            for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key)
                if (ScanCode(static_cast<ImGuiKey>(key)) == code) return label + ImGui::GetKeyName(static_cast<ImGuiKey>(key));
            return label + "Key " + std::to_string(code);
        }
    }
    bool View::Navigation(const Menu& menu, std::size_t& page) {
        bool changed{};
        for (std::size_t i = 0; i < menu.pages.size(); ++i) {
            const auto& label = menu.pages[i].name;
            const char* glyph = i == 0 ? modern::icon::Layers : modern::icon::Note;
            if (label == "Hotkeys") glyph = modern::icon::Keyboard;
            else if (label == "About") glyph = modern::icon::Info;
            ImGui::PushID(static_cast<int>(i));
            if (modern::w::NavigationItem("##page", label.c_str(), glyph, page == i)) { page = i; changed = true; }
            ImGui::PopID();
        }
        return changed;
    }

    void View::Draw(const Menu& menu, std::size_t pageIndex, const Submit& submit) {
        namespace theme = modern::theme;
        namespace w = modern::w;
        if (pageIndex >= menu.pages.size()) return;
        const auto& page = menu.pages[pageIndex];
        const auto identity = menu.mod + ":" + std::to_string(pageIndex);
        if (page_ != identity) { page_ = identity; buffers_.clear(); edits_.clear(); capture_ = -1; }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {theme::Px(28), theme::Px(24)});
        ImGui::BeginChild("##mcmPage", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding | ImGuiChildFlags_NavFlattened);
        ImGui::PopStyleVar();
        w::Title(menu.name.c_str());
        ImGui::TextDisabled("%s", page.name.c_str());
        ImGui::Spacing();
        if (!page.error.empty()) ImGui::TextWrapped("%s", page.error.c_str());
        for (std::size_t index = 0; index < page.rows.size(); ++index) {
            const auto& row = page.rows[index];
            if (!row.visible) continue;
            ImGui::PushID(static_cast<int>(index));
            const auto type = row.definition.value("type", "text");
            const auto& options = row.definition.at("valueOptions");
            const auto numericValue = [&] (double value) -> Json {
                const auto source = options.value("sourceType", "");
                if (source == "ModSettingInt" || source == "PropertyValueInt") return static_cast<int>(value);
                return value;
            };
            ImGui::BeginDisabled(!row.error.empty() || row.definition.value("disabled", false));
            if (type == "section") {
                ImGui::Spacing();
                w::Heading(row.text.c_str());
            } else if (type == "spacer") ImGui::Dummy({0, theme::Px(16)});
            else if (type == "text") ImGui::TextWrapped("%s", row.text.c_str());
            else if (type == "button") {
                if (w::Button(row.text.c_str())) submit(index, row.value);
            } else if (type == "switcher") {
                const bool value = Number(row.value) != 0;
                if (w::Switch(row.text.c_str(), value)) submit(index, !value);
            } else if (type == "slider" || ((type == "textinputInt" || type == "textinputFloat") &&
                                            options.contains("min") && options.contains("max"))) {
                ImGui::TextUnformatted(row.text.c_str());
                float value = static_cast<float>(Number(edits_.contains(static_cast<int>(index)) ? edits_[static_cast<int>(index)] : row.value));
                const float low = options.value("min", 0.0F), high = options.value("max", 1.0F), step = options.value("step", 0.0F);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::SliderFloat("##value", &value, low, std::max(low, high), step >= 1 ? "%.0f" : "%.2f", ImGuiSliderFlags_AlwaysClamp)) edits_[static_cast<int>(index)] = value;
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    if (step > 0) value = std::clamp(low + std::round((value - low) / step) * step, low, std::max(low, high));
                    submit(index, numericValue(value));
                    edits_.erase(static_cast<int>(index));
                }
                const auto minimum = w::Number(low), maximum = w::Number(high);
                const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
                ImGui::TextDisabled("%s", minimum.c_str());
                ImGui::SameLine();
                ImGui::SetCursorPosX(right - ImGui::CalcTextSize(maximum.c_str()).x);
                ImGui::TextDisabled("%s", maximum.c_str());
            } else if (type == "stepper" || type == "dropdown" || type == "dropdownFiles") {
                ImGui::TextUnformatted(row.text.c_str());
                int chosen = static_cast<int>(Number(row.value));
                if (type == "dropdownFiles" && row.value.is_string()) {
                    const auto found = std::ranges::find(row.options, row.value);
                    chosen = found == row.options.end() ? 0 : static_cast<int>(std::distance(row.options.begin(), found));
                }
                const auto selected = chosen >= 0 && chosen < static_cast<int>(row.options.size()) ? Caption(row.options[chosen]) : "Select...";
                ImGui::SetNextItemWidth(-1);
                if (ImGui::BeginCombo("##value", selected.c_str())) {
                    for (std::size_t i = 0; i < row.options.size(); ++i) {
                        ImGui::PushID(static_cast<int>(i));
                        if (ImGui::Selectable(Caption(row.options[i]).c_str(), static_cast<int>(i) == chosen))
                            submit(index, type == "dropdownFiles" ? row.options[i] : Json(i));
                        ImGui::PopID();
                    }
                    ImGui::EndCombo();
                }
            } else if (type == "textinputInt" || type == "textinputFloat") {
                ImGui::TextUnformatted(row.text.c_str());
                ImGui::SetNextItemWidth(-1);
                if (type == "textinputInt") {
                    int value = static_cast<int>(Number(edits_.contains(static_cast<int>(index)) ? edits_[static_cast<int>(index)] : row.value));
                    if (ImGui::InputInt("##value", &value)) edits_[static_cast<int>(index)] = value;
                    if (ImGui::IsItemDeactivatedAfterEdit()) { submit(index, value); edits_.erase(static_cast<int>(index)); }
                } else {
                    float value = static_cast<float>(Number(edits_.contains(static_cast<int>(index)) ? edits_[static_cast<int>(index)] : row.value));
                    if (ImGui::InputFloat("##value", &value)) edits_[static_cast<int>(index)] = value;
                    if (ImGui::IsItemDeactivatedAfterEdit()) { submit(index, value); edits_.erase(static_cast<int>(index)); }
                }
            } else if (type == "textinput") {
                ImGui::TextUnformatted(row.text.c_str());
                auto& buffer = buffers_[static_cast<int>(index)];
                const auto value = row.value.is_string() ? row.value.get<std::string>() : "";
                if (GImGui->ActiveId != ImGui::GetID("##value")) std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
                ImGui::SetNextItemWidth(-1);
                const bool enter = ImGui::InputText("##value", buffer.data(), buffer.size(), ImGuiInputTextFlags_EnterReturnsTrue);
                if (enter || ImGui::IsItemDeactivatedAfterEdit()) submit(index, std::string(buffer.data()));
            } else if (type == "hotkey" || type == "keyinput") {
                ImGui::TextUnformatted(row.text.c_str());
                if (w::Button(capture_ == static_cast<int>(index) ? "Press a key... (Esc cancels)" : KeyCaption(row.value).c_str())) {
                    capture_ = static_cast<int>(index); captureArmed_ = false;
                }
                ImGui::SameLine();
                if (w::Button("Clear")) { capture_ = -1; submit(index, type == "hotkey" ? Json::array({0, 0}) : Json("0,0")); }
                if (capture_ == static_cast<int>(index)) {
                    ImGui::SetKeyOwner(ImGuiKey_Escape, ImGuiKeyOwner_Any);
                    ImGui::SetKeyOwner(ImGuiKey_GamepadFaceRight, ImGuiKeyOwner_Any);
                    if (!ImGui::IsMouseDown(0) && !ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown)) captureArmed_ = true;
                    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) capture_ = -1;
                    else if (captureArmed_) {
                        int code{};
                        for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
                            if (key >= ImGuiKey_LeftCtrl && key <= ImGuiKey_RightSuper) continue;
                            if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(key), false)) { code = ScanCode(static_cast<ImGuiKey>(key)); if (code) break; }
                        }
                        for (int mouse = 0; mouse < 5; ++mouse) if (ImGui::IsMouseClicked(mouse)) code = 256 + mouse;
                        static constexpr ImGuiKey padKeys[]{ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown,
                            ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadStart, ImGuiKey_GamepadBack,
                            ImGuiKey_GamepadL3, ImGuiKey_GamepadR3, ImGuiKey_GamepadL1, ImGuiKey_GamepadR1,
                            ImGuiKey_GamepadFaceDown, ImGuiKey_GamepadFaceRight, ImGuiKey_GamepadFaceLeft,
                            ImGuiKey_GamepadFaceUp, ImGuiKey_GamepadL2, ImGuiKey_GamepadR2};
                        for (std::size_t i = 0; i < std::size(padKeys); ++i)
                            if (ImGui::IsKeyPressed(padKeys[i], false)) code = 266 + static_cast<int>(i);
                        if (code) {
                            const auto& io = ImGui::GetIO();
                            const int modifiers = (io.KeyShift ? 1 : 0) | (io.KeyCtrl ? 2 : 0) | (io.KeyAlt ? 4 : 0);
                            submit(index, type == "hotkey" ? Json::array({code, modifiers}) : Json(std::format("{},{}", code, modifiers)));
                            capture_ = -1;
                        }
                    }
                }
            } else if (type == "positioner" && row.value.is_object()) {
                w::Heading(row.text.c_str());
                auto result = edits_.contains(static_cast<int>(index)) ? edits_[static_cast<int>(index)] : row.value;
                for (const auto& [name, saved] : result.items()) {
                    float value = static_cast<float>(Number(saved));
                    if (ImGui::InputFloat(name.c_str(), &value)) { result[name] = value; edits_[static_cast<int>(index)] = result; }
                    if (ImGui::IsItemDeactivatedAfterEdit()) { submit(index, result); edits_.erase(static_cast<int>(index)); }
                }
            } else {
                ImGui::TextWrapped("%s", row.text.c_str());
                if (row.error.empty()) ImGui::TextDisabled("Unsupported MCM control: %s", type.c_str());
            }
            ImGui::EndDisabled();
            if (!row.help.empty()) w::Tooltip(row.help.c_str());
            if (!row.error.empty()) ImGui::TextWrapped("%s", row.error.c_str());
            if (!row.status.empty()) ImGui::TextWrapped("%s", row.status.c_str());
            if (type != "spacer") ImGui::Dummy({0, theme::Px(12)});
            ImGui::PopID();
        }
        ImGui::EndChild();
    }
}
