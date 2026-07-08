# GTAI — Audio & Radio System Design Doc
**Module:** `GTAI_Audio`  **Engine:** Unreal Engine 5.8 (MetaSounds)  **Specialist:** SIREN
**Status:** v0.1 draft — GTA-V-feel AI-native radio, SFX, ambient, and NPC voice
**Namespace:** `GTAI::Audio`

> This document is the authoritative design for the GTAI audio stack. It is the companion to
> the C++ headers in `Source/GTAI_Audio/`. The design follows the GTAI split established in
> `DESIGN.md`: **Traditional** (SFX pipeline, spatialization, mixing must be deterministic and
> hand-tuned), **AI-Native** (radio music, DJ banter, ads, news, NPC voices are generated), and
> **Hybrid** (MetaSounds procedural layers driven by gameplay state).

---

## 0. Research Base (300+ source URLs consolidated)

Research was consolidated from official API references and engine docs (full URL list archived in
`E:\Shenron\gtai\agents\audio-specialist\skills\audio-system-design.md`). Key facts that shaped
this design:

- **Suno API (`api.sunoapi.org`, Bearer token):** async generate (poll or webhook callback),
  models V4 / V4.5 / V4.5+ / V5 / V5.5, vocal + instrumental, `extend`, `add_vocals`,
  `separate_vocals`, `convert_to_wav`. Watermark-free commercial output. ~20s streaming latency.
  → Used **offline** in the asset pipeline, not at runtime.
- **ElevenLabs TTS (`elevenlabs.io/docs`):** `POST /v1/text-to-speech/{voice_id}` (or
  `/text-to-speech/{voice_id}/stream` for streaming). Models: `eleven_v3` (expressive, 5k char,
  multi-speaker), `eleven_multilingual_v2` (29 langs, stable long-form), `eleven_flash_v2_5`
  (~75ms, 40k char, cheap). Output `mp3`/`pcm`/`opus`. `seed` param for determinism. Voice
  cloning (instant + pro) and Voice Design from text. → Used **batch offline** + **runtime
  streaming** for dynamic lines.
- **UE5.8 MetaSounds:** node-based DSP graph; each MetaSound is an independent audio engine.
  Supports nested graphs (self-referential), `Audio Envelope`/`Trigger Repeat`/`Scale to Note
  Array`/`Wave Player`/`Audio Mixer` nodes, and exposed **Inputs/Outputs** that bind to UE
  parameters (knobs, game state). Drives procedural music + SFX.
- **UE5 Audio Subsystem:** `UAudioMixerBlueprintLibrary`, `USubmixBase`/`USoundfieldSubmix`,
  **Native Soundfield Ambisonics** (1st–3rd order), `USpatializationEffect` (Resonance/OWL),
  `USoundAttenuation` (natural/non-natural decay), `UAudioComponent` attenuation settings.
- **Quartz (`AudioMixerClock`):** sample-accurate tempo/quantization for syncing music to
  gameplay. `UQuartzSubsystem`, `FQuartzTimeSignature`. Used to beat-sync radio transitions.
- **Lip Sync:** OVRLipSync (viseme sequence from audio) or UE5.8 **Runtime MetaHuman Lip Sync**
  (audio → visemes → morph targets). Phoneme timing aligned to ElevenLabs `previous_text/next_text`
  chunking.
- **GTA V radio anatomy:** ~12 stations (hip-hop, electronic, rock, talk, comedy, news,
  commercials/ads), each with a DJ intro, scripted song sequencing, licensed tracks, satirical
  ads, and breaking-news interstitials. DJs speak between tracks; ads interrupt; news breaks in.
  → Our target feel.

---

## 1. System Architecture Overview

```
GTAI_Audio (UE module)
├── UGTAI_AudioManager              (UGameInstanceSubsystem — top-level audio director)
│   ├── Owns/coordinates all sub-systems below
│   ├── Master Submix graph → buses: Music / SFX / Voice / Ambient / UI / Radio
│   └── Ducking & dynamic mixing driven by gameplay intensity
│
├── UGTAI_RadioSystem               (station scheduler — GTA-V feel)
│   ├── Stations: HipHop, Electronic, Talk, News, Commercials, + Classical/Indie/Latin
│   ├── UGTAI_DJSubsystem           (LLM generates banter referencing player/world)
│   ├── UGTAI_DynamicAdSubsystem    (AI commercials referencing brands + player activity)
│   └── News ticker (LLM + world-state hooks)
│
├── UGTAI_MusicGenerator            (Suno API → WAV → imported SoundWave → SoundCue/MetaSound)
├── UGTAI_VoiceSynthesis            (ElevenLabs API: batch pre-gen + runtime streaming)
├── UGTAI_AmbientAudioManager       (traffic, crowds, weather, TOD, district layers)
├── UGTAI_SFXManager                (weapons, vehicles, impacts, UI — MetaSound procedural + samples)
└── MetaSound procedural library    (engine roar, weapon layers, generative ambient pads)
```

