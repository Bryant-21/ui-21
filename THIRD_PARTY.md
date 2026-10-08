# Third-party components

## Bundled in this repository

| Files | Component | License | Notice |
|---|---|---|---|
| `assets/fonts/Roboto-Regular.ttf`, `Roboto-Bold.ttf`, `RobotoCondensed-Regular.ttf`, `RobotoCondensed-Bold.ttf` | Roboto and Roboto Condensed, Copyright 2011 Google Inc. | Apache License 2.0 | `assets/fonts/Roboto-LICENSE.txt` |
| `assets/fonts/Inconsolata-Medium.ttf` | Inconsolata, Copyright 2006 The Inconsolata Project Authors | SIL Open Font License 1.1 | `assets/fonts/Inconsolata-OFL.txt` |
| `assets/fonts/Font_Awesome_7_Free-Solid-900.otf`, `Font_Awesome_7_Brands-Regular-400.otf` | Font Awesome Free 7, Fonticons, Inc. | Fonts: SIL OFL 1.1; icons: CC BY 4.0 | `assets/fonts/FontAwesome-LICENSE.txt` |

Brand icons in Font Awesome Brands are trademarks of their respective owners. The license files
are installed next to the fonts by `b21ui_install_assets`.

## Fetched at build time (not in this repository)

xmake downloads these from xmake-repo; their licenses apply to the built binaries.

| Package | License |
|---|---|
| Dear ImGui 1.92.7 (`imgui`, DX11 backend) | MIT |
| nlohmann/json (`nlohmann_json`) | MIT |
| stb (`stb`, image loading) | MIT or public domain |
| doctest (`doctest`, tests only) | MIT |

## Supplied by the consuming project

ui21 does not include CommonLibF4 or spdlog; the plugin that uses ui21 provides them (both MIT).
F4SE and Fallout 4 are not part of this project.
