# packaging Specification

## Purpose
Defines AppImage packaging behavior for launching Goggles, locating bundled assets, installing shader packs, and preserving Steam-compatible command passthrough.
## Requirements
### Requirement: AppImage Distribution

The project SHALL provide an AppImage distribution artifact for the Goggles viewer that runs on arbitrary Linux distributions without requiring root installation.

#### Scenario: AppImage starts viewer
- **GIVEN** the user has downloaded the Goggles AppImage
- **WHEN** the user executes the AppImage
- **THEN** the Goggles viewer SHALL start successfully

### Requirement: Steam Launch UX Compatibility

The packaging SHALL support Steam launch options of the form `goggles -- %command%`.

#### Scenario: Launch option passthrough
- **GIVEN** Steam is configured with launch options `goggles -- %command%`
- **WHEN** Steam launches the game
- **THEN** Goggles SHALL execute the target command exactly as provided by Steam
- **AND** it SHALL NOT require any Vulkan-layer-specific activation step

### Requirement: Packaged Assets Are Not CWD-Dependent
The packaged runtime SHALL locate shipped assets (configuration templates and shader assets) without
relying on the current working directory.

#### Scenario: AppImage provides a stable resource root
- **GIVEN** the Goggles AppImage is executed from an arbitrary working directory
- **WHEN** the viewer loads its default configuration template and shader assets
- **THEN** the viewer SHALL locate shipped assets via a stable `resource_dir` resolution rule
- **AND** it SHALL NOT require `./config` or `./shaders` to exist in the working directory

### Requirement: Optional Shader Pack Install Location

The packaging SHALL provide a way to install/update the full RetroArch shader pack (slang-shaders) into a stable user location without requiring Pixi.

#### Scenario: Shader pack is fetched into XDG data
- **WHEN** the user invokes the AppImage shader fetch/update flow
- **THEN** the shader pack SHALL be installed under `${XDG_DATA_HOME:-$HOME/.local/share}/goggles/shaders/retroarch/`

### Requirement: Cursor Theme Asset Exclusion

The packaging workflow SHALL NOT ship bundled cursor-theme assets as part of Goggles distribution
artifacts.

The packaging workflow SHALL:
- Exclude `assets/cursor` from AppImage payload content.
- Keep existing required packaged assets (configuration templates and shader assets) intact.
- Preserve software cursor runtime behavior through runtime/system/fallback cursor sourcing.

#### Scenario: AppImage payload excludes bundled cursor theme assets
- **GIVEN** AppImage staging is executed for a release build
- **WHEN** the staged payload tree is inspected
- **THEN** `usr/share/goggles/assets/cursor` is absent
- **AND** other expected asset roots remain present

#### Scenario: Viewer still starts with software cursor after packaging change
- **GIVEN** a packaged Goggles runtime without bundled cursor theme assets
- **WHEN** the viewer starts and presents a forwarded surface
- **THEN** software cursor rendering remains available through runtime/system/fallback cursor sourcing

