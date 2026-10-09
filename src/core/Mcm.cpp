#include "core/Mcm.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <sstream>

namespace b21ui::mcm {
    namespace {
        std::string Trim(std::string text) {
            const auto first = text.find_first_not_of(" \t\r\n");
            return first == std::string::npos ? "" : text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
        }
        bool SafeName(std::string_view name) {
            return !name.empty() && name != "." && name != ".." && name.find_first_of("/\\:\r\n") == name.npos;
        }
        Json ReadJson(const std::filesystem::path& file) {
            std::ifstream stream(file, std::ios::binary);
            return Json::parse(stream, nullptr, true, true);
        }
        bool Save(const std::filesystem::path& path, const std::string& text) {
            std::error_code error;
            std::filesystem::create_directories(path.parent_path(), error);
            if (error) return false;
            auto temporary = path;
            temporary += ".ui21.tmp";
            {
                std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
                stream << text;
                stream.close();
                if (!stream) return false;
            }
            return ::MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
        }
        std::string String(const Json& value) {
            return value.is_string() ? value.get<std::string>() : value.is_null() ? "" : value.dump();
        }
        std::string Requirements(const Json& config, Backend& backend) {
            std::string missing;
            for (const auto& plugin : config.value("pluginRequirements", Json::array())) {
                if (plugin.is_string() && !backend.Installed(plugin.get<std::string>()))
                    missing += (missing.empty() ? "Missing plugins: " : ", ") + plugin.get<std::string>();
            }
            if (config.value("minMcmVersion", 0) > Version) missing += " Requires a newer MCM API.";
            return missing.empty() ? missing : config.value("messageIfMissingReqs", missing);
        }
        bool Truth(const Json& value) {
            return value.is_boolean() ? value.get<bool>() : value.is_number() ? value.get<double>() != 0 : !value.empty();
        }
        Page MakePage(const Json& config, const Json& owner, Backend& backend, std::string name) {
            Page page{backend.Text(name), owner.value("modName", "")};
            page.error = Requirements(config, backend);
            if (!page.error.empty()) return page;
            for (auto definition : config.value("content", Json::array())) {
                if (!definition.is_object()) continue;
                definition["modName"] = definition.value("modName", page.mod);
                auto& source = definition["valueOptions"];
                if (source.is_null()) source = Json::object();
                const auto shared = source.value("sharedOptions", "");
                if (!shared.empty() && owner.contains("sharedLists"))
                    source["options"] = owner["sharedLists"].value(shared, Json::array());
                Row row;
                row.definition = std::move(definition);
                row.value = row.definition.value("value", Json{});
                page.rows.push_back(std::move(row));
            }
            Refresh(page, backend);
            return page;
        }
    }

