## MODIFIED Requirements

### Requirement: Present Wait Frame Pacing

The render backend SHALL use `VK_KHR_present_wait` when supported to pace viewer presentation and
avoid uncapped mailbox behavior on high-end GPUs.

`render.target_fps` SHALL be treated as the effective global pacing target for the current Goggles
session.

The viewer backend SHALL reuse the existing present-wait and CPU-throttle fallback behavior as the
viewer half of that global pacing contract.

#### Scenario: Present wait enabled
- **GIVEN** the physical device supports `VK_KHR_present_wait`
- **WHEN** the swapchain is created
- **THEN** the device SHALL enable the extension
- **AND** the present mode SHALL be `FIFO`
- **AND** the backend SHALL use present wait to pace viewer presentation to `render.target_fps`

#### Scenario: Uncapped target fps
- **GIVEN** `render.target_fps` is set to `0`
- **WHEN** present wait is available
- **THEN** the backend SHALL skip waiting for a target interval
- **AND** viewer presentation SHALL proceed as fast as `FIFO` allows

#### Scenario: Present wait unsupported
- **GIVEN** `VK_KHR_present_wait` is not supported
- **WHEN** the swapchain is created
- **THEN** the backend SHALL prefer `MAILBOX` present mode
- **AND** it SHALL apply CPU-side frame capping when `render.target_fps` is non-zero
- **AND** it SHALL fall back to `FIFO` if `MAILBOX` is unavailable

#### Scenario: Target fps changes via config or runtime control
- **GIVEN** a Goggles session has an effective `render.target_fps`
- **WHEN** configuration, CLI startup, or Application-window runtime controls change that target
- **THEN** the backend SHALL update viewer pacing to the new target without requiring restart
- **AND** `render.target_fps = 0` SHALL continue to mean uncapped pacing
