# Filter Chain C API Spec (Vulkan v1)

## 1. Purpose and scope

This specification defines the public C ABI contract for the standalone filter-chain runtime.

### In scope

- Public v1 C API behavior and ABI expectations.
- Runtime lifecycle, preset loading, resize handling, frame recording, and control APIs.
- Threading, ownership, error handling, and extensibility rules.

### Out of scope for v1

- Dynamic loader-table API surface.
- Non-Vulkan backends.
- Exposure of internal pass graph or shader runtime implementation types.

## 2. Locked v1 decisions

- Public symbols/types use the `goggles_chain*` namespace.
- Errors are status-code-first (`goggles_chain_status_t`).
- Structured diagnostics are optional but queried through a mandatory symbol.
- API surface is link-time only in v1.
- All symbols declared in the v1 header are mandatory ABI exports.
- Control listing is snapshot-based (not iterator/callback).
- Stage model is first-class: `prechain`, `effect`, `postchain`.
- Enum-like API scalars are fixed-width `uint32_t` typedef families.
- Every exported function uses explicit calling convention macro `GOGGLES_CHAIN_CALL`.
- Base ABI layer and convenience layer are separated (inline init helpers only for convenience).
- Async preset reload orchestration remains host-managed in v1.
- Callback-based logging/telemetry hooks are excluded from v1 to avoid reentrancy ambiguity.
- No global mutable runtime singleton is introduced in v1.

## 3. Current boundary behavior preserved

v1 preserves behavior from the existing C++ boundary:

- Runtime lifecycle create/destroy and preset load/resize/record semantics.
- Stage policy and prechain resolution control surfaces.
- Curated control listing and mutation by stable `control_id`.
- Clamp-on-set behavior for finite control-value updates.
- Deterministic list ordering for full control listing: prechain, effect, postchain.
- Stage execution ordering invariant: `prechain -> effect -> postchain`.

## 4. Public header and ABI model

The API is defined by `include/goggles_filter_chain.h` and includes:

- Export/linkage macros: `GOGGLES_CHAIN_API`, `GOGGLES_CHAIN_CALL`, `GOGGLES_CHAIN_DEPRECATED`.
- Version macros: `GOGGLES_CHAIN_MAKE_VERSION`, `GOGGLES_CHAIN_API_VERSION`, `GOGGLES_CHAIN_ABI_VERSION`.
- Struct helper macro: `GOGGLES_CHAIN_STRUCT_SIZE(type)`.
- Opaque handles: `goggles_chain_t`, `goggles_chain_control_snapshot_t`.
- ABI-visible constants and fixed-width scalar typedef families.

### 4.1 Scalar domains and constants

`goggles_chain_status_t` values:

- `GOGGLES_CHAIN_STATUS_OK`
- `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`
- `GOGGLES_CHAIN_STATUS_NOT_INITIALIZED`
- `GOGGLES_CHAIN_STATUS_NOT_FOUND`
- `GOGGLES_CHAIN_STATUS_PRESET_ERROR`
- `GOGGLES_CHAIN_STATUS_IO_ERROR`
- `GOGGLES_CHAIN_STATUS_VULKAN_ERROR`
- `GOGGLES_CHAIN_STATUS_OUT_OF_MEMORY`
- `GOGGLES_CHAIN_STATUS_NOT_SUPPORTED`
- `GOGGLES_CHAIN_STATUS_RUNTIME_ERROR`

`goggles_chain_stage_t` domain:

- `GOGGLES_CHAIN_STAGE_PRECHAIN`
- `GOGGLES_CHAIN_STAGE_EFFECT`
- `GOGGLES_CHAIN_STAGE_POSTCHAIN`

`goggles_chain_stage_mask_t` bits:

- `GOGGLES_CHAIN_STAGE_MASK_PRECHAIN`
- `GOGGLES_CHAIN_STAGE_MASK_EFFECT`
- `GOGGLES_CHAIN_STAGE_MASK_POSTCHAIN`
- `GOGGLES_CHAIN_STAGE_MASK_ALL`

`goggles_chain_scale_mode_t` domain:

