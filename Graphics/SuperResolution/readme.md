# Super Resolution

A unified super resolution upscaling library that abstracts multiple hardware-accelerated and software-based
backends behind two interfaces: `ISuperResolutionFactory` (enumeration, configuration, and creation) and
`ISuperResolution` (per-frame execution). The module automatically discovers available upscaler implementations
at factory creation time based on the render device type.

## Supported Backends

| Variant              | Type     | Graphics API        | Description                                                      |
|----------------------|----------|---------------------|------------------------------------------------------------------|
| NVIDIA DLSS          | Temporal | D3D11, D3D12, Vulkan| Deep-learning based temporal upscaler via NVIDIA NGX SDK         |
| Microsoft DirectSR   | Temporal | D3D12               | Windows built-in temporal upscaler via DirectSR API              |
| AMD FSR              | Spatial  | All                 | Shader-based spatial upscaler (Edge Adaptive Upsampling + Contrast Adaptive Sharpening) |
| Apple MetalFX Spatial| Spatial  | Metal               | Hardware-accelerated spatial upscaler via MetalFX framework      |
| Apple MetalFX Temporal| Temporal| Metal               | Hardware-accelerated temporal upscaler via MetalFX framework     |

## Spatial vs Temporal Upscaling

**Spatial** upscaling operates on a single frame. It requires only the low-resolution color texture as input and
produces an upscaled image using edge-aware filtering and optional sharpening. No motion vectors, depth buffer,
or jitter pattern is needed.

**Temporal** upscaling accumulates information from multiple frames. In addition to the color texture it requires:

- **Depth buffer** — for reprojection and disocclusion detection
- **Motion vectors** — per-pixel 2D motion in pixel space
- **Jitter offset** — sub-pixel offset applied to the projection matrix each frame (typically a Halton sequence)

Optional temporal inputs include:

- **Exposure texture** — a 1x1 texture containing the exposure scale value in the R channel.
  Ignored when `SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE` is set.
- **Reactive mask** — per-pixel value in [0, 1] controlling how much the current frame
  influences the final result. A value of 0.0 uses normal temporal accumulation; values closer
  to 1.0 reduce reliance on history. Useful for alpha-blended objects, particles, or areas
  with inaccurate motion vectors. It is recommended to clamp the maximum reactive value to
  around 0.9, as values very close to 1.0 rarely produce good results.
- **Ignore history mask** — a binary per-pixel mask where non-zero values
  indicate that temporal history should be completely discarded for that pixel (e.g. newly
  revealed areas after disocclusion).

## Jitter

Temporal upscalers rely on sub-pixel jitter applied to the projection matrix each frame to reconstruct detail
above the input resolution. The upscaler provides a recommended jitter pattern (Halton 2,3 sequence by default)
via `ISuperResolution::GetJitterOffset()`. The returned values are in **pixel space** (typically in the (-0.5, 0.5) range)
and must be converted to **clip space** before being added to the projection matrix:

```cpp
float JitterX = 0, JitterY = 0;
pSR->GetJitterOffset(FrameIndex, JitterX, JitterY);

// Convert from pixel space to clip space
float JitterClipX = +JitterX / (0.5f * InputWidth);
float JitterClipY = -JitterY / (0.5f * InputHeight);

// Apply to projection matrix (offset the X and Y translation components)
ProjMatrix[2][0] += JitterClipX;
ProjMatrix[2][1] += JitterClipY;
```

The Y component is negated because the pixel-space Y axis points downward while the clip-space Y axis points upward.
The same `JitterX` / `JitterY` values in pixel space must also be passed to `ExecuteSuperResolutionAttribs`
so the upscaler can undo the jitter during reprojection.

For spatial upscaling, `GetJitterOffset()` returns zero for both components and jitter is not needed.

## Texture MIP Bias

When rendering at a lower resolution for upscaling, the GPU selects coarser mipmap levels because screen-space
derivatives are larger relative to the texture coordinate range. To preserve texture detail that the upscaler
will reconstruct, apply a negative MIP LOD bias to texture samplers:

$$
\text{MipBias} = \log_2\left(\frac{\text{InputWidth}}{\text{OutputWidth}}\right)
$$

The bias should be applied to all material texture samplers (albedo, normal, roughness, etc.) via
`SamplerDesc::MipLODBias`. This compensates for the lower render resolution and prevents the upscaled
image from looking blurry.

## Motion Vectors

The API expects per-pixel 2D motion vectors in **pixel space** using the `Previous − Current` convention.

