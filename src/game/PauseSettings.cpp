#include <F4SE/F4SE.h>
#include <RE/Fallout.h>

#include "game/PauseSettings.h"
#include "b21ui/Settings.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace b21ui::game::PauseSettings {
namespace {

using Value = Scaleform::GFx::Value;

constexpr const char* kEntryText = "UI 21 Settings";
constexpr int kEntryIndex = -210077;

class EntryPress final : public Scaleform::GFx::FunctionHandler {
public:
    explicit EntryPress(Value list) : list_(std::move(list)) {}
    void Call(const Params& args) override
    {
        Value entry, tagged;
        if (list_.GetMember("selectedEntry", &entry) && entry.IsObject() && entry.GetMember("b21UISettings", &tagged) &&
            tagged.IsBoolean() && tagged.GetBoolean()) {
            if (args.argCount && args.args[0].IsObject()) args.args[0].Invoke("stopImmediatePropagation");
            OpenSettings();
        }
    }

private:
    Value list_;
};

bool FindMainMenu(RE::IMenu& menu, Scaleform::GFx::Movie& movie, Value& main)
{
    Value panel;
    if (menu.menuObj.IsObject() && menu.menuObj.GetMember("MainPanel_mc", &panel) && panel.IsObject()) {
        main = menu.menuObj;
        return true;
    }
    for (const auto* path : {"root.Menu_mc", "root1.Menu_mc", "root", "root1"})
        if (movie.GetVariable(&main, path) && main.IsObject() && main.GetMember("MainPanel_mc", &panel) && panel.IsObject())
            return true;
    return false;
}

bool Flagged(const Value& row, const char* flag)
{
    Value value;
    return row.IsObject() && row.GetMember(flag, &value) && value.IsBoolean() && value.GetBoolean();
}

std::optional<double> Number(const Value& value)
{
    if (value.IsNumber()) return value.GetNumber();
    if (value.IsInt()) return value.GetInt();
    if (value.IsUInt()) return value.GetUInt();
    return std::nullopt;
}

bool TextIs(const Value& row, std::string_view text)
{
    Value value;
    return row.IsObject() && row.GetMember("text", &value) && value.IsString() && value.GetString() == text;
}

// The pause list is authored with 9 row clips and a border that fits 9 rows; vanilla AE fills 8 and
// PHOTO MODE the ninth, so UI 21 Settings would only be reachable by scrolling. SetNumListItems is
// protected, so the extra clips are built the way it builds them, wired to the list's public handlers.
const char* GrowList(Scaleform::GFx::Movie& movie, Value& list, std::uint32_t rows)
{
    Value clips, first, holder, platform, rollover, press;
    const auto current = list.GetMember("numListItems", &clips) ? Number(clips) : std::nullopt;
    if (!current) return "list has no numListItems";
    if (rows <= *current) return nullptr;
    const std::array firstIndex{Value(0u)};
    if (!list.Invoke("GetClipByIndex", &first, firstIndex) || !first.IsDisplayObject() ||
        !first.GetMember("parent", &holder) || !holder.IsDisplayObject() ||
        !list.GetMember("onEntryRollover", &rollover) || !list.GetMember("onEntryPress", &press))
        return "list row clips are not reachable";
    first.GetMember("iPlatform", &platform);
    for (auto index = static_cast<std::uint32_t>(*current); index < rows; ++index) {
        Value clip;
        movie.CreateObject(&clip, "MainMenuListEntry");
        if (!clip.IsDisplayObject()) return "MainMenuListEntry could not be created";
        if (!platform.IsUndefined()) clip.SetMember("iPlatform", platform);
        clip.SetMember("clipIndex", Value(index));
        const std::array over{Value("mouseOver"), rollover};
        const std::array click{Value("click"), press};
        const std::array child{clip};
        if (!clip.Invoke("addEventListener", over) || !clip.Invoke("addEventListener", click) ||
            !holder.Invoke("addChild", child))
            return "new row clip could not be attached";
    }
    list.SetMember("numListItems", Value(rows));

    // UpdateList stops adding rows once they exceed border.height; size it the way
    // CalculateMaxScrollPosition measures rows (the first clip's defaultHeight or height).
    Value border, height, defaultHeight, spacing, borderHeight;
    first.GetMember("height", &height);
    first.GetMember("defaultHeight", &defaultHeight);
    list.GetMember("verticalSpacing", &spacing);
    const auto row = std::max(Number(height).value_or(0), Number(defaultHeight).value_or(0));
    if (row <= 0 || !list.GetMember("border", &border) || !border.IsDisplayObject() ||
        !border.GetMember("height", &borderHeight))
        return "list border could not be measured";
    const auto needed = rows * row + (rows - 1) * Number(spacing).value_or(0) + 1;
    if (Number(borderHeight).value_or(0) < needed) border.SetMember("height", Value(needed));
    return nullptr;
}

// Per pause-menu movie; reset when the menu closes because a new movie can reuse the address.
Scaleform::GFx::Movie* listenedMovie{};
bool listening{};
bool announced{};
std::uint32_t failedFrames{};

class PauseClose final : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
public:
    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (!event.opening && event.menuName == "PauseMenu") {
            listenedMovie = nullptr;
            announced = false;
            failedFrames = 0;
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};
PauseClose pauseClose;

// Returns why the row is not in the list, or nullptr once it is. Only PauseMenu's vtable is hooked, so
// no PauseMode check is needed (and MainMenu.PauseMode and SETTINGS_INDEX are private, which native
// GetMember cannot read).
const char* EnsureEntry(RE::IMenu& menu)
{
    if (SettingsPanels().empty()) return nullptr;
    auto* movie = menu.uiMovie.get();
    if (!movie) return "pause menu has no movie";
    Value main, panel, list, entries;
    if (!FindMainMenu(menu, *movie, main)) return "MainMenu object not found";
    if (!main.GetMember("MainPanel_mc", &panel) || !panel.GetMember("List_mc", &list) || !list.IsObject())
        return "MainPanel_mc.List_mc not found";
    if (!list.GetMember("entryList", &entries) || !entries.IsArray()) return "entryList is not an array";
    auto count = entries.GetArraySize();
    if (count == 0) return "entryList is empty";

    if (listenedMovie != movie) {
        listenedMovie = movie;
        Value callback;
        Scaleform::Ptr<EntryPress> handler{new EntryPress(list)};
        movie->CreateFunction(&callback, handler.get());
        const std::array args{Value("BSScrollingList::itemPress"), callback, Value(false), Value(1000)};
        listening = list.Invoke("addEventListener", args);
    }
    if (!listening) return "list refused the press listener";

    std::uint32_t position = count;
    for (std::uint32_t i = 0; i < count; ++i) {
        Value row;
        if (!entries.GetElement(i, &row)) continue;
        if (Flagged(row, "b21UISettings")) return nullptr;
        if (Flagged(row, "b21PhotoMode") || TextIs(row, "$SETTINGS")) position = i + 1;
    }

    Value row;
    movie->CreateObject(&row);
    row.SetMember("text", Value(kEntryText));
    row.SetMember("index", Value(kEntryIndex));
    row.SetMember("disabled", Value(false));
    row.SetMember("b21UISettings", Value(true));
    const std::array splice{Value(position), Value(0), row};
    if (!entries.Invoke("splice", splice)) return "entryList.splice failed";
    const auto* notGrown = GrowList(*movie, list, count + 1);
    if (!list.Invoke("InvalidateData")) return "InvalidateData failed";
    if (!announced) {
        announced = true;
        spdlog::info("UI21: UI 21 Settings is row {} of {}{}{}", position + 1, count + 1,
            notGrown ? "; list not grown, row needs scrolling: " : "", notGrown ? notGrown : "");
    }
    return nullptr;
}

REL::Relocation<void (*)(RE::IMenu*, float, std::uint64_t)> advancePause;

void AdvancePause(RE::IMenu* menu, float delta, std::uint64_t time)
{
    advancePause(menu, delta, time);
    if (!menu) return;
    // The first frames after opening have no list yet, so only a failure that persists is reported.
    if (const auto* failure = EnsureEntry(*menu); !failure) failedFrames = 0;
    else if (++failedFrames == 120) spdlog::warn("UI21: no UI 21 Settings row in the pause menu: {}", failure);
}

REL::Relocation<void (*)(RE::IMenu*)> initPauseList;

// StartMenuBase::InitMainList rebuilds the list through the movie's InitList; adding the row right
// after it means no native rebuild reaches the screen without it. AS3 also rebuilds on its own when
// backing out of a panel, which the per-frame check covers.
void InitPauseList(RE::IMenu* menu)
{
    initPauseList(menu);
    if (menu) EnsureEntry(*menu);
}

}

void Install()
{
    if (auto* ui = RE::UI::GetSingleton()) ui->GetEventSource<RE::MenuOpenCloseEvent>()->RegisterSink(&pauseClose);
    REL::Relocation<std::uintptr_t> table{RE::VTABLE::PauseMenu[0]};
    advancePause = table.write_vfunc(0x04, AdvancePause);
    initPauseList = table.write_vfunc(0x15, InitPauseList);
    spdlog::info("UI21: shared settings pause-menu hooks installed");
}
}