- `GOGGLES_CHAIN_SCALE_MODE_STRETCH`
- `GOGGLES_CHAIN_SCALE_MODE_FIT`
- `GOGGLES_CHAIN_SCALE_MODE_INTEGER`

`goggles_chain_capability_flags_t` bits include:

- `GOGGLES_CHAIN_CAPABILITY_NONE`
- `GOGGLES_CHAIN_CAPABILITY_LAST_ERROR_INFO`

## 5. Runtime lifecycle and state machine

### States

- `CREATED`: successful create, no successful preset load yet.
- `READY`: at least one successful preset load completed.
- `DEAD`: runtime destroyed and handle nulled.

### Transitions

- `goggles_chain_create_vk/_ex`: none -> `CREATED`.
- `goggles_chain_preset_load/_ex` success: `CREATED -> READY`, `READY -> READY`.
- `goggles_chain_preset_load/_ex` failure: state unchanged.
- `goggles_chain_destroy`: `CREATED|READY -> DEAD`.

### State-gated call groups

- `READY` only:
  - `goggles_chain_record_vk`
  - `goggles_chain_control_list`
  - `goggles_chain_control_list_stage`
  - `goggles_chain_control_set_value`
  - `goggles_chain_control_reset_value`
  - `goggles_chain_control_reset_all`
- `CREATED` or `READY`:
  - `goggles_chain_stage_policy_set/get`
  - `goggles_chain_prechain_resolution_set/get`
  - `goggles_chain_handle_resize`

Invalid state returns `GOGGLES_CHAIN_STATUS_NOT_INITIALIZED` unless explicitly documented otherwise.

## 6. Threading, reentrancy, and memory visibility

- No internal synchronization is provided for one `goggles_chain_t` instance.
- Caller must externally serialize all calls on the same runtime instance, including getters/listing APIs.
- API calls are non-reentrant per runtime instance.
- Different runtime instances may be used concurrently.
- Runtime does not spawn hidden background mutation threads in v1.
- Global metadata helpers are thread-safe and reentrant:
  - `goggles_chain_api_version`
  - `goggles_chain_abi_version`
  - `goggles_chain_capabilities_get`
  - `goggles_chain_status_to_string`
- Successful return from a runtime call happens-before the next externally serialized call on that runtime.

## 7. Nullability and `struct_size` contract

### Nullability

- Pointer parameters are non-null unless explicitly documented nullable.
- Nullable descriptor field in v1: `goggles_chain_control_desc_t.description_utf8`.
- Null runtime/snapshot handles return `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`, except null-safe destroy APIs and explicit null-tolerant snapshot query helpers (`goggles_chain_control_snapshot_get_count(NULL)`, `goggles_chain_control_snapshot_get_data(NULL)`).

### `struct_size` prefix rules

For extensible structs:

- If `struct_size < sizeof(v1_struct)`: return `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`.
- If `struct_size >= sizeof(v1_struct)`: read/write only known v1 prefix.
- Unknown input tail bytes are ignored.
- Unknown output tail bytes are left untouched.

### `struct_size` enforcement matrix

| API | Struct parameter | Direction | Rule |
|---|---|---|---|
| `goggles_chain_create_vk` | `goggles_chain_vk_create_info_t` | input | `struct_size >= sizeof(goggles_chain_vk_create_info_t)` |
| `goggles_chain_create_vk_ex` | `goggles_chain_vk_create_info_ex_t` | input | `struct_size >= sizeof(goggles_chain_vk_create_info_ex_t)` |
| `goggles_chain_record_vk` | `goggles_chain_vk_record_info_t` | input | `struct_size >= sizeof(goggles_chain_vk_record_info_t)` |
| `goggles_chain_stage_policy_set` | `goggles_chain_stage_policy_t` | input | `struct_size >= sizeof(goggles_chain_stage_policy_t)` |
| `goggles_chain_stage_policy_get` | `goggles_chain_stage_policy_t` | output | `struct_size >= sizeof(goggles_chain_stage_policy_t)`; failure keeps output unchanged |
| `goggles_chain_capabilities_get` | `goggles_chain_capabilities_t` | output | `struct_size >= sizeof(goggles_chain_capabilities_t)`; failure keeps output unchanged |
| `goggles_chain_error_last_info_get` | `goggles_chain_error_last_info_t` | output | `struct_size >= sizeof(goggles_chain_error_last_info_t)`; failure keeps output unchanged |

