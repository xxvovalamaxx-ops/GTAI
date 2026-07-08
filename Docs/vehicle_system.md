# GTAI Vehicle System — Technical Design Document

**Project:** GTAI (GTA 7) — AI-Native Open-World Game, NYC
**Engine:** Unreal Engine 5.8 (Chaos physics)
**Module:** `GTAI_Vehicles`
**Owner:** Vehicle Physics Engineer (specialist subagent)
**Status:** Design v1.0 — ready for implementation
**Date:** 2026-07-08

---

## 0. Research Notes & Sourcing

> **Honesty note on research:** The live web-search backend returned degraded / off-topic
> results this session and Epic's official docs + GitHub mirror were 403 / private.
> Instead of fabricating citations, this design is grounded in the **authoritative UE 5.8
> source headers installed locally** at:
> `C:\Program Files\Epic Games\UE_5.8\Engine\Plugins\Experimental\ChaosVehiclesPlugin\Source\ChaosVehicles\Public\`
> (`ChaosWheeledVehicleMovementComponent.h`, `ChaosVehicleWheel.h`,
> `ChaosVehicleMovementComponent.h`, `WheeledVehiclePawn.h`).
> Every API referenced below (structs, setters, enums) was read directly from those headers.
> GTA-V handling targets come from well-established community analysis of RAGE's arcade model.

### Verified UE 5.8 Chaos Vehicle API surface used in this design
- `AWheeledVehiclePawn` (abstract, `config=Game`, `BlueprintType`) — base pawn, owns
  `USkeletalMeshComponent* Mesh` and `UChaosVehicleMovementComponent* VehicleMovementComponent`.
  Subclass movement via `ObjectInitializer.SetDefaultSubobjectClass<UMyMovement>(VehicleMovementComponentName)`.
- `UChaosWheeledVehicleMovementComponent : UChaosVehicleMovementComponent` — the real vehicle sim.
- **Per-wheel inputs (from base):** `SetThrottleInput(float)`, `SetBrakeInput(float)`,
  `SetSteeringInput(float)` (range -1..1), `SetHandbrakeInput(bool)`, `SetPitchInput/RollInput/YawInput`.
- **Runtime tunables (Blueprint-callable on the wheeled component):**
  `SetMaxEngineTorque`, `SetDragCoefficient`, `SetDownforceCoefficient`,
  `SetWheelFrictionMultiplier(i, f)`, `SetWheelMaxBrakeTorque`, `SetWheelHandbrakeTorque`,
  `SetWheelMaxSteerAngle`, `SetWheelSlipGraphMultiplier`, `SetSuspensionParams`,
  `SetDriveTorque`, `SetBrakeTorque`, `SetTractionControlEnabled`, `SetABSEnabled`.
- **Wheel config struct** `UChaosVehicleWheel` editable properties:
  `WheelRadius, WheelWidth, WheelMass, CorneringStiffness, FrictionForceMultiplier,
  SideSlipModifier, SlipThreshold, SkidThreshold, MaxSteerAngle, bAffectedBySteering/Brake/
  Handbrake/Engine, bABSEnabled, bTractionControlEnabled, LateralSlipGraph (FRuntimeFloatCurve),
  SuspensionAxis, SuspensionForceOffset, SuspensionMaxRaise/Drop, SuspensionDampingRatio,
  SpringRate, SpringPreload, WheelLoadRatio, RollbarScaling, SweepShape (Raycast/Spherecast/Shapecast),
  MaxBrakeTorque, MaxHandBrakeTorque`.
- **Vehicle-level config structs:**
  `FVehicleEngineConfig` (TorqueCurve, MaxTorque, MaxRPM, EngineIdleRPM, EngineBrakeEffect,
  EngineRevUpMOI, EngineRevDownRate),
  `FVehicleTransmissionConfig` (bUseAutomaticGears, bUseAutoReverse, FinalRatio,
  ForwardGearRatios[], ReverseGearRatios[], ChangeUpRPM, ChangeDownRPM, GearChangeTime,
  TransmissionEfficiency),
  `FVehicleDifferentialConfig` (EVehicleDifferential: AllWheelDrive/FrontWheelDrive/RearWheelDrive,
  FrontRearSplit),
  `FVehicleSteeringConfig` (ESteeringType: SingleAngle/AngleRatio/Ackermann, AngleRatio,
  SteeringCurve as speed-vs-steer FRuntimeFloatCurve),
  `FChaosWheelSetup` (WheelClass, BoneName, AdditionalOffset).
- **Debug/state:** `FWheelStatus` exposes `bInContact, SlipAngle, bIsSlipping, SlipMagnitude,
  bIsSkidding, SkidMagnitude, SkidNormal, SpringForce, DriveTorque, BrakeTorque, bABSActivated` —
  ideal for grip-loss detection feeding the damage + skid-FX systems.

---

## 1. Design Philosophy — "GTA-feel", not iRacing

GTA's RAGE vehicle model is **arcade-leaning and forgiving**:
- **Assisted, not simulated.** Steering auto-centers, recovery from spins is fast, low-speed
  maneuverability is exaggerated, grip falls off gently (no cliff-edge spin-out).
- **Fun over realism.** High top speeds relative to mass, snappy throttle, "catchable" drifts.
- **Class fantasy.** Each class *feels* different instantly: sports cars are twitchy and quick,
  trucks are heavy and slow to rotate, sedans are the balanced default.
- **Forgiving collisions.** Small bumps barely matter; you only "total" a car after sustained
  heavy impact. Damage is visual + mechanical but never instantly fatal to control.

### How we get GTA-feel out of Chaos (which is sim-oriented)
1. **Substep the physics** — Chaos vehicles are notoriously stiff at 60Hz. We raise the vehicle
   substep count (`p.Vehicles.SubstepsPerFrame`, or `ChaosSolver` substep settings) to 4–8 so the
   suspension/friction integrates smoothly and the car feels planted, not floaty.
2. **Roll our own arcade layer on top of Chaos** — instead of feeding raw -1..1 inputs, we apply
   an *ArcadeInputModel* that boosts low-speed steer, eases high-speed steer (via our own curve so
   it is consistent regardless of Chaos `SteeringCurve` quirks), and adds subtle auto-steer-center
   and counter-steer assist when slipping.
3. **Tune Chaos away from sim** — high `WheelLoadRatio`→low (0.2–0.4) to kill lift-off oversteer,
   generous `SideSlipModifier`/thresholds, soft `LateralSlipGraph` so tire force ramps gently
   (a "fake Pacejka" that saturates early = forgiving), traction control + ABS on by default.
4. **Mechanical aids as first-class systems** — Traction Control, ABS, Stability Control are
   *gameplay features* we expose, not just sim flags.

---

## 2. Vehicle Architecture (C++ base + Blueprint per vehicle)

### 2.1 Class hierarchy
```
AActor / APawn
 └─ AWheeledVehiclePawn                (Chaos base, owns Mesh + MovementComp)
     └─ AGTAI_BaseVehicle              (C++: shared vehicle logic, damage, entry/exit hooks)
         ├─ AGTAI_Sedan                (C++ thin subclass OR pure BP child of Base)
         ├─ AGTAI_SportsCar
         ├─ AGTAI_Truck
         └─ AGTAI_TrafficVehicle       (AI-driven; reuses same movement, separate controller)