Use `MotionVectorScaleX` / `MotionVectorScaleY` to convert motion vectors from their native space
and adjust the sign convention at execution time. For example, if the shader computes
`NDC_Current − NDC_Previous`:

```cpp
// Negate direction and convert NDC to pixel space
ExecAttribs.MotionVectorScaleX = -0.5f * static_cast<float>(InputWidth);
ExecAttribs.MotionVectorScaleY = +0.5f * static_cast<float>(InputHeight);
```

X is negative to flip direction; Y is positive because the direction flip and the NDC-to-pixel Y axis flip cancel out.
If motion vectors are already in the `Previous − Current` convention, use `+0.5` for X and `-0.5` for Y.

Motion vectors must use the same resolution as the source color image.

## Optimization Types

`SUPER_RESOLUTION_OPTIMIZATION_TYPE` controls the quality/performance trade-off and determines the recommended
input resolution relative to the output. Default scale factors used when the backend does not provide its own:

| Optimization Type         | Scale Factor | Render Resolution (% of output) |
|---------------------------|:------------:|:-------------------------------:|
| `MAX_QUALITY`             | 0.75         | 75%                             |
| `HIGH_QUALITY`            | 0.69         | 69%                             |
| `BALANCED`                | 0.56         | 56%                             |
| `HIGH_PERFORMANCE`        | 0.50         | 50%                             |
| `MAX_PERFORMANCE`         | 0.34         | 34%                             |

Use `ISuperResolutionFactory::GetSourceSettings()` to query the exact optimal input resolution for a given
backend, output resolution, and optimization type.

## Usage

### Creating the Factory

The factory is created per render device. On Windows, the module can be loaded as a shared library:

```cpp
#include "SuperResolutionFactoryLoader.h"

RefCntAutoPtr<ISuperResolutionFactory> pSRFactory;
LoadAndCreateSuperResolutionFactory(pDevice, &pSRFactory);
```

### Enumerating Available Variants

Query the list of upscaler variants supported by the current device:

```cpp
Uint32 NumVariants = 0;
pSRFactory->EnumerateVariants(NumVariants, nullptr);

std::vector<SuperResolutionInfo> Variants(NumVariants);
pSRFactory->EnumerateVariants(NumVariants, Variants.data());

for (const auto& Variant : Variants)
{
    // Variant.Name       - human-readable name (e.g. "DLSS", "FSR")
    // Variant.VariantId  - unique identifier for creation
    // Variant.Type       - SUPER_RESOLUTION_TYPE_SPATIAL or SUPER_RESOLUTION_TYPE_TEMPORAL
}
```

### Querying Optimal Render Resolution

Before creating the upscaler, query the recommended input resolution:

```cpp
SuperResolutionSourceSettingsAttribs SourceAttribs;
SourceAttribs.VariantId        = SelectedVariant.VariantId;
SourceAttribs.OutputWidth      = SCDesc.Width;
SourceAttribs.OutputHeight     = SCDesc.Height;
SourceAttribs.OutputFormat     = TEX_FORMAT_R11G11B10_FLOAT
SourceAttribs.Flags            = SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE;
SourceAttribs.OptimizationType = SUPER_RESOLUTION_OPTIMIZATION_TYPE_BALANCED;

SuperResolutionSourceSettings SourceSettings;
pSRFactory->GetSourceSettings(SourceAttribs, SourceSettings);
```

### Creating the Upscaler

The upscaler must be recreated when the variant, input resolution, or output resolution changes:

```cpp
SuperResolutionDesc SRDesc;
SRDesc.VariantId    = SelectedVariant.VariantId;
SRDesc.InputWidth   = SourceSettings.OptimalInputWidth;
SRDesc.InputHeight  = SourceSettings.OptimalInputHeight;
SRDesc.OutputWidth  = SCDesc.Width;
SRDesc.OutputHeight = SCDesc.Height;
SRDesc.Flags        = SUPER_RESOLUTION_FLAG_AUTO_EXPOSURE;

if (SupportsSharpness)
    SRDesc.Flags = SRDesc.Flags | SUPER_RESOLUTION_FLAG_ENABLE_SHARPENING;

if (IsTemporalUpscaling)
{
    SRDesc.ColorFormat  = TEX_FORMAT_R11G11B10_FLOAT;
    SRDesc.OutputFormat = TEX_FORMAT_R11G11B10_FLOAT;
    SRDesc.DepthFormat  = TEX_FORMAT_R32_FLOAT;
    SRDesc.MotionFormat = TEX_FORMAT_RG16_FLOAT;
}
else
{
    SRDesc.ColorFormat  = TEX_FORMAT_RGBA8_UNORM_SRGB;
    SRDesc.OutputFormat = TEX_FORMAT_RGBA8_UNORM_SRGB;
}

RefCntAutoPtr<ISuperResolution> pSR;
pSRFactory->CreateSuperResolution(SRDesc, &pSR);
```

