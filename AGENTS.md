# GTA7_UE5 — Agent Operating Context

This file gives all AI agents (Claude Code, Codex, Cursor, Shenron subagents) the context they need to work on GTAI without losing the plot.

## Mission
Build GTAI — an AI-native open-world game (GTA 7). Set in New York City. Realistic art style. Unreal Engine 5.8.

## The Split
- **Traditional (hand-engineered):** Player movement, vehicle physics, combat, wanted system core, economy, collision, traffic, UI
- **AI-Native (constitutive):** NPC dialogue (LLM), quest generation, police tactics, radio/media, world reactivity, faction dynamics
- **Hybrid:** Mission framework (scripted skeleton + AI flavor), NPC schedules (state machine + LLM), wanted system (deterministic logic + AI tactics)

## Project Location
- Project root: `D:\Projects\GitHub Projects\GTAI\GTA7_UE5\`
- .uproject file: `GTA7.uproject`
**GitHub:** `https://github.com/xxvovalamaxx-ops/GTAI`
- Engine: UE 5.8 at `C:\Program Files\Epic Games\UE_5.8\`

## Module Architecture
```
GTA7/          → Core game module (game mode, game state, player)
GTAI_Core/     → Shared data types, interfaces, config
GTAI_NPC/      → NPC dialogue, memory, schedules, pedestrians
GTAI_World/    → City streaming, traffic, economy, factions, wanted system
GTAI_Vehicles/ → Vehicle physics, damage, vehicle classes
GTAI_Combat/   → Weapons, hit detection, damage, cover system
GTAI_AI/       → LLM integration, quest generation, police tactics
GTAI_UI/       → HUD, phone, minimap, menus
GTAI_Audio/    → Radio system, SFX, NPC voice
GTAI_Quests/   → Quest framework, procedural missions, reputation
```

## Vertical Slice Scope (Phase 1)
1. One NYC neighborhood (~2km x 2km) — start with Manhattan blocks
2. Player movement (third-person, cover)
3. 3 vehicle types (sedan, sports, truck)
4. Pistol + one more weapon
5. Wanted system (3 stars, basic police chase with AI tactics)
6. 5 LLM-driven NPCs with memory
7. 1 emergent quest chain
8. Traffic + pedestrians
9. Phone UI + minimap
10. Dynamic radio station

## Rules
1. Do NOT delete project files or branches without explicit user approval.
2. Do NOT force-push.
3. Do NOT commit Binaries/, Build/, Intermediate/, Saved/, DerivedDataCache/.
4. Keep commits small and readable.
5. All C++ code under namespace `GTA7::*` or `GTAI::*`.
6. Blueprints for gameplay logic, C++ for performance-critical systems.
7. Every AI-generated function MUST be reviewed by a human before merge.
8. Secrets/API keys NEVER go in the repo. Use `.env` or config files in `.gitignore`.
9. Load the `gtai-research-landscape` skill before starting any GTAI subtask.
10. Each specialist agent researches 300+ URLs in their domain and builds own skills.

## Coding Standards
- C++: Follow UE5 coding standards (F prefix for structs, A for actors, U for objects, I for interfaces)
- Blueprints: Descriptive names, no logic in Event Graph beyond input routing
- Python (tools): Type hints, docstrings, `uv` for dependency management

## Design Doc
- Full design document: `E:\Shenron\gtai\DESIGN.md`
- Subagent roster: `E:\Shenron\gtai\SUBAGENT_ROSTER.md`
- Research skill: Load `gtai-research-landscape` via skill_view()

## Available Tools on This Machine
- **Claude Code** (CLI): `claude` — primary AI coding agent
- **OpenAI Codex** (CLI): `codex` — secondary AI coding agent
- **Cursor**: IDE with AI integration
- **VS Code**: IDE (1.127.0)
- **Blender 5.1**: 3D modeling
- **UE5.8**: Game engine
- **Python 3.13**, **Node 24**, **uv**: Scripting
- **Ollama**: Local LLM inference (not running by default)
- **Docker**: Container runtime
- **Hugging Face CLI**: Model download
- **DeepSeek V4 API**: Cloud LLM for NPC dialogue, quest generation
- **Meshy 6 API**: AI 3D asset generation
- **ElevenLabs API**: AI voice synthesis
- **Suno API**: AI music generation