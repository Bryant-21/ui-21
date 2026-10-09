#include <F4SE/F4SE.h>
#include <RE/Fallout.h>

#include "game/McmRuntime.h"
#include "game/Host.h"
#include "b21ui/B21UI.h"
#include "b21ui/Tasks.h"
#include "b21ui/modern/Widgets.h"
#include "client/McmView.h"
#include "core/Mcm.h"

#include <Windows.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>

namespace b21ui::game::McmRuntime {
    namespace {
        using namespace mcm;
        SettingsStore settings;
        Keybinds keys;
        bool enabled{};
        std::atomic<std::uint64_t> revision{};
        std::map<std::string, std::string> translations;
        Session session;
        std::string activeMenu;

        bool OriginalInstalled() {
            return !ReplacementEnabled("Data/F4SE/Plugins", ::GetModuleHandleW(L"mcm.dll") || ::GetModuleHandleW(L"f4mcm.dll"));
        }
        RE::BSScript::IVirtualMachine* VM() {
            auto* game = RE::GameVM::GetSingleton();
            return game ? game->GetVM().get() : nullptr;
        }
        RE::TESForm* Form(std::string_view identifier) {
            const auto bar = identifier.find('|');
            if (bar == identifier.npos) return nullptr;
            try {
                auto* data = RE::TESDataHandler::GetSingleton();
                return data ? data->LookupForm(static_cast<std::uint32_t>(std::stoul(std::string(identifier.substr(bar + 1)), nullptr, 16)), identifier.substr(0, bar)) : nullptr;
            } catch (...) { return nullptr; }
        }
        RE::BSTSmartPointer<RE::BSScript::Object> Script(const Json& source) {
            RE::BSTSmartPointer<RE::BSScript::Object> object;
            auto* vm = VM();
            auto* form = Form(source.value("sourceForm", source.value("form", "")));
            if (!vm || !form) return object;
            auto& policy = vm->GetObjectHandlePolicy();
            const auto handle = policy.GetHandleForObject(static_cast<std::uint32_t>(form->GetFormType()), form);
            const auto script = source.value("scriptName", "");
            if (!script.empty()) vm->FindBoundObject(handle, script.c_str(), false, object, false);
            else vm->ForEachBoundObject(handle, [&](RE::BSScript::Object* candidate) {
                if (source.contains("propertyName") && !candidate->GetProperty(source["propertyName"].get<std::string>().c_str()))
                    return RE::BSContainer::ForEachResult::kContinue;
                object = RE::BSTSmartPointer<RE::BSScript::Object>(candidate);
                return RE::BSContainer::ForEachResult::kStop;
            });
            return object;
        }
        Json Unpack(const RE::BSScript::Variable& variable) {
            using namespace RE::BSScript;
            if (variable.is<bool>()) return get<bool>(variable);
            if (variable.is<std::int32_t>()) return get<std::int32_t>(variable);
            if (variable.is<float>()) return get<float>(variable);
            if (variable.is<RE::BSFixedString>()) return std::string(get<RE::BSFixedString>(variable).c_str());
            if (variable.is<Array>()) {
                Json array = Json::array();
                if (const auto values = get<Array>(variable)) for (const auto& value : *values) array.push_back(Unpack(value));
                return array;
            }
            if (variable.is<Object>()) {
                if (const auto object = get<Object>(variable)) {
                    if (const auto* form = static_cast<const RE::TESForm*>(object->Resolve(RE::BSScript::GetVMTypeID<RE::TESForm>())))
                        return std::string(RE::TESFullName::GetFullName(*form));
                }
            }
            return {};
        }
        RE::BSScript::Variable Pack(const Json& value) {
            RE::BSScript::Variable out;
            if (value.is_boolean()) out = value.get<bool>();
            else if (value.is_number_integer()) out = value.get<std::int32_t>();
            else if (value.is_number()) out = value.get<float>();
            else if (value.is_string()) out = RE::BSFixedString(value.get<std::string>().c_str());
            return out;
        }
        auto Arguments(const Json& params) {
            std::vector<RE::BSScript::Variable> values;
            for (const auto& parameter : params) values.push_back(Pack(parameter));
            return [values = std::move(values)](RE::BSScrapArray<RE::BSScript::Variable>& out) {
                out.resize(static_cast<std::uint32_t>(values.size()));
                for (std::size_t i = 0; i < values.size(); ++i) out[static_cast<std::uint32_t>(i)] = values[i];
                return true;
            };
        }
        void Send(std::string_view name, const Json& args) {
            const auto* api = F4SE::GetPapyrusInterface();
            if (!api) return;
            api->GetExternalEventRegistrations(name, const_cast<Json*>(&args),
                [](std::uint64_t handle, const char* script, const char* callback, void* raw) {
                    if (auto* vm = VM()) vm->DispatchMethodCall(handle, script, callback, Arguments(*static_cast<Json*>(raw)), {});
                });
        }
        std::string Utf8(std::wstring_view text) {
            const int size = ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
            std::string result(size, '\0');
            ::WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
            return result;
        }
        void LoadTranslations() {
            translations.clear();
            const auto* setting = RE::GetINISetting("sLanguage:General");
            const std::string language(setting ? setting->GetString() : "en");
            std::vector<std::filesystem::path> files;
            std::error_code error;
            for (const auto& file : std::filesystem::directory_iterator("Data/Interface/Translations", error))
                if (file.path().extension() == ".txt") files.push_back(file.path());
            std::ranges::sort(files);
            for (const auto& suffix : {std::string("_en"), "_" + language}) {
                for (const auto& file : files) {
                    auto stem = file.stem().string();
                    std::ranges::transform(stem, stem.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    auto wanted = suffix;
                    std::ranges::transform(wanted, wanted.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (!stem.ends_with(wanted)) continue;
                    std::ifstream stream(file, std::ios::binary);
                    std::string bytes((std::istreambuf_iterator<char>(stream)), {}), text;
                    if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) {
                        std::wstring wide;
                        for (std::size_t i = 2; i + 1 < bytes.size(); i += 2)
                            wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(bytes[i]) | static_cast<unsigned char>(bytes[i + 1]) << 8));
                        text = Utf8(wide);
                    } else text = std::move(bytes);
                    std::istringstream lines(text);
                    std::string line;
                    while (std::getline(lines, line)) {
                        const auto tab = line.find('\t');
                        if (tab == line.npos) continue;
                        if (!line.empty() && line.back() == '\r') line.pop_back();
                        translations[line.substr(0, tab)] = line.substr(tab + 1);
                    }
                }
            }
        }

