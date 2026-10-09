#include "client/KeybindingsView.h"
#include "b21ui/modern/Widgets.h"
#include <algorithm>
#include <cctype>
#include <format>
#include <set>

namespace b21ui::keys {
    namespace {
        namespace t = modern::theme;
        namespace w = modern::w;
        std::string Lower(std::string text) {
            std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return text;
        }
        std::string Scope(const Binding& b) {
            std::string result;
            for (const auto& c : b.contexts) { if (!result.empty()) result += ", "; result += c == "*" ? "All states" : c; }
            return result.empty() ? "State unknown" : result;
        }
    }
    void View::Navigation() {
        if (w::NavigationItem("##bindings", "Bindings", modern::icon::Keyboard, !coverage_)) coverage_ = false;
        if (w::NavigationItem("##coverage", "Discovery coverage", modern::icon::Info, coverage_)) coverage_ = true;
    }
    void View::Key(const Snapshot& snapshot, const std::vector<std::size_t>& visible, int code,
                   float x, float y, float width, float height) {
        std::vector<std::size_t> rows;
        auto level = Collision::None;
        for (const auto row : visible) if (snapshot.bindings[row].key == code) {
            rows.push_back(row); level = std::max(level, levels_[row]);
        }
        const auto border = level == Collision::Overlap ? t::Error : level == Collision::Possible ? t::Warn :
            rows.empty() ? t::Border : t::Accent;
        const auto fill = level == Collision::Overlap ? t::ErrorFill : level == Collision::Possible ? t::WarnFill :
            rows.empty() ? t::Background : t::AccentSoft;
        ImGui::SetCursorPos({x, y});
        ImGui::PushID(code);
        const bool clicked = ImGui::Button("##key", {width, height});
        if (clicked) selectedKey_ = selectedKey_ == code ? -1 : code;
        const auto a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(a, b, fill, t::Px(3));
        draw->AddRect(a, b, selectedKey_ == code || ImGui::IsItemHovered() || ImGui::IsItemFocused() ? t::Text : border,
            t::Px(3), 0, selectedKey_ == code ? 2.0F : 1.0F);
        const auto label = KeyName(code);
        const float fontSize = std::min(t::Px(14), height * 0.36F);
        ImGui::PushFont(t::CurrentFonts().body, fontSize);
        const auto textSize = ImGui::CalcTextSize(label.c_str());
        draw->PushClipRect(a, b, true);
        draw->AddText({a.x + std::max(3.0F, (width - textSize.x) / 2), a.y + (height - textSize.y) / 2},
            rows.empty() ? t::Faint : t::Text, label.c_str());
        if (rows.size() > 1) draw->AddCircleFilled({b.x - 5, a.y + 5}, 2.5F, border);
        draw->PopClipRect();
        ImGui::PopFont();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip();
            ImGui::TextUnformatted(label.c_str());
            if (rows.empty()) ImGui::TextDisabled("No discovered binding in this filter");
            for (const auto row : rows) {
                const auto& item = snapshot.bindings[row];
                ImGui::Text("%s  |  %s", item.label.c_str(), item.source.c_str());
                ImGui::TextDisabled("%s  /  %s  /  %s", ChordName(item).c_str(), item.trigger.c_str(), Scope(item).c_str());
            }
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
    void View::Map(const Snapshot& snapshot, const std::vector<std::size_t>& visible) {
        const float width = ImGui::GetContentRegionAvail().x;
        const float unit = std::min(t::Px(44), (width - 12) / 28.0F);
        const float height = gamepad_ ? std::clamp(ImGui::GetContentRegionAvail().y-t::Px(42),t::Px(270),t::Px(430)) : unit * 6.6F;
        diagramClicked_=false;
        ImGui::PushStyleColor(ImGuiCol_ChildBg, t::Surface);
        ImGui::BeginChild("inputMap", {0, height + t::Px(20)}, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();
        const float ox = std::max(6.0F, (width - unit * 27.6F) / 2), oy = t::Px(10);
        const auto key = [&](int code, float x, float y, float w = 1.0F) {
            Key(snapshot, visible, code, ox + x * unit, oy + y * unit, w * unit - 3, unit - 3);
        };
        if (!gamepad_) {
            key(1, 0, 0);
            for (int i = 0; i < 12; ++i) key(i < 10 ? 59 + i : 87 + i - 10, 2.0F + i + (i / 4) * 0.3F, 0);
            key(183, 15.5F, 0); key(70, 16.5F, 0); key(197, 17.5F, 0);
            struct KeyWidth { int code; float width; };
            const auto row = [&](std::initializer_list<KeyWidth> codes, float y) {
                float x = 0;
                for (const auto& [code, w] : codes) { key(code, x, y, w); x += w; }
            };
            row({{41,1},{2,1},{3,1},{4,1},{5,1},{6,1},{7,1},{8,1},{9,1},{10,1},{11,1},{12,1},{13,1},{14,2}}, 1.3F);
            row({{15,1.5F},{16,1},{17,1},{18,1},{19,1},{20,1},{21,1},{22,1},{23,1},{24,1},{25,1},{26,1},{27,1},{43,1.5F}}, 2.3F);
            row({{58,1.75F},{30,1},{31,1},{32,1},{33,1},{34,1},{35,1},{36,1},{37,1},{38,1},{39,1},{40,1},{28,2.25F}}, 3.3F);
            row({{42,2.25F},{44,1},{45,1},{46,1},{47,1},{48,1},{49,1},{50,1},{51,1},{52,1},{53,1},{54,2.75F}}, 4.3F);
            row({{29,1.25F},{219,1.25F},{56,1.25F},{57,6.25F},{184,1.25F},{220,1.25F},{221,1.25F},{157,1.25F}}, 5.3F);
            const int nav[]{210,199,201,211,207,209};
            for (int i = 0; i < 6; ++i) key(nav[i], 15.5F + i % 3, 1.3F + i / 3);
            key(200,16.5F,4.3F); key(203,15.5F,5.3F); key(208,16.5F,5.3F); key(205,17.5F,5.3F);
            key(69,19,1.3F); key(181,20,1.3F); key(55,21,1.3F); key(74,22,1.3F);
            const int num[]{71,72,73,75,76,77,79,80,81};
            for (int i = 0; i < 9; ++i) key(num[i],19.0F + i % 3,2.3F + i / 3);
            key(82,19,5.3F,2); key(83,21,5.3F);
            Key(snapshot,visible,78,ox+22*unit,oy+2.3F*unit,unit-3,unit*2-3);
            Key(snapshot,visible,156,ox+22*unit,oy+4.3F*unit,unit-3,unit*2-3);
            Mouse(snapshot,visible,{ox+23.5F*unit,oy+0.2F*unit},unit*0.027F);
            key(261,23.5F,5.3F,1.3F); key(262,24.8F,5.3F,1.3F); key(263,26.1F,5.3F,1.3F);
        } else {
            Controller(snapshot,visible,width,height);
        }
        ImGui::EndChild();
        if (gamepad_ && diagramClicked_) ImGui::OpenPopup("padDetails");
        if (ImGui::BeginPopup("padDetails")) {
            ImGui::TextUnformatted(KeyName(selectedKey_).c_str());
            bool found{};
            for (const auto i : visible) {
                const auto& binding=snapshot.bindings[i]; if (binding.key!=selectedKey_) continue;
                found=true; ImGui::Separator();
                ImGui::Text("%s / %s",binding.label.c_str(),binding.source.c_str());
                ImGui::TextDisabled("%s / %s",Scope(binding).c_str(),binding.trigger.c_str());
                if (!binding.conditions.empty()) {
                    ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+t::Px(430));
                    ImGui::TextUnformatted(binding.conditions.c_str()); ImGui::PopTextWrapPos();
                }
                for (const auto& conflict : snapshot.conflicts) if (conflict.first==i || conflict.second==i) {
                    const auto& peer=snapshot.bindings[conflict.first==i ? conflict.second : conflict.first];
                    if (InContext(peer,context_)) ImGui::TextColored(t::Vec(conflict.kind==Collision::Overlap ? t::Error : t::Warn),
                        "%s: %s / %s (%s)",conflict.kind==Collision::Overlap ? "Overlap" : "Possible",peer.label.c_str(),peer.source.c_str(),peer.trigger.c_str());
                }
            }
            if (!found) ImGui::TextDisabled("No discovered binding in this filter");
            ImGui::EndPopup();
        }
    }
    void View::Coverage(const Snapshot& snapshot) {
        w::Title("Discovery coverage");
        ImGui::TextWrapped("Fallout 4 control tables, MCM keybind definitions and native providers are discoverable. F4SE has no public registry of private native hotkeys. Plugins below may have additional bindings.");
        ImGui::Spacing();
        for (const auto& source : snapshot.sources) {
            ImGui::Text("%s  (%zu bindings)", source.label.c_str(), source.count);
            ImGui::TextDisabled("%s", source.status.c_str());
        }
        ImGui::Spacing(); w::Heading("Native plugins without published bindings");
        if (snapshot.undisclosed.empty()) ImGui::TextDisabled("None found. Papyrus/private handlers can still be undisclosed.");
        for (const auto& module : snapshot.undisclosed) ImGui::BulletText("%s", module.c_str());
        ImGui::Spacing();
        ImGui::TextWrapped("State filters describe declared input contexts, not a simulation of every menu priority or mod condition. Pip-Boy includes shared menu navigation. Activation distinguishes press, tap, hold and release where known. Unknown handlers are not assumed to use press. Overlap means the declared key, modifiers, state and activation coincide; possible collisions need their conditions checked. No bindings are changed.");
    }
    bool View::Draw(const Snapshot& snapshot) {
        ImGui::SetCursorPos({t::Px(24),t::Px(20)});
        ImGui::BeginChild("keybindingsContent", { -t::Px(24), -t::Px(16) });
        if (coverage_) { Coverage(snapshot); ImGui::EndChild(); return false; }
        w::Title("Keybindings");
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth() - t::Px(115));
        const bool refresh = ImGui::Button("Refresh", {t::Px(100),0});
        if (ImGui::RadioButton("Keyboard / mouse", !gamepad_)) { gamepad_=false; selectedKey_=-1; }
        ImGui::SameLine();
        if (ImGui::RadioButton("Xbox controller", gamepad_)) { gamepad_=true; showMap_=true; selectedKey_=-1; }
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetWindowWidth()-t::Px(145));
        ImGui::Checkbox("Show input map",&showMap_);
        std::vector<std::string> contexts{"Gameplay", "Pip-Boy"}, sources;
        for (const auto& b : snapshot.bindings) {
            for (const auto& c : b.contexts) if (c != "*" && std::ranges::find(contexts,c)==contexts.end()) contexts.push_back(c);
            if (std::ranges::find(sources,b.source)==sources.end()) sources.push_back(b.source);
        }
        std::ranges::sort(contexts); std::ranges::sort(sources);
        const auto combo = [](const char* id, std::string& selected, const std::vector<std::string>& choices, const char* all, float width) {
            ImGui::SetNextItemWidth(width);
            if (ImGui::BeginCombo(id, selected.empty() ? all : selected.c_str())) {
                if (ImGui::Selectable(all, selected.empty())) selected.clear();
                for (const auto& choice : choices) if (ImGui::Selectable(choice.c_str(), selected==choice)) selected=choice;
                ImGui::EndCombo();
            }
        };
        const float fieldWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3;
        combo("##state",context_,contexts,"All states",fieldWidth); ImGui::SameLine();
        combo("##source",source_,sources,"All sources",fieldWidth); ImGui::SameLine();
        ImGui::SetNextItemWidth(fieldWidth); ImGui::InputTextWithHint("##search","Search actions or keys",search_.data(),search_.size());
        ImGui::Checkbox("Conflicts only", &conflictsOnly_); ImGui::SameLine();
        ImGui::TextColored(t::Vec(t::Error), "Overlap"); ImGui::SameLine();
        ImGui::TextColored(t::Vec(t::Warn), "Possible"); ImGui::SameLine();
        ImGui::TextDisabled("Click a key to filter");
        std::vector<std::size_t> visible;
        levels_.assign(snapshot.bindings.size(),Collision::None);
        for (const auto& conflict : snapshot.conflicts) {
            if (!InContext(snapshot.bindings[conflict.first],context_) || !InContext(snapshot.bindings[conflict.second],context_)) continue;
            levels_[conflict.first]=std::max(levels_[conflict.first],conflict.kind);
            levels_[conflict.second]=std::max(levels_[conflict.second],conflict.kind);
        }
        const auto query = Lower(search_.data());
        for (std::size_t i=0; i<snapshot.bindings.size(); ++i) {
            const auto& b=snapshot.bindings[i];
            if ((b.key>=266)!=gamepad_ || !InContext(b,context_) || (!source_.empty() && source_!=b.source)) continue;
            if (conflictsOnly_ && levels_[i]==Collision::None) continue;
            if (!query.empty() && Lower(b.label+" "+b.source+" "+ChordName(b)+" "+b.trigger+" "+Scope(b)).find(query)==std::string::npos) continue;
            visible.push_back(i);
        }
        std::ranges::stable_sort(visible,[&](auto a,auto b) {
            if (levels_[a]!=levels_[b]) return levels_[a]>levels_[b];
            const auto& first=snapshot.bindings[a]; const auto& second=snapshot.bindings[b];
            if (first.key!=second.key) return first.key<second.key;
            return first.label<second.label;
        });
        if (showMap_) Map(snapshot, visible);
        std::size_t count{};
        std::set<std::string_view> visibleSources;
        for (const auto i : visible) if (selectedKey_<0 || selectedKey_==snapshot.bindings[i].key) {
            ++count; visibleSources.insert(snapshot.bindings[i].source);
        }
        ImGui::TextDisabled("%zu bindings  /  %zu sources",count,visibleSources.size());
        if (selectedKey_>=0) { ImGui::SameLine(); if (ImGui::SmallButton(("Clear " + KeyName(selectedKey_)+" filter").c_str())) selectedKey_=-1; }
        if (showMap_ && gamepad_ && ImGui::GetContentRegionAvail().y<t::Px(130)) { ImGui::EndChild(); return refresh; }
        if (ImGui::BeginTable("bindings",5,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerH|ImGuiTableFlags_ScrollY|ImGuiTableFlags_Resizable, {0, std::max(t::Px(120),ImGui::GetContentRegionAvail().y)})) {
            ImGui::TableSetupColumn("Action",ImGuiTableColumnFlags_WidthStretch,2.2F);
            ImGui::TableSetupColumn("Binding",ImGuiTableColumnFlags_WidthStretch,1.1F);
            ImGui::TableSetupColumn("Activation",ImGuiTableColumnFlags_WidthStretch,1.8F);
            ImGui::TableSetupColumn("Source / State",ImGuiTableColumnFlags_WidthStretch,2.4F);
            ImGui::TableSetupColumn("Conflicts",ImGuiTableColumnFlags_WidthStretch,1.0F);
            ImGui::TableSetupScrollFreeze(0,1); ImGui::TableHeadersRow();
            for (const auto i : visible) {
                const auto& b=snapshot.bindings[i]; if (selectedKey_>=0 && selectedKey_!=b.key) continue;
                ImGui::PushID(static_cast<int>(i)); ImGui::TableNextRow(); ImGui::TableNextColumn();
                if (ImGui::Selectable(b.label.c_str())) ImGui::OpenPopup("details");
                if (ImGui::BeginPopup("details")) {
                    ImGui::TextUnformatted(b.label.c_str()); ImGui::TextDisabled("%s / %s", b.source.c_str(), Scope(b).c_str());
                    ImGui::Text("%s (%s)",ChordName(b).c_str(),b.trigger.c_str());
                    if (!b.conditions.empty()) { ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+t::Px(480)); ImGui::TextUnformatted(b.conditions.c_str()); ImGui::PopTextWrapPos(); }
                    for (const auto& c : snapshot.conflicts) if (c.first==i || c.second==i) {
                        const auto& peer=snapshot.bindings[c.first==i ? c.second : c.first];
                        if (InContext(peer,context_)) ImGui::BulletText("%s: %s / %s (%s; %s)",c.kind==Collision::Overlap ? "Overlap" : "Possible",peer.label.c_str(),peer.source.c_str(),Scope(peer).c_str(),peer.trigger.c_str());
                    }
                    ImGui::EndPopup();
                }
                ImGui::TableNextColumn(); ImGui::TextUnformatted(ChordName(b).c_str());
                ImGui::TableNextColumn(); ImGui::TextWrapped("%s",b.trigger.c_str());
                ImGui::TableNextColumn(); ImGui::TextUnformatted(b.source.c_str()); ImGui::TextDisabled("%s", Scope(b).c_str());
                ImGui::TableNextColumn(); const auto level=levels_[i];
                ImGui::TextColored(t::Vec(level==Collision::Overlap ? t::Error : level==Collision::Possible ? t::Warn : t::Faint), "%s", level==Collision::Overlap ? "Overlap" : level==Collision::Possible ? "Possible" : "Clear");
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild(); return refresh;
    }
}
