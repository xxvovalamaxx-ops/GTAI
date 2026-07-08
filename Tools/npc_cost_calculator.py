#!/usr/bin/env python3
"""
npc_cost_calculator.py
======================
LUMEN — GTAI NPC cost-architecture estimator.

Estimates the *per-player monthly* cost of hosting AI-driven NPCs under the
GTAI graduated-intelligence model:

    Tier 1  Ambient      : on-device Phi-3 / small LM bark generation  ($0 cloud, GPU/CPU local)
    Tier 2  Cached       : deterministic response cache                ($0, <5ms)
    Tier 3  Deep         : cloud LLM (DeepSeek V4 flash) for novel dialogue

It also models the alternative of a *fully external* NPC vendor (Convai) so the
two architectures can be compared head-to-head, and reports the MassEntity
runtime cost (CPU/GPU on the player's machine, not a server bill).

All assumptions are grounded in LUMEN's 2026 research pass and are overridable
from the command line or by editing the CONFIG dict below.

Usage
-----
    python npc_cost_calculator.py                 # default scenario
    python npc_cost_calculator.py --players 1000  # scale to 1000 concurrent players
    python npc_cost_calculator.py --no-cache      # disable prompt-cache savings
    python npc_cost_calculator.py --vendor convai # compare against Convai pricing

Exit code 0 on success.
"""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass, field, asdict
from typing import Optional


# --------------------------------------------------------------------------- #
#  PRICING REFERENCE (USD), sourced from LUMEN 2026 research pass
# --------------------------------------------------------------------------- #
# DeepSeek V4 flash (OpenAI-compatible). Official: api-docs.deepseek.com
#   cache-hit input  $0.0028 / 1M tok   (90% discount vs cache-miss)
#   cache-miss input $0.14   / 1M tok
#   output          $0.28   / 1M tok
#   1M context, 384K max output, off-peak 16:30-00:30 GMT up to 50% off V4.
DEEPSEEK = {
    "input_cache_hit_per_1m": 0.0028,
    "input_cache_miss_per_1m": 0.14,
    "output_per_1m": 0.28,
}

# Convai (convai.com/pricing, 2026) — managed NPC vendor, per-month plans.
# Interactions are short request/response turns; we convert to tokens via
# CONVAI_TOKENS_PER_INTERACTION for apples-to-apples comparison.
CONVAI = {
    "free_monthly_interactions": 100,     # Free plan
    "indie_monthly": 22.0,                # Indie Dev ~ plan (approx, USD/mo)
    "indie_included_interactions": 5000,  # interactions bundled before overage
    "indie_overage_per_1k": 4.0,          # ~$4 / 1000 extra interactions (est.)
    # "Bring Your Own LLM" (Enterprise) lets you swap Convai's LLM for your own
    # inference — at that point the LLM cost collapses to your self-hosted cost.
}

# On-device Phi-3 (microsoft/Phi-3-mini-4k-instruct, 3.8B).
# Benchmarks (llama.cpp / ONNX, 2026):
#   - RTX 4090 (Q4)            ~ 80-110 tok/s
#   - Snapdragon X Elite NPU   ~ 13-15 tok/s
#   - Apple M-series NPU       ~ 20-30 tok/s
# Running locally costs $0 in API fees; the only cost is amortized HW + power.
PHI3 = {
    "params_b": 3.8,
    "tok_s_reference_gpu": 90.0,   # RTX-class reference throughput
    "hw_amortization_per_player_mo": 0.02,  # amortized GPU/console cost, negligible
    "power_usd_per_1m_tok": 0.01,  # electricity for local generation, ~$0
}

# MassEntity crowd simulation — runs on the *player's* machine, not a server.
# CPU budget for 200+ pedestrians is tiny; no per-player server cost.
MASENTITY = {
    "cpu_ms_per_npc": 0.05,        # ~0.05 ms/NPC/frame of simulation overhead
    "no_server_cost": True,        # on-client, already paid for by the game
}

# Prompt-caching effectiveness (LUMEN research):
#   - System prompt + city-state + NPC persona = stable prefix (~600 tok).
#   - Up to 90% input-token cost reduction on cache hit (DeepSeek/Anthropic).
#   - We model a cache-hit RATE: fraction of deep calls that reuse the prefix.
CACHE = {
    "prefix_tokens": 600,
    "baseline_hit_rate": 0.85,     # 85% of deep calls hit the cached prefix
    "discount_factor": 0.90,       # 90% input cost removed on a hit
}

# Token accounting assumptions for an NPC "deep" dialogue turn.
DIALOGUE = {
    "avg_input_tokens": 450,       # player line + retrieved memory + prefix (prefix cached)
    "avg_output_tokens": 120,      # NPC reply
}


