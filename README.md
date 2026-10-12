# ui21

A shared Dear ImGui UI framework for Fallout 4 F4SE plugins (the "B21UI" framework).

Every plugin that uses it compiles its own copy in. At runtime the copies find each other, elect
one **host**, and every plugin draws as a **client** of that host. Players install nothing extra:
no web runtime, no separate framework DLL. All windows share one cursor, one input owner and one
look that follows the player's HUD colour.

It ships three styles on one core:

| Style | Headers | For |
|---|---|---|
| fo4 | `b21ui/fo4/Style.h` | vanilla-looking HUD widgets, lists, container/pause/settings/message-box layouts |
| modern | `b21ui/modern/Theme.h`, `Widgets.h` | tools and data-heavy windows (tables, search, tabs) |
| fo76 | `b21ui/Kit.h` | the Fallout 76 map look |

## How it works

### Host and clients across DLLs

Each plugin exports `B21UI_Rendezvous_v1` (C ABI, `include/b21ui/Abi.h`). It returns a
`B21UI_Rendezvous` table with the plugin's framework version, ABI version and a pointer to its
`B21UI_HostApi`.

1. **Rendezvous (F4SE `kPostLoad`).** Every copy enumerates the loaded modules, calls each
   `B21UI_Rendezvous_v1` export and elects the same host: the highest framework version, ties
   broken by module name. Clients then register their `B21UI_ClientDesc` with that host's API.
   Calls made before the rendezvous are queued.
2. **Install (F4SE `kGameDataReady`).** Only the elected host installs the engine hooks: the
   D3D11 `Present`/`ResizeBuffers` hooks, the input sink, the stub cursor menus and a menu
   open/close watcher. The other copies install nothing.
3. **Frames.** On each `Present` the host builds a `B21UI_Frame` (display size, delta time,
   cursor, active device, HUD colour, translated input events) and calls each open client's
   `render` callback. Each client owns its own ImGui context, so plugins built against different
   framework versions never share ImGui state.

The structs are size-prefixed and only grow at the end, so old and new plugins interoperate:
a host checks `size` before touching an optional field, and the `*_MIN_SIZE` macros define the
v1 prefix. `B21UI_FRAMEWORK_VERSION` (`src/core/Version.h`) goes up with every release so the
newest installed copy hosts; `B21UI_ABI_VERSION` changes only if the table layout breaks. Keep
the export name, the `B21UI_*` struct names and the `b21ui` namespace as they are: other plugins
already in players' installs look them up.

### Engine input

While a modal client is open the host's input sink sits at the front of `MenuControls` and turns
keyboard, mouse and gamepad events into `B21UI_Event`s for the focused client; the game sees none
of them. A stub menu holds an input-enable layer that locks player controls and, if the client
asked for it (`pausesGame`), carries the engine's pause flag. The game's cursor is hidden and the framework draws its own; the
right stick drives a pointer and an on-screen keyboard appears for text fields. Overlays
(`modal = false`) only draw: input stays with the game.

Engine-facing work (opening menus, reading forms) must run on the game thread. Use
`b21ui::QueueGameTask` (`b21ui/Tasks.h`) from render or input callbacks: it submits through the
host's single worker, so a blocked F4SE task queue never stalls a frame.

## Using it in a plugin

Add ui21 as a git submodule next to (or inside) your plugin:

    git submodule add https://github.com/Bryant-21/ui-21.git extern/ui21

Your project supplies CommonLibF4. ui21 does not vendor it, and its sources are compiled inside
your plugin target, so they use whatever CommonLibF4 (and its spdlog) that target already has.

```lua
includes("lib/commonlibf4")          -- your CommonLibF4 checkout or package
includes("extern/ui21/xmake.lua")

target("MyPlugin")
    add_rules("commonlibf4.plugin", { name = "MyPlugin", author = "you" })
    b21ui_use()                       -- framework sources, include dirs, imgui/json/stb packages
    b21ui_install_assets("MyPlugin")  -- fonts -> Data/F4SE/Plugins/MyPlugin/fonts
```

`b21ui_install_assets` takes the DLL basename, because `b21ui::PluginAssetDir()` is
`Data/F4SE/Plugins/<DLL basename>/`. Run `xmake install -o <staging dir>` to collect the DLL and
fonts.

```cpp
class MyUi : public b21ui::Client {
    void OnContextCreated(const b21ui::FrameContext&) override { b21ui::fo4::InitContext(); }
    void Draw(const b21ui::FrameContext& frame) override { /* ImGui calls, render thread */ }
};

// F4SE listener: forward every message; register once.
b21ui::OnF4SEMessage(msg);
b21ui::Register(ui, {.name = "MyPlugin", .modal = true, .pausesGame = false});
b21ui::Open(ui);  /* ... */  b21ui::Close(ui);   // any thread
```