## 8. Error model and out-parameter policy

- Fallible APIs return `goggles_chain_status_t`; no exceptions/abort contract.
- Optional unsupported behavior returns `GOGGLES_CHAIN_STATUS_NOT_SUPPORTED`.
- `goggles_chain_status_to_string` returns stable static text and never allocates.
- Unknown status values map to `"UNKNOWN_STATUS"`.

Out-parameter policy:

- On success: documented outputs are fully initialized.
- On failure: outputs are unchanged, except:
  - create APIs set `*out_chain = NULL` when output pointer is non-null.
  - control-list APIs set `*out_snapshot = NULL` when output pointer is non-null.

Unless explicitly documented, failure leaves runtime state unchanged and runtime remains usable for later serialized calls.

## 9. Validation and robustness contract

- All extents require `width > 0` and `height > 0`.
- `num_sync_indices` must be in `[1, max_sync_indices]`.
- `frame_index` must be `< num_sync_indices` used at create.
- `integer_scale >= 1` when `scale_mode == GOGGLES_CHAIN_SCALE_MODE_INTEGER`; otherwise `integer_scale` is ignored.
- Stage policy mask must be non-zero and contain only known bits.
- Invalid enum/scalar values, malformed lengths, invalid pointers, and invalid struct sizes return `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`.
- Control set with non-finite values (`NaN`, `+Inf`, `-Inf`) returns invalid-argument and does not mutate state.
- Documented invalid-input paths must be no-crash.

## 10. String/path and FFI contract

- `_ex` functions accept explicit byte spans and do not require NUL termination.
- `_ex` path bytes must be valid UTF-8 and contain no embedded `\0`.
- `goggles_chain_preset_load(path)` is equivalent to `_ex(path, strlen(path))` for valid NUL-terminated UTF-8 input.

Create-time paths:

- `goggles_chain_vk_create_info_t.shader_dir_utf8`: required, non-empty UTF-8 C string.
- `goggles_chain_vk_create_info_t.cache_dir_utf8`: optional; null or empty disables disk cache.
- `goggles_chain_vk_create_info_ex_t.shader_dir_utf8`: required with `shader_dir_len > 0`.
- `goggles_chain_vk_create_info_ex_t.cache_dir_utf8`: optional; `cache_dir_len == 0` disables disk cache and allows null pointer.

## 11. Ownership and lifetime contract

- Runtime does not retain caller-provided input pointers after call return.
- Caller may mutate/free input structs and path buffers after return.
- `VkDevice`, `VkPhysicalDevice`, and `VkQueue` in create context must remain valid until runtime destroy.
- `VkCommandBuffer`, `VkImage`, and `VkImageView` passed to record are borrowed for call duration only.

Handle provenance:

- Non-null handles must come from successful corresponding create/list APIs in the same process and remain live.
- Passing forged, stale, or already-destroyed non-null handles is undefined behavior.
- Implementations should return invalid-argument for detectably invalid handles.

Ownership matrix:

| API | Ownership on success | Release API |
|---|---|---|
| `goggles_chain_create_vk/_ex(..., out_chain)` | caller owns `*out_chain` | `goggles_chain_destroy(&chain)` |
| `goggles_chain_control_list* (..., out_snapshot)` | caller owns `*out_snapshot` | `goggles_chain_control_snapshot_destroy(&snapshot)` |
| `goggles_chain_control_snapshot_get_data(snapshot)` | borrowed pointer into snapshot storage | destroy snapshot |
| `goggles_chain_control_desc_t.name_utf8/description_utf8` | borrowed pointer into snapshot storage | destroy snapshot |

## 12. Record-path behavior contract

- Record API records commands only; submit/present are host responsibilities.
- Caller controls command buffer state and external image layout lifecycle.
- Preconditions:
  - source image layout for source view is `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`.
  - target image layout for target view is `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`.
