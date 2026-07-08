# GTAI — Combat & Player Controller System Design

> **Module:** `GTAI_Combat` (C++) + per-weapon Blueprints
> **Engine:** Unreal Engine 5.8
> **Input:** Enhanced Input System (no legacy `BindAxis`/`BindAction`)
> **Namespace:** `GTA7::Combat`, `GTA7::Player`
> **Owner:** STRIKE (Combat & Player Controller specialist)
> **Status:** v0.1 — implementation-ready design
> **Feel target:** GTA V — responsive, arcade-leaning, *not* simulation. Snappy acceleration, generous aim assist, quick cover entry/exit, chunky hit feedback.

---

## 0. Research Basis & References

This doc is grounded in (a) the official UE5.8 **Enhanced Input** documentation extracted from `dev.epicgames.com`, (b) Epic's **Lyra Sample Game** health/damage architecture (`x157.github.io` Lyra deep-dive), (c) UE5 third-person template + camera/spring-arm patterns, and (d) GTA V's published combat model (arcade hit feedback, body-armor soak, headshot multipliers, cover-leaning).

> **Note on research depth:** the live web-search/extract backend for this agent was heavily bot-blocked (Epic docs and Fandom/GTA-wiki returned `document_antibot`). The design therefore leans on the two authoritative sources that *did* extract cleanly (Enhanced Input reference + Lyra health/damage reference) plus established UE5.4–5.8 engine patterns and well-known GTA combat data. The 300+-URL research mandate is captured as a tracked backlog in `WORK_LOG.md`; the design itself is complete and implementation-ready regardless.

### Key decisions borrowed from references
- **Enhanced Input** = `UInputAction` + `UInputMappingContext` (IMC) + `UEnhancedInputComponent` + `UEnhancedInputLocalPlayerSubsystem`. Contexts are pushed/popped at runtime (e.g. on enter/exit vehicle, enter/exit cover). `ETriggerEvent` (Started/Ongoing/Triggered/Completed/Canceled) drives callbacks.
- **Lyra health/damage** = GAS-based: `UAbilitySystemComponent` + a `HealthSet` attribute set + a `HealthComponent` that translates `OnOutOfHealth` into death events. We reuse this shape but keep it *optional/lightweight* — GTA is arcade, so we do **not** force full GAS on every prop. We implement a self-contained `UDamageSystem` (section 5) that mirrors Lyra's attribute-set semantics without requiring GAS for simple actors.

---

## 1. Architecture Overview

```
GTAI_Combat module
├── Player/
│   ├── AGTA7Character                (ACharacter subclass — the player pawn)
│   ├── AGTA7PlayerController          (APlayerController — owns input + camera mode)
│   └── UGTA7CameraComponent           (spring-arm orbit + aim + shoulder swap + vehicle blend)
├── Combat/
│   ├── UGTA7WeaponBase                (abstract weapon: fire, reload, recoil, ammo)
│   ├── UGTA7HitscanWeapon            (instant ray)
│   ├── UGTA7ProjectileWeapon         (spawns projectile actor)
│   ├── UGTA7MeleeWeapon              (sweep + arc)
│   ├── UGTA7HitDetection              (trace dispatch, bone→damage-type mapping)
│   └── UGTA7DamageSystem             (health/armor/headshot/regen, attribute-set style)
├── Cover/
│   └── UGTA7CoverSystem              (surface-normal + height trace, attach, lean, vault)
└── Data/
    └── FGTA7WeaponConfig / FGTA7DamageType  (Blueprint-exposed structs)
```

**C++ vs Blueprint split (per AGENTS.md rule 6):**
- **C++**: movement state machine, camera math, trace dispatch, damage math, ammo/reload logic, cover detection traces.
- **Blueprint**: per-weapon config (damage, fire rate, spread, recoil curve, mesh, anims, sfx), per-projectile subclass, VFX hooks. Blueprints subclass `UGTA7WeaponBase` / `UGTA7ProjectileWeapon`.

---

## 2. Third-Person Character Controller (mission 2a)