- `Draw` runs on the render thread. Snapshot game data on the game thread and hand it over under
  a lock; never call `Open`/`Close` while holding your own lock (the host calls
  `OnFocusChanged` synchronously).
- Your hotkey does not reach `MenuControls` while your modal is open. Handle the close key in
  `Draw`.
- `b21ui_use_preview()` builds a desktop preview binary with no game code (see `preview/` and
  `demo/`), for screenshots of a view without launching the game.

### Optional: support page

`b21ui::modern::SupportPage()` draws link cards. The links belong to your plugin: put a
`ui21_support_links.inc` on your target's include path, one entry per line:

```cpp
SupportLink{icon::Patreon, "Patreon", "https://www.patreon.com/you", IM_COL32(0xF9, 0x68, 0x54, 0xFF)},
```

Without the file the page lists no links.

### Shared settings menu

The elected host adds **MOD SETTINGS** to Fallout 4's pause menu when at least one settings
panel is registered. The window lists panels with icons under **Script Extender** in its left sidebar,
above **MCM** regardless of registration order.
Panels are discovered across all loaded UI21 plugins; they do not belong in the window catalog.
Switching panels keeps the cursor, input ownership and game pause active.

Opt in per client, so a mod can have an ordinary map/player/tools window and a separate settings
panel. `name` is the stable client identifier; the label and icon are its navigation presentation:

```cpp
b21ui::Register(settings, {
    .name = "MyPluginSettings", .modal = true, .pausesGame = true,
    .settings = true, .settingsLabel = "My Plugin",
    .settingsIcon = b21ui::modern::icon::Gear
});
```

A settings client's `Draw` renders content in the supplied child window, without an outer
`ImGui::Begin/End` or its own close/fullscreen controls. The framework initializes the modern
theme and fonts and handles closing with Esc/B. `DrawSettingsNavigation` can draw that panel's
page navigation directly under the selected mod; its default is empty. The selected mod's
chevron hides or shows its page list while keeping the current page open, so other panels
remain easy to reach. Clients must update their state in `Draw` even when navigation is hidden. Use
`modern::w::NavigationItem` for consistent icons, selection and controller hit areas. Interactive captures can own
Esc/B through ImGui's key ownership API to prevent the host from closing while cancelling.
An omitted icon defaults to a gear; an omitted label uses the client name.
`settingsCategory` defaults to **Script Extender**; older clients using **UI 21** join the same group.
The shared Appearance popover applies one theme, opacity
and text scale to every settings panel, including panels in other DLLs; it uses
`[Modern.UI21_Settings]` in `B21UI.ini`, falling back to `[Modern]`.

`b21ui/Settings.h` exposes `SettingsPanels()` and `OpenSettings()` (last selected panel, then
first registered). Opening a registered settings client directly selects it in the same host.
DevTools embeds its full workbench through a separate settings client and also retains its
standalone hotkey window. Both use the same pages, actions and saved workspace. Tales'
configuration is a settings client. Full Screen Map has a dedicated modern settings panel
sharing the map's existing options and saved values. Music Player remains an ordinary client.

### Native MCM replacement

When **MCM.dll is absent**, the elected UI21 host provides a native MCM category. It reads
`Data/MCM/Config/*/config.json`, `settings.ini`, `keybinds.json` and the player's overrides in
`Data/MCM/Settings`. When MCM.dll is installed or loaded, the replacement category, Papyrus
bindings and hotkey execution are all disabled. UI21's own settings panels remain available.
No MCM movie, Flash panel or ActionScript controller is loaded by this implementation.

The host supplies the existing `MCM` Papyrus API (version code 9), settings-change and menu
lifecycle external events, globals and script properties, and typed `CallFunction` /
`CallGlobalFunction` callbacks. It supports mod/page requirements, extension pages,
conditional groups, shared/dynamic lists, file dropdowns, switches, sliders, steppers, text
inputs, buttons, key inputs and hotkeys. Positioners expose their declared coordinate settings
as numeric controls. Hotkeys import and persist MCM's `Keybinds.json` format, reject conflicts,
and execute Papyrus calls, console commands or `OnControlDown` / `OnControlUp` events.
The Hotkeys page also lists definitions that have no dedicated config control.
Pages, including Overview, Hotkeys and About, are icon-led navigation items beneath their mod.
Sliders display the declared minimum and maximum (`w::SliderFloat`); numeric inputs with both bounds also use sliders.

