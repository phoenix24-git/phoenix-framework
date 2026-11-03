---
title: Unity components
nav_order: 5
---

# Unity components in Phoenix
{: .no_toc }

Every built-in Unity component has a Phoenix equivalent: a plain data struct stored in the
data-oriented ECS (archetype chunks), reflected for serialization, prefabs and the inspector,
and reachable through the Unity-style API (`GameObject`, `AddComponent<T>()`, `GetComponent<T>()`).
Scripts derive from **`MetaBehaviour`** (Phoenix's `MonoBehaviour`).

**141 components**: 13 simulated today, 128 data-only. Data-only components are fully usable as data (add, edit, save, query from systems);
their built-in behaviour lands with the server that owns it (physics, rendering, audio, …).

{: .note }
Vector/math types are the Phoenix foundation types (`Vector3`, `Quaternion`, `Color`, `Transform3D`),
in a right-handed, Y-up, −Z-forward space. The Unity importer converts Unity's left-handed data.

_Generated from `engine/components/catalogue.py` — edit that file, not this page._

1. TOC
{:toc}


## Core

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| Transform | `LocalTransform` | ✅ simulated | `position`, `rotation`, `scale` |
| Transform (world matrix) | `LocalToWorld` | ✅ simulated | `value` |
| Transform.parent | `Parent` | ✅ simulated | `value` |
| Transform children | `Children` | ✅ simulated | `items` |
| GameObject.name | `Name` | ✅ simulated | `value` |
| GameObject.tag | `Tag` | ✅ simulated | `value` |
| GameObject.layer | `Layer` | ✅ simulated | `value` |
| GameObject.activeSelf = false | `Inactive` | ✅ simulated | — |
| RectTransform | `RectTransform` | data | `anchorMin`, `anchorMax`, `anchoredPosition`, `sizeDelta`, `pivot` |

## Rendering

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| MeshFilter | `MeshFilter` | ✅ simulated | `mesh`, `primitive` |
| MeshRenderer | `MeshRenderer` | ✅ simulated | `color`, `materials`, `shadowCastingMode`, `receiveShadows`, `renderingLayerMask`, `sortingOrder` |
| SkinnedMeshRenderer | `SkinnedMeshRenderer` | data | `sharedMesh`, `materials`, `rootBone`, `bones`, `blendShapeWeights`, `updateWhenOffscreen` |
| Camera | `Camera` | ✅ simulated | `clearFlags`, `backgroundColor`, `cullingMask`, `projection`, `fieldOfView`, `orthographicSize`, `nearClipPlane`, `farClipPlane`, `rect`, `depth`, `targetTexture`, `allowHDR`, `allowMSAA` |
| Projector | `Projector` | data | `material`, `nearClipPlane`, `farClipPlane`, `fieldOfView`, `aspectRatio`, `orthographic`, `ignoreLayers` |
| Decal Projector | `DecalProjector` | data | `material`, `size`, `pivot`, `fadeFactor`, `drawDistance`, `renderingLayerMask` |
| LOD Group | `LODGroup` | data | `screenRelativeHeights`, `lodRenderers`, `fadeMode`, `animateCrossFading`, `size` |
| Occlusion Portal | `OcclusionPortal` | data | `open`, `center`, `size` |
| Occlusion Area | `OcclusionArea` | data | `center`, `size` |
| Streaming Controller | `StreamingController` | data | `streamingMipmapBias` |
| Sorting Group | `SortingGroup` | data | `sortingLayerId`, `sortingOrder` |
| Sprite Renderer | `SpriteRenderer` | data | `sprite`, `color`, `flipX`, `flipY`, `drawMode`, `size`, `material`, `sortingLayerId`, `sortingOrder`, `maskInteraction` |
| Sprite Mask | `SpriteMask` | data | `sprite`, `alphaCutoff`, `isCustomRangeActive` |
| Skybox | `Skybox` | data | `material` |
| Halo | `Halo` | data | `color`, `size` |
| Lens Flare | `LensFlare` | data | `flare`, `color`, `brightness`, `fadeSpeed` |
| Billboard Renderer | `BillboardRenderer` | data | `billboard`, `material` |

## Effects

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| Light | `Light` | ✅ simulated | `type`, `color`, `intensity`, `range`, `spotAngle`, `innerSpotAngle`, `areaSize`, `shadows`, `shadowStrength`, `bounceIntensity`, `lightmapBakeType`, `cookie`, `cullingMask`, `colorTemperature`, `useColorTemperature` |
| Light Probe Group | `LightProbeGroup` | data | `probePositions` |
| Light Probe Proxy Volume | `LightProbeProxyVolume` | data | `boundsSize`, `originCustom`, `gridResolutionX`, `gridResolutionY`, `gridResolutionZ` |
| Reflection Probe | `ReflectionProbe` | data | `mode`, `importance`, `intensity`, `boxProjection`, `blendDistance`, `size`, `center`, `resolution`, `hdr`, `customBakedTexture` |
| Visual Effect | `VisualEffect` | data | `visualEffectAsset`, `playRate`, `startSeed`, `resetSeedOnPlay` |
| Particle System | `ParticleSystem` | data | `duration`, `looping`, `startDelay`, `startLifetime`, `startSpeed`, `startSize`, `startColor`, `gravityModifier`, `simulationSpace`, `maxParticles`, `emissionRate`, `playOnAwake`, `material` |
| Particle System Force Field | `ParticleSystemForceField` | data | `shape`, `startRange`, `endRange`, `directionX`, `directionY`, `directionZ`, `gravity`, `rotationSpeed`, `drag` |
| Line Renderer | `LineRenderer` | data | `positions`, `startWidth`, `endWidth`, `startColor`, `endColor`, `loop`, `useWorldSpace`, `alignment`, `material` |
| Trail Renderer | `TrailRenderer` | data | `time`, `minVertexDistance`, `startWidth`, `endWidth`, `startColor`, `endColor`, `emitting`, `autodestruct`, `material` |
| Volume | `Volume` | data | `mode`, `weight`, `priority`, `blendDistance`, `profile` |
| Bloom | `Bloom` | data | `enabled`, `threshold`, `intensity`, `scatter`, `tint` |
| Color Grading | `ColorGrading` | data | `enabled`, `postExposure`, `contrast`, `saturation`, `hueShift`, `colorFilter`, `lookupTexture` |
| Depth of Field | `DepthOfField` | data | `enabled`, `focusDistance`, `aperture`, `focalLength` |
| Lens Distortion | `LensDistortion` | data | `enabled`, `intensity`, `scale`, `center` |
| Motion Blur | `MotionBlur` | data | `enabled`, `intensity`, `clamp` |
| Vignette | `Vignette` | data | `enabled`, `color`, `intensity`, `smoothness`, `center` |
| Chromatic Aberration | `ChromaticAberration` | data | `enabled`, `intensity` |
| Ambient Occlusion | `AmbientOcclusion` | data | `enabled`, `intensity`, `radius`, `directLightingStrength` |

## Physics

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| Rigidbody | `Rigidbody` | data | `mass`, `linearDamping`, `angularDamping`, `useGravity`, `isKinematic`, `interpolation`, `collisionDetectionMode`, `freezePosition`, `freezeRotation`, `linearVelocity`, `angularVelocity`, `centerOfMass` |
| Box Collider | `BoxCollider` | data | `isTrigger`, `material`, `center`, `size` |
| Sphere Collider | `SphereCollider` | data | `isTrigger`, `material`, `center`, `radius` |
| Capsule Collider | `CapsuleCollider` | data | `isTrigger`, `material`, `center`, `radius`, `height`, `direction` |
| Mesh Collider | `MeshCollider` | data | `isTrigger`, `material`, `sharedMesh`, `convex` |
| Wheel Collider | `WheelCollider` | data | `mass`, `radius`, `wheelDampingRate`, `suspensionDistance`, `forceAppPointDistance`, `center`, `springStiffness`, `springDamper`, `targetPosition`, `motorTorque`, `brakeTorque`, `steerAngle` |
| Terrain Collider | `TerrainCollider` | data | `terrainData`, `material`, `enableTreeColliders` |
| Fixed Joint | `FixedJoint` | data | `connectedBody`, `breakForce`, `breakTorque`, `enableCollision` |
| Spring Joint | `SpringJoint` | data | `connectedBody`, `anchor`, `connectedAnchor`, `spring`, `damper`, `minDistance`, `maxDistance`, `tolerance`, `breakForce`, `enableCollision` |
| Hinge Joint | `HingeJoint` | data | `connectedBody`, `anchor`, `axis`, `useSpring`, `useMotor`, `useLimits`, `minLimit`, `maxLimit`, `motorTargetVelocity`, `motorForce`, `breakForce`, `enableCollision` |
| Character Joint | `CharacterJoint` | data | `connectedBody`, `anchor`, `axis`, `swingAxis`, `lowTwistLimit`, `highTwistLimit`, `swing1Limit`, `swing2Limit`, `enableProjection` |
| Configurable Joint | `ConfigurableJoint` | data | `connectedBody`, `anchor`, `axis`, `secondaryAxis`, `xMotion`, `yMotion`, `zMotion`, `angularXMotion`, `angularYMotion`, `angularZMotion`, `linearLimit`, `targetPosition`, `targetVelocity`, `breakForce` |
| Character Controller | `CharacterController` | data | `slopeLimit`, `stepOffset`, `skinWidth`, `minMoveDistance`, `center`, `radius`, `height`, `velocity`, `isGrounded` |
| Constant Force | `ConstantForce` | data | `force`, `relativeForce`, `torque`, `relativeTorque` |
| Cloth | `Cloth` | data | `stretchingStiffness`, `bendingStiffness`, `damping`, `useGravity`, `friction`, `externalAcceleration`, `randomAcceleration` |
| Articulation Body | `ArticulationBody` | data | `jointType`, `mass`, `immovable`, `useGravity`, `linearDamping`, `angularDamping`, `jointFriction`, `anchorPosition`, `anchorRotation`, `lowerLimit`, `upperLimit`, `stiffness`, `damping`, `target` |
| Physics Material | `PhysicsMaterialOverride` | data | `dynamicFriction`, `staticFriction`, `bounciness`, `frictionCombine`, `bounceCombine` |

## Physics 2D

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| Rigidbody 2D | `Rigidbody2D` | data | `bodyType`, `mass`, `linearDamping`, `angularDamping`, `gravityScale`, `freezeRotation`, `simulated`, `linearVelocity`, `angularVelocity` |
| Box Collider 2D | `BoxCollider2D` | data | `isTrigger`, `offset`, `size`, `edgeRadius`, `material` |
| Circle Collider 2D | `CircleCollider2D` | data | `isTrigger`, `offset`, `radius`, `material` |
| Capsule Collider 2D | `CapsuleCollider2D` | data | `isTrigger`, `offset`, `size`, `vertical` |
| Polygon Collider 2D | `PolygonCollider2D` | data | `isTrigger`, `offset`, `points` |
| Edge Collider 2D | `EdgeCollider2D` | data | `isTrigger`, `offset`, `points`, `edgeRadius` |
| Composite Collider 2D | `CompositeCollider2D` | data | `geometryType`, `vertexDistance` |
| Tilemap Collider 2D | `TilemapCollider2D` | data | `isTrigger`, `maximumTileChangeCount` |
| Distance Joint 2D | `DistanceJoint2D` | data | `connectedBody`, `anchor`, `connectedAnchor`, `distance`, `maxDistanceOnly` |
| Fixed Joint 2D | `FixedJoint2D` | data | `connectedBody`, `dampingRatio`, `frequency` |
| Friction Joint 2D | `FrictionJoint2D` | data | `connectedBody`, `maxForce`, `maxTorque` |
| Hinge Joint 2D | `HingeJoint2D` | data | `connectedBody`, `anchor`, `useMotor`, `motorSpeed`, `maxMotorTorque`, `useLimits`, `lowerAngle`, `upperAngle` |
| Relative Joint 2D | `RelativeJoint2D` | data | `connectedBody`, `maxForce`, `maxTorque`, `linearOffset`, `angularOffset` |
| Slider Joint 2D | `SliderJoint2D` | data | `connectedBody`, `angle`, `useMotor`, `motorSpeed`, `useLimits`, `lowerTranslation`, `upperTranslation` |
| Spring Joint 2D | `SpringJoint2D` | data | `connectedBody`, `distance`, `dampingRatio`, `frequency` |
| Target Joint 2D | `TargetJoint2D` | data | `target`, `maxForce`, `dampingRatio`, `frequency` |
| Wheel Joint 2D | `WheelJoint2D` | data | `connectedBody`, `suspensionAngle`, `suspensionFrequency`, `suspensionDamping`, `useMotor`, `motorSpeed`, `maxMotorTorque` |
| Constant Force 2D | `ConstantForce2D` | data | `force`, `relativeForce`, `torque` |
| Area Effector 2D | `AreaEffector2D` | data | `forceAngle`, `forceMagnitude`, `forceVariation`, `drag`, `angularDrag` |
| Buoyancy Effector 2D | `BuoyancyEffector2D` | data | `surfaceLevel`, `density`, `linearDrag`, `angularDrag`, `flowAngle`, `flowMagnitude` |
| Platform Effector 2D | `PlatformEffector2D` | data | `useOneWay`, `surfaceArc`, `sideArc`, `rotationalOffset` |
| Surface Effector 2D | `SurfaceEffector2D` | data | `speed`, `speedVariation`, `forceScale` |
| Point Effector 2D | `PointEffector2D` | data | `forceMagnitude`, `forceVariation`, `distanceScale`, `forceMode` |

## Audio

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| Audio Listener | `AudioListener` | data | `velocityUpdateMode` |
| Audio Source | `AudioSource` | data | `clip`, `output`, `mute`, `playOnAwake`, `loop`, `priority`, `volume`, `pitch`, `panStereo`, `spatialBlend`, `dopplerLevel`, `spread`, `rolloffMode`, `minDistance`, `maxDistance` |
| Audio Reverb Zone | `AudioReverbZone` | data | `minDistance`, `maxDistance`, `reverbPreset` |
| Audio Low Pass Filter | `AudioLowPassFilter` | data | `cutoffFrequency`, `lowpassResonanceQ` |
| Audio High Pass Filter | `AudioHighPassFilter` | data | `cutoffFrequency`, `highpassResonanceQ` |
| Audio Echo Filter | `AudioEchoFilter` | data | `delay`, `decayRatio`, `wetMix`, `dryMix` |
| Audio Distortion Filter | `AudioDistortionFilter` | data | `distortionLevel` |
| Audio Reverb Filter | `AudioReverbFilter` | data | `reverbPreset`, `dryLevel`, `room`, `decayTime`, `reverbLevel` |
| Audio Chorus Filter | `AudioChorusFilter` | data | `dryMix`, `wetMix1`, `delay`, `rate`, `depth` |

## Video

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| Video Player | `VideoPlayer` | data | `source`, `clip`, `url`, `playOnAwake`, `isLooping`, `playbackSpeed`, `renderMode`, `targetTexture`, `targetCamera` |

## UI

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| Canvas | `Canvas` | data | `renderMode`, `worldCamera`, `planeDistance`, `pixelPerfect`, `sortingOrder` |
| Canvas Scaler | `CanvasScaler` | data | `uiScaleMode`, `scaleFactor`, `referenceResolution`, `matchWidthOrHeight`, `referencePixelsPerUnit` |
| Graphic Raycaster | `GraphicRaycaster` | data | `ignoreReversedGraphics`, `blockingMask` |
| Canvas Renderer | `CanvasRenderer` | data | `cullTransparentMesh` |
| Event System | `EventSystem` | data | `firstSelectedGameObject`, `sendNavigationEvents`, `pixelDragThreshold` |
| Standalone Input Module | `StandaloneInputModule` | data | `horizontalAxis`, `verticalAxis`, `submitButton`, `cancelButton`, `inputActionsPerSecond` |
| Text / TextMeshPro - Text | `Text` | data | `text`, `font`, `fontSize`, `color`, `alignment`, `richText`, `raycastTarget` |
| Image | `Image` | data | `sprite`, `color`, `material`, `type`, `fillAmount`, `preserveAspect`, `raycastTarget` |
| Raw Image | `RawImage` | data | `texture`, `color`, `uvRect`, `raycastTarget` |
| Button | `Button` | data | `interactable`, `targetGraphic`, `normalColor`, `highlightedColor`, `pressedColor`, `disabledColor` |
| Toggle | `Toggle` | data | `interactable`, `isOn`, `graphic`, `group` |
| Toggle Group | `ToggleGroup` | data | `allowSwitchOff` |
| Slider | `Slider` | data | `interactable`, `fillRect`, `handleRect`, `direction`, `minValue`, `maxValue`, `wholeNumbers`, `value` |
| Scrollbar | `Scrollbar` | data | `interactable`, `handleRect`, `direction`, `value`, `size`, `numberOfSteps` |
| Dropdown / TMP - Dropdown | `Dropdown` | data | `interactable`, `captionText`, `template_`, `value` |
| Input Field / TMP - Input Field | `InputField` | data | `interactable`, `text`, `characterLimit`, `contentType`, `placeholder`, `readOnly` |
| Scroll Rect | `ScrollRect` | data | `content`, `horizontal`, `vertical`, `movementType`, `elasticity`, `inertia`, `decelerationRate`, `scrollSensitivity`, `viewport` |
| Aspect Ratio Fitter | `AspectRatioFitter` | data | `aspectMode`, `aspectRatio` |
| Content Size Fitter | `ContentSizeFitter` | data | `horizontalFit`, `verticalFit` |
| Horizontal Layout Group | `HorizontalLayoutGroup` | data | `padding`, `spacing`, `childAlignment`, `childControlWidth`, `childControlHeight`, `childForceExpandWidth`, `childForceExpandHeight` |
| Vertical Layout Group | `VerticalLayoutGroup` | data | `padding`, `spacing`, `childAlignment`, `childControlWidth`, `childControlHeight`, `childForceExpandWidth`, `childForceExpandHeight` |
| Grid Layout Group | `GridLayoutGroup` | data | `padding`, `cellSize`, `spacing`, `childAlignment`, `constraint`, `constraintCount` |
| Layout Element | `LayoutElement` | data | `ignoreLayout`, `minWidth`, `minHeight`, `preferredWidth`, `preferredHeight`, `flexibleWidth`, `flexibleHeight` |
| UIDocument (UI Toolkit) | `UIDocument` | data | `panelSettings`, `visualTreeAsset`, `sortingOrder` |
| Event Trigger | `EventTrigger` | data | `triggers` |

## Animation

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| Animator | `Animator` | data | `controller`, `avatar`, `applyRootMotion`, `updateMode`, `cullingMode`, `speed` |
| Animation | `Animation` | data | `clip`, `clips`, `playAutomatically`, `wrapMode`, `animatePhysics` |
| Position Constraint | `PositionConstraint` | data | `sources`, `weight`, `translationOffset`, `constraintActive`, `locked` |
| Rotation Constraint | `RotationConstraint` | data | `sources`, `weight`, `rotationOffset`, `constraintActive`, `locked` |
| Scale Constraint | `ScaleConstraint` | data | `sources`, `weight`, `scaleOffset`, `constraintActive`, `locked` |
| Aim Constraint | `AimConstraint` | data | `sources`, `weight`, `aimVector`, `upVector`, `worldUpObject`, `constraintActive` |

## Navigation

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| NavMesh Agent | `NavMeshAgent` | data | `agentTypeId`, `baseOffset`, `speed`, `angularSpeed`, `acceleration`, `stoppingDistance`, `autoBraking`, `radius`, `height`, `obstacleAvoidanceType`, `avoidancePriority`, `destination`, `hasDestination` |
| NavMesh Obstacle | `NavMeshObstacle` | data | `shape`, `center`, `size`, `carving`, `carveOnlyStationary` |
| Off-Mesh Link | `OffMeshLink` | data | `startTransform`, `endTransform`, `costOverride`, `biDirectional`, `activated` |
| NavMesh Surface | `NavMeshSurface` | data | `agentTypeId`, `collectObjects`, `size`, `center`, `layerMask`, `navMeshData` |
| NavMesh Link | `NavMeshLink` | data | `startPoint`, `endPoint`, `width`, `costModifier`, `bidirectional`, `area` |

## Tilemap

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| Grid | `Grid` | data | `cellSize`, `cellGap`, `cellLayout` |
| Tilemap | `Tilemap` | data | `tiles`, `size`, `origin`, `color`, `tileAnchor` |
| Tilemap Renderer | `TilemapRenderer` | data | `mode`, `chunkSize`, `material`, `sortingOrder` |
| Grid Renderer | `GridRenderer` | data | `color`, `visible` |

## Terrain

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| Terrain | `Terrain` | data | `terrainData`, `layers`, `heightmapPixelError`, `basemapDistance`, `drawTreesAndFoliage`, `detailObjectDistance`, `treeDistance`, `materialTemplate` |
| Tree | `Tree` | data | `data` |
| Wind Zone | `WindZone` | data | `mode`, `radius`, `windMain`, `windTurbulence`, `windPulseMagnitude`, `windPulseFrequency` |

## Misc

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| Playable Director | `PlayableDirector` | data | `playableAsset`, `playOnAwake`, `extrapolationMode`, `timeUpdateMode`, `initialTime` |
| Physics Raycaster | `PhysicsRaycaster` | data | `eventMask`, `maxRayIntersections` |
| Physics 2D Raycaster | `Physics2DRaycaster` | data | `eventMask`, `maxRayIntersections` |

## Scripts

| Unity | Phoenix | Status | Fields |
|---|---|---|---|
| MonoBehaviour (Script) -> MetaBehaviour | `ScriptHost` | ✅ simulated | — |

## Not runtime components

Entries in Unity's lists that are assets, internals or duplicates, and where they live in Phoenix:

| Unity | In Phoenix |
|---|---|
| Physics Material | asset (PhysicsMaterial); per-entity override: PhysicsMaterialOverride |
| Terrain Layer | asset referenced by Terrain.layers |
| Sprite Atlas | asset produced by the importer |
| Tilemap Chunk | internal to TilemapRenderer (chunk mode) |
| Grid Layout | Grid.cellSize / cellLayout |
| Visual Element (UI Toolkit) | element inside a UIDocument tree (a Phoenix widget) |
| Fixed/Prismatic/Revolute/Spherical Link | ArticulationBody.jointType |
| Box Collider 3D (alternative definition) | BoxCollider |
| Force Field / Physics Scene Configurator | not Unity runtime components; physics settings live in phoenix.json |
| Studio Component (Unity Studio) | Unity Studio product feature, not an engine component |
