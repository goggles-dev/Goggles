## ADDED Requirements

### Requirement: RenderDoc tooling availability in Pixi environment
The project SHALL provide RenderDoc runtime tooling through Pixi so GPU-state validation tests can run without ad-hoc system setup.

#### Scenario: Python RenderDoc module import works
- **GIVEN** the default Pixi environment is installed from project configuration
- **WHEN** `pixi run python -c "import renderdoc"` is executed
- **THEN** the command SHALL exit with code 0
- **AND** no `ModuleNotFoundError` SHALL be emitted

#### Scenario: rdc CLI is available
- **GIVEN** the default Pixi environment is active
- **WHEN** `pixi run rdc --version` is executed
- **THEN** the command SHALL exit with code 0
- **AND** a RenderDoc/rdc version string SHALL be printed
- **AND** the GPU validation command contracts SHALL target `rdc assert-clean`, `rdc assert-state`, `rdc assert-pixel`, and `rdc diff`

### Requirement: RenderDoc package recipe is locally managed
RenderDoc integration SHALL be defined through a local package recipe to keep dependency provenance and reproducibility inside the repository.

#### Scenario: Local recipe is present and referenced
- **GIVEN** repository dependency manifests
- **WHEN** `packages/renderdoc/recipe.yaml` and `pixi.toml` are inspected
- **THEN** a local RenderDoc package definition SHALL exist
- **AND** Pixi dependency resolution SHALL reference that local package entry
