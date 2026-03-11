# Golden Reference Images

This directory contains reference PNG images used by the shader and diagnostics visual regression tests.

## What are golden images?

Golden images are pre-captured frames that represent the expected visual output of Goggles under specific configurations. The visual tests compare live-rendered output against these references pixel-by-pixel within a configurable tolerance.

## Tracked files

- `shader_bypass_quadrant.png` — output with no shader (passthrough mode), quadrant client input
- `shader_zfast_quadrant.png` — output with the zfast-crt shader applied, quadrant client input
- `runtime_format_pass0.png` / `runtime_format_pass1.png` — intermediate pass captures for the
  diagnostics format preset
- `runtime_history_frame1.png` / `runtime_history_frame3.png` — temporal final-output captures for
  the diagnostics history preset
- `runtime_history_pass0_frame1.png` / `runtime_history_pass0_frame3.png` — temporal intermediate
  captures for the diagnostics history preset

## How to generate golden images

Run the update task after a successful build:

```
pixi run update-golden
```

This rebuilds the project (debug preset by default) and captures fresh reference frames for the
existing shader regressions plus the diagnostics intermediate and temporal workflows. Override the
preset with the `BUILD_PRESET` environment variable:

```
BUILD_PRESET=release pixi run update-golden
```

## How to review golden images

Inspect the PNG files directly:

```
feh tests/golden/shader_bypass_quadrant.png
feh tests/golden/shader_zfast_quadrant.png
feh tests/golden/runtime_format_pass0.png
feh tests/golden/runtime_history_frame1.png
```

The bypass image should show the raw quadrant client output. The zfast image should show the same content with CRT scanline and curvature effects applied.

## Git LFS

The PNG files are tracked with Git LFS. After cloning, run:

```
git lfs pull
```

to download the actual image data. Without this step the files will contain LFS pointer text rather than image data.

## When to update

Regenerate golden images after any intentional change to:

- Shader code (`shaders/retroarch/crt/crt-lottes-fast.slangp` or dependencies)
- Render pipeline output (color space, blending, format)
- Headless capture logic
- Runtime diagnostics capture behavior or the `format.slangp` / `history.slangp` test presets

Do not update goldens to paper over unexpected regressions — investigate the cause first.
