# GTAI Quest System — Technical Design Document

> **Agent:** SAGA (Quest System Architect)
> **Date:** 2026-07-08
> **Engine:** Unreal Engine 5.8 (C++ framework, Python LLM pipeline)
> **Namespace:** `GTAI::Quests`
> **Integrates with:** ORACLE (factions/reputation), LUMEN (NPC dialogue)

---

## 0. Executive Summary

GTAI's quest system generates missions emergently from world state + player reputation, not from pre-written scripts. A deterministic skeleton (objective types, rewards, fail states) is filled with AI-generated content (dialogue, specific targets, flavor text). The player creates their own story by playing.

## 1. Quest Framework

### Objective Types
| Type | Description | Example |
|------|-------------|---------|
| Fetch | Retrieve item from location | "Get the package from the docks" |
| Assassinate | Eliminate specific NPC | "Take out the rival gang leader" |
| Escort | Protect NPC to destination | "Get the witness to the courthouse" |
| Hack | Reach terminal, mini-game | "Disable the security grid" |
| Race | Reach point in time limit | "Deliver before the cops catch you" |
| Delivery | Transport item, avoid detection | "Bring the cash to the launderer" |
| Survival | Survive waves for duration | "Hold the block until backup arrives" |
| Investigate | Gather clues, talk to NPCs | "Find out who talked to the cops" |

### Quest Structure (JSON)
```json
{
  "id": "quest_2026_001",
  "title": "The Docks Deal",
  "giver": "npc_fixer_01",
  "type": "delivery",
  "priority": "normal",
  "objectives": [
    {"id": "obj_1", "type": "goto", "target": "docks", "description": "Go to the docks"},
    {"id": "obj_2", "type": "collect", "item": "package_01", "description": "Pick up the package"},
    {"id": "obj_3", "type": "deliver", "target": "warehouse_05", "description": "Deliver to the warehouse"}
  ],
  "rewards": {"money": 5000, "reputation": {"southside_crew": 10}, "items": ["lockpick_mk2"]},
  "fail_conditions": ["player_arrested", "package_destroyed", "time_expired"],
  "branching": {
    "stealth_success": "quest_2026_002_silent_partner",
    "caught": "quest_2026_003_heat"
  },
  "dialogue": {"intro": "...", "success": "...", "fail": "..."},
  "time_limit": 600,
  "min_reputation": {"southside_crew": -20}
}
```

### Reward Structures
- Money (variable by difficulty + reputation)
- Reputation changes (faction-specific)
- Items (weapons, vehicles, access)
- Unlock flags (new areas, new contacts, new quest chains)

### Fail States
- Player arrested/killed
- Target escapes
- Time expired
- Item destroyed
- Reputation threshold breached (faction turns hostile)

### Branching
- Each quest has 2-3 branches based on outcome
- Branches unlock new quests or modify world state
- Failed quests can have consequences (not just "try again")

## 2. LLM Quest Generation Pipeline

### Input
```json
{
  "world_state": {"crime_index": 65, "police_presence": 40, "active_factions": [...]},
  "player_reputation": {"southside_crew": 35, "lcpd": -10, "corp_aether": 5},
  "player_level": 12,
  "recent_actions": ["killed_gang_member", "helped_shopkeeper"],
  "constraints": {"max_objectives": 5, "forbidden_types": ["assassinate"], "budget_tier": "low"}
}
```

### LLM Prompt (DeepSeek V4)
```
You are a quest generator for an open-world crime game set in NYC.
Given the current world state and player reputation, generate ONE quest.
Rules:
- Quest must be achievable in the current world state
- Must respect player's faction relationships
- 3-5 objectives maximum
- Include intro/success/fail dialogue
- Rewards must be balanced (money + reputation)
- Output as JSON matching the quest schema
- Do NOT generate quests that require locations not in the city
```

### Output
Validated quest JSON (schema-checked before injection into game).

### Cost
~$0.005-0.01 per quest generation (DeepSeek V4 flash, ~500 token output).

## 3. Reputation System

### Factions
| Faction | Type | Territory | Default Standing |
|---------|------|-----------|-----------------|
| Southside Crew | Gang | Southside | 0 |
| Downtown Syndicate | Gang | Financial District | 0 |
| Aether Corp | Corporation | Midtown | 0 |
| NYPD | Police | Citywide | 0 |
| City Hall | Political | Government District | 0 |
| Dock Workers | Union | Waterfront | 0 |

### Standing Scale: -100 to +100
- -100 to -50: Hostile (attack on sight, bounty on player)
- -50 to -20: Unfriendly (refuse services, charge more)
- -20 to +20: Neutral (default)
- +20 to +50: Friendly (discounts, tips, quests available)
- +50 to +100: Allied (backup in fights, exclusive quests, safe houses)

### Consequences
- Reputation changes ripple: helping one faction may anger another
- Territory influence shifts: faction control of districts changes based on player actions + world events
- Shop prices, quest availability, NPC greetings all affected by reputation
- Extreme negative reputation triggers bounty hunters

## 4. Emergent Narrative Engine

### Chain Quests
- Completing a quest affects future quest availability
- Quest A success → Quest B unlocked (chain)
- Quest A failure → Quest C unlocked (alternative chain)
- Callbacks: NPCs reference prior player actions in dialogue (via LUMEN memory)

### Consequence Ripple
- Kill a gang leader → power vacuum → faction infighting → new quest opportunities
- Help a shopkeeper → their friends give discounts → network of allies grows
- Get arrested → reputation drops → certain NPCs won't talk to you

### Narrative Memory
- World state tracks last 20 player actions
- LLM quest generator receives recent_actions as context
- NPC dialogue (LUMEN) receives player action history for callbacks

## 5. Mission Giver System

### How NPCs Offer Quests
1. NPC has `QuestGiverComponent`
2. On player interaction, checks: relationship level + world state + faction standing
3. If conditions met, requests quest from LLM pipeline
4. Quest is generated, validated, and offered to player
5. Player accepts/rejects via dialogue (LUMEN)

### Quest Giver Tiers
- Fixers: Professional quest givers, always have work, varied types
- Faction Contacts: Faction-specific quests, require standing +20
- Random NPCs: Emergent quests from world events, one-shot
- Phone Contacts: Call-based quests, can trigger remotely

## 6. Quest Tracking & Journal

### Active Quests
- Tracked in `GTAI_QuestJournal` (GameInstanceSubsystem)
- Each active quest has: current objective index, elapsed time, status
- Map markers spawned for current objective location
- Phone app shows quest list with details

### Quest States
- Available (not yet accepted)
- Active (in progress)
- Completed (success)
- Failed (fail state triggered)
- Abandoned (player cancelled)

### Journal UI (hooks to VISTA)
- Active quest list
- Objective checklist
- Map markers
- Reward preview
- Quest giver info
- Time remaining (if timed)