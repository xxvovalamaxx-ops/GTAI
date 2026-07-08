#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
quest_generator.py
==================
SAGA — GTAI Quest System Architect.

Takes a *world state JSON* (the ORACLE snapshot: districts, NPCs, factions,
player standing, heat, etc.) and produces a *schema-validated quest JSON*
via the DeepSeek V4 LLM API (OpenAI-compatible endpoint, JSON mode).

Design pillars (see E:\\Shenron\\agents\\workspaces\\quest-architect\\skills):
  1. Deterministic skeleton + AI content
       - Deterministic: objective types, reward math, fail states, branching.
       - AI-generated:  title, dialogue, flavor, specific target selection.
  2. Radiant selection (Skyrim/Fallout heritage)
       - Pick a giver, a target NPC, and a location the player has NOT yet
         discovered, biased by player reputation & territory control.
  3. Watch_Dogs_Legion "play-anyone" emergent NPC reuse
       - Any ambient NPC in the world state can be promoted to a quest role
         (giver / target / beneficiary) with a generated backstory.
  4. DeepSeek JSON mode best practices
       - response_format={'type':'json_object'}, the word "json" in the
         prompt, an example schema, generous max_tokens, retry-on-empty.
  5. Reward balance via a quadratic XP curve (Diablo-class economy) modulated
     by risk, faction weight, and chain depth.
  6. Faction reputation ripple: completing a quest moves standing with the
     quest's faction AND with its allied/rival factions (signed transfer).
  7. Quest-chain generation: a quest can spawn follow-ups; depth, branch
     factor and lock conditions are computed deterministically.

Usage
------
  # Generate a single quest from a world-state file:
  python quest_generator.py --world world_state.json --out quest.json

  # Generate a full quest chain (parent + auto follow-ups):
  python quest_generator.py --world world_state.json --chain --max-depth 3 \
        --out chain.json

  # Dry run (no network, deterministic mock quest) for testing the pipeline:
  python quest_generator.py --world world_state.json --dry-run --out quest.json

  # Point at a self-hosted / alt DeepSeek-compatible endpoint:
  python quest_generator.py --world world_state.json \
        --api-base https://api.deepseek.com --model deepseek-v4-pro

Exit code 0 on success, 1 on failure.

