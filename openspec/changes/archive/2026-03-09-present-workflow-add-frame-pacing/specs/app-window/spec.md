## MODIFIED Requirements

### Requirement: Target FPS CLI Override

The application SHALL allow overriding the effective global pacing target FPS from the command line.

#### Scenario: Override target fps via CLI
- **GIVEN** the application is started with `--target-fps 120`
- **WHEN** configuration is loaded
- **THEN** `config.render.target_fps` SHALL be set to `120`
- **AND** the override SHALL take precedence over the config file
- **AND** the effective global pacing target for the current session SHALL be `120`

#### Scenario: Disable frame cap via CLI
- **GIVEN** the application is started with `--target-fps 0`
- **WHEN** configuration is loaded
- **THEN** `config.render.target_fps` SHALL be set to `0`
- **AND** the effective global pacing target SHALL be uncapped

## ADDED Requirements

### Requirement: Application Window Runtime Frame Pacing Controls

The Application window SHALL expose runtime controls for the effective global pacing target used by
the current Goggles session.

The controls SHALL initialize from the resolved `render.target_fps` value and SHALL update the active
session pacing target without requiring restart.

#### Scenario: Runtime controls reflect startup pacing target
- **GIVEN** the application window is rendered after config and CLI resolution
- **WHEN** the runtime pacing controls are shown
- **THEN** they SHALL reflect the current effective `render.target_fps` value for the session

#### Scenario: Runtime controls update active pacing target
- **GIVEN** the Application window runtime pacing controls are visible
- **WHEN** the user selects a new non-zero target FPS
- **THEN** the effective global pacing target SHALL update for the current session
- **AND** the compositor and viewer pacing paths SHALL observe the same updated target

#### Scenario: Runtime controls allow uncapped mode
- **GIVEN** the Application window runtime pacing controls are visible
- **WHEN** the user selects uncapped pacing
- **THEN** the effective global pacing target SHALL become `0`
- **AND** the session SHALL switch to uncapped pacing without restart
