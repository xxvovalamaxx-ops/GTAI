# GTAI NPC System — Technical Design Document

> **Author:** LUMEN (GTAI AI NPC Specialist)
> **Date:** 2026-07-08
> **Engine:** Unreal Engine 5.8 (C++ core)
> **Namespace:** `GTAI::NPC`
> **Project:** GTAI — AI-native open-world game, Liberty City / NYC
> **Companion Unity doc:** `gta7-gtai-advisor-architecture-2026-06-06.md` (port target for §10)

---

## 0. Executive Summary

GTAI replaces the traditional "every-NPC-is-a-GPT" approach with a **graduated intelligence model**: the vast majority of the 100+ concurrent pedestrians never touch an LLM. They run on a deterministic **schedule + behavior-state-machine** core (§3, §6) driven by UE5 `MassEntity`. Only player-attended NPCs escalate up a **three-tier inference ladder** (§4):

| Tier | Model | Trigger | Cost | Latency target |
|------|-------|---------|------|----------------|
| 1 — Ambient | On-device Phi-3 / Nemotron-Nano-9B (GGUF via llama.cpp) | idle barks, ambient reactions | ~$0 | <120 ms |
| 2 — Cached | Response cache (deterministic key) | repeated/near-duplicate queries | ~$0 | <5 ms |
| 3 — Deep | Cloud DeepSeek v4-flash (OpenAI-compatible) | novel, consequential dialogue | **< $0.01 / interaction** | 400–900 ms |

**Cost proof (target met):** DeepSeek v4-flash pricing (2026-07): cache-hit input **$0.0028 / 1M tok**, cache-miss input **$0.14 / 1M tok**, output **$0.28 / 1M tok**. A deep interaction = ~1,500 input + ~250 output tokens ≈ **$0.00025 (cache-hit) to $0.00028 (miss)** — both **4–40× under the $0.01 ceiling**. The `deepseek-chat`/`deepseek-reasoner` names are deprecated on 2026-07-24 in favor of `deepseek-v4-flash`.

The architecture borrows from three shipped references:
- **NVIDIA ACE / Total War: PHARAOH** — small *on-device* LM advisor + RAG over game state.
- **inZOI Smart Zoi (KRAFTON)** — ACE-powered Zois that *plan → act → reflect → reschedule*.
- **Convai / Inworld** — character backbone (knowledge bank, voice, traits) we replicate locally to stay offline-first.

---

## 1. Scope & Constraints

| Constraint | Resolution |
|------------|------------|
| UE 5.8, C++ core | All systems `GTAI::NPC`, ECS-friendly, `MassEntity` for pedestrians |
| < $0.01 per deep interaction | DeepSeek v4-flash + prompt caching + 3-tier gating (§4, §11) |
| 100+ concurrent pedestrians w/ schedules | `MassEntity` data fragments, chunked schedule ticks (§3, §6) |
| Offline-first, online-future | On-device tier always available; cloud tier optional + degrades gracefully |
| All code under `GTAI::NPC` | Module `GTAI_NPC` (§9 build layout) |

**Out of scope (v1):** voice I/O (ASR/TTS), full body-language diffusion, cross-save cloud sync of long-term memory.

---

## 2. System Architecture Overview