    std::string PlainText(std::string_view text) {
        std::string out;
        bool tag{};
        for (std::size_t i = 0; i < text.size(); ++i) {
            if (text[i] == '<') {
                if (text.substr(i, 3) == "<br" || text.substr(i, 4) == "</p>") out += '\n';
                tag = true;
            } else if (text[i] == '>') tag = false;
            else if (!tag) out += text[i];
        }
        for (const auto& [entity, replacement] : std::vector<std::pair<std::string, std::string>>{
            {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&nbsp;", " "}}) {
            std::size_t pos{};
            while ((pos = out.find(entity, pos)) != out.npos) { out.replace(pos, entity.size(), replacement); pos += replacement.size(); }
        }
        return out;
    }

    bool ReplacementEnabled(const std::filesystem::path& plugins, bool originalLoaded) {
        return !originalLoaded && !std::filesystem::exists(plugins / "mcm.dll") && !std::filesystem::exists(plugins / "f4mcm.dll");
    }
    void Session::Select(std::string_view mod, Backend& backend) {
        if (mod_ == mod) return;
        if (mod_.empty()) backend.Event("OnMCMOpen", Json::array());
        else backend.Event("OnMCMMenuClose|" + mod_, Json::array());
        mod_ = mod;
        backend.Event("OnMCMMenuOpen", Json::array());
        backend.Event("OnMCMMenuOpen|" + mod_, Json::array());
    }
    void Session::PageChanged(Backend& backend) { backend.Event("OnMCMMenuOpen", Json::array()); }
    void Session::Close(Backend& backend) {
        if (mod_.empty()) return;
        backend.Event("OnMCMMenuClose|" + mod_, Json::array());
        backend.Event("OnMCMMenuClose", Json::array());
        backend.Event("OnMCMClose", Json::array());
        mod_.clear();
    }

    Menu Parse(const Json& config, Backend& backend) {
        Menu menu{config.at("modName").get<std::string>(), backend.Text(config.value("displayName", config.at("modName").get<std::string>()))};
        const auto error = Requirements(config, backend);
        if (!error.empty()) {
            if (!config.value("hideIfMissingReqs", false)) menu.pages.push_back({"Overview", menu.mod, {}, error});
            return menu;
        }
        if (config.contains("content")) menu.pages.push_back(MakePage(config, config, backend, "Overview"));
        for (const auto& page : config.value("pages", Json::array())) {
            if (!Requirements(page, backend).empty() && page.value("hideIfMissingReqs", false)) continue;
            menu.pages.push_back(MakePage(page, config, backend, page.value("pageDisplayName", "Settings")));
        }
        return menu;
    }

    std::vector<Menu> Load(const std::filesystem::path& configDir, Backend& backend) {
        std::vector<Menu> menus, extensions;
        std::error_code error;
        for (const auto& dir : std::filesystem::directory_iterator(configDir, error)) {
            if (!dir.is_directory()) continue;
            const auto file = dir.path() / "config.json";
            if (!std::filesystem::exists(file)) continue;
            try {
                const auto config = ReadJson(file);
                auto menu = Parse(config, backend);
                if (menu.pages.empty()) continue;
                if (config.contains("ownerModName")) {
                    menu.mod = config.at("ownerModName").get<std::string>();
                    extensions.push_back(std::move(menu));
                } else menus.push_back(std::move(menu));
            } catch (const std::exception& e) {
                const auto name = dir.path().filename().string();
                menus.push_back({name, name, {{"Configuration error", name, {}, e.what()}}});
            }
        }
        for (auto& extension : extensions) {
            const auto owner = std::ranges::find(menus, extension.mod, &Menu::mod);
            if (owner != menus.end())
                for (auto& page : extension.pages) owner->pages.push_back(std::move(page));
            else menus.push_back(std::move(extension));
        }
        std::ranges::sort(menus, {}, &Menu::name);
        return menus;
    }

    bool Visible(const Json& condition, const std::vector<int>& enabled) {
        if (condition.is_null()) return true;
        const auto has = [&](const Json& group) { return group.is_number_integer() && std::ranges::find(enabled, group.get<int>()) != enabled.end(); };
        if (condition.is_number_integer()) return has(condition);
        if (condition.is_array()) return std::ranges::any_of(condition, has);
        if (condition.is_object()) {
            if (condition.contains("AND")) return std::ranges::all_of(condition["AND"], has);
            if (condition.contains("ONLY")) return std::ranges::all_of(condition["ONLY"], has) &&
                condition["ONLY"].size() == static_cast<std::size_t>(std::ranges::count_if(enabled, [](int group) { return group != 0; }));
            if (condition.contains("OR")) return std::ranges::any_of(condition["OR"], has);
        }
        return false;
    }

    void Refresh(Page& page, Backend& backend) {
        std::vector<int> groups{0};
        for (auto& row : page.rows) {
            const auto& d = row.definition;
            const auto& source = d.at("valueOptions");
            const auto mod = d.value("modName", page.mod), id = d.value("id", "");
            row.text = PlainText(backend.Text(d.value("text", "")));
            row.help = PlainText(backend.Text(d.value("help", "")));
            if (const auto dynamic = backend.Describe(d, "text", row.text); !dynamic.empty()) row.text = PlainText(dynamic);
            if (const auto dynamic = backend.Describe(d, "help", row.help); !dynamic.empty()) row.help = PlainText(dynamic);
            row.error.clear();
            if (source.contains("sourceType") || d.value("type", "") == "hotkey") {
                auto readSource = source;
                if (d.value("type", "") == "hotkey") readSource["sourceType"] = "Hotkey";
                if (auto value = backend.Read(readSource, mod, id)) row.value = *value;
                else row.error = "Setting source is unavailable.";
            }
            if (d.value("type", "") == "positioner") {
                row.value = Json::object();
                for (const auto* field : {"x", "y", "scalex", "scaley", "rotation", "alpha"}) {
                    const auto key = std::string(field) + "Source";
                    if (!source.contains(key)) continue;
                    const auto& coordinate = source[key];
                    if (const auto value = backend.Read(coordinate, mod, coordinate.value("id", id))) row.value[field] = *value;
                    else row.error = "A position setting source is unavailable.";
                }
                if (row.value.empty()) row.error = "This positioner has no native coordinate settings.";
            }
            row.options = backend.List(source);
            if (d.contains("groupControl") && Truth(row.value)) groups.push_back(d["groupControl"].get<int>());
            const auto type = d.value("type", "");
            if (type == "customClipLoader" || type == "image") row.error = "This control requires a custom Flash asset.";
            if (d.value("action", Json::object()).value("type", "") == "CallExternalFunction")
                row.error = "This callback requires a Flash plugin extension.";
        }
        std::ranges::sort(groups);
        groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
        for (auto& row : page.rows)
            row.visible = row.definition.value("type", "") != "hiddenSwitcher" && Visible(row.definition.value("groupCondition", Json{}), groups);
    }

    Json Parameters(const Json& params, const Json& value) {
        Json out = Json::array();
        for (const auto& parameter : params) {
            if (!parameter.is_string()) { out.push_back(parameter); continue; }
            auto text = parameter.get<std::string>();
            if (text == "{value}") { out.push_back(value); continue; }
            std::size_t pos{};
            while ((pos = text.find("{value}", pos)) != text.npos) {
                const auto replacement = String(value);
                text.replace(pos, 7, replacement); pos += replacement.size();
            }
            try {
                if (text.starts_with("{i}")) out.push_back(std::stoi(text.substr(3)));
                else if (text.starts_with("{f}")) out.push_back(std::stof(text.substr(3)));
                else if (text.starts_with("{b}")) out.push_back(std::stoi(text.substr(3)) != 0);
                else out.push_back(text);
            } catch (...) { out.push_back(0); }
        }
        return out;
    }

    bool Change(Page& page, std::size_t index, const Json& input, Backend& backend) {
        if (index >= page.rows.size()) return false;
        auto& row = page.rows[index];
        if (!row.visible || !row.error.empty() || row.definition.value("disabled", false)) return false;
        row.status.clear();
        const auto& d = row.definition;
        auto source = d.at("valueOptions");
        const auto mod = d.value("modName", page.mod), id = d.value("id", ""), type = d.value("type", "");
        auto value = input;
        const auto sourceType = source.value("sourceType", "");
        if (sourceType.ends_with("Bool")) value = Truth(input);
        else if (sourceType.ends_with("Int") && (input.is_number() || input.is_boolean())) value = input.is_boolean() ? input.get<bool>() ? 1 : 0 : static_cast<int>(input.get<double>());
        else if ((sourceType.ends_with("Float") || sourceType == "GlobalValue") && (input.is_number() || input.is_boolean())) value = input.is_boolean() ? input.get<bool>() ? 1.0 : 0.0 : input.get<double>();
        if (value.is_number() && (type == "slider" || ((type == "textinputInt" || type == "textinputFloat") &&
                                                      source.contains("min") && source.contains("max")))) {
            const auto low = source.value("min", 0.0), high = std::max(low, source.value("max", 1.0)), step = source.value("step", 0.0);
            auto number = std::clamp(value.get<double>(), low, high);
            if (step > 0) number = std::clamp(low + std::round((number - low) / step) * step, low, high);
            value = sourceType.ends_with("Int") ? Json(static_cast<int>(number)) : Json(number);
        }
        if (type == "hotkey") source["sourceType"] = "Hotkey";
        if (type == "positioner" && value.is_object()) {
            for (const auto& [field, coordinate] : value.items()) {
                const auto key = field + "Source";
                if (!source.contains(key) || !backend.Write(source[key], mod, source[key].value("id", id), coordinate)) {
                    row.status = "The position could not be saved.";
                    return false;
                }
            }
        }
        if (source.contains("sourceType") && !backend.Write(source, mod, id, value)) {
            row.status = type == "hotkey" ? "The shortcut is already assigned or could not be saved." : "The setting could not be saved.";
            return false;
        }
        row.value = value;
        if (d.contains("action") && !backend.Call(d["action"], Parameters(d["action"].value("params", Json::array()), value))) {
            row.status = "The callback could not be dispatched.";
            return false;
        }
        if (!id.empty() && type != "button") {
            backend.Event("OnMCMSettingChange", {mod, id});
            backend.Event("OnMCMSettingChange|" + mod, {mod, id});
        }
        Refresh(page, backend);
        return true;
    }

    void SettingsStore::ReadIni(const std::string& mod, const std::filesystem::path& path, bool user) {
        std::ifstream file(path, std::ios::binary);
        std::string bytes((std::istreambuf_iterator<char>(file)), {});
        if (bytes.size() >= 2 && static_cast<unsigned char>(bytes[0]) == 0xFF && static_cast<unsigned char>(bytes[1]) == 0xFE) {
            std::wstring wide;
            for (std::size_t i = 2; i + 1 < bytes.size(); i += 2)
                wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(bytes[i]) | static_cast<unsigned char>(bytes[i + 1]) << 8));
            const int size = ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
            bytes.resize(size);
            ::WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), bytes.data(), size, nullptr, nullptr);
        } else if (bytes.starts_with("\xEF\xBB\xBF")) bytes.erase(0, 3);
        std::istringstream stream(bytes);
        std::string line, section;
        while (std::getline(stream, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            if (line.front() == '[' && line.back() == ']') { section = line.substr(1, line.size() - 2); continue; }
            const auto equals = line.find('=');
            if (equals == line.npos) continue;
            const auto key = Trim(line.substr(0, equals)) + ":" + section;
            auto value = Trim(line.substr(equals + 1));
            if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') || (value.front() == '\'' && value.back() == '\'')))
                value = value.substr(1, value.size() - 2);
            try {
                switch (key[0]) {
                case 'b': mods_[mod][key] = value != "0"; break;
                case 'i': mods_[mod][key] = std::stoi(value); break;
                case 'f': mods_[mod][key] = std::stof(value); break;
                default: mods_[mod][key] = value; break;
                }
                if (user) overrides_[mod][key] = mods_[mod][key];
            } catch (...) {}
        }
    }
    void SettingsStore::Load(const std::filesystem::path& root) {
        std::scoped_lock guard(mutex_);
        root_ = root;
        mods_.clear();
        overrides_.clear();
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(root / "Config", error))
            if (entry.is_directory()) ReadIni(entry.path().filename().string(), entry.path() / "settings.ini");
        error.clear();
        for (const auto& entry : std::filesystem::directory_iterator(root / "Settings", error))
            if (entry.path().extension() == ".ini") ReadIni(entry.path().stem().string(), entry.path(), true);
    }
    Json SettingsStore::Get(std::string_view mod, std::string_view id) const {
        std::scoped_lock guard(mutex_);
        const auto found = mods_.find(std::string(mod));
        if (found == mods_.end()) return {};
        const auto setting = found->second.find(std::string(id));
        return setting == found->second.end() ? Json{} : setting->second;
    }
    bool SettingsStore::Set(std::string_view mod, std::string_view id, const Json& value) {
        if (!SafeName(mod) || id.find_first_of("\r\n=") != id.npos) return false;
        if (value.is_string() && value.get_ref<const std::string&>().find_first_of("\r\n") != std::string::npos) return false;
        std::scoped_lock guard(mutex_);
        auto found = mods_.find(std::string(mod));
        if (found == mods_.end() || !found->second.contains(std::string(id))) return false;
        auto values = overrides_[std::string(mod)];
        values[std::string(id)] = value;
        std::map<std::string, std::map<std::string, Json>> sections;
        for (const auto& [key, setting] : values) {
            const auto colon = key.find(':');
            sections[colon == key.npos ? "" : key.substr(colon + 1)][key.substr(0, colon)] = setting;
        }
        std::string text;
        for (const auto& [section, settings] : sections) {
            text += "[" + section + "]\n";
            for (const auto& [key, setting] : settings)
                text += key + "=" + (setting.is_boolean() ? setting.get<bool>() ? "1" : "0" :
                    setting.is_string() ? "\"" + String(setting) + "\"" : String(setting)) + "\n";
        }
        if (!Save(root_ / "Settings" / (std::string(mod) + ".ini"), text)) return false;
        found->second[std::string(id)] = value;
        overrides_[std::string(mod)] = std::move(values);
        return true;
    }

    void Keybinds::Load(const std::filesystem::path& root) {
        std::scoped_lock guard(mutex_);
        file_ = root / "Settings/Keybinds.json";
        bindings_.clear();
        held_.clear();
        std::error_code error;
        for (const auto& dir : std::filesystem::directory_iterator(root / "Config", error)) {
            const auto file = dir.path() / "keybinds.json";
            if (!std::filesystem::exists(file)) continue;
            try {
                const auto config = ReadJson(file);
                for (const auto& key : config.at("keybinds"))
                    bindings_.push_back({config.at("modName").get<std::string>(), key.at("id").get<std::string>(),
                        key.value("desc", ""), key.at("action")});
            } catch (...) {}
        }
        if (!std::filesystem::exists(file_)) return;
        try {
            for (const auto& key : ReadJson(file_).at("keybinds")) {
                auto found = std::ranges::find_if(bindings_, [&](const Binding& b) { return b.mod == key.value("modName", "") && b.id == key.value("id", ""); });
                if (found != bindings_.end()) { found->key = key.value("keycode", 0); found->modifiers = key.value("modifiers", 0); }
            }
        } catch (...) {}
    }
    std::vector<Binding> Keybinds::All() const { std::scoped_lock guard(mutex_); return bindings_; }
    std::optional<Binding> Keybinds::Find(std::string_view mod, std::string_view id) const {
        std::scoped_lock guard(mutex_);
        for (const auto& b : bindings_) if (b.mod == mod && b.id == id) return b;
        return {};
    }
    std::optional<Binding> Keybinds::Match(int key, int modifiers) const {
        std::scoped_lock guard(mutex_);
        for (const auto& b : bindings_) if (key && b.key == key && b.modifiers == modifiers) return b;
        return {};
    }
    std::optional<Binding> Keybinds::Handle(int key, int modifiers, bool down) {
        std::scoped_lock guard(mutex_);
        if (down) {
            for (const auto& b : bindings_) if (key && b.key == key && b.modifiers == modifiers) {
                held_[key] = b;
                return b;
            }
        } else if (const auto found = held_.find(key); found != held_.end()) {
            const auto binding = found->second;
            held_.erase(found);
            return binding;
        }
        return {};
    }
    bool Keybinds::Set(std::string_view mod, std::string_view id, int key, int modifiers) {
        if (key < 0 || key > 281 || modifiers < 0 || modifiers > 7) return false;
        std::scoped_lock guard(mutex_);
        auto found = std::ranges::find_if(bindings_, [&](const Binding& b) { return b.mod == mod && b.id == id; });
        if (found == bindings_.end()) return false;
        for (const auto& b : bindings_) if (&b != &*found && key && b.key == key && b.modifiers == modifiers) return false;
        const auto old = *found;
        found->key = key; found->modifiers = modifiers;
        Json saved{{"version", 1}, {"keybinds", Json::array()}};
        for (const auto& b : bindings_)
            saved["keybinds"].push_back({{"modName", b.mod}, {"id", b.id}, {"keycode", b.key}, {"modifiers", b.modifiers}});
        if (Save(file_, saved.dump(2))) return true;
        *found = old;
        return false;
    }
}