- Success preserves those source/target layout states.
- On `GOGGLES_CHAIN_STATUS_INVALID_ARGUMENT`: no commands recorded.
- On `GOGGLES_CHAIN_STATUS_VULKAN_ERROR` or `GOGGLES_CHAIN_STATUS_RUNTIME_ERROR`: command buffer contents unspecified; caller must reset/re-record.
- Runtime only transitions/manages internal resources it owns.
- Stage execution order is invariant: `prechain -> effect -> postchain`.

## 13. Control contract

- `control_id` is the only mutation key.
- `control_id` is unique within active preset and stable across equivalent reload of same layout.
- Stage domain is closed in v1: prechain/effect/postchain.
- `goggles_chain_control_list` order is deterministic: prechain, effect, postchain.
- Listing model is snapshot-based; caller must free snapshot.
- `goggles_chain_control_desc_t` layout is frozen for all v1.x releases.
- Additive metadata in v1.x must use additive APIs, not in-place layout mutation.
- `goggles_chain_control_list_stage(..., POSTCHAIN, ...)` must return success with a valid empty snapshot in v1.
- Finite set-values are clamped to descriptor bounds.

## 14. Performance and observability contract

- Hot path (`goggles_chain_record_vk`) performs no heap allocation, file I/O, shader compilation, or blocking waits in v1.
- `goggles_chain_preset_load*` and `goggles_chain_handle_resize` may allocate and may block.
- No hidden global lock is introduced by v1 API.
- Version and capability negotiation:
  - `goggles_chain_api_version` returns packed semver.
  - `goggles_chain_abi_version` returns ABI major.
  - `goggles_chain_capabilities_get` reports optional behavior flags and limits.
  - Capability flags never indicate symbol presence.
- `goggles_chain_error_last_info_get` exposes stable numeric diagnostics with no dynamic allocation.

## 15. Function-by-function contract

### 15.1 Global metadata functions

| Function | Valid state | Preconditions | Postconditions / side effects | Failure behavior |
|---|---|---|---|---|
| `goggles_chain_api_version(void)` | n/a | none | returns packed semantic API version | n/a |
| `goggles_chain_abi_version(void)` | n/a | none | returns ABI major | n/a |
| `goggles_chain_capabilities_get(out_caps)` | n/a | `out_caps != NULL`; valid `struct_size` | fills known capability prefix | invalid args: status error, output unchanged |
| `goggles_chain_status_to_string(status)` | n/a | none | returns stable static C string; no allocation | unknown status -> `"UNKNOWN_STATUS"` |

### 15.2 Runtime lifecycle

| Function | Valid state | Preconditions | Postconditions / side effects | Failure behavior |
|---|---|---|---|---|
| `goggles_chain_create_vk(vk, create_info, out_chain)` | none (new runtime) | all pointers non-null; valid create struct; valid extents; `num_sync_indices` range; required paths valid | allocates runtime, enters `CREATED`, caller owns handle | sets `*out_chain = NULL`; no runtime created |
| `goggles_chain_create_vk_ex(vk, create_info, out_chain)` | none (new runtime) | same as create plus explicit-length UTF-8 rules (non-empty required spans, no embedded NUL) | same as create | sets `*out_chain = NULL`; no runtime created |
| `goggles_chain_destroy(&chain)` | `CREATED`, `READY`, null-safe | pointer to handle may be null; handle may be null | releases runtime if live, nulls handle, state becomes `DEAD` | always returns `GOGGLES_CHAIN_STATUS_OK`; idempotent |

### 15.3 Preset, resize, and record

| Function | Valid state | Preconditions | Postconditions / side effects | Failure behavior |
|---|---|---|---|---|
| `goggles_chain_preset_load(chain, path)` | `CREATED` or `READY` | non-null chain; non-empty NUL-terminated UTF-8 path | on success enters/keeps `READY`; runtime may allocate/block | state unchanged; if already `READY`, previous preset stays active |
| `goggles_chain_preset_load_ex(chain, bytes, len)` | `CREATED` or `READY` | non-null chain; non-null bytes; `len > 0`; valid UTF-8; no embedded NUL | same as above | same as above |
| `goggles_chain_handle_resize(chain, new_extent)` | `CREATED` or `READY` | non-null chain; extent width/height > 0 | updates runtime target sizing context; may allocate/block | invalid args return error; runtime remains usable |
| `goggles_chain_record_vk(chain, record_info)` | `READY` | non-null chain; valid struct; command buffer in recording state; frame_index bounds; valid source/target handles/layouts; source image matches source view | records commands only; no submit/present; stage order `prechain -> effect -> postchain` | invalid-arg: records no commands; Vulkan/runtime error: command buffer contents unspecified |