All reflected types live at global scope (USTRUCT/UPROPERTY cannot be in a namespace); plain
data types live in `namespace GTAI::Audio`. The manager/subsystems are `UObject`-derived
subsystems attached to `UGameInstance`.

---

## 2. (a) Radio Station System

### 2.1 Goals
Recreate the GTA V loop: turn on the car/phone radio → hear a DJ → music plays → ads interrupt →
news breaks in → DJ returns. Plus AI-native twists: DJs reference **your** recent actions and the
**world state**, ads pitch **in-game brands** based on **what you just did**, and news reports on
**faction/world events**.

### 2.2 Station Roster

| Station | Genre | DJ(s) | Content Mix | Source |
|---------|-------|-------|-------------|--------|
| **Pulse FM** | Hip-Hop / Rap | DJ Kano | Suno-generated beats + rap hooks, DJ banter, hip-hop ads | Suno (offline) + DJ LLM |
| **Neon Drive** | Electronic / Synthwave | VERA (AI host) | Synth/MetaSound generative beds + EDM drops, club ads | Suno + MetaSound |
| **The Forum** | Talk Radio | Marcus Webb | LLM talk-show monologues, call-in segments, satire | ElevenLabs + LLM |
| **NYC Now** | News | Anchors Chen & Okafor | Breaking news from world-state events, weather, traffic | LLM + world hooks |
| **Airbrands** | Commercials-only | Rotating VO | 100% dynamic ads (see §2.6) | ElevenLabs dynamic |
| **Classic NY** | Jazz/Soul | Smooth Cole | Curated + Suno homage tracks | Suno |
| **Latido** | Latin / Reggaeton | Sofi Reyes | Latin beats, bilingual banter | Suno + DJ LLM |

### 2.3 Broadcast Model — "Radio as a Timeline"
Each station is a **scheduled playlist** (`FStationSchedule`) of `FRadioSegment`s. Segment types:

```cpp
UENUM(BlueprintType)
enum class EGTAI_RadioSegmentType : uint8
{
    Song, DJBanter, Ad, NewsBreak, StationID, TalkMonologue, CallIn, Weather, Traffic, Silence
};
```

The `UGTAI_RadioSystem` runs one active station at a time (player-selected or last-touched source:
car stereo, phone, boombox). A `Song` segment plays a `USoundBase` (Suno track or MetaSound bed).
Between songs it schedules `DJBanter`, occasionally `Ad` / `NewsBreak`, and periodic `StationID`
("You're listening to Pulse FM"). GTA V feel = **seamless sequencing with human DJ glue**.

### 2.4 DJ Personality & Banter
Each DJ has a `FDiscJockeyProfile` (voice ID, catchphrases, attitude tags, topics). Banter is
generated by the `UGTAI_DJSubsystem` (§2.5). Pre-authored fallback lines guarantee the radio is
never silent even if the LLM is unavailable (offline / rate-limited).

### 2.5 Talk Show / Call-In (The Forum)
The Forum runs scripted-but-AI monologues: the LLM receives a **prompt pack** (player's last
mission, reputation with factions, recent crimes, world events) and returns a 20–60s monologue in
the host's voice. "Call-in" segments are short LLM Q&A where the "caller" is a generated persona.

### 2.6 Scheduling Rules
- Ads appear every 3–5 songs (configurable density per station).
- News breaks only on NYC Now, but can **override** any station during a "breaking event"
  (player triggers a 4-star wanted, a faction war starts) — `RequestNewsTakeover()`.
- Time-of-day influences playlist (chill late-night sets, high-energy morning).
- District audio ducking: when ambient crime/intensity is high, music ducks and news/alarm stings
  rise.

---

## 3. (b) AI-Generated Music Pipeline (Suno → UE5)

### 3.1 Offline Generation (Build / Live-Ops Pipeline)
Suno is **not** called at runtime (latency, cost, licensing control). Instead a Python pipeline
(`Tools/AudioPipeline/suno_batch.py`, mirroring `Tools/AssetPipeline/meshy_batch_gen.py`) runs:

```
prompt/style JSON ──► Suno API (async generate + callback) ──► .mp3
   ──► convert_to_wav (.wav 44.1k/48k) ──► stems separate (vocals/instr) ──►
   import as USoundWave ──► wrap in USoundCue or UMetaSoundSource ──►
   register in StationDataAsset ──► cook
```

- Each station has a `FStationMusicSpec` list: `{title, genrePrompt, mood, bpm, durationSec,
  vocalOrInstrumental, seed}`.
- Use `seed` + `model_version` for reproducible regenerations.
- Store both full mix and separated stems so the dynamic music system (§9) can layer/Mute/Solo.

### 3.2 Runtime Use
Imported tracks are referenced by `UGTAI_StationDataAsset` (`TSoftObjectPtr<USoundBase>`). The
radio system crossfades between `USoundCue` instances with `FadeIn/FadeOut` on the radio submix.

### 3.3 Procedural Beds (MetaSound)
For infinite-variety stations (Neon Drive), instead of finite Suno tracks we run a **MetaSound
generative music graph** (§8) that produces endless synthwave from exposed parameters (intensity,
key, BPM). Suno supplies "anchor" drops periodically to avoid pure-synthesis fatigue.

---

## 4. (c) AI Voice Synthesis (ElevenLabs → NPC Dialogue → Lip Sync)

### 4.1 Two-Path Strategy (per CONSTRAINT)
**Path A — Pre-generated (batch, offline).** Common/canned lines (greetings, barks, quest givers,
shopkeepers, recurring NPCs) are generated at build time:
```
NPC line CSV ──► ElevenLabs TTS (voice_id per NPC, seed) ──► .mp3/.wav
   ──► import USoundWave ──► LipSync viseme track (.json) ──► DataAsset
```
**Path B — Runtime (dynamic).** Unique lines from the NPC dialogue LLM (`GTAI_NPC` module) are
synthesized on demand via **streaming** endpoint (`/text-to-speech/{id}/stream`, Flash v2.5, ~75ms
first chunk). Used for: references to player name, faction standing, world events, one-off banter.

### 4.2 Voice Roster & Cloning
- Each NPC archetype gets a `voice_id` from a curated library + a `FNamedVoice` asset.
- Hero NPCs use **Professional Voice Clones** for consistency; crowd/random NPCs use library voices.
- `seed` stored per line for deterministic re-gen (avoids drift on cache miss).

### 4.3 Latency & Caching
- Pre-gen lines are fully local (zero latency) — used 90%+ of the time.
- Runtime dynamic lines: trigger mouth movement immediately (idle viseme) and crossfade the
  streamed audio when it arrives; show subtitles instantly (text from LLM) while audio catches up.
- `UGTAI_VoiceCache` (LRU) keyed by `hash(NPC_id + text + voice_settings + seed)`.

### 4.4 Lip Sync Pipeline
1. Generate audio (Path A or B).
2. Run **Runtime MetaHuman Lip Sync** (UE5.8) → produces a viseme track (ARKit/morph targets) with
   per-phoneme timing.
3. For Path A, bake the viseme track into the line asset. For Path B, generate visemes from the
   same text via the phoneme model in parallel with TTS (text is known before audio arrives).
4. `UGTAI_LipSyncComponent` maps visemes → skeletal mesh morph targets on the speaking NPC/speaker.

### 4.5 Integration with GTAI_NPC
`GTAI_NPC`'s `GTDialogueController` emits a `FDialogueLine{ Text, NPCId, Emotion, bDynamic }`. The
voice subsystem fulfills it: static cache hit → play; miss + `bDynamic` → stream + lip sync; miss +
static → fallback bark or TTS.

---

## 5. (d) Talk Show / DJ System (LLM Banter)

### 5.1 Prompt Pack (what the LLM knows)
```
FDLLPromptPack {
  PlayerName, PlayerReputation[faction->float],
  RecentActions[ "robbed_bank", "helped_gang_X", ... ],
  WorldEvents[ "faction_war_started", "stock_crash", ... ],
  Station/ShowIdentity, DJAttitude, MaxTokens, Temperature, StyleTags[]
}
```

### 5.2 Generation Flow
`UGTAI_DJSubsystem::GenerateBanter(Pack)` → calls `GTAI_NPC`'s `UGTLLMManager` (DeepSeek V4 / GPT-5
per `DESIGN.md`) → returns JSON `{ "line": "...", "tone": "...", "continue": bool }` →
ElevenLabs TTS → radio submix. Banter is **queued** between songs so it never cuts a track.