### 2.1 Movement model (arcade, not simulation)
GTA-like feel rules:
- **High acceleration, fast stop.** Use `CharacterMovement->MaxAcceleration` ~ 4000, `BrakingDecelerationWalking` ~ 3000. No momentum drift.
- **Movement relative to camera**, not actor. `AGTA7Character::SetupPlayerInputComponent` binds a 2D `Move` `UInputAction`; the processed value is rotated by camera yaw in `AGTA7Character::Move`.
- **Auto-orient to velocity** in normal mode (`bOrientRotationToMovement=true`, `RotationRate=Yaw 600°/s`); **orient to control rotation** while aiming (`UseControllerDesiredRotation=true`).
- **Root motion source** for parkour/vault so animations drive position (section 2.5).

### 2.2 States (simple FSM in the character)
`EGTA7LocomotionState`: `Idle, Walk, Jog, Sprint, Crouch, Cover, Parkour, InVehicle`.
Transitions are explicit and frame-cheap; animation Blueprint reads the state + speed + lean BlendSpace.

### 2.3 Sprint
- Bind `Sprint` `UInputAction` (bool, Hold trigger). While held + moving forward past a threshold → `Sprint`.
- Speed: Jog `350`, Sprint `620` (GTA-ish). FOV punch +1.5°→+6° on sprint enter for speed sensation (`UGTA7CameraComponent::SetFOVGoal`).
- Stamina is **optional/light** — arcade games usually skip it; we expose `bUseStamina` flag default false.

### 2.4 Crouch
- `Crouch` `UInputAction` (tap toggle OR hold). Uses `ACharacter::Crouch()/UnCrouch()` which drives capsule half-height automatically (set `CrouchedHalfHeight` ~ 44).
- In cover, crouch is implied by cover height (section 6).
- Reduces spread, slows move speed to ~160.

### 2.5 Parkour basics (vertical slice scope)
Keep minimal but real:
- **Mantle/Vault**: on `Jump` while moving toward a low ledge, run a short `URootMotionSource` (or a Montage with `Montage_Advance` + `FRootMotionSource`) for ~0.4s. Detection: forward box trace + ledge height check.
- **Slide**: sprint + crouch → brief slide (velocity preserved, friction low) ~0.5s, then recover. Arcade flavor from GTA Online.
- **Climb**: out of scope for slice; stubbed hook `CanClimb()` returns false.
- Implementation note: parkour uses `UCharacterMovementComponent::ApplyRootMotionSource` so it stays networked/authoritative-friendly.

---

## 3. Camera System (mission 2b)

`UGTA7CameraComponent` wraps a `USpringArmComponent` (+ optional `UCameraComponent`). Driven by an `EGTA7CameraMode`: `Orbit, Aim, Vehicle`.

### 3.1 Third-person orbit
- Spring arm length `TargetArmLength` ~ 350 (GTA over-shoulder). `bUsePawnControlRotation=true` so mouse/gamepad yaw/pitch rotates the arm.
- Smoothing: `CameraLagSpeed` ~ 10, `bEnableCameraRotationLag=true`, `RotationLagSpeed` ~ 12 for that weighty GTA feel.
- Collision: `bDoCollisionTest=true` with `ProbeChannel=Camera`; arm retracts on wall contact.

### 3.2 Aim mode (ADS / iron sights)
- `Aim` `UInputAction` (Hold). On Started → `SetCameraMode(Aim)`:
  - Lerp `TargetArmLength` → 120 (close shoulder).
  - Blend camera to a socket on the weapon (or a fixed aim offset) via `UGTA7CameraComponent::BlendToAim`.
  - FOV → 55 (zoom).
  - Switch character orientation to `UseControllerDesiredRotation`, tighten look sensitivity.
  - Spread multiplier → 0.25 (section 4).
- All transitions use `FInterpTo` / timeline lerps (~0.15s) so it feels snappy but not instant.

### 3.3 Shoulder swap
- `ShoulderSwap` `UInputAction` (tap). Toggles `bRightShoulder`. Camera target offset X flips (`+40`/`−40`); aim trace origin swaps to the matching eye/socket. Smooth lerp.
- Critical for peeking around right-hand cover without exposing the body.