@dataclass
class Scenario:
    players: int = 100
    # How many NPCs does an average player actually *engage* with deeply per day?
    deep_dialogues_per_player_day: float = 8.0
    # Ambient barks generated on-device per player per day (Tier 1).
    ambient_barks_per_player_day: float = 40.0
    # Fraction of "deep" intents resolvable from the deterministic cache (Tier 2).
    cached_resolution_rate: float = 0.40
    # Fraction of engaged NPCs that are "consequential" -> Tier 3 deep call.
    deep_escalation_rate: float = 0.60
    days_per_month: int = 30
    use_cache: bool = True
    vendor: str = "self"          # "self" | "convai"
    off_peak_discount: float = 0.0  # 0..0.5 off-peak batching discount


def _tok_cost(input_tok: float, output_tok: float, cache_hit_rate: float) -> float:
    """Cost in USD for a single deep call given cache-hit economics."""
    d = DEEPSEEK
    hit = cache_hit_rate
    # Input cost: blend of cache-hit and cache-miss price.
    input_cost = input_tok / 1e6 * (
        hit * d["input_cache_hit_per_1m"] + (1 - hit) * d["input_cache_miss_per_1m"]
    )
    output_cost = output_tok / 1e6 * d["output_per_1m"]
    return input_cost + output_cost


def estimate(scn: Scenario) -> dict:
    days = scn.days_per_month
    monthly_players = scn.players

    # ---- Deep (Tier 3) cloud LLM ----
    # Engaged dialogues/player/day, minus those resolved by deterministic cache.
    deep_per_player_day = (
        scn.deep_dialogues_per_player_day
        * (1 - scn.cached_resolution_rate)
        * scn.deep_escalation_rate
    )
    deep_per_month = deep_per_player_day * days * monthly_players

    cache_hit_rate = CACHE["baseline_hit_rate"] if scn.use_cache else 0.0
    # Discount the prefix portion on cache hit.
    prefix = CACHE["prefix_tokens"]
    dynamic_input = max(DIALOGUE["avg_input_tokens"] - prefix, 0)
    # Effective per-call input cost: prefix mostly cached, dynamic always fresh.
    per_call_prefix_cost = (prefix / 1e6) * (
        cache_hit_rate * DEEPSEEK["input_cache_hit_per_1m"]
        + (1 - cache_hit_rate) * DEEPSEEK["input_cache_miss_per_1m"]
    ) * (1 - (CACHE["discount_factor"] * cache_hit_rate))
    per_call_dynamic_cost = (dynamic_input / 1e6) * DEEPSEEK["input_cache_miss_per_1m"]
    per_call_output_cost = (DIALOGUE["avg_output_tokens"] / 1e6) * DEEPSEEK["output_per_1m"]
    per_deep_call = per_call_prefix_cost + per_call_dynamic_cost + per_call_output_cost

    deep_cost = deep_per_month * per_deep_call
    if scn.off_peak_discount:
        deep_cost *= (1 - scn.off_peak_discount)

    # ---- Cached (Tier 2) ----
    cached_per_player_day = (
        scn.deep_dialogues_per_player_day * scn.cached_resolution_rate
    )
    cached_per_month = cached_per_player_day * days * monthly_players
    cached_cost = 0.0  # deterministic local cache, no API fee

    # ---- Ambient (Tier 1, on-device Phi-3) ----
    ambient_per_month = scn.ambient_barks_per_player_day * days * monthly_players
    ambient_cost = (
        ambient_per_month
        / PHI3["tok_s_reference_gpu"]
        * 0.0  # no token fee; only amortized HW below
    )
    ambient_hw_cost = ambient_per_month * 0.0 + (
        PHI3["hw_amortization_per_player_mo"] * monthly_players
    )

    # ---- Vendor comparison (Convai) ----
    vendor_cost: Optional[float] = None
    vendor_detail = ""
    if scn.vendor == "convai":
        # Equivalent workload expressed as interactions (deep + cached turns).
        interactions = deep_per_month + cached_per_month
        free = CONVAI["free_monthly_interactions"]
        billable = max(interactions - free, 0)
        if billable <= CONVAI["indie_included_interactions"]:
            vendor_cost = CONVAI["indie_monthly"] * max(1, monthly_players / 100.0)
            vendor_detail = (
                f"Within Indie tier; scaled plan cost x{monthly_players/100.0:.2f}"
            )
        else:
            over = billable - CONVAI["indie_included_interactions"]
            vendor_cost = (
                CONVAI["indie_monthly"]
                + over / 1000.0 * CONVAI["indie_overage_per_1k"]
            ) * max(1.0, monthly_players / 100.0)
            vendor_detail = "Indie + overage, scaled by player count"

    # ---- MassEntity runtime (client-side, no server bill) ----
    # 200+ NPCs simulated on each player's machine; cost is already in the game.
    me_npc_count = 200
    me_cpu_ms = me_npc_count * MASENTITY["cpu_ms_per_npc"]

    total_cloud = deep_cost + cached_cost + ambient_cost
    per_player_mo = (total_cloud + ambient_hw_cost) / max(monthly_players, 1)

    return {
        "scenario": asdict(scn),
        "deep": {
            "dialogues_per_player_day": round(deep_per_player_day, 3),
            "total_deep_calls_month": int(deep_per_month),
            "cost_per_deep_call_usd": round(per_deep_call, 6),
            "monthly_cost_usd": round(deep_cost, 2),
        },
        "cached": {
            "total_cached_hits_month": int(cached_per_month),
            "monthly_cost_usd": round(cached_cost, 2),
        },
        "ambient": {
            "total_barks_month": int(ambient_per_month),
            "inference_cost_usd": round(ambient_cost, 2),
            "hw_amortization_usd": round(ambient_hw_cost, 2),
        },
        "vendor_comparison": {
            "vendor": scn.vendor,
            "monthly_cost_usd": (round(vendor_cost, 2) if vendor_cost is not None else None),
            "detail": vendor_detail,
        },
        "masentity": {
            "npcs_simulated": me_npc_count,
            "cpu_ms_per_frame": round(me_cpu_ms, 2),
            "server_cost_usd": 0.0,
            "note": "On-client simulation; no per-player server bill.",
        },
        "totals": {
            "cloud_monthly_usd": round(total_cloud, 2),
            "per_player_monthly_usd": round(per_player_mo, 4),
            "per_player_yearly_usd": round(per_player_mo * 12, 2),
        },
    }


