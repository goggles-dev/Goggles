# RetroArch Shader Workflow

## Purpose

Describes the core flow for loading and running RetroArch shader presets (`.slangp`) in Goggles.

## Workflow

### 1. Select a preset

Goggles reads a preset path from config or from the runtime UI. If no preset is selected, the
effect stage stays in passthrough mode.

### 2. Parse the preset

The preset loader reads the `.slangp` file, resolves referenced shader files, and collects pass
metadata such as scaling, filtering, feedback, and history requirements.

### 3. Preprocess shader sources

RetroArch shader sources are preprocessed before compilation:

- resolve includes
- extract `#pragma parameter` metadata
- split combined `.slang` files into vertex and fragment stages

### 4. Compile and reflect

Goggles compiles the generated GLSL through Slang, then reflects resource bindings and push
constants for each pass.

### 5. Build the filter chain

The filter chain allocates the pass sequence, intermediate framebuffers, and any history or
feedback resources required by the preset.

### 6. Record passes each frame

For every frame, the chain records passes in order and binds the semantics expected by RetroArch
shaders, including:

- `Source`
- `Original`
- `OriginalHistory#`
- `PassOutput#`
- `PassFeedback#`

### 7. Present the result

The output pass writes the final processed image to the viewer swapchain.

## Runtime Controls

The ImGui overlay can:

- enable or disable the effect stage
- apply or reload presets
- adjust exposed shader parameters at runtime

## References

- [Filter Chain](filter_chain_workflow.md) - Detailed render-pass flow
- [Shader Compatibility Report](shader_compatibility.md) - Batch compilation status
- [RetroArch Slang spec](https://docs.libretro.com/development/shader/slang-shaders/)