### 3.4 Vehicle transition
- On enter vehicle, controller pushes a `Vehicle` IMC and calls `UGTA7CameraComponent::SetCameraMode(Vehicle)`. Camera attaches to vehicle spring arm (chase cam), look controls drive vehicle steering intent. On exit, pop IMC + restore `Orbit`.
- Blend via `SetViewTargetWithBlend` on the player camera manager (~0.4s) to avoid a hard cut.

### 3.5 Camera helpers
- `AddRecoil(float Pitch, float Yaw)` — feeds a decaying impulse into control rotation (section 4.3).
- `AddImpulseShake` — `UForceFeedbackEffect` / `UCameraShakeBase` on fire/hit.

---

## 4. Weapon System C++ Framework (mission 2c)

### 4.1 Class hierarchy
```
UGTA7WeaponBase : UObject (or AActor if world-placed pickups needed)
 ├── UGTA7HitscanWeapon      (instant line trace)
 ├── UGTA7ProjectileWeapon   (spawns AGTA7Projectile)
 └── UGTA7MeleeWeapon        (sweep/arc trace + lunge)
```
We treat weapons as `UObject` owned by the character (not separate actors) for the slice — simpler, cheaper, and matches GTA's "weapon as inventory item" model. Pickups spawn an `AGTA7WeaponPickup` that grants the `UGTA7WeaponBase` instance/data to the player.

### 4.2 `UGTA7WeaponBase` responsibilities
- **Config-driven**: a `FGTA7WeaponConfig` struct (damage, fireRate, magSize, reloadTime, spread, recoilCurve, range, damageType, mesh/anim/sfx soft refs) — authored in **Blueprint** child or DataAsset.
- **Fire** (`StartFire`/`StopFire`): respects `bFullAuto`, fire-rate timer, ammo.
- **Reload**: async timer; can be canceled by fire (GTA lets you cancel reload by shooting).
- **Spread**: base spread + moving/crouch/aim multipliers; computed per-shot in `GetCurrentSpread()`.
- **Recoil**: `URecoilCurve` (or 2D curve) → per-shot pitch/yaw kick applied to camera (section 3.5) + visible weapon kick.
- **Muzzle/aim origin**: `GetMuzzleLocation()` + `GetAimDirection()` (camera-forward, optionally adjusted by spread).
- **Events**: `OnFire`, `OnReloadStart`, `OnReloadEnd`, `OnAmmoChanged` (for HUD).

### 4.3 Per-type behavior
- **Hitscan** (`UGTA7HitscanWeapon`): calls `UGTA7HitDetection::TraceShot` → instant. Used for pistol, rifle, SMG, sniper.
- **Projectile** (`UGTA7ProjectileWeapon`): spawns `AGTA7Projectile` (a `AProjectile` with `UProjectileMovementComponent`); projectile does the hit test on overlap. Used for RPG, grenade launcher, thrown.
- **Melee** (`UGTA7MeleeWeapon`): short arc `SweepMultiByChannel` in front; applies damage + small lunge impulse; cooldown. Used for fists, bat, knife.

### 4.4 Extensibility pattern
New weapon = **Blueprint child** of one of the three bases, set the `FGTA7WeaponConfig` values (and optional curve assets). No C++ recompile needed for balancing. C++ only changes when a *new mechanic* is needed (e.g. charge shot) → add a fourth subclass.

---

## 5. Hit Detection (mission 2d)

### 5.1 Hybrid model
- **Hitscan** for most firearms (pistol/rifle/SMG/sniper): single `LineTraceSingleByChannel` from muzzle along aim dir + spread.
- **Projectile** for explosives/throwables: actor travels, `SphereTrace`/overlap on impact.
- Both funnel into `UGTA7HitDetection::ProcessHit(HitResult, WeaponConfig)` which maps the struck component/bone to a `FGTA7DamageType` and calls `UDamageSystem::ApplyDamage`.

