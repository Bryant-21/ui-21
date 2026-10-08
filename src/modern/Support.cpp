#include "b21ui/modern/Support.h"

#include "b21ui/modern/Theme.h"
#include "b21ui/modern/Widgets.h"

#include <imgui_internal.h>
#include <Windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <algorithm>
#include <format>
#include <string>
#include <thread>

namespace b21ui::modern {
    namespace {
        // The links belong to the consumer, not the framework: a mod that shows the page puts
        // ui21_support_links.inc on its include path, one `SupportLink{...},` per line.
#if __has_include("ui21_support_links.inc")
        constexpr SupportLink kLinks[]{
#include "ui21_support_links.inc"
        };
        constexpr std::span<const SupportLink> kLinkList{kLinks};
#else
        constexpr std::span<const SupportLink> kLinkList{};
#endif
    }

    std::span<const SupportLink> SupportLinks() { return kLinkList; }

    void OpenUrl(const char* url) {
        std::thread([target = std::string(url)] {
            const std::wstring wide(target.begin(), target.end());
            const bool com = SUCCEEDED(::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
            ::ShellExecuteW(nullptr, L"open", wide.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            if (com) ::CoUninitialize();
        }).detach();
    }

    void SupportPage() {
        const auto& fonts = theme::CurrentFonts();
        w::Title("Support");
        w::Wrapped("I do this for free, but you can support me below.", theme::Muted);
        ImGui::Dummy({0, theme::Px(10)});

        const float cardWidth = std::min(theme::Px(420), ImGui::GetContentRegionAvail().x);
        const float cardHeight = theme::Px(88);
        for (const auto& link : kLinkList) {
            ImGui::PushID(link.name);
            const auto pos = ImGui::GetCursorScreenPos();
            const bool open = ImGui::InvisibleButton("##card", {cardWidth, cardHeight}, ImGuiButtonFlags_EnableNav);
            const bool hot = ImGui::IsItemHovered() || (ImGui::IsItemFocused() && GImGui->NavCursorVisible);
            if (hot) w::Tooltip(std::format("Open {} in your browser", link.url).c_str());
            auto* list = ImGui::GetWindowDrawList();
            const ImVec2 max{pos.x + cardWidth, pos.y + cardHeight};
            list->AddRectFilled(pos, max, hot ? theme::AccentSoft : theme::Surface, theme::Px(10));
            list->AddRect(pos, max, hot ? theme::Accent : theme::Border, theme::Px(10));

            const float logo = theme::Px(34);
            list->AddText(fonts.body, logo, {pos.x + theme::Px(22), pos.y + (cardHeight - logo) / 2}, link.color, link.glyph);
            const float textX = pos.x + theme::Px(80);
            list->AddText(fonts.bold, theme::Px(theme::HeadingSize), {textX, pos.y + theme::Px(18)}, theme::Text, link.name);
            list->PushClipRect(pos, {max.x - theme::Px(14), max.y}, true);
            list->AddText(fonts.mono, theme::Px(theme::SmallSize), {textX, pos.y + theme::Px(50)}, theme::Muted, link.url);
            list->PopClipRect();
            if (open) OpenUrl(link.url);

            ImGui::SameLine(0, theme::Px(8));
            ImGui::SetCursorScreenPos({max.x + theme::Px(8), pos.y + (cardHeight - ImGui::GetFrameHeight()) / 2});
            if (w::Icon("copy", icon::Copy, "Copy link")) ImGui::SetClipboardText(link.url);
            ImGui::SetCursorScreenPos({pos.x, max.y + theme::Px(12)});
            ImGui::Dummy({0, 0});
            ImGui::PopID();
        }
        ImGui::Dummy({0, theme::Px(6)});
        w::Muted("Links open in your web browser; the game keeps running.");
    }
}
