#pragma once
#include <nlohmann/json.hpp>

#include <filesystem>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace b21ui::mcm {
    using Json = nlohmann::json;
    inline constexpr int Version = 9;

    struct Row {
        Json definition;
        Json value;
        Json options = Json::array();
        std::string text, help, error, status;
        bool visible = true;
    };
    struct Page {
        std::string name, mod;
        std::vector<Row> rows;
        std::string error;
    };
    struct Menu {
        std::string mod, name;
        std::vector<Page> pages;
    };

    class Backend {
    public:
        virtual ~Backend() = default;
        virtual bool Installed(std::string_view plugin) = 0;
        virtual std::optional<Json> Read(const Json& source, std::string_view mod, std::string_view id) = 0;
        virtual bool Write(const Json& source, std::string_view mod, std::string_view id, const Json& value) = 0;
        virtual bool Call(const Json& action, const Json& params) = 0;
        virtual void Event(std::string_view event, const Json& args) = 0;
        virtual std::string Text(std::string_view text) { return std::string(text); }
        virtual Json List(const Json& source) { return source.value("options", Json::array()); }
        virtual std::string Describe(const Json&, std::string_view, std::string_view) { return {}; }
    };

    std::vector<Menu> Load(const std::filesystem::path& configDir, Backend& backend);
    Menu Parse(const Json& config, Backend& backend);
    void Refresh(Page& page, Backend& backend);
    bool Change(Page& page, std::size_t row, const Json& value, Backend& backend);
    Json Parameters(const Json& params, const Json& value);
    bool Visible(const Json& condition, const std::vector<int>& enabled);
    std::string PlainText(std::string_view text);
    bool ReplacementEnabled(const std::filesystem::path& plugins, bool originalLoaded);

    class Session {
    public:
        void Select(std::string_view mod, Backend& backend);
        void PageChanged(Backend& backend);
        void Close(Backend& backend);
    private:
        std::string mod_;
    };

    class SettingsStore {
    public:
        void Load(const std::filesystem::path& mcmDir);
        Json Get(std::string_view mod, std::string_view id) const;
        bool Set(std::string_view mod, std::string_view id, const Json& value);
    private:
        void ReadIni(const std::string& mod, const std::filesystem::path& path, bool user = false);
        mutable std::mutex mutex_;
        std::filesystem::path root_;
        std::map<std::string, std::map<std::string, Json>> mods_;
        std::map<std::string, std::map<std::string, Json>> overrides_;
    };

    struct Binding {
        std::string mod, id, description;
        Json action;
        int key{}, modifiers{};
    };
    class Keybinds {
    public:
        void Load(const std::filesystem::path& mcmDir);
        std::vector<Binding> All() const;
        std::optional<Binding> Find(std::string_view mod, std::string_view id) const;
        std::optional<Binding> Match(int key, int modifiers) const;
        std::optional<Binding> Handle(int key, int modifiers, bool down);
        bool Set(std::string_view mod, std::string_view id, int key, int modifiers);
    private:
        mutable std::mutex mutex_;
        std::filesystem::path file_;
        std::vector<Binding> bindings_;
        std::map<int, Binding> held_;
    };
}