        class NativeBackend final : public Backend {
        public:
            bool Installed(std::string_view plugin) override {
                auto* data = RE::TESDataHandler::GetSingleton();
                const auto* file = data ? data->LookupModByName(plugin) : nullptr;
                return file && file->compileIndex != 0xFF;
            }
            std::string Text(std::string_view text) override {
                const auto found = translations.find(std::string(text));
                return found == translations.end() ? std::string(text) : found->second;
            }
            std::optional<Json> Read(const Json& source, std::string_view mod, std::string_view id) override {
                const auto type = source.value("sourceType", "");
                if (type.starts_with("ModSetting")) {
                    const auto value = settings.Get(mod, id);
                    return value.is_null() ? std::nullopt : std::optional<Json>(value);
                }
                if (type == "GlobalValue") {
                    if (auto* form = Form(source.value("sourceForm", "")); form)
                        if (auto* global = form->As<RE::TESGlobal>()) return global->value;
                }
                if (type.starts_with("PropertyValue")) {
                    if (const auto object = Script(source))
                        if (const auto* property = object->GetProperty(source.value("propertyName", "").c_str())) return Unpack(*property);
                }
                if (type == "Hotkey") {
                    if (const auto binding = keys.Find(mod, id)) return Json::array({binding->key, binding->modifiers});
                }
                return {};
            }
            bool Write(const Json& source, std::string_view mod, std::string_view id, const Json& input) override {
                const auto type = source.value("sourceType", "");
                auto value = input;
                if (type.ends_with("Bool")) value = input.is_boolean() ? input.get<bool>() : input.get<double>() != 0;
                else if (type.ends_with("Int")) value = input.is_boolean() ? input.get<bool>() ? 1 : 0 : static_cast<int>(input.get<double>());
                else if (type.ends_with("Float") || type == "GlobalValue") value = input.is_boolean() ? input.get<bool>() ? 1.0F : 0.0F : input.get<float>();
                if (type.starts_with("ModSetting")) return settings.Set(mod, id, value);
                if (type == "GlobalValue") {
                    if (auto* form = Form(source.value("sourceForm", "")); form)
                        if (auto* global = form->As<RE::TESGlobal>()) { global->value = value.get<float>(); return true; }
                }
                if (type.starts_with("PropertyValue")) {
                    if (const auto object = Script(source); object && VM())
                        return VM()->SetPropertyValue(object, source.value("propertyName", "").c_str(), Pack(value), {});
                }
                if (type == "Hotkey" && value.is_array() && value.size() == 2)
                    return keys.Set(mod, id, value[0].get<int>(), value[1].get<int>());
                return false;
            }
            bool Call(const Json& action, const Json& params) override {
                auto* vm = VM();
                if (!vm) return false;
                const auto type = action.value("type", "");
                if (type == "CallGlobalFunction")
                    return vm->DispatchStaticCall(action.value("script", "").c_str(), action.value("function", "").c_str(), Arguments(params), {});
                if (type == "CallFunction") {
                    if (auto object = Script(action)) return vm->DispatchMethodCall(object, action.value("function", "").c_str(), Arguments(params), {});
                }
                if (type == "RunConsoleCommand") { RE::Console::ExecuteCommand(action.value("command", "").c_str()); return true; }
                return false;
            }
            void Event(std::string_view name, const Json& args) override { Send(name, args); }
            Json List(const Json& source) override {
                Json values = source.value("options", Json::array());
                if (source.contains("listFromForm")) {
                    values = Json::array();
                    if (auto* form = Form(source["listFromForm"].get<std::string>()))
                        if (auto* list = form->As<RE::BGSListForm>()) list->ForEachForm([&](RE::TESForm* entry) {
                            values.push_back(std::string(RE::TESFullName::GetFullName(*entry))); return RE::BSContainer::ForEachResult::kContinue;
                        });
                } else if (source.contains("listFromProperty")) {
                    auto property = source["listFromProperty"]; property["sourceType"] = "PropertyValueString";
                    values = Read(property, "", "").value_or(Json::array());
                } else if (source.contains("path")) {
                    values = Json::array({"None"});
                    WIN32_FIND_DATAW found{};
                    const auto pattern = std::filesystem::path(source.value("path", "")) / source.value("mask", "*");
                    const auto handle = ::FindFirstFileW(pattern.c_str(), &found);
                    if (handle != INVALID_HANDLE_VALUE) {
                        do { if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) values.push_back(Utf8(found.cFileName)); }
                        while (::FindNextFileW(handle, &found));
                        ::FindClose(handle);
                    }
                }
                if (!values.is_array()) return Json::array();
                for (auto& value : values) if (value.is_string()) value = Text(value.get<std::string>());
                return values;
            }
            std::string Describe(const Json& d, std::string_view field, std::string_view fallback) override {
                const auto prefix = std::string(field);
                if (d.value(prefix + "FromFormName", false)) {
                    if (const auto* form = Form(fallback)) return std::string(RE::TESFullName::GetFullName(*form));
                }
                if (d.value(prefix + "FromFormDescription", false)) {
                    if (auto* form = Form(fallback)) {
                        if (auto* description = RE::fallout_cast<RE::TESDescription*>(form)) {
                            RE::BSString text; description->GetDescription(text, form); return text.c_str();
                        }
                    }
                }
                for (const auto& suffix : {"FromStringProperty", "FromStringArrayProperty"}) {
                    if (!d.contains(prefix + suffix)) continue;
                    auto source = d[prefix + suffix]; source["sourceType"] = "PropertyValueString";
                    auto value = Read(source, "", "").value_or(Json{});
                    if (value.is_array()) {
                        const auto index = source.value("index", 0u);
                        value = index < value.size() ? value[index] : Json{};
                    }
                    if (value.is_string()) return Text(value.get<std::string>());
                }
                return {};
            }
        } backend;

        class McmClient final : public Client {
        public:
            explicit McmClient(Menu menu) : menu_(std::move(menu)), registration_("MCM:" + menu_.mod) {}
            void Register() {
                b21ui::Register(*this, {.name = registration_.c_str(), .pausesGame = true, .settings = true,
                    .settingsLabel = menu_.name.c_str(), .settingsIcon = modern::icon::Gear, .settingsCategory = "MCM"});
            }
            void OnFocusChanged(bool focused) override {
                std::scoped_lock guard(mutex_);
                if (focused) {
                    activeMenu = menu_.mod;
                    session.Select(menu_.mod, backend);
                    Refresh(menu_.pages[page_], backend);
                } else {
                    activeMenu.clear();
                    QueueGameTask([] { if (activeMenu.empty()) session.Close(backend); });
                }
            }
            void DrawSettingsNavigation(const FrameContext&) override {
                std::scoped_lock guard(mutex_);
                if (mcm::View::Navigation(menu_, page_)) {
                    lastRevision_ = ~revision.load();
                    QueueGameTask([] { session.PageChanged(backend); });
                }
            }
            void Draw(const FrameContext&) override {
                std::size_t page{};
                {
                    std::scoped_lock guard(mutex_);
                    page = page_;
                    view_.Draw(menu_, page_, [this, page](std::size_t row, const Json& value) {
                        QueueGameTask([this, page, row, value] {
                            std::scoped_lock guard(mutex_);
                            Change(menu_.pages[page], row, value, backend);
                            ++revision;
                        });
                    });
                }
                const auto now = std::chrono::steady_clock::now();
                if ((now >= nextRefresh_ || lastRevision_ != revision.load()) && !refreshQueued_.exchange(true)) {
                    nextRefresh_ = now + std::chrono::milliseconds(250);
                    lastRevision_ = revision.load();
                    QueueGameTask([this, page] {
                        std::scoped_lock guard(mutex_);
                        Refresh(menu_.pages[page], backend);
                        refreshQueued_ = false;
                    });
                }
            }
        private:
            Menu menu_;
            std::string registration_;
            std::mutex mutex_;
            std::size_t page_{};
            View view_;
            std::chrono::steady_clock::time_point nextRefresh_{};
            std::atomic<bool> refreshQueued_{};
            std::uint64_t lastRevision_{};
        };
        std::vector<std::unique_ptr<McmClient>> clients;

        bool HotkeysAllowed() {
            const auto* ui = RE::UI::GetSingleton();
            if (!ui || Host::Get().WantsGameState() || ui->GetMenuOpen(RE::MainMenu::MENU_NAME) ||
                ui->GetMenuOpen(RE::LoadingMenu::MENU_NAME)) return false;
            for (const auto& menu : ui->menuStack)
                if (menu && menu->menuFlags.any(RE::UI_MENU_FLAGS::kPausesGame)) return false;
            return true;
        }

        bool Bind(RE::BSScript::IVirtualMachine* vm) {
            if (!enabled || !vm) return true;
            vm->BindNativeMethod("MCM", "IsInstalled", +[](std::monostate) { return true; });
            vm->BindNativeMethod("MCM", "GetVersionCode", +[](std::monostate) { return Version; });
            vm->BindNativeMethod("MCM", "RefreshMenu", +[](std::monostate) { ++revision; });
            vm->BindNativeMethod("MCM", "GetModSettingInt", +[](std::monostate, RE::BSFixedString mod, RE::BSFixedString id) {
                const auto value = settings.Get(mod.c_str(), id.c_str()); return value.is_number() ? value.get<int>() : 0;
            });
            vm->BindNativeMethod("MCM", "GetModSettingBool", +[](std::monostate, RE::BSFixedString mod, RE::BSFixedString id) {
                const auto value = settings.Get(mod.c_str(), id.c_str()); return value.is_boolean() ? value.get<bool>() : value.is_number() && value.get<double>() != 0;
            });
            vm->BindNativeMethod("MCM", "GetModSettingFloat", +[](std::monostate, RE::BSFixedString mod, RE::BSFixedString id) {
                const auto value = settings.Get(mod.c_str(), id.c_str()); return value.is_number() ? value.get<float>() : 0.0F;
            });
            vm->BindNativeMethod("MCM", "GetModSettingString", +[](std::monostate, RE::BSFixedString mod, RE::BSFixedString id) {
                const auto value = settings.Get(mod.c_str(), id.c_str()); return RE::BSFixedString(value.is_string() ? value.get<std::string>().c_str() : "");
            });
            vm->BindNativeMethod("MCM", "SetModSettingInt", +[](std::monostate, RE::BSFixedString mod, RE::BSFixedString id, int value) { settings.Set(mod.c_str(), id.c_str(), value); ++revision; });
            vm->BindNativeMethod("MCM", "SetModSettingBool", +[](std::monostate, RE::BSFixedString mod, RE::BSFixedString id, bool value) { settings.Set(mod.c_str(), id.c_str(), value); ++revision; });
            vm->BindNativeMethod("MCM", "SetModSettingFloat", +[](std::monostate, RE::BSFixedString mod, RE::BSFixedString id, float value) { settings.Set(mod.c_str(), id.c_str(), value); ++revision; });
            vm->BindNativeMethod("MCM", "SetModSettingString", +[](std::monostate, RE::BSFixedString mod, RE::BSFixedString id, RE::BSFixedString value) { settings.Set(mod.c_str(), id.c_str(), value.c_str()); ++revision; });
            spdlog::info("UI21: registered native MCM Papyrus API v{}", Version);
            return true;
        }
    }

    void RegisterPapyrus() {
        enabled = !OriginalInstalled();
        if (!enabled) { spdlog::info("UI21: MCM.dll installed; native MCM category, API and hotkeys disabled"); return; }
        settings.Load("Data/MCM");
        if (const auto* papyrus = F4SE::GetPapyrusInterface()) papyrus->Register(&Bind);
    }
    void Start() {
        if (!enabled) return;
        keys.Load("Data/MCM");
        LoadTranslations();
        for (auto& menu : mcm::Load("Data/MCM/Config", backend)) {
            auto client = std::make_unique<McmClient>(std::move(menu));
            client->Register();
            clients.push_back(std::move(client));
        }
        if (const auto bindings = keys.All(); !bindings.empty()) {
            Json content = Json::array();
            for (const auto& binding : bindings)
                content.push_back({{"type", "hotkey"}, {"text", binding.description.empty() ? binding.id : binding.description},
                    {"id", binding.id}, {"modName", binding.mod}, {"help", binding.mod}});
            auto client = std::make_unique<McmClient>(Parse({{"modName", "UI21_MCM_Hotkeys"}, {"displayName", "Hotkeys"}, {"content", content}}, backend));
            client->Register();
            clients.push_back(std::move(client));
        }
        spdlog::info("UI21: loaded {} native MCM menus and {} keybind definitions", clients.size(), keys.All().size());
    }
    std::vector<b21ui::keys::Binding> CatalogBindings() {
        Keybinds installed;
        if (!enabled) { installed.Load("Data/MCM"); LoadTranslations(); }
        std::vector<b21ui::keys::Binding> result;
        for (const auto& key : enabled ? keys.All() : installed.All()) {
            if (!key.key) continue;
            b21ui::keys::Binding binding{key.id, PlainText(backend.Text(key.description.empty() ? key.id : key.description)),
                "MCM / " + key.mod, key.key, key.modifiers, true};
            const auto type = key.action.is_object() ? key.action.value("type", "") : "";
            binding.trigger = type == "SendEvent" ? "press / release" :
                type == "CallFunction" || type == "CallGlobalFunction" || type == "RunConsoleCommand" ? "press" : "unknown";
            binding.conditions = "MCM binding; mod activation conditions are not declared in keybinds.json.";
            if (binding.trigger == "press / release")
                binding.conditions += " The mod receives OnControlUp's held duration and decides hold behavior in its script.";
            result.push_back(std::move(binding));
        }
        return result;
    }
    void OnButton(const RE::ButtonEvent& event) {
        if (!enabled || Host::Get().WantsGameState()) return;
        if (!(event.value > 0 && event.heldDownSecs == 0) && !(event.value == 0 && event.heldDownSecs > 0)) return;
        int code = static_cast<int>(event.idCode);
        if (event.device == RE::INPUT_DEVICE::kMouse) {
            if (code < 0 || code > 7) return;
            code += 256;
        } else if (event.device == RE::INPUT_DEVICE::kGamepad) {
            static constexpr int masks[]{1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 4096, 8192, 16384, 32768, 9, 10};
            const auto found = std::ranges::find(masks, code);
            if (found == std::end(masks)) return;
            code = 266 + static_cast<int>(std::distance(std::begin(masks), found));
        } else if (event.device != RE::INPUT_DEVICE::kKeyboard) return;
        if (code == 42 || code == 54 || code == 29 || code == 157 || code == 56 || code == 184) return;
        const int modifiers = (::GetAsyncKeyState(VK_SHIFT) & 0x8000 ? 1 : 0) | (::GetAsyncKeyState(VK_CONTROL) & 0x8000 ? 2 : 0) | (::GetAsyncKeyState(VK_MENU) & 0x8000 ? 4 : 0);
        if (auto binding = keys.Handle(code, modifiers, event.value > 0)) {
            QueueGameTask([binding = *binding, down = event.value > 0, held = event.heldDownSecs] {
                if (!HotkeysAllowed()) return;
                if (binding.action.value("type", "") == "SendEvent") {
                    if (auto object = Script(binding.action); object && VM()) {
                        const Json args = down ? Json::array({binding.id}) : Json::array({binding.id, held});
                        VM()->SendEvent(object->GetHandle(), down ? "OnControlDown" : "OnControlUp", Arguments(args),
                            [](const RE::BSTSmartPointer<RE::BSScript::Object>&) { return true; }, {});
                    }
                } else if (down && !backend.Call(binding.action, Parameters(binding.action.value("params", Json::array()), Json{})))
                    spdlog::warn("UI21: MCM keybind callback failed: {} / {}", binding.mod, binding.id);
            });
        }
    }
}
