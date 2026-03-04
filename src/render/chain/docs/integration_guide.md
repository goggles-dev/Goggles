# Filter Chain C API Integration Guide

This guide is for host applications integrating the Vulkan v1 filter-chain C API into an existing render path.

## Integration flow

1. Query capabilities and validate limits.
2. Create a runtime (`goggles_chain_create_vk` or `_ex`).
3. Load a preset (`goggles_chain_preset_load` or `_ex`).
4. Record filter-chain commands every frame (`goggles_chain_record_vk`).
5. Handle control updates and resize events as needed.
6. Destroy snapshot/runtime objects explicitly.

## Optimized path and non-goals

Optimized integration path in v1:

- Create runtime once, load one preset, record every frame, destroy at shutdown.
- Update controls by stable `control_id` from periodic snapshots.
- React to target resize through `goggles_chain_handle_resize` and continue recording.

Integration non-goals in v1:

- No hidden background threads or implicit async preset reload.
- No global mutable runtime singleton.
- No implicit command submission or presentation from record APIs.
- No caller-visible ownership of runtime-internal Vulkan resources.

The runtime state model is:

- `CREATED`: runtime exists, no successful preset load yet.
- `READY`: at least one successful preset load has completed.
- `DEAD`: runtime destroyed; handle nulled by destroy API.

`goggles_chain_record_vk` and control mutation/listing APIs are valid only in `READY`.

## 1) Query capabilities first

Always query capabilities at startup so you can validate optional behavior and limits.

```c
goggles_chain_capabilities_t caps = goggles_chain_capabilities_init();
goggles_chain_status_t st = goggles_chain_capabilities_get(&caps);
if (st != GOGGLES_CHAIN_STATUS_OK) {
    return st;
}

/* Example: enforce frame index ring size for your host setup. */
if (requested_sync_indices > caps.max_sync_indices) {
    return GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT;
}
```

## 2) Create the runtime

Use `goggles_chain_create_vk` for C-string inputs or `goggles_chain_create_vk_ex` for explicit-length UTF-8 byte spans.

### Required create-time rules

- `num_sync_indices >= 1` and `<= max_sync_indices`.
- `initial_prechain_resolution.width > 0` and `height > 0`.
- `shader_dir_utf8` is required and non-empty.
- `cache_dir_utf8` is optional; null or empty disables disk cache.
- `VkDevice`, `VkPhysicalDevice`, and `VkQueue` must remain valid until destroy.

```c
goggles_chain_vk_create_info_t ci = goggles_chain_vk_create_info_init();
ci.target_format = VK_FORMAT_R8G8B8A8_UNORM;
ci.num_sync_indices = 1u;
ci.shader_dir_utf8 = "./shaders";
ci.cache_dir_utf8 = "./cache";
ci.initial_prechain_resolution.width = width;
ci.initial_prechain_resolution.height = height;

goggles_chain_t* chain = NULL;
goggles_chain_status_t st = goggles_chain_create_vk(vk, &ci, &chain);
if (st != GOGGLES_CHAIN_STATUS_OK) {
    return st;
}
```

On create failure, `*out_chain` is set to `NULL`.

## 3) Load presets

Preset load is synchronous in v1 and remains host-managed for async orchestration.

- `goggles_chain_preset_load(chain, path)` expects non-empty NUL-terminated UTF-8.
- `goggles_chain_preset_load_ex(chain, bytes, len)` expects non-null bytes and `len > 0`; bytes must be UTF-8 with no embedded NUL.

Behavior:

- Success transitions `CREATED -> READY` or keeps `READY`.
- Failure leaves runtime state unchanged.
- If runtime was already `READY`, a failed load keeps the previous active preset.

## 4) Record commands each frame

`goggles_chain_record_vk` records commands only. Submission/presentation stay in host code.

### Record preconditions

- Runtime must be `READY`.
- `record_info.struct_size` must be valid.
- `command_buffer` must be in recording state.
- `frame_index < num_sync_indices` used at create.
- `source_image` and `source_view` must refer to the same source image/subresource.
- Source image layout for `source_view`: `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.
- Target image layout for `target_view`: `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`.

### Record postconditions

- On success, source remains shader-read-only and target remains color-attachment-optimal.
- On `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`, no commands are recorded.
- On `GOGGLES_CHAIN_STATUS_VULKAN_ERROR` or `GOGGLES_CHAIN_STATUS_RUNTIME_ERROR`, command buffer contents are unspecified; reset/re-record before submit.

```c
goggles_chain_vk_record_info_t ri = goggles_chain_vk_record_info_init();
ri.command_buffer = cmd;
ri.source_image = src_image;
ri.source_view = src_view;
ri.source_extent.width = width;
ri.source_extent.height = height;
ri.target_view = dst_view;
ri.target_extent.width = width;
ri.target_extent.height = height;
ri.frame_index = frame_index;
ri.scale_mode = GOGGLES_CHAIN_SCALE_MODE_FIT;
ri.integer_scale = 1u; /* required >= 1 only when scale_mode is INTEGER; ignored otherwise */

