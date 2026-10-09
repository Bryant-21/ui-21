-- ui21: include this file from a consumer's xmake.lua, then call the helpers inside a target.
-- CommonLibF4 is never vendored here: b21ui_use() compiles the game-facing sources inside the
-- consumer's own F4SE plugin target, which already gets CommonLibF4 (and spdlog) from the
-- consumer's `includes("<path>/commonlibf4")` + `add_rules("commonlibf4.plugin")`.
local b21ui_root = os.scriptdir()

add_requires("imgui v1.92.7", { configs = { dx11 = true } })
add_requires("nlohmann_json")
add_requires("stb")

local function b21ui_dirs()
    add_includedirs(path.join(b21ui_root, "include"), path.join(b21ui_root, "src"))
    add_defines("NOMINMAX")
end

-- Call inside an F4SE plugin target (after the consumer includes CommonLibF4).
function b21ui_use()
    b21ui_dirs()
    add_files(path.join(b21ui_root, "src/core/*.cpp"), path.join(b21ui_root, "src/client/*.cpp"),
        path.join(b21ui_root, "src/kit/*.cpp"), path.join(b21ui_root, "src/game/*.cpp"),
        path.join(b21ui_root, "src/modern/*.cpp"), path.join(b21ui_root, "src/fo4/*.cpp"))
    add_packages("imgui", "nlohmann_json", "stb")
    add_syslinks("d3d11", "d3dcompiler", "dxgi", "user32", "psapi", "shell32", "ole32")
end

-- Call inside a desktop preview binary target (no game code).
function b21ui_use_preview()
    b21ui_dirs()
    add_includedirs(b21ui_root)
    add_files(path.join(b21ui_root, "src/core/*.cpp"), path.join(b21ui_root, "src/client/*.cpp"),
        path.join(b21ui_root, "src/kit/*.cpp"), path.join(b21ui_root, "src/modern/*.cpp"),
        path.join(b21ui_root, "src/fo4/*.cpp"), path.join(b21ui_root, "preview/PreviewHost.cpp"), path.join(b21ui_root, "demo/DemoClient.cpp"))
    add_packages("imgui", "nlohmann_json", "stb")
    add_syslinks("d3d11", "d3dcompiler", "dxgi", "user32", "xinput", "windowscodecs", "shell32", "ole32")
end

-- Ship the shared fonts and their licenses into Data/F4SE/Plugins/<plugin>/fonts.
-- `plugin_name` must be the DLL basename: PluginAssetDir() is derived from it.
function b21ui_install_assets(plugin_name)
    add_installfiles(path.join(b21ui_root, "data/Scripts/MCM.pex"), { prefixdir = "data/Scripts" })
    add_installfiles(path.join(b21ui_root, "assets/fonts/*.ttf"), path.join(b21ui_root, "assets/fonts/*.otf"),
        path.join(b21ui_root, "assets/fonts/*.txt"),
        { prefixdir = "F4SE/Plugins/" .. plugin_name .. "/fonts" })
end

if os.scriptdir() == os.projectdir() then
    set_project("ui21")
    set_languages("c++23")
    set_warnings("allextra")
    add_rules("mode.debug", "mode.releasedbg")
    add_requires("doctest")

    target("ui21_tests")
        set_kind("binary")
        b21ui_dirs()
        add_files("tests/*.cpp", "src/core/*.cpp", "src/client/InputMap.cpp", "src/client/Keycodes.cpp", "src/client/PadPointer.cpp",
            "src/kit/DdsParse.cpp", "src/kit/IconAtlasData.cpp", "src/kit/PathsPure.cpp",
            "src/modern/AppearanceCodec.cpp")
        add_packages("doctest", "imgui", "nlohmann_json")
        add_syslinks("d3d11", "d3dcompiler", "user32")

    target("ui21_preview")
        set_kind("binary")
        set_default(false)
        b21ui_use_preview()
        add_files("preview/main.cpp", "preview/McmDemo.cpp", "preview/KeybindingsDemo.cpp", "demo/Fo4Replicas.cpp")
        set_rundir("$(projectdir)")
end
