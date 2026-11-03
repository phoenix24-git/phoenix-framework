---
title: Rendering features
parent: Engine roadmap
nav_order: 3
---

# Rendering features (Unreal-class, built from public research)
{: .no_toc }

This is the full feature catalogue for photorealistic, AAA-class rendering. Every row says which
**pipeline tier** gets it ([pipelines](render-pipelines.md)) and which **backend capability** it
needs. Techniques are implemented from public papers and talks, never from engine source (see the
[clean-room policy](index.md#clean-room-policy-read-before-contributing)).

Legend:
- **Tier**: **M** Mobile/Web · **D** Desktop · **U** Ultra
- **Backend**: **SG** works on SDL_GPU (incl. WebGPU) · **RHI** needs the slang-rhi backend (bindless / mesh shaders / ray tracing)

1. TOC
{:toc}

## Geometry

| Feature | What it is | Tier | Backend | Phase | References |
|---|---|---|---|---|---|
| Mesh LOD + LOD groups | discrete LODs, screen-size selection, cross-fade | M D U | SG | 2 | Unity LODGroup; meshoptimizer `simplify` |
| GPU frustum + occlusion culling | per-instance compute culling, two-pass hierarchical-Z (HZB) | D U | SG | 2–3 | Haar & Aaltonen, "GPU-Driven Rendering Pipelines" (SIGGRAPH 2015) |
| Meshlets / clusters | 64–128-triangle clusters with bounds and normal cones for culling | D U | SG | 3 | meshoptimizer `buildMeshlets` |
| **Virtual Geometry** (cluster-LOD DAG) | clusters grouped and simplified into a DAG; per-cluster LOD cut chosen on GPU so detail is roughly pixel-sized and seamless | U (D lite) | SG v1 (compute + indirect) · RHI v2 (mesh shaders) | 3–4 | Karis et al., "Nanite: A Deep Dive" (SIGGRAPH 2021); meshoptimizer `clusterlod` |
| Visibility buffer | write triangle/cluster ids, shade in a resolve pass; decouples geometry from materials | U | SG | 3 | Burns & Hunt, "The Visibility Buffer" (JCGT 2013) |
| Software rasterizer for tiny triangles | compute raster with 64-bit atomics for sub-pixel triangles | U | RHI (`atomics64`) | 4 | Karis 2021. On WebGPU (no 64-bit atomics) we fall back to hardware raster. |
| Geometry streaming | cluster pages streamed by visibility feedback | U | SG/RHI | 4 | |
| BVH | CPU dynamic BVH for culling/picking; GPU BLAS/TLAS for ray tracing | all / U | SG / RHI | 2 / 4 | Godot `DynamicBVH`; DXR/VK_KHR_ray_tracing |
| Skinning, morphs | GPU compute skinning into stream-out buffers (also feeds the RT BLAS refit) | M D U | SG | 3 | |
| Terrain | clipmap/CDLOD heightfield, virtual texturing, holes, tessellated displacement (Ultra) | M D U | SG | 3 | Strugar, "CDLOD" (2010) |
| Foliage & instancing | GPU instancing, wind, impostors / octahedral billboards | M D U | SG | 3 | |
| Hair & fur | strand rendering (Ultra), cards (Mobile/Desktop) | D U | SG/RHI | 4 | |

## Lighting and shadows

| Feature | What it is | Tier | Backend | Phase | References |
|---|---|---|---|---|---|
| PBR | metal/rough GGX, multi-scatter energy compensation, clear coat, sheen, anisotropy, subsurface (screen-space and profile), transmission | M D U | SG | 2 | Karis, "Real Shading in UE4" (SIGGRAPH 2013); Kulla & Conty 2017; Filament docs |
| Layered materials | a layered BSDF stack (coat over metal over …), similar in spirit to "Substrate" | U | SG | 4 | public SIGGRAPH 2023 course material |
| Clustered lighting | froxel light lists in compute; thousands of punctual lights | M D U | SG | 2 | Olsson et al., "Clustered Deferred and Forward Shading" (HPG 2012) |
| Cascaded shadow maps | 2–4 cascades, stabilization, PCF/PCSS, cascade blending | M D | SG | 2 | Microsoft "Cascaded Shadow Maps"; PCSS (Fernando 2005) |
| **Virtual Shadow Maps** | very high-resolution virtual shadow maps: a clipmap for the directional light, cube pages for local lights, page caching and invalidation, rendered only where visible | U | SG v1 (page tables in storage buffers) | 3 | public "Virtual Shadow Maps" talks/docs; "Sparse Virtual Shadow Maps" literature |
| Contact shadows | screen-space ray march for small-scale occlusion | D U | SG | 2 | |
| Ray-traced shadows | soft, area-light shadows, denoised | U | RHI | 4 | |
| **Stochastic Many-Lights** | per-pixel light sampling with reservoir resampling + RT visibility + denoiser, for hundreds of shadowed lights | U | RHI | 4 | Bitterli et al., "ReSTIR" (SIGGRAPH 2020); Wyman et al. course notes |
| Light probes / irradiance volumes | baked SH probes (Mobile), DDGI-style updated probes (Desktop) | M D | SG | 2–3 | Majercik et al., "DDGI" (JCGT 2019) |
| **Dynamic GI** | screen-space traces → SDF / surface-cache traces → hardware RT; infinite bounces via a radiance cache | U (D: probes + SSGI) | SG (SDF path) / RHI (RT path) | 3–4 | public Lumen talks (Wright et al., SIGGRAPH 2022); "Radiance Cascades" (Sannikov 2023) |
| Lightmaps | GPU path-traced baking in the editor, with directional lightmaps | M D | SG (bake: compute) | 3 | |
| Reflection probes | baked/real-time cubemaps, box projection, blending | M D U | SG | 2 | |
| **SSR** | hierarchical-Z screen-space reflections, roughness-aware, temporally filtered | D U | SG | 3 | Uludag, "Hi-Z Screen-Space Cone-Traced Reflections" (GPU Pro 5); Stachowiak, "Stochastic SSR" (2015) |
| Ray-traced reflections | hybrid: SSR first, RT fallback | U | RHI | 4 | |
| Ambient occlusion | GTAO (Desktop/Ultra), cheap SSAO (Mobile), RTAO (Ultra + RHI) | M D U | SG / RHI | 2 / 4 | Jimenez et al., "GTAO" (2016) |
| Area lights | LTC rect/disk/line lights | D U | SG | 3 | Heitz et al., "LTC" (SIGGRAPH 2016) |
| IES profiles, light functions, cookies | | D U | SG | 3 | |

## Atmosphere, volumes, effects

| Feature | Tier | Backend | Phase | References |
|---|---|---|---|---|
| Physically based sky & atmosphere (time of day) | M D U | SG | 3 | Hillaire, "A Scalable and Production Ready Sky and Atmosphere" (EGSR 2020) |
| Volumetric fog (froxels), light shafts | D U | SG | 3 | Wronski, "Volumetric Fog" (SIGGRAPH 2014) |
| Volumetric clouds | U (D low) | SG | 3 | Schneider, "Nubis" (SIGGRAPH 2015/2017) |
| Water (FFT ocean, refraction, caustics) | D U | SG | 4 | Tessendorf 2001 |
| Decals (deferred / clustered) | M D U | SG | 2 | |
| GPU particles (VFX graph style: emitters, events, collision vs depth/SDF) | M D U | SG | 3 | |

## Camera, temporal and post

| Feature | Tier | Backend | Phase | References |
|---|---|---|---|---|
| **TAA** | D U (M optional) | SG | 2 | Karis, "High Quality Temporal Supersampling" (SIGGRAPH 2014) |
| **Temporal Super Resolution** (render low, reconstruct high) | D U | SG | 3 | Jimenez, "Filmic SMAA" / temporal upscaling literature; FSR 2 (open source, MIT) as a study reference |
| FXAA / SMAA / MSAA | M D | SG | 2 | Jimenez et al., "SMAA" (2012) |
| Motion blur, depth of field (bokeh), bloom, lens flares, chromatic aberration, vignette, film grain | M D U | SG | 2–3 | Jimenez, "Next Generation Post Processing in CoD:AW" (2014) |
| Auto exposure (histogram), local exposure | M D U | SG | 2 | |
| Tonemapping (ACES, AgX, neutral) + color grading LUTs, HDR10 output | M D U | SG | 2 | |
| Upscaler plugins (DLSS / FSR / XeSS) | D U | RHI | 4 | vendor SDKs |

## Tooling that makes it AAA

- **Frame graph** with automatic barriers, transient aliasing and async-compute scheduling ([pipelines](render-pipelines.md)).
- **GPU profiler** (timestamp queries per pass) in DevTools and the editor.
- **Shader permutation management** with Slang specialization constants, plus a pipeline-state cache warmed at load.
- **Golden-image tests** per feature per tier in CI (SwiftShader / Lavapipe for software GPUs).
- **Debug views**: overdraw, cluster LOD colouring, shadow pages, light complexity, GI probes, motion vectors.

## Honest constraints

- **WebGPU today** has no mesh shaders, no ray tracing, no bindless and no 64-bit atomics. The web
  gets the Desktop tier at best, plus Virtual Geometry v1 (compute culling + indirect draws). The
  feature flags light up automatically as WebGPU extensions ship.
- **SDL_GPU** gives us compute, storage buffers/textures and indirect draws on every platform. That
  covers everything marked **SG**. The **RHI** column is why the [server architecture](servers.md)
  exists.
- **Consoles** need platform-holder SDKs under NDA. The RHI is designed so console backends live in
  private repositories.
