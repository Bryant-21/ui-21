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
save/restore (on WARP), DDS parsing and the gamepad keyboard and pointer. `python
tools/leak_check.py` checks the tree and commit messages for local paths, private addresses,
emails and secrets before publishing.

## Releases

Framework version, as in `B21UI_FRAMEWORK_VERSION`:

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
