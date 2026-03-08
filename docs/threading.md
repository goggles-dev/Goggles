# Threading

## Purpose

Defines the current threading model for Goggles. The runtime optimizes for predictable frame
latency: Vulkan submission stays on the main thread, the compositor owns its own event-loop
thread, and background work uses `goggles::util::JobSystem` only when it can stay off the hot
path.

## Overview

```
┌─────────────────────────────────────────────────────────────────┐
│ Main Thread                                                     │
│ - SDL window + ImGui                                            │
│ - VulkanBackend + FilterChain record/present                    │
│ - Vulkan queue submission                                       │
└─────────────────────┬───────────────────────────────────────────┘
                      │ schedule bounded background work
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│ JobSystem (`BS::thread_pool`)                                   │
│ - async preset compile/rebuild                                  │
│ - other non-hot-path background jobs                            │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ Compositor Thread (`std::jthread`)                              │
│ - wlroots / Wayland event loop                                  │
│ - XWayland lifecycle                                            │
│ - surface, focus, pointer-constraint, and layer-shell handling  │
└─────────────────────────────────────────────────────────────────┘
```

## Main Thread

- Owns the Vulkan device, swapchain, and render loop.
- Records filter-chain work and presents frames.
- Is the only thread that submits Vulkan queue work.
- Applies completed background filter-chain rebuilds at safe synchronization points.

The render backend and filter-chain execution remain single-threaded by default.

## Compositor Thread

The compositor runs its wlroots display loop on a dedicated `std::jthread` owned by
`CompositorState`.

- Starts and stops with compositor lifecycle management.
- Handles Wayland and XWayland client activity.
- Owns focus routing, pointer constraints, cursor behavior, and layer-shell interactions.

This thread is an allowed exception to the render-path `JobSystem` rule because it owns an
external event loop rather than pipeline work.

## Job System

`goggles::util::JobSystem` wraps a global `BS::thread_pool`.

- It is the required mechanism for concurrent render or pipeline work.
- It initializes lazily and exposes `submit`, `wait_all`, and `shutdown`.
- The current render-path use is asynchronous shader preset rebuild in
  `src/render/backend/filter_chain_controller.cpp`.

Avoid creating ad-hoc worker threads for render or pipeline tasks.

## Cross-Thread Communication

- Use `util::SPSCQueue` for bounded single-producer/single-consumer handoff where that pattern
  fits, such as compositor input and resize event queues.
- Keep blocking synchronization out of the per-frame render path.
- Use narrow mutex-protected shared state only where snapshotting shared compositor-presented data
  is unavoidable.

## Rules

- Keep render backend and per-frame filter-chain execution single-threaded unless profiling proves
  otherwise.
- Only the main thread may submit Vulkan queue work.
- Concurrent pipeline or render work must go through `goggles::util::JobSystem`.
- External integration code outside the real-time render path may use `std::jthread` with
  RAII-managed lifetime.

## References

- [Project Policies](project_policies.md) - Section 7 threading and real-time policy
- [Vulkan threading](https://www.khronos.org/registry/vulkan/specs/1.3/html/chap3.html#fundamentals-threadingbehavior)