goggles_chain_status_t st = goggles_chain_record_vk(chain, &ri);
```

Execution order is fixed: `prechain -> effect -> postchain`.

## 5) Update stage policy and prechain resolution

These APIs are valid in `CREATED` and `READY`:

- `goggles_chain_stage_policy_set/get`
- `goggles_chain_prechain_resolution_set/get`
- `goggles_chain_handle_resize`

Rules:

- Stage mask must be non-zero and contain only known stage bits.
- Resolution extents must have non-zero width and height.
- Resize may allocate and may block in v1.

## 6) Work with controls safely

Controls are mutable by `control_id` only.

### Listing controls

- `goggles_chain_control_list` returns all controls in deterministic order: prechain, then effect, then postchain.
- `goggles_chain_control_list_stage` returns controls for one stage.
- For `GOGGLES_CHAIN_STAGE_POSTCHAIN`, v1 returns a valid empty snapshot (`count == 0`) with success.
- On list failure, `*out_snapshot` is set to `NULL` when the output pointer is non-null.

Snapshots are owned by the caller and must be destroyed.

```c
goggles_chain_control_snapshot_t* snapshot = NULL;
goggles_chain_status_t st = goggles_chain_control_list(chain, &snapshot);
if (st == GOGGLES_CHAIN_STATUS_OK) {
    size_t count = goggles_chain_control_snapshot_get_count(snapshot);
    const goggles_chain_control_desc_t* desc = goggles_chain_control_snapshot_get_data(snapshot);

    for (size_t i = 0; i < count; ++i) {
        /* desc[i].control_id is the mutation key */
    }
}
(void)goggles_chain_control_snapshot_destroy(&snapshot);
```

Descriptor strings are borrowed memory valid only while the owning snapshot is alive.

### Mutating controls

- `goggles_chain_control_set_value`: finite values are clamped to `[min_value, max_value]`.
- Non-finite inputs (`NaN`, `+Inf`, `-Inf`) are rejected with `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT` and do not mutate state.
- `goggles_chain_control_reset_value` resets one control by id.
- `goggles_chain_control_reset_all` resets all controls.

## 7) Threading model and call serialization

For one `goggles_chain_t` instance:

- No internal synchronization is provided.
- All API calls (including getters and list APIs) must be externally serialized.
- Calls are non-reentrant.

Across different `goggles_chain_t` instances:

- Concurrent use is allowed.

Global metadata helpers are thread-safe and reentrant:

- `goggles_chain_api_version`
- `goggles_chain_abi_version`
- `goggles_chain_capabilities_get`
- `goggles_chain_status_to_string`

Memory visibility guarantee: a successful call return on one runtime happens-before the next externally serialized call on that same runtime.

## 8) Error handling and diagnostics

All fallible APIs return `goggles_chain_status_t`.

- `goggles_chain_status_to_string` returns stable static strings and never allocates.
- Unknown status values map to `"UNKNOWN_STATUS"`.
- Optional per-runtime diagnostics are available through `goggles_chain_error_last_info_get` when capability flag `GOGGLES_CHAIN_CAPABILITY_LAST_ERROR_INFO` is set.
- If that capability flag is absent, `goggles_chain_error_last_info_get` returns `GOGGLES_CHAIN_STATUS_NOT_SUPPORTED` and leaves output unchanged.
- Diagnostics are updated on failed chain calls and remain available until replaced by a later failed chain call.

Recommended pattern:

```c
goggles_chain_status_t st = goggles_chain_record_vk(chain, &ri);
if (st != GOGGLES_CHAIN_STATUS_OK) {
    const char* msg = goggles_chain_status_to_string(st);

    goggles_chain_error_last_info_t info = goggles_chain_error_last_info_init();
    if (goggles_chain_error_last_info_get(chain, &info) == GOGGLES_CHAIN_STATUS_OK) {
        /* use info.status / info.vk_result / info.subsystem_code for triage */
    }
}
```

## 9) Cleanup

- Destroy control snapshots with `goggles_chain_control_snapshot_destroy(&snapshot)`.
- Destroy runtime with `goggles_chain_destroy(&chain)`.
- Destroy APIs are null-safe, idempotent, and null out owned handles.

## 10) Practical checklist

- Validate `struct_size` for all extensible structs (use init helpers).
- Keep `frame_index` inside `[0, num_sync_indices)`.
- Serialize calls per runtime instance.
- Do not read snapshot-borrowed pointers after snapshot destroy.
- Keep host Vulkan handles valid for runtime lifetime.