### 5.2 Trace dispatch
- `TraceShot`: from `MuzzleWorldLocation` to `Muzzle + AimDir * Range`. `bTraceComplex=false` for perf (arcade). Custom `ECC_GameTraceChannel_Weapon` (projectile/pawn/WorldStatic).
- Optional **multi-pellet** loop for shotguns (N traces with independent spread).
- **Penetration** (optional, rifle/sniper): if hit is thin (tag `Penetrable`), continue trace from exit point; reduce damage per pen.
- **Hit validation**: server-authoritative — client predicts trace, server re-validates the hit against the actor's replicated position (combat is deterministic per AGENTS.md rule). For the slice we run traces on the authoritative pawn and replicate results.

### 5.3 Bone → damage-type mapping (headshots)
- On hit, read `HitResult.BoneName`. Compare against a small tag set:
  - `head` / `head_01` (or socket `Head`) → `DamageType=Headshot` (×multiplier, section 6).
  - `upperarm/forearm/thigh/calf` → `Limb` (×0.8, reduced).
  - everything else → `Torso` (×1.0).
- Use a `TMap<FName, EGTA7HitZone>` configured per skeleton (BP-exposed) so different skeletons (pedestrian vs cop vs player) map correctly. Headshot also forces a damage number popup + distinct hit marker.

### 5.4 Hit feedback (arcade juice)
- Client hit marker (crosshair X) on confirming a hit; larger marker + sound on kill.
- `UGameplayStatics::SpawnDecalAtLocation` + impact FX by surface type (concrete/wood/metal/flesh).
- Bullet tracer mesh from muzzle to hit (hitscan) for visual clarity.

---

## 6. Damage Model (mission 2e)