### 15.4 Runtime policy and resolution

| Function | Valid state | Preconditions | Postconditions / side effects | Failure behavior |
|---|---|---|---|---|
| `goggles_chain_stage_policy_set(chain, policy)` | `CREATED` or `READY` | non-null chain/policy; valid struct; non-zero stage mask; known bits only | updates enabled-stage policy | invalid args return error and do not mutate state |
| `goggles_chain_stage_policy_get(chain, out_policy)` | `CREATED` or `READY` | non-null chain/out pointer; valid output struct size | writes current stage policy | on failure output unchanged |
| `goggles_chain_prechain_resolution_set(chain, resolution)` | `CREATED` or `READY` | non-null chain; non-zero extent dimensions | updates prechain resolution policy | invalid args return error and do not mutate state |
| `goggles_chain_prechain_resolution_get(chain, out_resolution)` | `CREATED` or `READY` | non-null chain/out pointer | writes current prechain resolution | on failure output unchanged |

### 15.5 Control snapshot APIs

| Function | Valid state | Preconditions | Postconditions / side effects | Failure behavior |
|---|---|---|---|---|
| `goggles_chain_control_list(chain, out_snapshot)` | `READY` | non-null chain/out pointer | allocates snapshot; ordered prechain, effect, postchain | sets `*out_snapshot = NULL` on failure |
| `goggles_chain_control_list_stage(chain, stage, out_snapshot)` | `READY` | non-null chain/out pointer; valid stage enum | allocates stage-filtered snapshot; postchain yields valid empty snapshot in v1 | sets `*out_snapshot = NULL` on failure |
| `goggles_chain_control_snapshot_get_count(snapshot)` | any | none | returns number of descriptors in snapshot | returns `0` for null snapshot |
| `goggles_chain_control_snapshot_get_data(snapshot)` | any | none | returns borrowed descriptor array pointer | returns `NULL` for null snapshot |
| `goggles_chain_control_snapshot_destroy(&snapshot)` | any | pointer to snapshot handle may be null | frees snapshot if live and nulls handle | always `OK`; null-safe and idempotent |

### 15.6 Control mutation and diagnostics

| Function | Valid state | Preconditions | Postconditions / side effects | Failure behavior |
|---|---|---|---|---|
| `goggles_chain_control_set_value(chain, control_id, value)` | `READY` | non-null chain; valid control id; finite value | clamps value to descriptor bounds, applies value | non-finite -> invalid-arg with no mutation; missing control -> not-found |
| `goggles_chain_control_reset_value(chain, control_id)` | `READY` | non-null chain; valid control id | resets one control to default | missing control -> not-found |
| `goggles_chain_control_reset_all(chain)` | `READY` | non-null chain | resets all controls to defaults | invalid state/args return status error |
| `goggles_chain_error_last_info_get(chain, out_info)` | live runtime | non-null chain/out pointer; valid output struct size | writes latest per-runtime failed-call diagnostics (`status`, `vk_result`, `subsystem_code`) | returns `NOT_SUPPORTED` when capability absent; failure leaves output unchanged |

## 16. ABI/versioning policy

- v1.x patch releases: no source or ABI break.
- v1.x minor releases: additive APIs only; existing APIs remain available.
- v1.x behavioral compatibility: documented preconditions, postconditions, and error semantics are stable.
- Bug-fix tightening is allowed only where prior behavior was undocumented or ambiguous.
- ABI-incompatible changes require major ABI bump.
- No versioned symbol names in v1; namespace is `goggles_chain_`.
- Deprecated declarations should use `GOGGLES_CHAIN_DEPRECATED("message")` and remain available for at least two minor releases before removal in next ABI major.