### Per-Frame Execution (Temporal)

For temporal upscaling, apply the jitter offset to the projection matrix before rendering the scene
(see [Jitter](#jitter) for details), then execute the upscaler on the **pre-tone-mapped HDR** color buffer:

```cpp
float2 Jitter = {};
pSR->GetJitterOffset(FrameIndex, Jitter.x, Jitter.y);

ExecuteSuperResolutionAttribs ExecAttribs;
ExecAttribs.pContext              = pContext;
ExecAttribs.pColorTextureSRV      = pRadianceSRV;      // HDR pre-tone-mapped color
ExecAttribs.pDepthTextureSRV      = pDepthSRV;
ExecAttribs.pMotionVectorsSRV     = pMotionVectorsSRV;
ExecAttribs.pOutputTextureView    = pOutputUAV;
ExecAttribs.JitterX               = Jitter.x;
ExecAttribs.JitterY               = Jitter.y;
ExecAttribs.MotionVectorScaleX    = -0.5f * static_cast<float>(InputWidth);
ExecAttribs.MotionVectorScaleY    = +0.5f * static_cast<float>(InputHeight);
ExecAttribs.CameraNear            = ZNear;
ExecAttribs.CameraFar             = ZFar;
ExecAttribs.CameraFovAngleVert    = YFov;
ExecAttribs.TimeDeltaInSeconds    = ElapsedTime;
ExecAttribs.Sharpness             = Sharpness;
ExecAttribs.ResetHistory          = ResetHistory;

pSR->Execute(ExecAttribs);
```

**`ResetHistory`** should be set to `True` when temporal history is no longer valid:

- First frame after creating or recreating the upscaler
- Camera cut or teleportation (abrupt camera position change)
- Switching between upscaler variants
- Any event that invalidates the correspondence between the current and previous frames

When history is reset, the upscaler discards accumulated temporal data and produces output
based solely on the current frame, which may temporarily reduce quality.

**Depth and camera notes:**

- `DepthFormat` in `SuperResolutionDesc` must be the **SRV-compatible format** (e.g. `TEX_FORMAT_R32_FLOAT`),
  not the depth-stencil format (e.g. `TEX_FORMAT_D32_FLOAT`). Use the format of the depth texture's
  shader resource view.
- `CameraNear` and `CameraFar` assume depth Z values go from 0 at the near plane to 1 at the far plane.
  If using **reverse Z**, swap the two values so that `CameraNear` contains the far plane distance and
  `CameraFar` contains the near plane distance.

### Per-Frame Execution (Spatial)

For spatial upscaling, only the color texture and output are required. Execute after tone mapping:

```cpp
ExecuteSuperResolutionAttribs ExecAttribs;
ExecAttribs.pContext           = pContext;
ExecAttribs.pColorTextureSRV   = pToneMappedSRV;   // LDR tone-mapped color
ExecAttribs.pOutputTextureView = pOutputRTV;
ExecAttribs.Sharpness          = Sharpness;

pSR->Execute(ExecAttribs);
```

### Render Pipeline Order

The position of the super resolution pass in the rendering pipeline depends on the upscaling type:

**Temporal upscaling** (operates on HDR data, replaces TAA):

```
G-Buffer → Lighting → Super Resolution → Bloom → Tone Mapping → Gamma Correction
```

**Spatial upscaling** (operates on LDR data, after tone mapping):

```
G-Buffer → Lighting → TAA → Bloom → Tone Mapping → Super Resolution → Gamma Correction
```

## References

- [NVIDIA DLSS Programming Guide](https://github.com/NVIDIA/DLSS/blob/main/doc/DLSS_Programming_Guide_Release.pdf)
- [Microsoft DirectSR Specification](https://github.com/microsoft/DirectX-Specs/blob/master/DirectSR/DirectSR.md)
- [AMD FidelityFX Super Resolution](https://github.com/GPUOpen-Effects/FidelityFX-FSR/blob/master/docs/FidelityFX-FSR-Overview-Integration.pdf)
- [Apple MetalFX Documentation](https://developer.apple.com/documentation/metalfx)
