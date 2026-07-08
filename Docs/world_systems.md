# GTAI World Systems — Technical Design Document

**Project:** GTAI (GTA 7) — AI-Native Open-World Game, NYC
**Engine:** Unreal Engine 5.8 (World Partition, MassEntity, AI Framework)
**Module:** `GTAI_World`
**Owner:** ORACLE — World Systems Designer (specialist subagent)
**Status:** Design v1.0 — ready for implementation
**Date:** 2026-07-08

---

## 0. Research Notes & Sourcing (honesty disclosure)

> **Same degraded-search condition as the vehicle doc.** This session's live web-search
> backend returned off-topic / spam results (Open University, Opendoor stock, Poki games…)
> and Fandom/GTA wiki pages were bot-blocked. I do **not** fabricate 300 URLs to satisfy the
> "300+ sources" line item. Instead this design is grounded in:
>
> 1. **Authoritative UE 5.8 source/doc pages pulled directly** (not via search):
>    - Behavior Trees — `dev.epicgames.com/documentation/en-us/unreal-engine/behavior-trees-in-unreal-engine`
>    - Environment Query System — `.../environment-query-system-in-unreal-engine`
>    - AI Perception — `.../ai-perception-in-unreal-engine`
>    - MassEntity — `.../mass-entity-in-unreal-engine`
>    - World Partition — `.../world-partition-in-unreal-engine`
>    - Data-Driven Gameplay (DataTables/CurveTables) — `.../data-driven-gameplay-elements-in-unreal-engine`
> 2. **The project's own DESIGN.md** (pillars: "Mechanical Rigor", "AI as Nervous System",
>    the Traditional/AI-Native/Hybrid split) and the existing `vehicle_system.md` + `GTAI_VehicleTypes.h`.
> 3. **Domain expertise** on GTA-class wanted/police/economy/faction systems, validated against
>    the DESIGN.md split (e.g. "Wanted system core = Traditional/deterministic + AI police tactics").
>
> To close the sourcing gap, the skill doc (`world-systems-design.md`) carries a curated
> reading list of 40+ concrete URLs (UE docs + design post-mortems + academic world-sim papers)
> that a future session can fetch in batches when the search backend is healthy.

**The split, per DESIGN.md:**
- **Wanted core, traffic spawn, economy prices, property values** → *Traditional* (deterministic, perf-critical C++).
- **Police tactical response, faction AI, world reactivity flavor** → *AI-Native / Hybrid*.
- **Everything** lives under `namespace GTAI::World` and is tuned via DataTables/CurveTables (designer-facing, no recompile).

---

## 1. Architecture Overview

```
GTAI_World (namespace GTAI::World)
├── UGTAI_WorldStateManager        (central singleton / subsystem)
│   ├── Registers: Wanted, Traffic, Economy, Factions, Reactivity, Events
│   ├── Per-cell influence/heat grids (World Partition aligned)
│   └── Tick() fans out to all systems at fixed cadence
├── Wanted System (GTAI_World_Wanted.h)
│   ├── Deterministic core: star level, heat, bribe, de-escalation
│   └── AI tactical layer: police BT + EQS (flank/ambush/roadblock)
├── Traffic System (GTAI_World_TrafficSpawner.h)
│   ├── Deterministic density spawner (cell-based, Poisson)
│   ├── Route AI (spline road network + navmesh fallback)
│   └── Signal controller (traffic lights, crosswalks)
├── Economy System  (designed; header stubbed here)
├── Faction System  (designed; header stubbed here)
└── Reactivity System (designed; header stubbed here)
```

**Performance budget (hard constraint from brief):** 200+ active NPCs and 50+ vehicles within streaming range.
- Ambient NPCs/vehicles run on **MassEntity** (data-oriented, no per-actor tick) where possible.
- The *systems* (managers, spawners, dispatchers) are plain C++ singletons updated at a fixed
  simulation tick (e.g. 10 Hz) decoupled from the render frame.
- Per-cell budgets: spawn requests are produced deterministically from cell density + time-of-day,
  then fulfilled via object pooling so we never cross a hard cap.

---

## 2. Wanted System

### 2.1 Design goals
- Deterministic, predictable **core** (the player must always understand why they have N stars).
- **AI police tactics** on top: how cops flank, cut off, set up roadblocks, and adapt to player behavior.
- Supports 1–5 stars, heat zones, bribes, hiding/lying low, escalation & de-escalation.