## 17. C++ boundary mapping (reference)

| Current boundary | C API |
|---|---|
| n/a (public API metadata) | `goggles_chain_api_version(...)`, `goggles_chain_abi_version(...)` |
| n/a (capability negotiation) | `goggles_chain_capabilities_get(...)` |
| `FilterChain::create(...)` | `goggles_chain_create_vk(...)` |
| n/a (FFI create variant) | `goggles_chain_create_vk_ex(...)` |
| `shutdown()` / destructor | `goggles_chain_destroy(...)` |
| `load_preset(path)` | `goggles_chain_preset_load(...)` |
| n/a (FFI preset variant) | `goggles_chain_preset_load_ex(...)` |
| `handle_resize(extent)` | `goggles_chain_handle_resize(...)` |
| `record(...)` | `goggles_chain_record_vk(...)` |
| `set_stage_policy(...)` | `goggles_chain_stage_policy_set(...)` |
| n/a (readback helper) | `goggles_chain_stage_policy_get(...)` |
| `set_prechain_resolution(...)` | `goggles_chain_prechain_resolution_set(...)` |
| `get_prechain_resolution()` | `goggles_chain_prechain_resolution_get(...)` |
| `list_controls()` / `list_controls(stage)` | `goggles_chain_control_list*` + `goggles_chain_control_snapshot_*` |
| `set_control_value(...)` | `goggles_chain_control_set_value(...)` |
| `reset_control_value(...)` | `goggles_chain_control_reset_value(...)` |
| `reset_controls()` | `goggles_chain_control_reset_all(...)` |
| n/a (diagnostics query) | `goggles_chain_error_last_info_get(...)` |

## 18. Migration sequence (recommended)

1. Add public C header and thin C shim over current boundary classes.
2. Lock ABI-visible scalar widths and apply `GOGGLES_CHAIN_CALL` to all exports.
3. Keep runtime behavior parity while swapping host usage onto C surface.
4. Add `_ex` create/load variants plus capabilities and diagnostics queries.
5. Keep async reload orchestration host-managed and keep C API preset load synchronous in v1.
6. Add/keep contract tests for ordering, clamping, swap-safety, lifecycle matrix, out-param semantics, and idempotent destroy behavior.
7. Add tests for `frame_index` bounds, UTF-8/path validation, and non-finite control-value rejection.
8. Freeze v1 ABI and publish packaging/install shape.

## 19. Conformance test matrix (v1)

| Area | Unit tests | Integration tests |
|---|---|---|
| Lifecycle/state machine | valid/invalid call-state matrix; post-failure invariants | create->load->record->destroy host flow |
| Input validation | nulls, invalid enums, bad lengths, zero extents, NaN/Inf | malformed preset path handling through shim |
| Out-param semantics | unchanged-on-failure and forced-null exceptions | FFI harness checks for safe failure behavior |
| Threading contract | serialization and non-reentrancy checks | multi-thread host test with per-instance concurrency |
| Record contract | `frame_index` bounds and no-command-on-invalid-arg | image-layout pre/post contract in real command buffers |
| Ownership/lifetime | snapshot borrowed-pointer lifetime; idempotent destroys | host Vulkan-handle lifetime test across runtime life |
| ABI durability | `sizeof`/`offsetof` checks for public structs and typedef widths | cross-compiler shared/static smoke |
| Version/capability | packed semver, stable ABI major, capability semantics | feature-gated host behavior and `NOT_SUPPORTED` validation |

## 20. Security and privacy notes

- Preset files and paths are untrusted input.
- Documented invalid-input paths must return status codes without crashes.
- API performs no network I/O in v1.
- Runtime diagnostics must not expose sensitive absolute paths unless host explicitly opts in.

## 21. Open questions

1. Should v1.1 add optional callback-based telemetry hooks, and with which exact reentrancy/threading constraints?
2. Should `goggles_chain_status_to_string` be guaranteed as stable machine-readable token text in addition to human-readable text?
3. For future string-bearing APIs, should `_ex` variants be introduced immediately with each new API?