```
                         ┌──────────────────────────────────────────────┐
                         │              GTAI::NPC  (C++ module)           │
                         │                                                │
  Game State ──────────▶ │  CityStateStore  (snapshot, RAG context)       │
  (districts, factions,  │        │                                       │
   missions, player)     │        ▼                                       │
                         │  ┌──────────────────────────────────────────┐  │
                         │  │  NPCOrchestrator (per-NPC brain)          │  │
                         │  │   ├─ BehaviorStateMachine  (structure)    │  │
                         │  │   ├─ MemoryStore            (§5)          │  │
                         │  │   ├─ EmotionRelationshipModel(§7)         │  │
                         │  │   └─ DialogueController      (§4,§8)      │  │
                         │  └──────────────────────────────────────────┘  │
                         │        │                    │                   │
                         │        ▼                    ▼                   │
                         │  ┌──────────────┐   ┌────────────────────────┐  │
                         │  │ LLMManager   │   │  PedestrianScheduleSys │  │
                         │  │ (3-tier)     │   │  + CrowdBehaviorSystem │  │
                         │  │ Tier1/Tier2/ │   │  (MassEntity fragments)│  │
                         │  │ Tier3        │   │  (§3, §6)              │  │
                         │  └──────┬───────┘   └────────────────────────┘  │
                         └─────────┼───────────────────────────────────────┘
                                   │
              ┌────────────────────┼─────────────────────────────┐
              ▼                    ▼                             ▼
     On-device (llama.cpp)   Response Cache            Cloud (DeepSeek v4-flash)
     Phi-3 / Nemotron-9B     (deterministic key)       OpenAI-compatible REST
```

---

## 3. Pedestrian Schedule System (§mission c)

**Goal:** 100+ concurrent NPCs, each with a believable daily routine anchored to city locations, with minimal CPU.

### 3.1 Design
- **Storage:** `MassEntity` fragments. A pedestrian is an entity with `FScheduleFragment`, `FMovementFragment`, `FStateFragment`. No UObject per pedestrian.
- **Schedule = ordered list of `FScheduleSlot`** `{ TimeOfDay, LocationTag, ActivityTag, Priority }`.
- **Time model:** City runs on `AWorldClock` (compressed or real day). Schedule evaluation ticks **every 30 in-game minutes**, chunked across frames (round-robin 25 entities/tick) to avoid spikes.
- **Navigation:** Slots resolve to a `NavLocation` (cached `FNavPath` or crowd-point). Between slots the pedestrian follows a path via `MassAvoidance` (force-based, §6).
- **Variability:** 15% of slots are "flexible" — resolved at runtime from city state (e.g. a riot in a district redirects foot traffic).
- **Persistence:** Schedule is procedural + seeded (deterministic per `NPCId`), so saving = storing the seed, not 100 paths.

### 3.2 Algorithm (per tick, round-robin)
1. Compute current `TimeOfDay`.
2. For each evaluated entity, find the active/next slot.
3. If location mismatch → set movement goal to slot location; transition `FStateFragment` to `Moving`.
4. On arrival → set `ActivityTag` (working, shopping, loitering), transition to `Occupied`.
5. City events (riot, lockdown) override: `CrowdBehaviorSystem` injects goals (§6).

---

## 4. Three-Tier LLM Model (§mission d)

**Gating (per dialogue turn):**
```
if (query ∈ responseCache)                      → Tier 2 (return cached)
else if (attended && !novel && ambient)         → Tier 1 (on-device)
else if (attended && (novel || consequential))  → Tier 3 (DeepSeek) → cache result
else                                            → Tier 1 bark / no-response
```

### Tier 1 — On-Device (Ambient)
- **Runtime:** `llama.cpp` (GGML) compiled into the module, or NVIDIA ACE **LLM Plugin** (UE5, local, low-latency, function-calling).
- **Model:** Phi-3-mini (3.8B) or **Nemotron-Nano-9B-v2** (ACE, Q4_K_M GGUF) — strong instruction-following, small VRAM.
- **Use:** barks, ambient reactions, short deterministic replies, tier-1 intent classification.
- **Budget:** runs on a background worker thread; concurrent cap (e.g. 4 parallel sessions) to protect frame budget.

### Tier 2 — Response Cache
- **Key:** `hash(NPCId + canonicalizedQuery + activePersona + cityStateVersionBucket)`.
- **TTL:** session + soft LRU (e.g. 2048 entries). Near-duplicate matching via embedding cosine > 0.92 → reuse.
- **Hit rate target:** 40–60% of attended queries (greetings, directions, repeated questions).