def _print_report(r: dict) -> None:
    t = r["totals"]
    print("=" * 64)
    print("  GTAI / LUMEN — NPC HOSTING COST ESTIMATE")
    print("=" * 64)
    print(f"  Players (concurrent-month avg) : {r['scenario']['players']}")
    print(f"  Architecture                  : {'self-hosted (graduated)' if r['scenario']['vendor']=='self' else 'Convai vendor'}")
    print(f"  Prompt cache                  : {'ON' if r['scenario']['use_cache'] else 'OFF'}")
    print("-" * 64)
    print("  TIER 3 — Deep cloud LLM (DeepSeek V4 flash)")
    print(f"    deep calls / month         : {r['deep']['total_deep_calls_month']:,}")
    print(f"    cost / call (USD)          : {r['deep']['cost_per_deep_call_usd']}")
    print(f"    monthly cost (USD)         : ${r['deep']['monthly_cost_usd']:,}")
    print("  TIER 2 — Deterministic cache")
    print(f"    cached hits / month        : {r['cached']['total_cached_hits_month']:,}")
    print(f"    monthly cost (USD)         : ${r['cached']['monthly_cost_usd']:,}")
    print("  TIER 1 — On-device Phi-3 barks")
    print(f"    barks / month              : {r['ambient']['total_barks_month']:,}")
    print(f"    inference cost (USD)       : ${r['ambient']['inference_cost_usd']:,}")
    print(f"    HW amortization (USD)      : ${r['ambient']['hw_amortization_usd']:,}")
    print("-" * 64)
    vc = r["vendor_comparison"]
    if vc["monthly_cost_usd"] is not None:
        print(f"  VENDOR ({vc['vendor']}) monthly    : ${vc['monthly_cost_usd']:,}  ({vc['detail']})")
    print("  MASSENTITY crowd (200 NPCs)")
    print(f"    CPU ms/frame               : {r['masentity']['cpu_ms_per_frame']}")
    print(f"    server cost (USD)          : ${r['masentity']['server_cost_usd']}")
    print("-" * 64)
    print(f"  CLOUD TOTAL / month (USD)    : ${t['cloud_monthly_usd']:,}")
    print(f"  PER-PLAYER / month (USD)     : ${t['per_player_monthly_usd']}")
    print(f"  PER-PLAYER / year  (USD)     : ${t['per_player_yearly_usd']}")
    print("=" * 64)


def main() -> None:
    ap = argparse.ArgumentParser(description="GTAI NPC hosting cost estimator")
    ap.add_argument("--players", type=int, default=100)
    ap.add_argument("--deep-per-day", type=float, default=8.0,
                    help="deep dialogues per player per day")
    ap.add_argument("--ambient-per-day", type=float, default=40.0,
                    help="on-device barks per player per day")
    ap.add_argument("--cache-rate", type=float, default=0.40,
                    help="fraction of engaged dialogue resolved by deterministic cache")
    ap.add_argument("--escalation", type=float, default=0.60,
                    help="fraction of engaged NPCs escalating to Tier 3")
    ap.add_argument("--no-cache", action="store_true", help="disable prompt caching")
    ap.add_argument("--vendor", choices=["self", "convai"], default="self")
    ap.add_argument("--off-peak", type=float, default=0.0,
                    help="0..0.5 off-peak batching discount")
    ap.add_argument("--json", action="store_true", help="emit JSON only")
    a = ap.parse_args()

    scn = Scenario(
        players=a.players,
        deep_dialogues_per_player_day=a.deep_per_day,
        ambient_barks_per_player_day=a.ambient_per_day,
        cached_resolution_rate=a.cache_rate,
        deep_escalation_rate=a.escalation,
        use_cache=not a.no_cache,
        vendor=a.vendor,
        off_peak_discount=a.off_peak,
    )
    r = estimate(scn)
    if a.json:
        print(json.dumps(r, indent=2))
    else:
        _print_report(r)


if __name__ == "__main__":
    main()
