---
title: Render pipelines (SRP-style)
parent: Engine roadmap
nav_order: 4
---

# Scriptable render pipelines: Mobile, Desktop, Ultra
{: .no_toc }

Unity separates *what* gets rendered (scene, materials) from *how* (a Scriptable Render Pipeline
asset), and ships URP for scalable hardware and HDRP for high-end. Godot ships Mobile, Forward+ and
Compatibility renderers. Phoenix combines the two ideas: a **frame graph** with a
**RenderPipeline asset** on top, and three built-in pipelines.

1. TOC
{:toc}

## Built-in pipelines

| Pipeline | Analogue | Targets | Lighting path | Highlights |
|---|---|---|---|---|
| **Mobile** | URP / Godot Mobile | phones, tablets, Switch-class hardware, low-end web | single-pass forward, clustered lights (capped), baked lightmaps + probes | MSAA, 1–2 CSM cascades, SSAO lite, FXAA, LDR/HDR-lite post, half-precision shading |
| **Desktop** | URP high / Godot Forward+ | PC, current consoles at 60 fps, web with WebGPU | clustered Forward+ (deferred optional), DDGI probes + SSGI | CSM with PCSS, GTAO, SSR, TAA/TSR, volumetric fog, full post |
| **Ultra** | HDRP + Unreal high end | high-end PC, current-gen consoles | visibility buffer + Virtual Geometry, Virtual Shadow Maps, Dynamic GI | RT shadows/reflections, stochastic many-lights, clouds, layered materials (needs the RHI backend for RT features) |

A project picks a default pipeline per platform in `phoenix.json`, and can switch at runtime (for
example, as a quality setting):

```json
"rendering": {
  "pipeline": { "default": "desktop", "web": "desktop", "android": "mobile", "ios": "mobile" },
  "quality": { "shadows": "high", "gi": "probes", "upscaler": "tsr" }
}
```

## The frame graph

All passes, built-in and custom, are declared against a frame graph (render graph). It handles:

- resource lifetimes and transient aliasing (memory comes from the VRAM pool),
- barriers and layout transitions (no-ops on SDL_GPU, explicit on slang-rhi),
- culling passes whose outputs are unused (debug views cost nothing when off),
- async-compute scheduling where the backend supports it.

```cpp
void DesktopPipeline::render(FrameGraph& fg, const ViewContext& view) {
    auto depth  = fg.add_pass<DepthPrepass>(view);
    auto lights = fg.add_pass<ClusterLights>(view, depth.output);
    auto ao     = fg.add_pass<Gtao>(view, depth.output);
    auto shadow = fg.add_pass<CascadedShadows>(view);
    auto color  = fg.add_pass<ForwardPlus>(view, depth.output, lights.output, ao.output, shadow.output);
    auto ssr    = fg.add_pass<ScreenSpaceReflections>(view, color.output, depth.output);
    auto taa    = fg.add_pass<TemporalUpscale>(view, ssr.output, depth.output);
    fg.add_pass<PostStack>(view, taa.output, view.target);
}
```

## Scriptable from C#

As with Unity's SRP, a C# `RenderPipelineAsset` can assemble passes. The passes themselves stay in
C++/Slang, and C# only describes the graph, so there is no per-frame managed overhead beyond
building it:

```csharp
public sealed class MyPipeline : RenderPipeline
{
    protected override void Render(FrameGraphBuilder fg, in ViewContext view)
    {
        var gbuffer = fg.AddPass(BuiltInPasses.GBuffer, view);
        var lit     = fg.AddPass(BuiltInPasses.DeferredLighting, view, gbuffer);
        fg.AddPass(MyPasses.ToonOutline, view, lit);   // custom Slang pass
        fg.AddPass(BuiltInPasses.PostStack, view, lit, view.Target);
    }
}
```

## Materials across pipelines

- Shaders are written in **Slang** against a stable surface interface (`IMaterial`:
  `albedo`, `normal`, `roughness`, `metallic`, `emissive`, `opacity`, …). Each pipeline supplies its
  own lighting model and generates specializations. One material works in all three pipelines,
  which removes the URP/HDRP material-conversion pain Unity users know.
- A node-based **Shader Graph** in the editor emits Slang.
- Godot `.gdshader` files can be imported for the common subset (spatial and canvas_item shaders),
  translated to Slang ([Godot formats](godot-formats.md)).

## Per-platform defaults

| Platform | Default | Notes |
|---|---|---|
| Web (WebGPU) | Desktop (reduced) | no RT/mesh shaders; Virtual Geometry v1 via compute |
| Web (WebGL2 fallback) | Mobile | compatibility path through SDL_Renderer/GLES-style features |
| Android / iOS | Mobile | Vulkan / Metal via SDL_GPU |
| PC | Desktop, Ultra optional | Ultra needs the slang-rhi backend for RT features |
| Consoles | Desktop / Ultra | partner SDK backends |