UChaosWheeledVehicleMovementComponent
 └─ UGTAI_VehicleMovementComponent     (C++: arcade input model, assists, class presets)
```

**Rule of the split:**
- **C++ (`GTAI_Vehicles` module):** `AGTAI_BaseVehicle`, `UGTAI_VehicleMovementComponent`,
  `UGTAI_VehicleWheel` (subclass of `UChaosVehicleWheel` for class presets), `UGTAI_TrafficAI`,
  `UGTAI_ChaseCamera` (or camera lives in player controller), damage *logic* types.
- **Blueprint (per vehicle):** A `BP_GTAI_Sedan_X` child of `AGTAI_Sedan` that sets:
  skeletal mesh, `WheelSetups` (bone names + offsets), the `EngineSetup/TransmissionSetup/
  DifferentialSetup/SteeringSetup` curve values, `Mass`, `CenterOfMass` override, paint/material,
  `UDamageProfile` asset reference, engine/horn/tire SFX, and assigns a `UGTAI_VehicleTuning`
  `UDataAsset` (the arcade assists config).

### 2.2 Data-driven tuning: `UGTAI_VehicleTuning` (UDataAsset)
To keep Blueprints clean and make balancing trivial, per-class handling lives in a
`UGTAI_VehicleTuning` DataAsset referenced by the Blueprint. This holds the *arcade* layer
parameters (steer assist, counter-steer, grip bias, air-control, recover assist) plus a snapshot
of the Chaos setup struct values so a designer can re-tune without recompiling.

### 2.3 Component inventory on `AGTAI_BaseVehicle`
- `UGTAI_VehicleMovementComponent` (replaces default Chaos movement)
- `USkeletalMeshComponent` (chaos-managed wheels via `UChaosVehicleWheel` + `UWheeledVehicleMovementComponent` wheel controller anim node)
- `UBoxComponent` *EntryTrigger* (overlap volume for enter/exit)
- `UGTAI_DamageComponent` (visual + mechanical state)
- `UGTAI_VehicleFxComponent` (skid decals, exhaust, damage emitters)
- `USpringArmComponent` *ChaseCamArm* (only on player-controlled vehicle; traffic cars skip it)

---

## 3. Handling Model — 3 Vehicle Classes

All three share the same code path; they differ only by the `UGTAI_VehicleTuning` DataAsset +
Chaos setup values. Targets below are **arcade targets**, then the Chaos mapping.

### 3.1 Sedan (balanced default)
| Property | Arcade target | Chaos mapping |
|---|---|---|
| Top speed | ~150 mph (240 km/h) | TorqueCurve peak ~0.85 @ mid RPM, MaxTorque ~520, FinalRatio ~3.4 |
| 0–60 | ~7 s | EngineRevUpMOI moderate, 6 fwd gears |
| Steering | predictable, mild understeer | SteeringCurve: 1.0→0.9@20→0.5@60→0.32@120 mph; SteeringType=Ackermann |
| Grip | balanced | WheelLoadRatio 0.35, CorneringStiffness medium, FrictionForceMultiplier 1.0 |
| Mass | 1500 kg | Mesh mass + CoM override low |
| Differential | AWD bias 0.55 rear | FrontRearSplit 0.45 |

### 3.2 Sports Car (fast, twitchy)
| Property | Arcade target | Chaos mapping |
|---|---|---|
| Top speed | ~205 mph (330 km/h) | MaxTorque ~780, FinalRatio ~3.1, aggressive TorqueCurve |
| 0–60 | ~3.2 s | EngineRevUpMOI low (revs snap), short gears, ChangeUpRPM high |
| Steering | hyper-responsive, darty | SteeringCurve flatter at speed but higher gain low-end; SteeringType=SingleAngle; MaxSteerAngle high |
| Grip | high but narrow window → twitchy | CorneringStiffness high, **narrow LateralSlipGraph** (force saturates close to linear → easy to break loose); WheelLoadRatio 0.25 |
| Mass | 1250 kg (light) | low CoM |
| Differential | RWD | FrontRearSplit 0.85 (mostly rear) → power-on oversteer when provoked |
| Assists | weaker TC/ABS so it feels raw | bTractionControlEnabled=false on rears by default |

### 3.3 Truck (heavy, slow turning)
| Property | Arcade target | Chaos mapping |
|---|---|---|
| Top speed | ~95 mph (150 km/h) | MaxTorque ~900 (lots of grunt, low RPM), FinalRatio ~4.1, tall gears |
| 0–60 | ~12 s | EngineRevUpMOI high (sluggish), slow ChangeUpRPM |
| Steering | lazy, big turning circle | SteeringCurve: 0.8@0 → 0.35@60 → 0.18@100; lower MaxSteerAngle; SteeringType=Ackermann for stability |
| Grip | huge, very forgiving | CorneringStiffness high but SpringRate stiff; WheelLoadRatio 0.45; FrictionForceMultiplier 1.2 |
| Mass | 4000+ kg | high CoM (tips easier if abused) but RollbarScaling high |
| Suspension | soft, wallowy | SpringRate low, SuspensionDampingRatio high, SuspensionMaxDrop large |
| Differential | AWD | FrontRearSplit 0.5 |

### 3.4 Arcade input model (in `UGTAI_VehicleMovementComponent::ProcessArcadeInput`)
1. Read raw analog input `Steer∈[-1,1]`, `Throttle∈[0,1]`, `Brake∈[0,1]`, `Handbrake bool`.
2. **Speed-sensitive steer gain:** `EffectiveSteer = Steer * SteerCurveBySpeed(SpeedMPH) *
   Tuning.SteerGain`. Low-speed gets extra gain so parking-lot turns are easy.
3. **Auto-center:** when `|Steer|<0.05` and grounded, lerp applied steer toward 0 faster than
   Chaos would (GTA "snap back").
4. **Counter-steer assist:** if `FWheelStatus.bIsSkidding` and player not counter-steering, add a
   small corrective steer toward velocity heading (catchable drift). Scaled by `Tuning.DriftAssist`.
5. **Throttle shaping:** apply `Tuning.ThrottleLinearity` exponent so tip-in is gentle.
6. Feed finalized values to Chaos via `SetSteeringInput/SetThrottleInput/SetBrakeInput/
   SetHandbrakeInput`.
7. **Air control:** when `UChaosVehicleWheel::IsInAir()` true for all wheels, allow small
   `SetPitchInput/SetRollInput` for flips/landing (GTA flavor).

---

## 4. Damage System — Visual + Mechanical

### 4.1 Two-layer model
- **Visual damage:** pure cosmetics, never affects handling. Driven by `UGTAI_DamageComponent`.
- **Mechanical damage:** degrades performance, driven by accumulated *structural* damage mapped
  to subsystems. This is the GTA model — a car stays drivable until it's wrecked.

### 4.2 `UGTAI_DamageComponent` design
```
struct FVehicleDamageState {
  float VisualDamage;        // 0..1 overall cosmetic
  float StructuralDamage;    // 0..1 -> when >=1 the car is "wrecked" (engine dies / wheels lock)
  // mechanical subsystems (0=healthy..1=dead)
  float EngineHealth;        // affects MaxTorque (SetMaxEngineTorque scales down)
  float TransmissionHealth;  // affects GearChangeTime / FinalRatio (slips)
  float SuspensionHealth[4]; // per wheel -> SetSuspensionParams softens / collapses
  float TireHealth[4];       // -> SetWheelFrictionMultiplier reduces grip, can blow out
  float BrakeHealth[4];      // -> SetWheelMaxBrakeTorque reduces
  bool  bOnFire;             // after threshold -> FX + eventual explosion (wreck)
  bool  bWrecked;
};
```

### 4.3 Damage sources
- **Impact events:** bind to `OnComponentHit` / `OnActorHit`. Compute `ImpactSeverity = f(RelativeSpeed, Mass, HitNormalAngle)`. Above a threshold, route to:
  - *Visual:* spawn/enable localized dent material (Runtime Virtual Texture or morph-target dent
    mesh / swapped damaged material), add to `VisualDamage`.
  - *Mechanical:* subtract from the subsystem nearest the hit bone (use hit bone name → subsystem
    map). E.g., front hit → EngineHealth; wheel-area hit → that Tire/SuspensionHealth.
- **Continuous:** rolling on with `TireHealth` low → periodic micro-damage; fire → drains
  StructuralDamage over time.
- **Environmental:** landing from big air (compare pre/post velocity) → suspension/structural hit.

### 4.4 Visual implementation (cheap, scalable for traffic)
- **Hero/player cars:** per-bone dent via **morph targets** or a **damage material instance**
  (e.g., `MIT_Damage` blending a scratch/dent normal + vertex-paint mask driven by hit position).
  Glass shatter = swap to broken-glass material + hide shards.
- **Traffic cars (pooled, many):** lightweight — a single `DamageLevel` (0–3) swaps a LOD material
  variant + enables smoke emitter. No per-bone denting (perf).
- **Persistent decals:** skid marks + blood/scorch via `UGTAI_DecalManager` (pooled, world-space).
- **FX:** `UGTAI_VehicleFxComponent` drives Niagara for exhaust, tire smoke (from
  `FWheelStatus.SkidMagnitude`), spark on scrape, fire, explosion.

### 4.5 Mechanical effect application (every frame, throttled)
```
ApplyMechanicalDamage() {
  Movement->SetMaxEngineTorque(BaseTorque * (1 - 0.7*EngineHealth));
  if (TransmissionHealth<0.5) Movement->Set... (slippier gear change via GearChangeTime up);
  for each wheel i:
     Movement->SetWheelFrictionMultiplier(i, BaseFric * TireHealth[i]);
     Movement->SetSuspensionParams(softRate, damp, ..., i);  // collapse if ~0
     Movement->SetWheelMaxBrakeTorque(i, BaseBrake * BrakeHealth[i]);
  if (StructuralDamage>=1) SetThrottleInput(0); lock wheels; bWrecked=true;
}
```

---

## 5. Vehicle Entry / Exit System

### 5.1 Actors & flow
- **Player** (`AGTAI_Character`): when near a vehicle (overlap `EntryTrigger` or line trace),
  show "Press F to enter" prompt. On `Enter`:
  1. Possess the vehicle: `PlayerController->Possess(Vehicle)`, disable character mesh, attach
     character to a *DriverSeat* socket (hidden or visible torso).
  2. Vehicle switches to `EControlMode::Player`; `ChaseCamArm` activates.
  3. Character state saved (weapon, etc.).
- **Exit** (`F` again or context action):
  1. Trace for a safe spawn point beside the vehicle (left/right based on traffic/obstacles).
  2. Possess character pawn, place at spawn point, re-enable mesh, detach from seat.
  3. Vehicle → `EControlMode::Idle` (engine idles) or `Abandoned` (eligible for traffic reuse/pooling).
- **Hotwire/steal**: if locked, play a quick mini-interaction (AI-native flavor: optional).

### 5.2 Multi-seat (future)
`SeatSetups` array (Driver, PassengerFL, PassengerFR, RearL, RearR) with sockets + entry triggers.
NPCs (companions, driven-by AI) occupy passenger seats.

### 5.3 Code hooks (C++ in `AGTAI_BaseVehicle`)
- `bool CanEnter(APawn* Ped) const;`
- `void EnterVehicle(APawn* Ped, int32 SeatIndex);`
- `APawn* ExitVehicle(int32 SeatIndex, FVector& OutSpawn);`
- Delegates: `OnVehicleEntered`, `OnVehicleExited` (for HUD, camera, audio).

---

## 6. Camera System — Third-Person Chase Cam

### 6.1 `UGTAI_ChaseCameraComponent` (SpringArm-based)
- Uses `USpringArmComponent` with `bUsePawnControlRotation=false`, `bEnableCameraLag=true`,
  `bEnableCameraRotationLag=true` (smooth, GTA-like).
- **Modes:**
  - *Drive:* pulled-back chase, slight look-ahead toward velocity, fov widens with speed
    (`FOV = BaseFOV + SpeedFactor*K`) for sense of speed.
  - *Reverse:* arm flips to front so you see where you're backing.
  - *Aim/Combat (drive-by):* tighter, player can free-look with right stick/mouse.
  - *Crash/Impact:* brief shake (`UGameplayStatics::PlayWorldCameraShake`) scaled by impact.
  - *Cinematic enter:* quick ease from character cam to chase cam on entry.
- **Collision:** SpringArm auto-pulls in when occluded by buildings (NYC canyons).
- **Speed-based dip:** arm lowers slightly at high speed for aggressive stance.

### 6.2 Why SpringArm and not manual lerp
SpringArm's built-in lag + socket offset gives 90% of the GTA feel for free; we only override FOV,
reverse-flip, and shake. Keep it data-driven via `UGTAI_CameraTuning` DataAsset.

---

## 7. Traffic AI Vehicle Controller

### 7.1 Goals
Hundreds of ambient cars in NYC that: drive on roads, obey (loosely) traffic, don't clip the
player absurdly, react to crashes, and pool efficiently for World Partition streaming.

### 7.2 `UGTAI_TrafficAI` (subsystem on `AGTAI_TrafficVehicle`)
Drives the **same** `UGTAI_VehicleMovementComponent` (so handling is consistent) but with an AI
input source instead of player input.

- **Pathing:** Follow a **road spline network** (generated from OSM/Houdini city data →
  `AGTAI_RoadNetwork` with `USplineComponent` lanes). The AI samples the next spline point ahead,
  computes a desired heading, and converts that to `Steer` + `Throttle/Brake`.
- **Behavior states:**
  - `Cruise` — follow lane, target speed from road type (highway fast, side-street slow).
  - `Stop` — at red light / stopped car ahead (raycast / proximity check).
  - `Yield` — player cuts in → brief brake.
  - `Avoid` — obstacle/pedestrian → steer around within lane.
  - `Flee` (optional AI-native) — if player is wanted & near, some drivers panic-flee.
  - `Wrecked` — if damaged past threshold, pull over / stop, emit smoke (reuse damage FX).
- **Input mapping:** `DesiredHeadingError → Steer` (P-controller + feedforward from spline
  curvature); `GapAhead vs TargetSpeed → Throttle/Brake`.
- **Pooling:** Traffic cars are object-pooled and **sleep** (movement disabled, mesh hidden or
  LOD0-culled) outside the player's 3×3 World Partition grid, then re-skinned (random BP class +
  paint) on wake. Use `MassEntity` or a lightweight `UGTAI_TrafficSpawner` manager for scale.
- **Performance:** traffic AI runs on a fixed tick (e.g., 15 Hz) and uses cheap spline queries, not
  physics-heavy pathfinding. Chaos substeps still apply but we can lower them for traffic vs player.

### 7.3 Determinism & safety
- Every traffic car validates "am I about to drive into a wall?" via short forward trace; if so,
  hard brake. Prevents the classic UE traffic bug of cars climbing buildings.
- Speed caps + steering clamps inherited from the same tuning structs keep them stable.

---

## 8. Implementation Roadmap (phased)

1. **Module wiring** — add `ChaosVehicles` + `ChaosVehiclesEditor` to `GTAI_Vehicles.Build.cs`;
   enable `ChaosVehiclesPlugin` in `.uproject`.
2. **Movement component** — `UGTAI_VehicleMovementComponent` subclass with arcade input model +
   assists; verify on a test sedan.
3. **Base vehicle + 3 classes** — `AGTAI_BaseVehicle`, sedan/sports/truck, each with a BP +
   `UGTAI_VehicleTuning` DataAsset. Tune until GTA-feel achieved (playtest loop).
4. **Damage** — `UGTAI_DamageComponent` + FX + mechanical degradation. Test wreck threshold.
5. **Entry/exit + camera** — integrate with player character + `UGTAI_ChaseCameraComponent`.
6. **Traffic AI** — `UGTAI_TrafficAI` + `AGTAI_RoadNetwork` + spawner/pooler. Populate NYC slice.
7. **Polish** — skid decals, audio (engine note per class via `GTAI_Audio`), replays/slow-mo crash.

---

## 9. Key Risks & Mitigations
| Risk | Mitigation |
|---|---|
| Chaos vehicles feel stiff/floaty at 60Hz | Raise substeps; verify `p.Vehicles.*` debug pages |
| Arcade feel hard to tune | Isolate arcade layer in C++ so designers tune via DataAssets, not engine internals |
| Traffic perf with many Chaos vehicles | Pool + sleep + lower substeps for AI; consider MassEntity for thousands |
| Damage breaking handling unexpectedly | Mechanical effects throttled + clamped; wreck is a clean state, not gradual lock-up |
| Trucks tip over | High `RollbarScaling`, lower `WheelLoadRatio` effect, capped CoM height |

---

## 10. File Map (deliverables)
```
Docs/vehicle_system.md                         ← this document
Source/GTAI_Vehicles/
  GTAI_Vehicles.Build.cs
  GTAI_VehicleTypes.h                           ← shared enums/structs
  GTAI_VehicleMovementComponent.h              ← arcade input + assists
  GTAI_VehicleWheel.h                           ← class preset wheel
  GTAI_BaseVehicle.h                            ← base pawn: damage/entry/exit hooks
  GTAI_Sedan.h / GTAI_SportsCar.h / GTAI_Truck.h
  GTAI_TrafficVehicle.h
  GTAI_TrafficAI.h                              ← AI controller
  GTAI_DamageComponent.h
  GTAI_ChaseCameraComponent.h
  GTAI_VehicleTuning.h                          ← UDataAsset for per-class tuning
E:/Shenron/skills/gtai/gtai-vehicle-engineer/SKILL.md   ← reusable skill
```