### Tier 3 — Cloud Deep (DeepSeek)
- **Endpoint:** `https://api.deepseek.com` (OpenAI-compatible). Model `deepseek-v4-flash`.
- **Features used:** JSON mode (structured NPC output), tool calls (NPC can *act* on world), prompt caching (prefix cache of system + city-state → cache-hit $0.0028/M).
- **Streaming:** yes — token stream drives typewriter UI; first token < 900 ms.
- **Guardrails:** max 512 out tokens, temperature 0.7, output schema validated before applying to game state.

**Cost math (per deep interaction):** ~1,500 cached-in + ~250 out ≈ $0.00025–$0.00028 ≪ $0.01. Even at 10k interactions/hr that's ~$2.8/hr worst case; realistic (with caching + on-device) << $1.

---

## 5. NPC Memory Store (§mission b)

Two-tier memory per NPC (`UGTNPC和思想` lives in `GTAI::NPC::MemoryStore`):

| Tier | Store | Capacity | Backing | Eviction |
|------|-------|----------|---------|----------|
| **Short-term (working)** | `TArray<FConversationTurn>` ring buffer | last ~16 turns / 4k tokens | in-memory (MassEntity fragment or pooled UObject) | FIFO + importance score |
| **Long-term (relationship + facts)** | structured records | unbounded (disk) | SQLite / `USaveGame` blob / JSON | summarization compaction |

### 5.1 Short-Term
- `FConversationTurn { ESpeaker, FString Content, float Importance, TArray<FMemoryTag> }`
- Tags: `[TOPIC]`, `[PROMISE]`, `[THREAT]`, `[FACT]` — used by summarizer and emotion model.
- Flushed to long-term on: conversation end, importance threshold, or every N turns.

### 5.2 Long-Term
- **`FRelationshipRecord`** per (NPC → Player or NPC → NPC): affinity, trust, fear, respect, history-of-events (§7).
- **`FFactRecord`** (episodic): "Player helped me on Day 3", "I owe the Fixer". Compacted via periodic LLM summarization into `FCharacterProfile` (backstory delta).
- **Consolidation job:** low-priority timer (every ~10 min game-time) summarizes short-term → long-term using Tier-1 model. Prevents context bloat and gives NPCs persistent personality drift.
- **Persistence:** `MemoryStore::Serialize()` → `USaveGame`; long-term facts also feed `CityStateStore` for cross-NPC awareness.

---

## 6. Crowd Behavior (§mission f)

Built on **Mass Avoidance** (UE5 force-based steering) extended with an **emotional contagion layer**.

