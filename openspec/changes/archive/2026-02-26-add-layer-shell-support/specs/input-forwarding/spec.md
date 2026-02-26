## MODIFIED Requirements

### Requirement: Input Forwarding Infrastructure

The system SHALL provide a compositor server that supports both XWayland (for X11 apps) and native
Wayland clients using a unified `wlr_seat` input path, and SHALL additionally support
`wlr-layer-shell-unstable-v1` overlay surfaces.

The compositor server SHALL:
- Create a headless wlroots backend
- Bind a Wayland socket for client connections
- Start XWayland server for X11 application support
- Create a wlr_seat with keyboard and pointer capabilities
- Connect XWayland to the seat via `wlr_xwayland_set_seat()`
- Create a `wlr_layer_shell_v1` global (version 4) for overlay surface support
- Run the compositor event loop on a dedicated thread

#### Scenario: Compositor initializes with unified input
- **WHEN** the input forwarding system starts
- **THEN** a Wayland socket is created (wayland-N)
- **AND** an XWayland server is started (DISPLAY :N)
- **AND** XWayland is connected to the seat for automatic input translation
- **AND** a seat with keyboard and pointer capabilities is available
- **AND** a `zwlr_layer_shell_v1` global is advertised on the display

## ADDED Requirements

### Requirement: Layer Surface Keyboard Interactivity

The system SHALL temporarily transfer seat keyboard focus to a mapped layer surface that requests
`exclusive` keyboard interactivity, without changing the surface used for frame capture or surface
enumeration.

The compositor server SHALL:
- In the layer surface `map` handler: if `keyboard_interactive` is `exclusive`, call
  `wlr_seat_keyboard_enter()` for the layer surface's `wlr_surface`
- In the layer surface `unmap` and `destroy` handlers: if `focused_surface` is non-null, restore
  keyboard focus via `wlr_seat_keyboard_enter()` for `focused_surface`
- NOT modify `focused_surface` or `focused_xsurface` in any layer surface handler
- NOT include layer surfaces in `get_surfaces()` or `focus_surface_by_id()` enumeration

#### Scenario: Exclusive layer surface takes keyboard focus
- **GIVEN** a game surface (`focused_surface`) has keyboard focus
- **WHEN** a layer surface with `exclusive` keyboard interactivity maps
- **THEN** `wlr_seat_keyboard_enter()` is called for the layer surface's `wlr_surface`
- **AND** `focused_surface` remains pointing to the game surface

#### Scenario: Exclusive layer surface unmaps, focus restored
- **GIVEN** a layer surface with `exclusive` interactivity currently has keyboard focus
- **WHEN** the layer surface unmaps or is destroyed
- **THEN** `wlr_seat_keyboard_enter()` is called to restore focus to `focused_surface`

#### Scenario: None-interactivity layer surface does not take keyboard focus
- **GIVEN** a game surface has keyboard focus
- **WHEN** a layer surface with `none` keyboard interactivity maps
- **THEN** keyboard focus remains with the game surface unchanged

#### Scenario: Layer surface not enumerated as input target
- **WHEN** `get_surfaces()` is called
- **THEN** the returned list does not include any layer surfaces
- **AND** layer surfaces cannot be selected via `set_input_target()`