### 2.2 Star levels 1–5 (deterministic)

| Stars | Trigger (representative) | Police response |
|-------|--------------------------|-----------------|
| 1 | Minor crime witnessed (assault, car theft w/o pursuit) | 1 patrol car, slow, investigates last-seen |
| 2 | Armed, or 1-star crime repeats | 2 patrol cars, will chase on sight |
| 3 | Killing a cop / sustained violence | Unmarked + patrols, roadblock capable, helicopters deploy |
| 4 | Multiple kills / heavy weapons | NOOSE tactical, aggressive, spike strips, coordinated air+ground |
| 5 | Maximum threat (tanks/explosives, mass kills) | Full city lockdown, military-tier, relentless |

> Star level is `uint8` clamped [0,5]. The **core** transitions are pure functions of
> (crime severity, witness presence, prior stars, heat) — no RNG in the transition itself.

### 2.3 Heat zones
- The world is divided into **heat cells** aligned to World Partition grid cells.
- Each cell carries a `Heat` scalar [0,1] that decays over time and rises when crimes occur there.
- Police **dispatch** prefers the cell with the highest heat near the player; a cell can be
  "hot" (recent crime) even at 0 stars — influencing which precinct responds and response delay.
- `Heat` drives *where* police come from (precinct influence), not *how many* (that's stars).

### 2.4 Police dispatch logic (deterministic)
```
OnCrimeCommitted(severity, location, bWitnessed):
    if bWitnessed:  targetStars = severityToStars(severity)
                    WantedStars = max(WantedStars, targetStars)
    Heat[cell(location)] += severityToHeat(severity)
    RequestDispatch(WantedStars, location)   // picks precinct by influence map

RequestDispatch(stars, location):
    units = DispatchTable->GetUnitsForStars(stars)   // DataTable, BP-tuned
    spawnAt = SelectPrecinctSpawn(stars, location)    // influence-weighted
    ScheduleUnitArrival(spawnAt, location, travelTimeEst)
```
- `DispatchTable` is a **DataTable** (`FDispatchRow`: stars → {patrolCars, unmarked,
  helicopter, noose, roadblockCapable}`) so designers tune without recompile.
- Spawn points are police stations weighted by the **faction influence map** (§5).

### 2.5 AI-driven chase tactics
- Each police unit runs a **Behavior Tree** whose Blackboard is fed by **AI Perception**
  (Sight/Hearing/Damage) and an **EQS** query for tactical positioning.
- EQS tests used:
  - *Best intercept*: generate points ahead of player's velocity vector, score by time-to-intercept.
  - *Cover/flank*: points with LOS to player but offset from teammates (avoid clumping).
  - *Roadblock candidate*: chokepoints on player's route (narrow nodes on road graph).
- Tactical behaviors scale with stars:
  - 1–2: follow + report.
  - 3: cut-off (one unit EQS-positions ahead), helicopter paces.
  - 4–5: pincer (two units flank), spike strips, roadblock composition.
- **Adaptation:** Blackboard key `PlayerTacticSignature` (e.g. "always drives into tunnels",
  "uses crowds") is updated by an AI layer that watches recent player behavior and re-weights
  EQS generators. This is the *AI-native* part — the deterministic core is unchanged.

### 2.6 Escalation / de-escalation
- **Escalation:** committing new witnessed crimes raises stars (§2.4). Also: destroying a
  police vehicle or killing an officer adds a forced +1 within a grace window.
- **De-escalation (deterministic, time + distance based):**
  ```
  if player not seen for T_cooldown(stars) AND distance_to_nearest_cop > R_evade(stars):
      WantedStars = max(0, WantedStars - 1)
      reset cooldown
  ```
  - `T_cooldown` and `R_evade` come from a **CurveTable** keyed by stars (longer at higher stars).
  - Stars cannot drop while any cop has an active perception stimulus of the player.

### 2.7 Bribes
- A deterministic offer: `BribeCost = BribeBase * (stars+1) * CostOfLiving(cell)`.
- Paying immediately drops stars to 0 **only if** no cop currently has LOS (otherwise the bribe
  fails / is refused). Bribes consume cash via the Economy system and apply a short "blind eye"
  window to the nearest precinct's influence.

### 2.8 Hiding / lying low
- Player enters a "hidden" state when: inside a building/interior, or outside all cops'
  Sight radius for `T_hidden`, or in a designated safehouse. Hidden state freezes the
  de-escalation timer start and lets Heat decay faster. Garages/pay-n-spray-style mod shops
  also clear visual identification (classic GTA "change appearance" escape).

---

## 3. Traffic System

### 3.1 Deterministic spawning (NOT LLM)
- Spawn budget per World Partition cell: `Budget(cell) = BaseDensity(cell) * TimeOfDayMult(t)
  * DistrictTypeMult(cell) * WeatherMult`, clamped to a hard cap.
- Arrivals are modeled as a **deterministic Poisson process** seeded by `(cellID, timeSlot)` so
  the city looks alive but is reproducible for debugging/networking. No LLM, no per-frame RNG churn.
- Pooling: a fixed pool of `AMassTrafficVehicle` actors per cell; spawner activates/deactivates
  from pool based on Budget delta — avoids GC hitches at 50+ vehicles.

### 3.2 Route AI
- Roads are a **spline/edge graph** baked from OSM-derived street data (per DESIGN.md asset pipeline).
- Each traffic vehicle picks a route = random walk on the graph biased by lane direction and
  turn penalties. Route recomputed only at intersections (event-driven, not per-frame).
- Fallback: if off-graph (player shoved it), steer back toward nearest graph node via navmesh.

### 3.3 Traffic lights & pedestrian crossings
- `UGTAI_TrafficSignalController` owns intersection state machines (NS/EW green cycles) with
  per-intersection phase offsets (DataTable-tuned) to create realistic wave flow.
- Crosswalks: when signal is pedestrian-green, nearby peds get a "cross" BT task and vehicles
  get a yield test (EQS/raycheck) before entering the crosswalk box.
- Emergency vehicles (incl. police chases) request **preempt** — controller force-greens their axis.

### 3.4 Parking
- Parking slots are pre-baked spline anchors (curbside + lots). Idle traffic vehicles route to a
  free slot when their route ends or during low-density night hours, freeing road capacity.

---

## 4. Economy System (design; header stubbed)

- **Currency:** single `$` pool, `int64` to avoid overflow at city scale.
- **Shops:** DataTable of `FShopRow` (type, markup, stock level, priceVol).
- **Property values:** per-cell `PropertyValue` scalar driven by (crime heat ↓, faction control,
  business density ↑, world events). Buying a property is a Faction-standing + Economy transaction.
- **Dynamic pricing:** each good price = `Base * DistrictMult * (1 + SupplyDemandShock)`.
  `SupplyDemandShock` integrates player buying/selling pressure + faction disruptions (e.g. a
  gang war spikes weapon/ammo prices in that cell). Updated on the economy tick (low frequency).
- **Black market:** a hidden shop tier unlocked by Faction standing (e.g. cartels ≥ +40) or found
  via world exploration. Prices lower but raise heat / risk of sting (random deterministic check).

---

## 5. Faction System (design; header stubbed)

- **Factions:** gangs, corporations, police, political. Each is an `FFactionState`.
- **Territory control:** a low-res grid (coarser than heat cells) where each cell has a dominant
  faction + contestation value. Control shifts from scripted events AND from simulated conflicts
  (AI-native: gangs "probe" weak borders on a timer).
- **Influence map:** continuous [0,1] per-faction per-cell field used by dispatch (§2.4), economy
  pricing, and NPC behavior. Diffused each tick (cheap box blur) for smooth gradients.
- **Player standing:** `int8` per faction in range [-100, +100].
  - < -50: hostile (shoot on sight / refuse service)
  - -50..-20: wary
  - -20..+20: neutral
  - +20..+50: friendly
  - > +50: ally (gives passes, discounts, quest hooks)
- Standing moves deterministically from player actions (sell to them ↑, kill their members ↓)
  with diminishing returns near the extremes.

---

## 6. World Reactivity (design; header stubbed)

Driven by the World State Manager tick + Faction/Economy state:
- **Graffiti / tags:** faction-controlled cells slowly accrue the dominant faction's tag; player
  can tag rival turf (raises that faction's hostility, lowers rival standing).
- **Business open/close:** shops open/close on time-of-day AND on economic health (a shop bankrupted
  by repeated robberies or a faction war closes permanently until rebuilt).
- **News headlines:** a generated feed (AI-native) referencing player crimes, faction shifts,
  market crashes — consumed by radio/phone (per DESIGN.md AI radio pillar).
- **Street events:** scheduled + emergent micro-events (protest, accident, mugging) spawned by the
  Event System from current world state; executed as short BT routines on ambient NPCs.

---

## 7. World State Manager (central singleton)

- Implemented as a **UE Subsystem** (`UEngineSubsystem` or `UGameInstanceSubsystem`) — the
  canonical "central singleton" pattern in UE5 (not a raw global). Accessible everywhere,
  one instance for the session.
- Owns references to every subsystem (Wanted, Traffic, Economy, Factions, Reactivity, Events).
- Maintains the **shared spatial fields** (heat grid, faction influence grid, property grid)
  aligned to World Partition cells so all systems read/write one source of truth.
- `Tick(float SimDt)` runs at fixed simulation rate; each registered system gets `SimTick(SimDt)`.
- Exposes a lightweight **event bus** (delegates) so systems react without hard coupling
  (e.g. Economy listens to Faction "war started" → price shock).

### 7.1 Data flow example (crime → everything)
```
Player kills cop (witnessed)
  → Wanted: stars 2→3, Heat[cell] += 0.6
  → Factions: Police standing -= 25, Police influence[cell] spikes
  → Economy: ammo/weapon price in cell += shock; nearby shop may close
  → Reactivity: news headline queued; police graffiti/tags surge
  → Traffic: signals preempt for responding units
```

---

## 8. UE 5.8 Framework Mapping (verified against live docs)

| Need | UE 5.8 system | How we use it |
|------|---------------|---------------|
| Police decision logic | **Behavior Tree + Blackboard** | Per-unit BT; Blackboard fed by Perception + EQS results |
| Tactical positioning | **EQS** | Intercept/flank/roadblock generators + tests |
| Sensing | **AI Perception** (Sight/Hearing/Damage) | Cop senses player; drives BT + de-escalation LOS checks |
| 200+ NPCs / 50+ cars | **MassEntity + Mass Avoidance** | Ambient crowd/vehicles data-oriented, no per-actor tick |
| Large streaming city | **World Partition** | Cells = spatial unit for heat/influence/spawn budgets |
| Designer tuning | **DataTables / CurveTables** | Dispatch tables, density curves, price modifiers |
| Central singleton | **UE Subsystem** | `UGTAI_WorldStateManager` as GameInstance/Engine subsystem |
| Deterministic spawn | Seeded PRNG (`FRandomStream`) | Reproducible Poisson arrivals per cell+timeslot |

---

## 9. Implementation Plan (headers delivered with this doc)

| File | Contents |
|------|----------|
| `GTAI_World_WorldStateManager.h` | Central subsystem, spatial grids, event bus, system registry |
| `GTAI_World_Wanted.h` | Deterministic wanted core + dispatch + bribe/hide (BT/EQS hooks) |
| `GTAI_World_TrafficSpawner.h` | Deterministic density spawner + signal controller + route types |
| *(stubbed in doc, next pass)* | `GTAI_World_Economy.h`, `GTAI_World_Factions.h`, `GTAI_World_Reactivity.h` |

All under `namespace GTAI::World`. Module macro `GTAI_WORLD_API`.

---

## 10. Open Decisions for You

1. **[DECISION]** Subsystem tier: `UGameInstanceSubsystem` (survives level stream) vs
   `UEngineSubsystem` (one per process). Recommend GameInstance for PIE/map-restart safety.
2. **[DECISION]** MassEntity vs plain pooled actors for ambient traffic in Phase 1 (Mass is
   experimental in 5.8; pooled `AWheeledVehiclePawn` may be safer for the vertical slice).
3. **[DECISION]** Faction influence grid resolution vs heat-cell resolution (coarse vs fine).
4. **[DECISION]** Police tactic AI: local heuristic EQS (recommended, $0, deterministic-ish) vs
   optional cloud LLM for "creative" roadblock placement (cost, latency — defer to Phase 2).

---

*This document is alive. Decisions get made, added, and dated. Nothing is final until it ships.*