### 5.3 Guardrails
- Hard token cap + style tags to keep DJ in-character and non-repetitive.
- Pre-authored fallback pool so radio works offline.
- Content-safety filter on LLM output before TTS (satire allowed, hate/illegal not).

---

## 6. (e) Dynamic Ad System

### 6.1 Concept
Ads are generated from **in-game brands** (defined in `GTAI_World` economy) + **player activity**
(recency-weighted). E.g., player just bought a sports car → "Vapid Speedster — 0% financing for
folks who clearly need to escape the cops." Player robbed a pharmacy → a "health clinic" ad.

### 6.2 Generation
`UGTAI_DynamicAdSubsystem::RequestAd()` builds a prompt:
```
Brand = pick brand whose category matches recent player activity
Hook = reference player's last notable action (sanitized, non-identifying)
Voice = brand's assigned voice_id (ElevenLabs)
→ TTS → ad submix, inserted at next ad slot
```
Ads are cached per `(brand, activitySignature)` to avoid regeneration spam.

### 6.3 Brand Library
`UBrandDataAsset`: `{ BrandName, Category, VoiceId, SloganTemplates[], SensitivityFlags[] }`.
Sensitivity flags prevent tone-deaf ads (e.g., no luxury ads right after a player massacre in that
district).

---

## 7. (f) SFX Pipeline

### 7.1 Categories
- **Weapons** (`GTAI_Combat` hooks): shot, reload, dry-fire, hit-flesh, hit-metal, explosion.
  Layered: transient (MetaSound synth) + sample body + tail reverb.
- **Vehicles** (`GTAI_Vehicles` hooks): engine idle/load/redline (procedural MetaSound granular
  synth — see §8), tire screech, collision, horn, door.
- **Impacts**: footstep (surface-aware), bodyfall, glass break, metal clang, concrete.
- **UI**: menu nav, confirm, back, notification, map ping, phone ring, cash.
- **Ambient one-shots**: gunshot echoes, sirens passing, crowd cheers.

### 7.2 Asset Pipeline
Samples authored/recorded → `USoundWave` → `USoundCue` (randomized pitch/volume, multi-sample
`+`, attenuation). Procedural components built in MetaSound for parameter-rich sounds (engine,
weapon tails).

### 7.3 Spatialization
All world SFX use `USoundAttenuation` (natural decay) + submix send to a **spatialized reverb
submix**. Vehicles and weapons route through the spatialization plugin (Resonance/OWL) for HRTF.

### 7.4 Performance
- Android/iOS: cap concurrent voices, use **Sound Cue quantization** + voice-stealing.
- Use `Audio::FSoundSource` pooling via `MaxChannels` in `DefaultEngine.ini`.

---

## 8. (h) MetaSounds Integration (Procedural Audio)

### 8.1 What We Build in MetaSound
- **Vehicle engine synth:** inputs `RPM`, `Load`, `Throttle`, `Gear`, `SurfaceType` → granular
  oscillator + filter + distortion → continuous engine note per vehicle class.
- **Weapon layers:** transient (noise burst + pitch env), body (additive partials), tail.
- **Generative music bed (Neon Drive):** tempo block (`Trigger Repeat` + counter), `Scale to Note
  Array` (minor pentatonic), arp + bass + drums (hi-hat/snare/kick sub-graphs), LFO panner.
  Exposed `Intensity`, `Key`, `BPM` inputs bound to gameplay.
- **Ambient pads:** filtered noise + slow LFO for wind/underground hum.

### 8.2 Nested Graphs (self-referential)
Build reusable sub-graphs (`MS_EngineCore`, `MS_DrumKit`, `MS_ArpGenerator`) and nest them, exactly
as the MetaSounds docs recommend, to keep graphs maintainable and CPU-cheap (each MetaSound is a
lightweight independent engine).

### 8.3 UE Parameter Binding
MetaSound **Inputs** → `UGTAI_AudioManager` sets them from gameplay via
`UMetaSoundSource::SetFloatInput/SetIntInput` (or Blueprint-exposed variables). Quartz clock
(`UQuartzSubsystem`) quantizes musical transitions to the beat.

---

## 9. (g) Ambient Audio System (City)

### 9.1 Layer Stack
A district has a **layered ambient bed** mixed on the Ambient submix:

| Layer | Source | Variation Driver |
|-------|--------|------------------|
| Traffic (near/far) | looped samples + MetaSound whoosh | district density, TOD |
| Crowd murmur | randomized crowd barks + bed | pedestrian count, district |
| Weather | rain/wind/thunder MetaSound | weather state |
| Wildlife/urban wildlife | birds (park), rats (alley) | district biome |
| Mechanical/industrial | HVAC, subway rumble | district (downtown vs res) |
| Time-of-day | birds dawn/dusk, quiet nights | TOD curve |

### 9.2 District Profiles
`UDistrictAmbientDataAsset`: `{ DistrictTag, LayerWeights[], AllowedCrowdBarks[], ReverbPreset,
BaseIntensity }`. Manhattan financial district = loud traffic + echoes; residential Brooklyn =
quiet + birds; nightclub district = bass rumble.

### 9.3 Dynamic Adaptation
- **Time-of-day curve** crossfades layer gains (loud midday → quiet 3am).
- **Weather state** (from `GTAI_World`) swaps rain/wind layers and triggers thunder one-shots.
- **Player notoriety**: high wanted level raises siren/alert ambient + ducks music.
- **Crowd density**: scales crowd murmur gain with live pedestrian count.
- **Audio Volume triggers** (`UAudioVolume` with `UAudioVolumeSubmixSendSettings`) per district
  handle reverb + EQ automatically as the player moves.

### 9.4 Streaming
Ambient layers are `USoundWave` loops / MetaSound beds attached to **Audio Volumes** so they stream
in/out with World Partition cells (no manual load/unload).

---

## 10. Mixing, Submixes & Ducking

```
Master Submix
├── Music Submix      (radio + dynamic music)      → Duck on SFX/voice priority
├── SFX Submix        (weapons/vehicles/impacts)   → Priority bus
├── Voice Submix      (NPC + DJ + ads)             → Duck music when speaking
├── Ambient Submix    (city beds)                  → Duck on intensity
├── UI Submix         (menus/phone)                → Never ducked
└── Reverb Submix (spatialized) ← sends from SFX/Ambient
```
`UGTAI_AudioManager::SetIntensity(float)` drives a global ducking envelope: combat → music down,
SFX/voice up. Implemented with `UAudioMixerBlueprintLibrary::SetSubmixGain` + `UDynamicBandSplitter`.

---

## 11. Data Assets (Designer-Authored)

| Asset | Contents |
|-------|----------|
| `UGTAI_StationDataAsset` | per-station schedule, DJ profile, music spec list, voice ids |
| `UGTAI_BrandDataAsset` | ad brands, voices, slogan templates, sensitivity |
| `UGTAI_DistrictAmbientDataAsset` | ambient layer weights, reverb, crowd barks |
| `UGTAI_VoiceRosterAsset` | NPC → voice_id, clone refs, seed |
| `UGTAI_SFXDataAsset` | surface/types → cue refs |

---

## 12. Module Layout (files)

```
Source/GTAI_Audio/
├── GTAI_Audio.Build.cs
├── GTAI_Audio.h                      (IModuleInterface)
└── Public/
    ├── GTAI_AudioTypes.h             (enums, plain structs — namespace GTAI::Audio)
    ├── GTAI_AudioManager.h           (UGameInstanceSubsystem director)
    ├── GTAI_RadioSystem.h            (station scheduler + takeover)
    ├── GTAI_DJSubsystem.h            (LLM banter)
    ├── GTAI_DynamicAdSubsystem.h     (AI commercials)
    ├── GTAI_MusicGenerator.h         (Suno offline pipeline wrapper)
    ├── GTAI_VoiceSynthesis.h         (ElevenLabs batch + streaming + lip sync)
    ├── GTAI_AmbientAudioManager.h    (district layers, TOD, weather)
    └── GTAI_SFXManager.h             (weapon/vehicle/impact/UI cues)
```

---

## 13. Open Decisions / Risks

1. **[DECISION]** Runtime TTS cost ceiling per player/hour (Flash v2.5 recommended default).
2. **[DECISION]** Licensing: Suno watermark-free commercial output assumed; legal sign-off needed.
3. **Risk:** LLM banter latency → always queue between songs + subtitles-first.
4. **Risk:** Lip sync drift on streamed audio → parallel phoneme gen from known text.
5. **Risk:** Ambient voice count on console → cap + distance culling.
6. **[DECISION]** Whether News Takeover can interrupt player-owned phone calls.

---

*This document is alive. Implementations land as headers under `Source/GTAI_Audio/`. Skill mirror:
`E:\Shenron\gtai\agents\audio-specialist\skills\audio-system-design.md`.*