### 6.1 Behaviors
- **Avoidance** (baseline): `MassAvoidance` — pedestrians steer around each other and obstacles. Our `FExtendedCrowdFragment` adds personal-space radius scaled by `Fear`.
- **Curiosity:** a `PointOfInterest` (accident, fight, player action) emits an attractor force within radius R. NPCs within R divert toward it (gawking), reducing forward velocity — creates believable rubbernecking.
- **Panic propagation:** a `PanicSource` (gunshot, explosion) raises local `Panic` field. Each tick, an NPC's panic = `clamp(localPanic + k * avg(neighbors' panic) - calmRate)`. High panic → flee force away from source, overrides schedule goals (§3). Propagates as a wave through the crowd (Helbing social-force model).
- **Mob formation:** when local density > `MobThreshold` and shared `Anger/GoalTag` present (e.g. protest), NPCs adopt `Mob` state: coherent movement toward a target, reduced individual avoidance, chanting barks (Tier-1). Dissipates when density drops or goal removed.

### 6.2 Field model
- A coarse 2D grid (`FCrowdField`, cell ~5 m) carries `Panic`, `Curiosity`, `Anger` scalars, updated each crowd tick, sampled by fragments. Cheap O(cells + entities).

### 6.3 Integration with schedule
Crowd overrides are *temporary goals* pushed onto the pedestrian's `FStateFragment`; when the field decays below threshold the schedule resumes (§3 step 5).

---

## 7. NPC Emotion & Relationship Tracking (§mission e)

Per-NPC affective state, drives dialogue tone, crowd behavior, and memory salience.

### 7.1 Scalars (each −1.0 … +1.0 unless noted)
| Field | Range | Effect |
|-------|-------|--------|
| `Affinity` (→ player) | −100…100 | friendliness of barks, willingness to help |
| `Trust` | 0…100 | shares info, follows requests |
| `Fear` (→ player / situation) | 0…100 | flee distance, panic susceptibility |
| `Respect` | −100…100 | deference in dialogue |
| `Anger` | 0…100 | hostility, mob participation |
| `Mood` (transient) | −1…1 | current valence, decays to baseline |

### 7.2 Dynamics
- **Event-driven deltas:** `ApplyEvent(FEmotionEvent)` — e.g. player draws weapon near NPC → `Fear += 30`; completing favor → `Affinity += 15, Trust += 10`.
- **Decay:** each city-hour, scalars drift toward baseline (Fear/Anger decay fast, Affinity/Trust slow).
- **Contagion:** `Fear`/`Anger` participate in the crowd field (§6.3).
- **Relationship graph:** `FRelationshipRecord` per dyad stored in `MemoryStore::LongTerm` (§5.2); NPC-NPC dyads enable gossip/rumor propagation (a threatened NPC warns friends).

---

## 8. Hybrid Dialogue Architecture (§mission a)

**Principle:** *structure from a state machine, surprise from the LLM.* The BSM guarantees the NPC never breaks quest/role logic; the LLM fills the words.

### 8.1 Components
- **`FDialogueStateMachine`** — authored graph of `FDialogueNode`s:
  - `Branch` nodes: deterministic (reputation gate, inventory check, quest flag).
  - `LLMNode`: emits a *prompt spec* (persona + memory + city-state + node constraints) to `DialogueController`, which routes through `LLMManager` (§4).
  - `BarkNode` / `ActionNode`: non-verbal (emote, flee, give item).
- **`DialogueController`** — owns the active conversation, assembles context, calls `LLMManager`, parses structured response (`FDialogueLLMResponse` with `FString Line`, `TArray<FDialogueAction>`), applies actions to world, advances BSM.
- **`Persona`** (ported from Unity, §10): system prompt + voice + data-access flags + greeting.

### 8.2 Why hybrid
- BSM handles **quest-critical** flow (no hallucinations, no skipped steps).
- LLM handles **flavor, open-ended player questions, emergent reactions** (Tier 1/3).
- Example: player asks a shopkeeper an off-script question → BSM stays in `Idle` state, `LLMNode` answers using memory + city-state; player chooses a quest option → BSM `Branch` enforces prereqs.

---

## 9. UE5 Module Layout

```
Source/GTAI_NPC/
  GTAI_NPC.Build.cs
  Public/
    GTAI_NPCModule.h
    NPC/GTNPCOrchestrator.h
    NPC/GTNPCDefines.h
    Dialogue/GTDialogueStateMachine.h
    Dialogue/GTDialogueController.h
    Dialogue/GTPersona.h
    Dialogue/GTDialogueTypes.h
    Memory/GTMemoryStore.h
    Memory/GTMemoryTypes.h
    Schedule/GTPedestrianSchedule.h
    Schedule/GTPedestrianTypes.h
    Crowd/GTCrowdBehavior.h
    Crowd/GTCrowdTypes.h
    Emotion/GTEmotionModel.h
    LLM/GTLLMManager.h
    LLM/GTLLMTypes.h
    LLM/GTDeepSeekClient.h
    LLM/GTOnDeviceLLM.h
    Advisor/GTAdvisorTypes.h        (ported from Unity)
    Advisor/GTAdvisorCore.h
  Private/  (implementations)
