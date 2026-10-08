#include "demo/DemoClient.h"
#include "b21ui/Kit.h"

#include <imgui.h>

#include <array>
#include <format>

namespace b21ui::demo {
    void DemoClient::Log(std::string line) {
        log_.push_back(std::move(line));
        while (log_.size() > 30) log_.pop_front();
    }

    void DemoClient::Draw(const FrameContext& f) {
        using namespace kit;
        const ImVec2 size{f.width * 0.45F, f.height * 0.7F};
        if (BeginPanel(title_.c_str(), {f.width * 0.08F, f.height * 0.1F}, size)) {
            Heading(title_.c_str());
            ImGui::Text("Device: %s  Cursor %.0f, %.0f  %.0f FPS", f.device == Device::Gamepad ? "Gamepad" : "Keyboard/Mouse",
                        f.cursorX, f.cursorY, ImGui::GetIO().Framerate);
            ImGui::InputText("Search", text_, sizeof(text_));
            SliderFloat("Scale", &slider_, 0.0F, 1.0F);
            static constexpr std::array items{"Commonwealth", "Far Harbor", "Nuka-World", "Appalachia"};
            for (int i = 0; i < static_cast<int>(items.size()); ++i)
                if (ImGui::Selectable(items[i], selected_ == i)) { selected_ = i; Log(std::format("selected {}", items[i])); }
            if (Button("Travel", {}, true, B21UI_PAD_X)) Log("Travel pressed");
            ImGui::SameLine();
            const bool closeClicked = Button("Close", {}, true, B21UI_PAD_B);
            ImGui::SameLine();
            Button("Disabled", {}, false, B21UI_PAD_Y);
            if (closeClicked) b21ui::Close(*this);
            ImGui::Separator();
            ImGui::BeginChild("log", {0, 0});
            for (const auto& line : log_) ImGui::TextUnformatted(line.c_str());
            ImGui::SetScrollHereY(1.0F);
            ImGui::EndChild();
        }
        EndPanel();
        // b21ui::Close marshals to the main thread, so calling it from the render pass is safe.
        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight, false)) b21ui::Close(*this);
        static constexpr std::array prompts{Prompt{B21UI_PAD_A, "Enter", "Select"}, Prompt{B21UI_PAD_B, "Esc", "Back"}};
        PromptBar(prompts, static_cast<std::uint32_t>(f.device), {f.width - 24.0F * TextScale(), f.height - 24.0F * TextScale()});
    }
}