Env / config:
  DEEPSEEK_API_KEY  (required unless --dry-run)
  DEEPSEEK_API_BASE (default https://api.deepseek.com)
  DEEPSEEK_MODEL    (default deepseek-v4-pro)
"""

from __future__ import annotations

import argparse
import json
import math
import os
import random
import sys
from dataclasses import dataclass, field, asdict
from typing import Any, Dict, List, Optional

try:
    import requests
except ImportError:  # pragma: no cover
    requests = None

try:
    import jsonschema
except ImportError:  # pragma: no cover
    jsonschema = None


# --------------------------------------------------------------------------- #
#  GTAI CONSTANTS (mirror of GTAI_ReputationSystem.h / GTAI_QuestTypes.h)
# --------------------------------------------------------------------------- #
FACTIONS = [
    "Southside Crew",
    "Downtown Syndicate",
    "Aether Corp",
    "NYPD",
    "City Hall",
    "Dock Workers",
]

# Standing tiers (ReputationSystem, -100..+100)
TIERS = [
    (-100, -71, "Hostile"),
    (-70, -31, "Unfriendly"),
    (-30, 30, "Neutral"),
    (31, 70, "Friendly"),
    (71, 100, "Allied"),
]

# Faction relationship matrix.  +1 ally, -1 rival, 0 neutral.
# Symmetric.  Used for the *reputation ripple* (a quest for one faction
# nudges its allies/rivals as well).  Grounded in classic GTA-style
# gang/police/corp triangles.
FACTION_RELATIONS: Dict[str, Dict[str, int]] = {
    "Southside Crew":     {"Downtown Syndicate": -1, "NYPD": -1, "Dock Workers": 1},
    "Downtown Syndicate": {"Southside Crew": -1, "Aether Corp": 1, "NYPD": -1},
    "Aether Corp":        {"Downtown Syndicate": 1, "City Hall": 1, "NYPD": 1},
    "NYPD":               {"Southside Crew": -1, "Downtown Syndicate": -1,
                           "Aether Corp": 1, "City Hall": 1},
    "City Hall":          {"Aether Corp": 1, "NYPD": 1, "Dock Workers": -1},
    "Dock Workers":       {"Southside Crew": 1, "City Hall": -1},
}

# Quest archetypes (deterministic templates the LLM fills in).
QUEST_ARCHETYPES = [
    "retrieval", "assassination", "escort", "delivery",
    "intel", "sabotage", "heist", "recruitment",
]

# Objective types that may appear inside a quest (QuestTypes.h EObjectiveType).
OBJECTIVE_TYPES = [
    "ReachLocation", "EliminateTarget", "CollectItem", "TalkToNPC",
    "HackTerminal", "ProtectNPC", "DeliverItem", "TriggerEvent",
]

REWARD_TYPES = ["cash", "xp", "reputation", "item", "territory", "unlock"]


# --------------------------------------------------------------------------- #
#  REWARD BALANCE (quadratic XP curve, Diablo-class economy)
# --------------------------------------------------------------------------- #
# XP offered for a quest of nominal difficulty d (1..10) at chain depth p:
#     XP(d, p) = BASE * d^2 * (1 + DEPTH_BONUS * p)
# Cash scales with XP times a risk multiplier.  Reputation is a fixed signed
# delta independent of level (so a single quest never flips a faction tier).
XP_BASE = 50.0
DEPTH_BONUS = 0.35          # each chain step adds 35% XP
RISK_CASH_RATIO = 0.4       # cash_per_xp baseline; scaled by archetype risk
REP_DELTA = 8                # standing points granted to quest faction


def tier_for(standing: int) -> str:
    for lo, hi, name in TIERS:
        if lo <= standing <= hi:
            return name
    return "Neutral"


def clamp(v: float, lo: float = -100.0, hi: float = 100.0) -> float:
    return max(lo, min(hi, v))


def compute_rewards(
    difficulty: int,
    chain_depth: int,
    archetype: str,
    player_level: int,
) -> Dict[str, Any]:
    """Deterministic reward math (the 'skeleton' half of the pipeline).

    difficulty   : 1..10 nominal quest difficulty
    chain_depth  : 0 = root quest, 1+ = follow-up
    archetype    : drives a risk multiplier
    player_level : used to soft-target a level-appropriate band
    """
    d = max(1, min(10, int(difficulty)))
    p = max(0, int(chain_depth))
    risk = {
        "retrieval": 1.0, "delivery": 1.0, "escort": 1.3, "intel": 1.4,
        "recruitment": 1.5, "sabotage": 1.8, "heist": 2.0,
        "assassination": 2.2,
    }.get(archetype, 1.2)

    xp = int(round(XP_BASE * (d ** 2) * (1.0 + DEPTH_BONUS * p)))
    # Level-scaling: keep offered XP within ~0.6x..1.4x of the player's
    # expected band (expected ~ 40 * level^1.5) — a gentle rubber band.
    expected = 40 * (player_level ** 1.5)
    band = clamp(xp / max(expected, 1.0), 0.6, 1.4)
    xp = int(round(xp / band))

    cash = int(round(xp * RISK_CASH_RATIO * risk))
    rep = int(round(REP_DELTA * (1.0 + 0.1 * p)))

    return {
        "xp": xp,
        "cash": cash,
        "reputation_delta": rep,      # signed; applied to quest faction
        "risk_multiplier": round(risk, 2),
    }


# --------------------------------------------------------------------------- #
#  REPUTATION RIPPLE
# --------------------------------------------------------------------------- #
def reputation_ripple(
    faction: str,
    delta: int,
    relations: Dict[str, Dict[str, int]] = FACTION_RELATIONS,
) -> Dict[str, int]:
    """Compute the standing deltas across ALL factions for a quest outcome.

    Core faction gets the full signed delta.  Allies receive a fraction of the
    SAME sign (helping a gang's ally improves your standing with the ally too);
    rivals receive the OPPOSITE sign scaled down (helping a gang angers its
    enemies).  This is the 'consequences ripple' called out in the skill doc.
    """
    out: Dict[str, int] = {f: 0 for f in FACTIONS}
    out[faction] = int(delta)
    rel = relations.get(faction, {})
    for other, kind in rel.items():
        if other not in out:
            continue
        if kind == 1:                      # ally -> same sign, half weight
            out[other] = int(round(delta * 0.5))
        elif kind == -1:                    # rival -> opposite sign, third weight
            out[other] = int(round(-delta * 0.33))
    return out


# --------------------------------------------------------------------------- #
#  WORLD-STATE MODEL + RADIANT SELECTION
# --------------------------------------------------------------------------- #
@dataclass
class NPC:
    id: str
    name: str
    faction: Optional[str] = None
    district: Optional[str] = None
    archetype: str = "civilian"     # civilian / enforcer / fixer / corp / cop
    discovered: bool = False
    alive: bool = True
    reputation: int = 0             # personal standing w/ player (optional)
    traits: List[str] = field(default_factory=list)
    backstory: str = ""


@dataclass
class WorldState:
    player_level: int = 1
    player_faction_standing: Dict[str, int] = field(
        default_factory=lambda: {f: 0 for f in FACTIONS})
    districts: Dict[str, Dict[str, Any]] = field(default_factory=dict)
    npcs: List[NPC] = field(default_factory=list)
    heat: int = 0
    active_quests: List[str] = field(default_factory=list)
    seed: int = 0

    @classmethod
    def from_dict(cls, d: Dict[str, Any]) -> "WorldState":
        npcs = [NPC(**{k: v for k, v in n.items() if k in NPC.__dataclass_fields__})
                for n in d.get("npcs", [])]
        fs = d.get("player_faction_standing") or {f: 0 for f in FACTIONS}
        # normalise: ensure every known faction present
        for f in FACTIONS:
            fs.setdefault(f, 0)
        return cls(
            player_level=int(d.get("player_level", 1)),
            player_faction_standing=fs,
            districts=d.get("districts", {}),
            npcs=npcs,
            heat=int(d.get("heat", 0)),
            active_quests=d.get("active_quests", []),
            seed=int(d.get("seed", 0)),
        )


def _pick(rng: random.Random, seq: List[Any], default: Any = None) -> Any:
    return rng.choice(seq) if seq else default


def radiant_select(ws: WorldState, rng: random.Random) -> Dict[str, Any]:
    """Skyrim/Fallout radiant selection.

    - giver: a non-hostile NPC (prefer fixers / faction contacts) or,
      failing that, any living NPC.
    - target: an NPC the player has NOT discovered, optionally from a
      rival district, to pull the player toward fresh content.
    - location: a district with low player territory control / undiscovered.
    - faction: the giver's faction, biased by which faction the player is
      currently Friendly/Allied with (more work from friends) but still
      allowing rival contracts.
    """
    living = [n for n in ws.npcs if n.alive]
    # Giver: prefer fixers/contacts, exclude hostile personal rep.
    giver_pool = [n for n in living
                  if n.reputation > -50 and n.archetype in
                  ("fixer", "enforcer", "corp", "cop", "civilian")]
    giver = _pick(rng, giver_pool) or _pick(rng, living)
    if giver is None:
        # Fully emergent fallback (Watch Dogs Legion "play anyone"):
        giver = NPC(id="npc_emergent", name="A Stranger", archetype="civilian")

    # Target: undiscovered, ideally not same as giver.
    undiscovered = [n for n in living if not n.discovered and n.id != giver.id]
    target = _pick(rng, undiscovered) or _pick(rng, living)
    if target is None:
        target = NPC(id="npc_target", name="The Mark", archetype="enforcer")

    # Location: a district the player does not yet control.
    districts = ws.districts or {giver.district or "Downtown": {"control": 0}}
    low_ctrl = sorted(
        districts.items(),
        key=lambda kv: kv[1].get("control", 0) if isinstance(kv[1], dict) else 0)
    loc_name, loc_data = low_ctrl[0] if low_ctrl else (giver.district or "Downtown", {})
    loc_control = loc_data.get("control", 0) if isinstance(loc_data, dict) else 0

    faction = giver.faction or _pick(rng, FACTIONS, "Southside Crew")

    # Archetype of quest: bias by faction & giver archetype.
    if faction in ("NYPD", "City Hall"):
        arch_pool = ["intel", "retrieval", "escort", "delivery"]
    elif faction in ("Aether Corp",):
        arch_pool = ["sabotage", "intel", "heist", "retrieval"]
    elif faction in ("Southside Crew", "Downtown Syndicate"):
        arch_pool = ["assassination", "heist", "retrieval", "recruitment"]
    else:
        arch_pool = QUEST_ARCHETYPES
    archetype = _pick(rng, arch_pool, "retrieval")

    difficulty = clamp(
        round(2 + rng.random() * 6 + (100 - loc_control) / 25.0), 1, 10)

    return {
        "giver": asdict(giver),
        "target": asdict(target),
        "location": loc_name,
        "location_control": loc_control,
        "faction": faction,
        "archetype": archetype,
        "difficulty": int(difficulty),
        "faction_tier": tier_for(ws.player_faction_standing.get(faction, 0)),
    }


# --------------------------------------------------------------------------- #
#  QUEST-CHAIN GENERATION (deterministic structure)
# --------------------------------------------------------------------------- #
def plan_chain(
    root_archetype: str,
    root_faction: str,
    chain_depth: int,
    rng: random.Random,
) -> List[Dict[str, Any]]:
    """Produce a deterministic chain plan: one entry per follow-up.

    Each follow-up:
      - depth        : 1..chain_depth
      - branch_id    : stable id for graph linking
      - archetype    : may escalate (retrieval -> heist -> assassination)
      - locked_until : condition string (e.g. previous quest completed)
      - faction      : usually same faction, sometimes an ally (ripple).
    """
    escalation = ["retrieval", "delivery", "intel", "sabotage",
                  "heist", "assassination"]
    plan: List[Dict[str, Any]] = []
    cur = root_archetype
    for depth in range(1, chain_depth + 1):
        # escalate toward harder archetypes 60% of the time
        if rng.random() < 0.6 and cur in escalation:
            idx = min(escalation.index(cur) + 1, len(escalation) - 1)
            cur = escalation[idx]
        # 25% chance the follow-up is brokered by an ally faction (ripple)
        fac = root_faction
        allies = [f for f, k in FACTION_RELATIONS.get(root_faction, {}).items()
                  if k == 1]
        if allies and rng.random() < 0.25:
            fac = _pick(rng, allies)
        plan.append({
            "depth": depth,
            "branch_id": f"b{depth}",
            "archetype": cur,
            "faction": fac,
            "locked_until": f"quest_completed:depth_{depth - 1}",
        })
    return plan


# --------------------------------------------------------------------------- #
#  PROMPT CONSTRUCTION (DeepSeek JSON-mode best practices)
# --------------------------------------------------------------------------- #
SCHEMA_EXAMPLE = '''
EXAMPLE OUTPUT (your response must follow this exact shape):
{
  "quest_id": "q_8f3a",
  "title": "Last Call at the Velvet Room",
  "giver": "Marcus Vale",
  "faction": "Southside Crew",
  "archetype": "retrieval",
  "difficulty": 5,
  "synopsis": "Marcus needs a ledger recovered from a rival safehouse.",
  "briefing": "Walk-up dialogue from the giver (2-3 sentences, in-character).",
  "hook": "One punchy line that sells the job to the player.",
  "objectives": [
    {"type": "ReachLocation", "target": "Dockside Safehouse", "detail": "Slip past the guard."},
    {"type": "CollectItem", "target": "Ledger", "detail": "In the back office."},
    {"type": "EliminateTarget", "target": "The Mark", "detail": "Optional silent takedown."}
  ],
  "rewards": {"xp": 250, "cash": 400, "reputation_delta": 8},
  "fail_states": ["Player detected by NYPD", "Target escapes"],
  "branch": {"on_success": "q_8f3a_s", "on_fail": "q_8f3a_f"},
  "chain": []
}
'''


def build_messages(ws: WorldState, sel: Dict[str, Any],
                   chain_plan: List[Dict[str, Any]]) -> List[Dict[str, str]]:
    system = (
        "You are SAGA, the quest architect for GTAI, an open-world crime game. "
        "You output STRICT JSON only. Include the word json in your reasoning. "
        "Fill the quest template from the provided world state. Keep titles short "
        "and punchy. Objectives must use only these types: "
        + ", ".join(OBJECTIVE_TYPES) + ". "
        "Rewards must match the supplied deterministic math. Do not invent new "
        "faction names. Write in a gritty neo-noir tone."
    )
    sel_block = json.dumps(sel, indent=2)
    chain_block = json.dumps(chain_plan, indent=2) if chain_plan else "[] (single quest, no chain)"
    rewards_hint = json.dumps(compute_rewards(
        sel["difficulty"], 0, sel["archetype"], ws.player_level))
    user = (
        f"WORLD STATE (player level {ws.player_level}, heat {ws.heat}):\n"
        f"{sel_block}\n\n"
        f"CHAIN PLAN (follow-ups to encode in 'chain'):\n{chain_block}\n\n"
        f"REWARD MATH (already computed, echo into rewards):\n{rewards_hint}\n\n"
        "Generate the quest JSON now. " + SCHEMA_EXAMPLE
    )
    return [{"role": "system", "content": system},
            {"role": "user", "content": user}]


# --------------------------------------------------------------------------- #
#  LLM CALL (DeepSeek V4, OpenAI-compatible, JSON mode)
# --------------------------------------------------------------------------- #
def call_deepseek(messages: List[Dict[str, str]], *,
                  api_key: str, api_base: str, model: str,
                  max_tokens: int = 2048, temperature: float = 0.8,
                  timeout: int = 60, retries: int = 3) -> Dict[str, Any]:
    if requests is None:
        raise RuntimeError("requests module not available; install with `pip install requests`")
    url = api_base.rstrip("/") + "/chat/completions"
    payload = {
        "model": model,
        "messages": messages,
        "response_format": {"type": "json_object"},  # DeepSeek JSON mode
        "max_tokens": max_tokens,
        "temperature": temperature,
    }
    headers = {"Authorization": f"Bearer {api_key}",
               "Content-Type": "application/json"}
    last_err: Optional[Exception] = None
    for attempt in range(1, retries + 1):
        try:
            r = requests.post(url, headers=headers, json=payload, timeout=timeout)
            r.raise_for_status()
            content = r.json()["choices"][0]["message"]["content"]
            if not content or not content.strip():
                # DeepSeek JSON mode can return empty content; retry per docs.
                last_err = ValueError("empty content from model")
                continue
            return json.loads(content)
        except Exception as e:  # network / JSON / HTTP
            last_err = e
            if attempt < retries:
                continue
    raise RuntimeError(f"DeepSeek call failed after {retries} tries: {last_err}")


# --------------------------------------------------------------------------- #
#  DRY-RUN (deterministic mock, no network)
# --------------------------------------------------------------------------- #
def dry_run_quest(ws: WorldState, sel: Dict[str, Any],
                  chain_plan: List[Dict[str, Any]]) -> Dict[str, Any]:
    rewards = compute_rewards(sel["difficulty"], 0, sel["archetype"], ws.player_level)
    chain = [{
        "depth": c["depth"],
        "branch_id": c["branch_id"],
        "archetype": c["archetype"],
        "faction": c["faction"],
        "locked_until": c["locked_until"],
        "rewards": compute_rewards(sel["difficulty"] + c["depth"],
                                   c["depth"], c["archetype"], ws.player_level),
    } for c in chain_plan]
    return {
        "quest_id": f"q_dry_{abs(hash(sel['giver']['id'])) % 10000:04x}",
        "title": f"{sel['archetype'].title()} for {sel['faction']}",
        "giver": sel["giver"]["name"],
        "faction": sel["faction"],
        "archetype": sel["archetype"],
        "difficulty": sel["difficulty"],
        "synopsis": f"DRY-RUN: {sel['giver']['name']} wants a {sel['archetype']} "
                    f"job done in {sel['location']}.",
        "briefing": "(dry-run) Generated without LLM. Wire DEEPSEEK_API_KEY to "
                    "produce real dialogue.",
        "hook": "(dry-run) hook line",
        "objectives": [
            {"type": "ReachLocation", "target": sel["location"],
             "detail": "Get to the site."},
            {"type": "EliminateTarget", "target": sel["target"]["name"],
             "detail": "Resolve the target."},
        ],
        "rewards": rewards,
        "fail_states": ["Player detected", "Target escapes"],
        "branch": {"on_success": None, "on_fail": None},
        "chain": chain,
        "reputation_ripple": reputation_ripple(sel["faction"], rewards["reputation_delta"]),
    }


# --------------------------------------------------------------------------- #
#  SCHEMA VALIDATION
# --------------------------------------------------------------------------- #
QUEST_SCHEMA = {
    "type": "object",
    "required": ["quest_id", "title", "faction", "archetype", "difficulty",
                 "objectives", "rewards"],
    "properties": {
        "quest_id": {"type": "string"},
        "title": {"type": "string"},
        "giver": {"type": ["string", "object"]},
        "faction": {"type": "string", "enum": FACTIONS},
        "archetype": {"type": "string", "enum": QUEST_ARCHETYPES},
        "difficulty": {"type": "integer", "minimum": 1, "maximum": 10},
        "synopsis": {"type": "string"},
        "briefing": {"type": "string"},
        "hook": {"type": "string"},
        "objectives": {
            "type": "array",
            "items": {
                "type": "object",
                "required": ["type", "target"],
                "properties": {
                    "type": {"type": "string", "enum": OBJECTIVE_TYPES},
                    "target": {"type": "string"},
                    "detail": {"type": "string"},
                },
            },
        },
        "rewards": {
            "type": "object",
            "required": ["xp", "cash", "reputation_delta"],
            "properties": {
                "xp": {"type": "integer", "minimum": 0},
                "cash": {"type": "integer", "minimum": 0},
                "reputation_delta": {"type": "integer"},
            },
        },
        "fail_states": {"type": "array", "items": {"type": "string"}},
        "branch": {"type": "object"},
        "chain": {"type": "array"},
        "reputation_ripple": {"type": "object"},
    },
}


def validate_quest(quest: Dict[str, Any]) -> List[str]:
    errors: List[str] = []
    if jsonschema is not None:
        try:
            jsonschema.validate(instance=quest, schema=QUEST_SCHEMA)
        except jsonschema.ValidationError as e:  # type: ignore[attr-defined]
            errors.append(f"schema: {e.message} (path={list(e.path)})")
    else:
        # minimal fallback validation
        for key in QUEST_SCHEMA["required"]:
            if key not in quest:
                errors.append(f"missing required key: {key}")
        if quest.get("faction") not in FACTIONS:
            errors.append(f"faction {quest.get('faction')!r} not in known factions")
    return errors


# --------------------------------------------------------------------------- #
#  EXAMPLE WORLD STATE (for --example / demos / tests)
# --------------------------------------------------------------------------- #
def example_world_state() -> Dict[str, Any]:
    return {
        "player_level": 7,
        "heat": 22,
        "player_faction_standing": {
            "Southside Crew": 45, "Downtown Syndicate": -20,
            "Aether Corp": 10, "NYPD": -35, "City Hall": 5, "Dock Workers": 30,
        },
        "districts": {
            "Downtown": {"control": 70},
            "Southside": {"control": 55},
            "Docks": {"control": 20},
            "Uptown": {"control": 5},
        },
        "npcs": [
            {"id": "npc_marcus", "name": "Marcus Vale", "faction": "Southside Crew",
             "district": "Southside", "archetype": "fixer", "discovered": True,
             "alive": True, "reputation": 40, "traits": ["charismatic", "wanted"]},
            {"id": "npc_chen", "name": "Lt. Chen", "faction": "NYPD",
             "district": "Downtown", "archetype": "cop", "discovered": True,
             "alive": True, "reputation": -30},
            {"id": "npc_dockworker", "name": "Sal", "faction": "Dock Workers",
             "district": "Docks", "archetype": "civilian", "discovered": False,
             "alive": True, "reputation": 10, "traits": ["ex-military"]},
            {"id": "npc_mark", "name": "The Mark", "faction": "Downtown Syndicate",
             "district": "Uptown", "archetype": "enforcer", "discovered": False,
             "alive": True, "reputation": -10},
        ],
    }


# --------------------------------------------------------------------------- #
#  CLI
# --------------------------------------------------------------------------- #
def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="SAGA GTAI quest generator (DeepSeek V4).")
    ap.add_argument("--world", help="Path to world-state JSON.")
    ap.add_argument("--out", help="Output quest JSON path.", default="quest_out.json")
    ap.add_argument("--chain", action="store_true", help="Generate a quest chain.")
    ap.add_argument("--max-depth", type=int, default=3, help="Max chain depth.")
    ap.add_argument("--seed", type=int, default=0, help="RNG seed (0=use world.seed).")
    ap.add_argument("--dry-run", action="store_true",
                    help="Skip the LLM; emit a deterministic mock quest.")
    ap.add_argument("--example", action="store_true",
                    help="Ignore --world and use the built-in example world state.")
    ap.add_argument("--api-key", default=os.environ.get("DEEPSEEK_API_KEY", ""))
    ap.add_argument("--api-base", default=os.environ.get("DEEPSEEK_API_BASE",
                                                         "https://api.deepseek.com"))
    ap.add_argument("--model", default=os.environ.get("DEEPSEEK_MODEL", "deepseek-v4-pro"))
    ap.add_argument("--max-tokens", type=int, default=2048)
    ap.add_argument("--temperature", type=float, default=0.8)
    ap.add_argument("--pretty", action="store_true", help="Pretty-print JSON to stdout.")
    args = ap.parse_args(argv)

    # ---- Load world state ----
    if args.example:
        raw = example_world_state()
    elif args.world:
        with open(args.world, "r", encoding="utf-8") as fh:
            raw = json.load(fh)
    else:
        print("ERROR: provide --world <file.json> or --example", file=sys.stderr)
        return 1

    ws = WorldState.from_dict(raw)
    seed = args.seed if args.seed else (ws.seed or 12345)
    rng = random.Random(seed)

    # ---- Radiant selection + chain plan ----
    sel = radiant_select(ws, rng)
    chain_plan = plan_chain(sel["archetype"], sel["faction"],
                            args.max_depth if args.chain else 0, rng) if args.chain \
        else []

    # ---- Generate quest (LLM or dry-run) ----
    if args.dry_run or not args.api_key:
        if not args.dry_run and not args.api_key:
            print("WARNING: no DEEPSEEK_API_KEY set — using --dry-run mock.",
                  file=sys.stderr)
        quest = dry_run_quest(ws, sel, chain_plan)
    else:
        messages = build_messages(ws, sel, chain_plan)
        try:
            quest = call_deepseek(
                messages, api_key=args.api_key, api_base=args.api_base,
                model=args.model, max_tokens=args.max_tokens,
                temperature=args.temperature)
        except Exception as e:
            print(f"ERROR: LLM call failed: {e}", file=sys.stderr)
            return 1
        # Always attach the deterministic reputation_ripple for downstream use.
        rep_delta = quest.get("rewards", {}).get("reputation_delta", REP_DELTA)
        quest["reputation_ripple"] = reputation_ripple(sel["faction"], rep_delta)

    # ---- Validate ----
    errors = validate_quest(quest)
    if errors:
        print("WARNING: quest failed validation:", file=sys.stderr)
        for e in errors:
            print(f"  - {e}", file=sys.stderr)

    # ---- Persist ----
    with open(args.out, "w", encoding="utf-8") as fh:
        json.dump(quest, fh, indent=2, ensure_ascii=False)

    if args.pretty:
        print(json.dumps(quest, indent=2, ensure_ascii=False))
    else:
        print(f"OK: wrote quest '{quest.get('title')}' -> {args.out}")
        print(f"    faction={quest.get('faction')} archetype={quest.get('archetype')} "
              f"difficulty={quest.get('difficulty')} "
              f"chain_followups={len(quest.get('chain', []))}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