Custom SWF images/panels and `CallExternalFunction` callbacks require Flash code. They are
shown as unavailable, with an explanation, and require a native port. HTML labels are reduced
to plain text and line breaks. Mods that invoke private `root.mcm_loader` movie paths also need
a native port. This is config/Papyrus compatibility, not compatibility with arbitrary Flash
extensions. The upstream format/API reference is [reg2k/f4mcm](https://github.com/reg2k/f4mcm).

`Scripts/Source/User/MCM.psc` defines the compatibility declarations. Compile it with
`modkit mod compile ui21 --verify-stock` before staging consumers. `b21ui_install_assets`
includes the compiled `data/Scripts/MCM.pex` in each consumer's staging tree. Retain existing
MCM configs and saved settings when replacing the original DLL. Installing UI21 alone does
not remove or disable MCM.dll.

Native config preview (sample configs, no game access):

```
xmake run ui21_preview --demo mcm --hidden --size 1280x720 --screenshot build/mcm.png --frames 30
```

Use `--mcm-configs <directory>` to inspect other configuration files with the preview's fake
backend. It prints write/callback/event payloads; it does not execute game callbacks.

## Keybinding discovery (read-only V1)

The elected host adds **Keybindings** under **Script Extender**. It collects bound entries from all 33
Fallout 4 control-map contexts, MCM keybind definitions/current assignments, and native mod
providers. Opening the page or pressing Refresh takes a new game-thread snapshot. The list
puts overlapping bindings first, with action, source, state, modifiers and an Activation column.
Search accepts key names such as End and F3, and gestures such as hold. Source/state filters and Conflicts only narrow it.
Bindings are never changed by this page.

Activation distinguishes press, tap, hold, release and multiple taps. Native providers publish
their actual gestures and hold durations. Fallout 4's control tables do not store that behavior;
verified gameplay handler metadata covers movement, ready/reload/holster, Pip-Boy/light,
melee/grenade, view/workshop, activate/grab/power armor, jump and attack controls. Details show
the live `Controls` INI hold delays where available. Other game/menu handlers remain **unknown**.
MCM function/console actions run on press; SendEvent bindings receive press/release events
and held duration, so the mod's script decides its hold behavior. Missing provider metadata
also stays unknown instead of defaulting to press. Press-versus-hold sharing remains a possible
collision because an initial press can still reach both handlers.

The optional input maps use native vector shapes: a full keyboard, five-button mouse with
wheel directions (extra mouse inputs stay separate), and an Xbox controller with offset
sticks, triggers, bumpers, D-pad, View/Menu and ABXY buttons. The controller shell and button
positions are traced from the supplied Xbox One line drawing. Selecting a key filters its list;
controller buttons and their action cards open binding details. Guide/Share are system
buttons outside the game's XInput binding table. All views follow the shared settings style.

Red **Overlap** means the declared key/modifiers/state/activation coincide. Amber **Possible** means
scope or activation is unknown, gestures differ, or additional conditions apply. Separate known states do
not conflict. Pip-Boy includes shared menu navigation; engine context priority and private
menu handlers can further restrict execution. Intentional sharing (for example Fishing/Heal)
remains visible with its conditions. Unknown Xbox inputs stay in the list with their raw code.

F4SE exposes no public registry of all private native or Papyrus key handlers. **Discovery
coverage** lists providers and installed native plugins without metadata; absence from the
binding list does not establish that a plugin uses no keys. Providers disclose the bindings
they publish, which may omit internal focused-UI shortcuts. MCM definitions are readable even
with MCM.dll installed; this does not enable UI21's MCM replacement/category alongside it.

Native clients publish a callback once, after their hotkeys register. Use the DLL basename
as the provider ID. Registration does not create a settings panel for the publishing mod:

```cpp
#include <b21ui/Keybindings.h>
b21ui::keys::Register("B21_MyMod", "My Mod", [] {
    return std::vector<b21ui::keys::Binding>{
        {"toggle", "Open my panel", "", b21ui::keys::FromVirtualKey(VK_END),
         0, false, {"Gameplay"}, "press", "Feature enabled; no text entry"}
    };
});
```

Callbacks run on the game thread and must outlive the plugin. Empty source uses the provider
label. Keyboard codes are DirectInput scan codes, mouse codes 256–265, Xbox codes 266–281;
`FromGamepadMask` converts raw/BSButton gamepad masks. Modifiers are Shift=1, Ctrl=2, Alt=4.
`exactModifiers=false` permits extra modifiers; true requires an exact match. Empty contexts
mean unknown scope, `"*"` means all states; otherwise use the state names shown in the page,
or a custom name for a focused UI. ABI v1 is extended with an optional provider registration
function and keeps its original prefix. No Flash/ActionScript is involved.

Desktop fixtures (sample bindings, no game access):

```
xmake run ui21_preview --demo keybindings --hidden --size 1280x720 --screenshot build/keybindings.png --frames 30
```

## Performance overlay, metrics and capture (framework 14)

The elected host measures every frame itself and owns one overlay, one page and one capture for
all B21UI mods. Mods add their own metrics and do that work only while someone is looking.

**Core metrics** (owner `b21ui`, all headline): `b21ui.frame.ms` (Present to Present),
`b21ui.frame.fps`, `b21ui.gpu.ms` (one D3D11 timestamp pair per frame, read back a few frames
later without stalling), `b21ui.cpu.percent` (process, all cores = 100), `b21ui.mem.ram_mb`
(working set) and `b21ui.mem.vram_mb` (local `QueryVideoMemoryInfo` usage). CPU, RAM and VRAM
are sampled at most once a second and only while `Active()`. Mode Off with no capture and no
page open does no work at all; Compact times the frame and the GPU only.

**Overlay modes** (Shift+F11 cycles, also on the page): Off, Compact (`FPS · frame ms · GPU ms`),
Expanded (frame-time graph, 1% / 0.1% lows, GPU, CPU, RAM, VRAM, then pinned metrics under their
owner), Full (Expanded plus every metric by owner, `current (average)`; text metrics as text).
It is input-transparent, scales with height / 1080, follows the HUD colour and is placed by a
normalized anchor that is also its pivot (default top-right). Lows are `1000 / p99` and
`1000 / p99.9` of the frame-time history.

**Page**: MOD SETTINGS → B21UI → Performance (Live: stat cards, mode, anchor, graphs; Metrics:
search, owner and group, current / average / P99 / baseline delta, pin; Capture: duration,
delay, start / stop, past captures, load as baseline, open folder). Any mod can draw the same
page inside its own window with `b21ui::perf::DrawPage(extraTabs)`; extra tabs are drawn in that
mod's ImGui context only. ImGui calls never cross DLLs, so a mod's overlay content is limited to
its metrics (use `Unit::Text` for words).

```cpp
#include <b21ui/Perf.h>
namespace perf = b21ui::perf;
static const perf::Metric cells("b21_mymod.world.cells", "Loaded cells", perf::Unit::Count,
                                {.group = "World", .headline = true});
static const perf::Metric state("b21_mymod.state.phase", "Phase", perf::Unit::Text);

void OnFrame() {
    if (!perf::Active()) return;          // cheap; nobody is looking -> no work
    cells.Push(CountLoadedCells());       // any thread; last push before a Present wins
    state.PushText("streaming");
}
```

Ids are `owner.group.name` (the owner is the text before the first dot; printable ASCII without
spaces, commas or quotes). Registering an id again returns the same metric. Metrics constructed
before the rendezvous are queued and registered at `kPostLoad`. Headline metrics are pinned to the
Expanded overlay until the user unpins them. History is the last 1024 pushes per metric; the
page's average / P50 / P99 cover that window. `Active()`, `Capturing()`, `CurrentMode()`,
`SetMode()`, `StartCapture()` and `StopCapture()` go through the host. Against a host older than
14 everything is a no-op and `DrawPage` shows one line saying so.

**Hotkeys**: Shift+F11 cycles the overlay, Shift+F12 starts or stops a capture (not while a
B21UI window has focus). Both are published to the keybinding catalog as source **B21UI**.

**Settings** live in `Data/F4SE/Plugins/B21UI.ini`, `[Performance]`: `iMode` (0 Off, 1 Compact,
2 Expanded, 3 Full), `fAnchorX`, `fAnchorY` (0..1; default 1, 0), `fCaptureSeconds` (default 30,
0 = until stopped), `fCaptureDelay` (default 0), `sPinned` / `sUnpinned` (comma lists of ids the
user pinned or unpinned against the headline default). Changes are saved a second after the
last edit.

**Captures** go to `Documents\My Games\Fallout4\F4SE\B21UI\Captures\` and are written on a
background thread when the capture stops:

- `B21UI_Capture_<yyyyMMdd_HHmmss>.csv`: one row per frame, `frame,time_s`, then one column per
  metric id; numbers carry the last value forward, text is quoted.
- `.json` summary: `timestamp`, `durationSeconds`, `frames`, `framework`, `game` (executable
  and file version), `mods` (every rendezvous module, its framework version, which is host),
  `settings`, `baseline` (if one was loaded), `fps` (`average`, `low1`, `low01`) and `metrics`
  (`id`, `label`, `group`, `owner`, `unit`, `samples`, `avg`, `min`, `max`, `p50`, `p99`;
  `low1Fps` / `low01Fps` on `b21ui.frame.ms`; `last` for text). Only frames that pushed a
  metric count as its samples.

Loading a summary as the baseline compares every metric's average with that capture's.

Desktop preview (fake providers on the demo window; the preview is its own host):

```
xmake run ui21_preview --demo perf --hidden --size 1600x900 --screenshot build/perf.png --frames 150
xmake run ui21_preview --demo perf-page --hidden --size 1600x900 --screenshot build/perf_page.png --frames 150
```

## Supported runtimes

Fallout 4 on Windows x64 with F4SE, on the runtimes your CommonLibF4 build targets. The
framework uses no address IDs of its own (only CommonLibF4's `RE::` types and the D3D11 swap
chain), and is used with `COMMONLIB_RUNTIMECOUNT=3`: Old Gen 1.10.163, Next Gen 1.10.984 and
Anniversary 1.11.x. Needs MSVC with C++23 and xmake.

## Building and testing

```
xmake f -m releasedbg -y
xmake build ui21_tests && xmake run ui21_tests
xmake build ui21_preview && xmake run ui21_preview -- --demo fo4-container --screenshot shot.png --frames 30
```

The tests cover host election, input translation, focus, the deferred task queue, D3D11 state
save/restore (on WARP), DDS parsing, the gamepad keyboard and pointer, and the performance metric
store, lows, pin codec and capture writers. `python
tools/leak_check.py` checks the tree and commit messages for local paths, private addresses,
emails and secrets before publishing.

## Releases

Framework version, as in `B21UI_FRAMEWORK_VERSION`:

- **14**: shared performance overlay, metrics and capture (`b21ui/Perf.h`). The host samples frame,
  GPU, CPU, RAM and VRAM, draws the Off / Compact / Expanded / Full overlay (Shift+F11), adds the
  Performance settings page and writes CSV + JSON captures (Shift+F12) with baseline comparison.
  `B21UI_HostApi` gains the `perf*` calls at its end; older hosts make the API a no-op.
- **12**: `modern::w::SliderFloat` / `SliderInt` show the minimum and maximum inside the frame's edges
  with the value centred (in the tooltip when the frame is too narrow); `w::SliderLimits` adds them to a
  slider drawn elsewhere. MCM sliders and the Appearance popover use them. No ABI change.
- **11**: read-only keybinding catalog, state-aware collision checks, keyboard/mouse and Xbox
  vector maps, discovery coverage and cross-plugin binding providers. Rebinding is deferred.
- **10**: native MCM replacement, settings categories, shared settings Appearance controls and
  per-context appearance state. MCM.dll presence suppresses the replacement runtime.
- **9**: shared settings host, left navigation with per-panel icons, settings client flag and
  optional page navigation. The host owns the pause-menu entry and cross-plugin registry.
  Settings metadata and host functions extend ABI v1 without changing its existing prefix.
- **8**: window launcher. `b21ui/Windows.h` lists the B21 windows; `w::WindowLauncher` draws a
  top-bar button that opens any other installed one via `kOpenWindowMessage`. No ABI change.
- **7**: the modern theme follows the live HUD colour carried in each frame.
- **6**: `QueueGameTask` goes through the elected host's single worker (optional host API field,
  ABI v1 prefix kept; older hosts fall back to a local worker).
- **5**: `b21ui::QueueGameTask` added, so render and input callbacks never wait on F4SE's task lock.
- **4**: D3D11 state restore covers pixel UAVs, sparse render targets and hull/domain/compute
  shaders.
- **3**: `b21ui::SetPausesGame` for a registered modal, including while open.
- **2**: modals never open during loading screens or at the main menu; on-screen keyboard,
  shared pad pointer speed, modern-style tables, Font Awesome icons, support page.
- **1**: host election, D3D11 `Present` hook, input capture, stub cursor menu, fo4 / modern /
  fo76 styles.

## License

Copyright (C) 2026 Bryant-21

ui21 is free software: you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version (`GPL-3.0-or-later`). See [LICENSE](LICENSE).

Bundled fonts and the build dependencies keep their own licenses; see
[THIRD_PARTY.md](THIRD_PARTY.md).