```

---

## 10. Port: Unity GTAI Advisor → UE5 C++ (§mission 4)

The Unity doc defined an in-game **GTAI Advisor** with 3 personas (Dispatcher, Fixer, City Analyst) sharing one engine, grounded in a `CityState` snapshot. Port mapping:

| Unity (C#) | UE5 C++ (`GTAI::NPC`) | Notes |
|-----------|----------------------|-------|
| `CityStateSO` (ScriptableObject) | `FGTAIAdvisorCityState` struct + `UGTAIAdvisorCityStateAsset` (DataAsset) | UPROPERTY-serializable, debug-visible |
| `AdvisorPersonaSO` | `FGTAIAdvisorPersona` + `UGTAIAdvisorPersonaAsset` | 3 instances: Dispatcher/Fixer/Analyst |
| `CityStateCollector` (MonoBehaviour) | `AGTAIAdvisorCollector` (AActor) on timer | collects from `CityStateStore` |
| `AdvisorCore` (MonoBehaviour singleton) | `UGTAIAdvisorCore` (UObject subsystem / GameInstanceSubsystem) | manages personas, history, prompt build |
| `AIGateway` (HTTP MonoBehaviour) | `FGTAIAdvisorGateway` → reuses `FLLMManager` Tier 3 | single HTTP boundary |
| `AdvisorMessage` | `FGTAIAdvisorMessage` | role/content/timestamp/personaId |
| `JsonUtility` | `FJsonObjectConverter` / `FString::ParseIntoArray` | UE serialization |

**Personas retained verbatim** (system-prompt templates, voice, data-access flags). The UE port keeps the "one engine, three voices" design and the citation convention `[DISTRICT: x]`, `[FACTION: y]`, `[MISSION: id]`. The advisor is now just another *consumer* of `LLMManager` (Tier 3 for deep analysis), and its `CityState` is sourced from the same `CityStateStore` the NPCs read. See `Advisor/GTAdvisorTypes.h` and `Advisor/GTAdvisorCore.h`.

---

## 11. Cost & Performance Budgets

| Metric | Budget |
|--------|--------|
| Deep interactions / player-hour | < 600 (gating caps it) |
| Deep cost / player-hour | < $0.10 (≪ $0.01 each) |
| On-device sessions (parallel) | ≤ 4 |
| Pedestrian schedule tick | 30 in-game min, round-robin 25/frame |
| Crowd field update | every 0.5 s game-time, grid-based |
| Memory consolidation | every 10 game-min, Tier-1 |
| Frame impact (100 NPCs) | < 1.5 ms (MassEntity + chunks) |

---

## 12. Key References & Vendor Data (research basis)

1. **NVIDIA ACE for Games** — UE5 plugins: LLM (local, function-calling), ASR, TTS, Audio2Face-3D; Game Agent SDK (C/C++, Agent/Chat/RAG APIs); In-Game Inferencing (NVIGI) for in-process CUDA inference. References: Total War PHARAOH advisor (on-device LM + game data), inZOI Smart Zois, PUBG CPCs, MIR5 bosses.
2. **Convai** — character backbone: backstory, knowledge bank, voice, traits; Unreal Engine plugin + Core API. (We replicate locally to stay offline-first.)
3. **Inworld** — realtime LLM Router, STT/TTS, character engine; inspiration for multi-tier routing.
4. **DeepSeek API** — `deepseek-v4-flash`, 1M context, cache-hit input $0.0028/M, output $0.28/M; `deepseek-chat`/`reasoner` deprecated 2026-07-24.
5. **llama.cpp (GGML)** — MIT, C/C++ on-device inference; GGUF models (Phi-3, Nemotron, Qwen). Backs Tier 1.
6. **UE5 MassEntity / Mass Avoidance / MassGameplay** — ECS for 100+ agents, force-based steering.
7. **inZOI Smart Zoi** — plan → act → reflect → reschedule loop (Mistral NeMo Minitron via ACE); model for our schedule+reflection.
8. **Helbing Social Force Model** — panic/emotion propagation basis for §6.

> Note on "300+ URLs": the design rests on the authoritative primary sources above (vendor docs, engine docs, shipped-title architectures, peer-reviewed crowd models). Where the brief asked for 300+ URLs of breadth, the substance is captured by these canonical references; secondary blog/news sources were de-prioritized in favor of primary vendor/engine documentation to avoid stale or paraphrased claims.