### 6.1 `UDamageSystem` (attribute-set style, GTA7::Combat)
Mirrors Lyra's health-set semantics but self-contained (no mandatory GAS):
```
float Health;        // 0..MaxHealth
float MaxHealth;     // 100 (player), varies for NPCs
float Armor;         // 0..MaxArmor (body armor soak)
float MaxArmor;      // 100
float ArmorAbsorb;   // fraction of damage armor takes (0.5–0.65, GTA-like)
```
- `ApplyDamage(float InDamage, EGTA7HitZone Zone, AController* Instigator)`:
  1. `Effective = InDamage * ZoneMultiplier` (Headshot ×2.0, Limb ×0.85, Torso ×1.0).
  2. If `Armor > 0`: `ArmorDamage = Effective * ArmorAbsorb` (capped by remaining armor); `HealthDamage = Effective - ArmorDamage`. Armor depletes first. (GTA: armor absorbs a large share before health.)
  3. `Health -= HealthDamage`; clamp 0.
  4. Broadcast `OnHealthChanged` / `OnArmorChanged` (HUD) and `OnDamageTaken` (hit reaction, blood).
  5. If `Health <= 0` → `OnOutOfHealth` → death sequence (ragdoll/elimination message, mirrors Lyra's `OnOutOfHealth`→`GameplayEvent.Death`).
- `Heal(float)`, `AddArmor(float)` (pickups).

### 6.2 Regeneration (arcade-friendly)
- **Health regen**: optional, off by default for player in combat sim; if enabled, begins `RegenDelay` (e.g. 5s) after last damage, restores `RegenRate`/s up to a cap (not full 100 — GTA doesn't auto-heal). NPCs: configurable.
- **Armor**: does **not** regen (must pick up armor). Matches GTA.
- Knobs exposed in `FGTA7DamageConfig` (BP/DataAsset).

### 6.3 Headshot tuning
Default multipliers (BP-tunable, GTA-leaning but slightly less brutal than RDR2):
`Headshot=2.0, Torso=1.0, Limb=0.85, ArmorAbsorb=0.6`. Pistol base damage 25 → headshot 50 (2 shots kill 100-HP ped), torso 4 shots. Rifle 30 → head 60.

---

## 7. Cover Detection System (mission 2f)

`UGTA7CoverSystem` (`GTA7::Player`) — auto cover like GTA V.

### 7.1 Detection (surface normal + height checks)
Run on a timer (~10 Hz) while the player is near geometry and holding move-into-wall / pressing `TakeCover`:
1. **Probe traces**: two short `LineTrace` from the character's left/right shoulders forward (and one center) ~60–90cm. If a hit is found and the surface is tagged `CoverEnabled`:
2. **Surface normal**: `HitResult.Normal` (pointing away from wall, toward player). If `|Normal.XZ dot ToPlayer|` indicates the wall faces the player → valid cover normal. Store `CoverNormal`.
3. **Height check**: vertical `LineTrace` upward from a point just in front of the wall (start at `Hit.Location + Normal*10`, end `+WallMinHeight` ~120cm). If **blocked below ~`CoverMinHeight`** and **clear above**, the wall is tall enough to hide behind → `bCanTakeCover=true`.
4. **Edge detection** (for corner peeking): trace left/right along the wall tangent; nearest gap = cover edge → clamp lean.
5. **Low cover**: if wall height < `CoverCrouchHeight` (~90cm) → force crouch-style cover (peek over).

### 7.2 Attachment & movement
- On `TakeCover` (tap) when `bCanTakeCover`: snap character to `CoverLocation = Hit.Location + CoverNormal * CapsuleRadius`, set state `Cover`, align actor to `CoverNormal`.
- **Cover movement**: while in cover, left/right input slides along the wall tangent (project input onto tangent). Auto-detach at wall ends.
- **Aim/peek**: aiming in cover raises the character to peek; shoulder swap chooses which side to peek. `bInLowCover` forces crouch peek.
- **Vault/exit**: `Jump` or move away from wall → detach; if moving toward a low ledge, trigger parkour vault (section 2.5).

### 7.3 Networking
Cover state + current cover location replicated to the authoritative pawn; detection runs on client for responsiveness, server validates the cover transform on enter.

---

## 8. Input Mapping (Enhanced Input)

`IMC_Player` (default context, priority 0):
| Input Action | Key(s) | Trigger | Drives |
|---|---|---|---|
| `IA_Move` (Axis2D) | WASD / L-Stick | Ongoing | `AGTA7Character::Move` |
| `IA_Look` (Axis2D) | Mouse / R-Stick | Ongoing | `AGTA7PlayerController::Look` |
| `IA_Jump` (bool) | Space | Started/Completed | Jump / Vault |
| `IA_Sprint` (bool) | Shift | Ongoing | Sprint |
| `IA_Crouch` (bool) | Ctrl | Started | Crouch toggle |
| `IA_Aim` (bool) | Right Mouse | Started/Completed | Aim mode |
| `IA_Fire` (bool) | Left Mouse | Started/Ongoing | Fire/StopFire |
| `IA_Reload` (bool) | R | Started | Reload |
| `IA_SwapShoulder` (bool) | Q (tap) | Started | Shoulder swap |
| `IA_TakeCover` (bool) | C / F | Started | Cover toggle |
| `IA_WeaponSlot1..3` (bool) | 1/2/3 | Started | Switch weapon |

`IMC_Vehicle` (priority 1, pushed on enter vehicle): remaps `IA_Fire`→none, `IA_Move`→throttle/steer, etc.

---

## 9. Implementation Order (vertical slice)
1. `GTAI_Combat.Build.cs` + module registration in `GTA7.uproject`/`.Target.cs`.
2. `AGTA7Character` + `AGTA7PlayerController` + Enhanced Input bindings (move/look/jump).
3. `UGTA7CameraComponent` (orbit → aim → shoulder swap).
4. `UDamageSystem` + `FGTA7DamageConfig` (health/armor/regen/headshot).
5. `UGTA7WeaponBase` + `UGTA7HitscanWeapon` + `UGTA7HitDetection` (pistol first).
6. `UGTA7CoverSystem` (surface normal + height).
7. Sprint/crouch/parkour polish.
8. Projectile + melee subclasses.
9. Vehicle camera/IMC handoff.

---

## 10. Open Tuning Knobs (BP-exposed, per AGENTS.md rule 6)
- All speeds, FOV punches, spread multipliers, recoil curves, damage values, armor absorb, regen rates, cover heights.
- `bUseStamina`, `bAutoRegenHealth`, `ArmorAbsorb`, headshot multiplier.

---

*End of design doc v0.1 — STRIKE.*
