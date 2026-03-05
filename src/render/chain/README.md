# Filter Chain C API (Vulkan v1)

This directory documents the public C API contract for extracting the Goggles filter chain as a standalone library.

## Audience and integration style

- Engine and render-backend integrators embedding the runtime into an existing Vulkan frame path.
- FFI and binding authors targeting host languages like Rust, Python, or C#.
- Plugin/module authors linking the library into a larger host process.
- Integration styles in v1: static linking, shared-library linking, and in-process embed in an existing render loop.
- Not a standalone process boundary in v1.

## What this project is

- A Vulkan-only C API surface for creating a filter-chain runtime, loading presets, recording frame commands, and managing controls.
- A stable v1 ABI boundary centered on opaque handles, explicit ownership, explicit status codes, and fixed-width public scalar types.
- A host-integrated runtime model where the host keeps ownership of command submission, presentation, and external synchronization.

## What this project is not

- Not a dynamic loader-table API in v1.
- Not a non-Vulkan backend in v1.
- Not an API that exposes internal pass graph or shader runtime implementation types.
- Not an API that performs implicit submit/present or runs hidden background mutation threads.

## Quick start

```c
#include "goggles_filter_chain.h"

goggles_chain_status_t render_once(
    const goggles_chain_vk_context_t* vk,
    VkCommandBuffer cmd,
    VkImage src_image,
    VkImageView src_view,
    VkImageView dst_view,
    uint32_t width,
    uint32_t height)
{
    goggles_chain_vk_create_info_t ci = goggles_chain_vk_create_info_init();
    ci.target_format = VK_FORMAT_R8G8B8A8_UNORM;
    ci.num_sync_indices = 1u; /* valid portable default; raise after capability query */
    ci.shader_dir_utf8 = "./shaders";
    ci.cache_dir_utf8 = "./cache";
    ci.initial_prechain_resolution.width = width;
    ci.initial_prechain_resolution.height = height;

    goggles_chain_t* chain = NULL;
    goggles_chain_status_t st = goggles_chain_create_vk(vk, &ci, &chain);
    if (st != GOGGLES_CHAIN_STATUS_OK) {
        return st;
    }

    st = goggles_chain_preset_load(chain, "./presets/example.slangp");
    if (st == GOGGLES_CHAIN_STATUS_OK) {
        goggles_chain_vk_record_info_t ri = goggles_chain_vk_record_info_init();
        ri.command_buffer = cmd;
        ri.source_image = src_image;
        ri.source_view = src_view;
        ri.source_extent.width = width;
        ri.source_extent.height = height;
        ri.target_view = dst_view;
        ri.target_extent.width = width;
        ri.target_extent.height = height;
        ri.frame_index = 0u;
        ri.scale_mode = GOGGLES_CHAIN_SCALE_MODE_FIT;
        st = goggles_chain_record_vk(chain, &ri);
    }

    (void)goggles_chain_destroy(&chain);
    return st;
}
```

## Versioning and ABI stability

- `goggles_chain_api_version()` returns packed semantic version (`major << 22 | minor << 12 | patch`).
- `goggles_chain_abi_version()` returns the ABI major.
- v1.x source compatibility: patch releases are non-breaking, minor releases are additive.
- v1.x binary compatibility: patch and minor releases keep ABI compatibility.
- v1.x behavioral compatibility: documented preconditions, postconditions, and error semantics are stable.
- Bug fixes may tighten behavior only where prior behavior was undocumented or ambiguous.
- Incompatible ABI layout/signature/calling-convention changes require a major ABI bump.
- All symbols declared by the v1 public header are mandatory exports for ABI v1.

## Full documentation

- Integration guide: `docs/integration_guide.md`
- Spec: `docs/spec.md`
- Source draft retained for review: `../../../docs/filter_chain_c_api_design_draft.md`
