## ADDED Requirements

### Requirement: Compositor Capture Participates in Global Frame Pacing

The compositor capture path SHALL participate in the same effective target FPS contract that drives
viewer presentation for the current Goggles session.

For the active capture target, the compositor SHALL pace callback/publication flow so the nested
target application is not driven solely by immediate commit-triggered `frame_done` issuance.

#### Scenario: Active capture target follows global pacing target
- **GIVEN** an active capture target and a non-zero effective target FPS
- **WHEN** the compositor is issuing callbacks and publishing captured frames for that target
- **THEN** the compositor SHALL apply pacing for that target using the effective global target FPS
- **AND** the target SHALL NOT be driven solely by immediate commit-triggered callback issuance

#### Scenario: Uncapped mode bypasses compositor pacing delays
- **GIVEN** the effective global target FPS is `0`
- **WHEN** the compositor is issuing callbacks and publishing frames for the active target
- **THEN** the compositor SHALL bypass target-interval pacing delays
- **AND** the workflow SHALL remain explicitly uncapped

#### Scenario: Host acceptance scope covers Wayland and X11
- **GIVEN** Goggles is running on either a Wayland host or an X11 host
- **WHEN** the active capture target participates in the paced compositor path
- **THEN** the compositor pacing contract SHALL apply in both host environments
- **AND** acceptance SHALL use the same target-FPS rule in both environments
