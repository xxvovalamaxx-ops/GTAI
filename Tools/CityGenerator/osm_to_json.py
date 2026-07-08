#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
osm_to_json.py
==============
GTAI City Architect — OpenStreetMap -> UE5-ready JSON pipeline (vertical slice).

Downloads raw OSM vector data for a bounded Manhattan area from the Overpass API,
parses it, and emits a single JSON file containing:

    * streets      : road polylines with width / lane count / direction / oneway
    * buildings    : footprint polygons with estimated height + use type
    * water        : water-body polygons (rivers, bays, reservoirs)
    * parks        : green-space / park polygons (leisure=park, landuse=grass, ...)
    * districts    : administrative / neighborhood boundaries (MultiPolygon)
    * metadata     : CRS, bounds, scale factors, stats

Design goals
------------
* **Zero third-party deps** — only the Python standard library. Runs on a clean
  Python 3.13 install on Windows with no `pip install` step. Network access to
  the Overpass API is the only hard requirement.
* **UE5-ready output** — coordinates are double-precision lat/lon (WGS84) plus a
  precomputed UE5 centroid-relative local metric frame (X east, Y north) so the
  C++ importer can place geometry without any numeric drift. JSON is plain
  RFC-8259 and parses with `FJsonSerializer` / `rapidjson` / `nlohmann::json`.
* **Idempotent & cached** — the raw Overpass response is cached to
  `cache/<slug>.json`; reruns reuse it. Pass `--no-cache` to force a download.

Usage
-----
    python osm_to_json.py                         # uses default Manhattan slice
    python osm_to_json.py --south 40.70 --north 40.74 --west -74.02 --east -73.97
    python osm_to_json.py --out manhattan.json --pretty
    python osm_to_json.py --no-cache             # ignore cached Overpass response

Outputs (next to this script, under the same CityGenerator folder by default):
    <out>.json              the city data
    cache/<slug>.json       the raw Overpass payload (for reproducibility)

Author: GTAI City Architect specialist
"""

from __future__ import annotations

import argparse
import json
import math
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from datetime import datetime, timezone

# --------------------------------------------------------------------------- #
# Configuration
# --------------------------------------------------------------------------- #

# Default vertical slice: Manhattan south of Central Park.
#   40.74..40.78 lat  /  -74.02..-73.97 lon  (roughly Houston St up to ~86th St)
DEFAULT_BBOX = {
    "south": 40.74,
    "north": 40.78,
    "west": -74.02,
    "east": -73.97,
}

# Overpass endpoints tried in order (the first success wins).
OVERPASS_ENDPOINTS = [
    "https://overpass-api.de/api/interpreter",
    "https://overpass.kumi.systems/api/interpreter",
    "https://maps.mail.ru/osm/tools/overpass/api/interpreter",
]

# OSM building-use heuristics: a building=* / amenity=* / landuse=* / shop=*
# value maps to one of our simplified GTAI use categories. Order matters:
# first matching rule wins.
USE_RULES = [
    # --- industrial / utility ---
    ({"building": {"industrial", "warehouse", "factory", "manufacture",
                   "depot", "garage", "service", "transportation", "train_station",
                   "train_station_entrance", "tower", "storage_tank", "silo",
                   "construction", "civic", "barn", "greenhouse"}},
     "industrial"),
    ({"landuse": {"industrial", "railway", "brownfield", "construction",
                  "port", "harbour", "depot"}}, "industrial"),
    ({"amenity": {"fuel", "parking", "parking_entrance"}}, "industrial"),

    # --- commercial ---
    ({"building": {"commercial", "office", "retail", "shop", "supermarket",
                   "civic", "public", "government", "bank", "theatre", "cinema",
                   "hotel", "stadium", "sports_hall", "conference_centre",
                   "marketplace", "hall", "university", "college", "school",
                   "hospital", "clinic", "church", "cathedral", "mosque",
                   "synagogue", "temple", "chapel", "museum", "library",
                   "townhall", "courthouse", "prison", "fire_station",
                   "police", "embassy"}}, "commercial"),
    ({"amenity": {"restaurant", "cafe", "bar", "pub", "fast_food", "bank",
                  "shop", "marketplace", "theatre", "cinema", "nightclub",
                  "casino", "hotel", "office", "clinic", "hospital", "school",
                  "university", "college", "library", "museum", "townhall",
                  "courthouse", "prison", "fire_station", "police", "embassy",
                  "place_of_worship", "community_centre", "arts_centre",
                  "conference_centre"}}, "commercial"),
    ({"shop": {"any"}}, "commercial"),
    ({"office": {"any"}}, "commercial"),
    ({"landuse": {"commercial", "retail", "education", "institutional"}}, "commercial"),

    # --- residential (default fallthrough) ---
    ({"building": {"house", "residential", "apartments", "detached", "terrace",
                   "semidetached_house", "dormitory", "bungalow", "cabin", "hut",
                   "static_caravan", "ger"}}, "residential"),
    ({"landuse": {"residential"}}, "residential"),
]

# Rough global fallback heights (metres) when OSM has no height/building:levels.
# Keyed by use type; refined by building class below.
FALLBACK_HEIGHT = {"commercial": 24.0, "residential": 12.0, "industrial": 9.0}

# Per-building-class typical storey heights (metres) and default floors.
BUILDING_CLASS_HEIGHT = {
    # (use, class_token) -> (storey_m, default_floors)
    ("residential", "house"): (3.2, 2),
    ("residential", "detached"): (3.2, 2),
    ("residential", "terrace"): (3.0, 3),
    ("residential", "semidetached"): (3.0, 2),
    ("residential", "apartments"): (3.0, 6),
    ("residential", "dormitory"): (3.2, 6),
    ("commercial", "office"): (3.6, 8),
    ("commercial", "commercial"): (3.6, 4),
    ("commercial", "retail"): (4.0, 2),
    ("commercial", "hotel"): (3.2, 10),
    ("commercial", "school"): (3.6, 3),
    ("commercial", "university"): (3.6, 5),
    ("commercial", "hospital"): (3.8, 6),
    ("industrial", "warehouse"): (7.0, 1),
    ("industrial", "industrial"): (7.0, 1),
    ("industrial", "factory"): (7.0, 1),
}

# Houdini/UE5 convention: 1 OSM metre == 1 UE5 centimetre would explode LWC.
# We keep METRES as the world unit in JSON and let the importer scale to UE
# (UE5 typically uses 1 unit = 1 cm, so multiply metres by 100, or set up a
# 1 unit = 1 m world). We export `height_m` + a `ue_scale` hint (cm per metre).
UE_CM_PER_METRE = 100.0

# HTTP tuning
HTTP_TIMEOUT = 300
USER_AGENT = "GTAI-CityArchitect/0.1 (openworld NYC generator; contact: gtai-dev)"


# --------------------------------------------------------------------------- #
# Geo helpers
# --------------------------------------------------------------------------- #

def _to_float(v):
    try:
        return float(v)
    except (TypeError, ValueError):
        return None


def haversine_m(lat1, lon1, lat2, lon2):
    """Great-circle distance in metres (for sanity / scale checks)."""
    r = 6371008.8
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlmb = math.radians(lon2 - lon1)
    a = math.sin(dphi / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dlmb / 2) ** 2
    return 2 * r * math.asin(math.sqrt(a))


def local_meters(lat, lon, lat0, lon0):
    """Project WGS84 (lat,lon) to a local east/north tangent plane in metres.

    Uses an equirectangular approximation anchored at (lat0, lon0). For a
    ~4 km city slice the error is < 1 cm, well within game tolerances, and it
    is perfectly reproducible (no external proj dependency).
    """
    m_per_deg_lat = 111320.0
    m_per_deg_lon = 111320.0 * math.cos(math.radians(lat0))
    x = (lon - lon0) * m_per_deg_lon
    y = (lat - lat0) * m_per_deg_lat
    return x, y


# --------------------------------------------------------------------------- #
# OSM tag interpretation
# --------------------------------------------------------------------------- #

def classify_use(tags: dict) -> str:
    """Map OSM tags -> one of commercial / residential / industrial."""
    # Normalize a few common variants.
    b = tags.get("building", "")
    amen = tags.get("amenity", "")
    lu = tags.get("landuse", "")
    shop = tags.get("shop", "")
    office = tags.get("office", "")

    for rule_map, use in USE_RULES:
        for key, values in rule_map.items():
            val = {"building": b, "amenity": amen, "landuse": lu,
                   "shop": shop, "office": office}.get(key, "")
            if val and ("any" in values or val in values):
                return use
    return "residential"


def estimate_height_m(tags: dict, use: str) -> float:
    """Estimate building height (metres) from OSM tags, else heuristic."""
    h = _to_float(tags.get("height"))            # explicit height (m or 'X m')
    if h is None and "height" in tags:
        # sometimes written '12 m' or '12m'
        m = "".join(ch for ch in tags["height"] if ch.isdigit() or ch == ".")
        h = _to_float(m)
    if h is not None and h > 0:
        return round(min(max(h, 2.0), 600.0), 2)

    levels = _to_float(tags.get("building:levels"))
    if levels is None:
        levels = _to_float(tags.get("levels"))
    if levels is not None and levels > 0:
        # account for roof + ground adjustments if present
        roof = _to_float(tags.get("roof:levels")) or 0.0
        storey_h = 3.0
        bcls = tags.get("building", "")
        for (u, tok), (sh, _) in BUILDING_CLASS_HEIGHT.items():
            if u == use and tok in bcls:
                storey_h = sh
                break
        return round((levels + roof) * storey_h, 2)

    # Heuristic fallback by class.
    bcls = tags.get("building", "")
    for (u, tok), (sh, df) in BUILDING_CLASS_HEIGHT.items():
        if u == use and tok in bcls:
            return round(df * sh, 2)
    return round(FALLBACK_HEIGHT.get(use, 12.0), 2)


def estimate_lanes(tags: dict) -> int:
    lanes = _to_float(tags.get("lanes"))
    if lanes and lanes > 0:
        return int(lanes)
    hw = tags.get("highway", "")
    default = {
        "motorway": 3, "motorway_link": 2, "trunk": 3, "trunk_link": 2,
        "primary": 3, "primary_link": 2, "secondary": 2, "secondary_link": 2,
        "tertiary": 2, "tertiary_link": 1, "residential": 2, "unclassified": 2,
        "service": 1, "living_street": 1, "pedestrian": 1, "track": 1,
        "footway": 0, "path": 0, "cycleway": 0, "steps": 0, "construction": 2,
    }
    return default.get(hw, 2)


def estimate_road_width_m(tags: dict, lanes: int) -> float:
    """Width in metres from OSM `width`, else derived from lanes + type."""
    w = _to_float(tags.get("width"))
    if w and w > 0:
        return round(min(max(w, 2.0), 200.0), 2)
    hw = tags.get("highway", "")
    # lane width by class, plus shoulders/curbs
    lane_w = {
        "motorway": 3.7, "motorway_link": 3.5, "trunk": 3.7, "trunk_link": 3.5,
        "primary": 3.5, "primary_link": 3.3, "secondary": 3.3, "secondary_link": 3.2,
        "tertiary": 3.2, "tertiary_link": 3.0, "residential": 3.0,
        "unclassified": 3.0, "service": 2.8, "living_street": 3.0,
        "pedestrian": 4.0, "track": 2.5, "footway": 2.0, "path": 1.5,
        "cycleway": 1.8, "steps": 1.5, "construction": 3.0,
    }.get(hw, 3.0)
    lanes = max(lanes, 1)
    # add curbs/margins
    margin = 0.5 if hw in ("footway", "path", "cycleway") else 1.2
    return round(lanes * lane_w + margin, 2)


def road_direction(tags: dict):
    """Return (oneway: bool, dir: int).

    dir is +1 (forward along polyline), -1 (reverse), 0 (two-way).
    OSM `oneway=yes|-1|reversible|alternating` drives this.
    """
    ow = tags.get("oneway", "")
    if ow in ("yes", "true", "1"):
        return True, 1
    if ow in ("-1", "reverse", "no_exit"):
        return True, -1
    if ow in ("reversible", "alternating"):
        return True, 0
    return False, 0


# --------------------------------------------------------------------------- #
# Overpass query + download
# --------------------------------------------------------------------------- #

def build_overpass_query(bbox: dict, use_geom: bool = True) -> str:
    """Build a bounded Overpass QL query.

    With `out geom;` the API returns resolved geometry for ways/relations
    inline (lat/lon per nd), so we never have to resolve node references
    ourselves — critical for relations (parks, water, districts) whose
    members are ways.
    """
    s, n, w, e = bbox["south"], bbox["north"], bbox["west"], bbox["east"]
    bb = f"({s},{w},{n},{e})"
    out = "out geom;" if use_geom else "out body;"
    # nwr = node+way+relation
    return f"""
[timeout:300][out:json];
(
  way["highway"]{bb};
  way["building"]{bb};
  way["waterway"="riverbank"]{bb};
  way["natural"="water"]{bb};
  way["water"]{bb};
  way["leisure"~"park|garden|pitch|playground|dog_park|nature_reserve"]{bb};
  way["landuse"~"grass|recreation_ground|forest|meadow|cemetery|village_green"]{bb};
  way["boundary"="administrative"]{bb};
  way["place"~"neighbourhood|suburb"]{bb};
  relation["water"]{bb};
  relation["natural"="water"]{bb};
  relation["leisure"="park"]{bb};
  relation["landuse"~"grass|forest|recreation_ground|meadow|cemetery"]{bb};
  relation["boundary"="administrative"]{bb};
  relation["place"~"neighbourhood|suburb"]{bb};
);
{out}
"""


def download_overpass(query: str, cache_path: str, force: bool) -> dict:
    if not force and os.path.exists(cache_path):
        print(f"[overpass] using cached response: {cache_path}")
        with open(cache_path, "r", encoding="utf-8") as f:
            return json.load(f)

    last_err = None
    for url in OVERPASS_ENDPOINTS:
        try:
            print(f"[overpass] POST {url}")
            data = urllib.parse.urlencode({"data": query}).encode("utf-8")
            req = urllib.request.Request(
                url, data=data, headers={"User-Agent": USER_AGENT}, method="POST")
            with urllib.request.urlopen(req, timeout=HTTP_TIMEOUT) as resp:
                raw = resp.read().decode("utf-8")
            obj = json.loads(raw)
            os.makedirs(os.path.dirname(cache_path), exist_ok=True)
            with open(cache_path, "w", encoding="utf-8") as f:
                json.dump(obj, f)
            print(f"[overpass] ok ({len(raw)} bytes) -> cached {cache_path}")
            return obj
        except (urllib.error.URLError, urllib.error.HTTPError, OSError,
                json.JSONDecodeError) as e:
            last_err = e
            print(f"[overpass] failed: {e}; trying next endpoint")
            time.sleep(1.0)
    raise RuntimeError(f"All Overpass endpoints failed. Last error: {last_err}")


# --------------------------------------------------------------------------- #
# Parsing
# --------------------------------------------------------------------------- #

def _geom_to_points(geom):
    """Extract [(lon, lat), ...] from an Overpass `geom` element."""
    pts = []
    for c in geom:
        if isinstance(c, dict) and "lon" in c and "lat" in c:
            pts.append((float(c["lon"]), float(c["lat"])))
        elif isinstance(c, (list, tuple)) and len(c) >= 2:
            # rare nested form
            pts.append((float(c[0]), float(c[1])))
    return pts


def parse_osm(obj: dict, bbox: dict):
    elements = obj.get("elements", [])
    streets, buildings, water, parks, districts, signals = [], [], [], [], [], []

    lat0 = (bbox["south"] + bbox["north"]) / 2.0
    lon0 = (bbox["west"] + bbox["east"]) / 2.0

    for el in elements:
        etype = el.get("type")
        tags = el.get("tags", {}) or {}
        geom = el.get("geometry")  # present when `out geom;` used

        # ---- STREETS (ways only) ----
        if etype == "way" and "highway" in tags and geom:
            pts = _geom_to_points(geom)
            if len(pts) < 2:
                continue
            lanes = estimate_lanes(tags)
            width = estimate_road_width_m(tags, lanes)
            oneway, direction = road_direction(tags)
            street = {
                "id": el.get("id"),
                "name": tags.get("name", ""),
                "highway": tags.get("highway", ""),
                "polyline": [[round(lon, 7), round(lat, 7)] for lon, lat in pts],
                "polyline_local_m": [
                    [round(x, 3), round(y, 3)]
                    for x, y in (local_meters(lat, lon, lat0, lon0)
                                 for lon, lat in pts)
                ],
                "width_m": width,
                "lanes": lanes,
                "oneway": oneway,
                "direction": direction,
                "maxspeed_kmh": _to_float(tags.get("maxspeed")) if _to_float(tags.get("maxspeed")) else None,
                "surface": tags.get("surface", ""),
                "lit": tags.get("lit", ""),
                "length_m": round(haversine_chain(pts), 2),
            }
            streets.append(street)

        # ---- TRAFFIC SIGNALS (nodes only) ----
        # OSM convention: highway=traffic_signals on a node. The node geometry
        # is the node's own (lon,lat). Used by the World Partition importer to
        # seed intersection signal phases.
        elif etype == "node" and tags.get("highway") == "traffic_signals":
            lon = float(el.get("lon", 0.0))
            lat = float(el.get("lat", 0.0))
            signals.append({
                "id": el.get("id"),
                "name": tags.get("name", tags.get("crossing", "")),
                "location": [round(lon, 7), round(lat, 7)],
                "location_local_m": [
                    round(x, 3) for x in local_meters(lat, lon, lat0, lon0)
                ],
            })
            continue

        # ---- BUILDINGS (ways only) ----
        elif etype == "way" and "building" in tags and geom:
            pts = _geom_to_points(geom)
            if len(pts) < 3:
                continue
            use = classify_use(tags)
            height = estimate_height_m(tags, use)
            ring = [[round(lon, 7), round(lat, 7)] for lon, lat in pts]
            ring_local = [[round(x, 3), round(y, 3)]
                          for x, y in (local_meters(lat, lon, lat0, lon0)
                                       for lon, lat in pts)]
            buildings.append({
                "id": el.get("id"),
                "name": tags.get("name", ""),
                "building_class": tags.get("building", ""),
                "use": use,
                "height_m": height,
                "height_ue": round(height * UE_CM_PER_METRE, 2),
                "floors": int(round(height / 3.0)),
                "footprint": ring,
                "footprint_local_m": ring_local,
                "addr_housenumber": tags.get("addr:housenumber", ""),
                "addr_street": tags.get("addr:street", ""),
            })

        # ---- WATER (ways + relations) ----
        elif ("natural" in tags and tags["natural"] == "water") or \
             ("water" in tags) or \
             (etype == "relation" and tags.get("natural") == "water") or \
             (etype == "relation" and "water" in tags) or \
             ("waterway" in tags and tags.get("waterway") == "riverbank"):
            poly = _extract_polygon(el, geom, tags)
            if poly:
                water.append({
                    "id": el.get("id"),
                    "name": tags.get("name", ""),
                    "water": tags.get("water", tags.get("natural", "")),
                    "polygons": _localize_polygon(poly, lat0, lon0),
                    "polygons_latlon": poly,
                })

        # ---- PARKS / GREEN (ways + relations) ----
        elif ("leisure" in tags and tags["leisure"] in
              {"park", "garden", "pitch", "playground", "dog_park",
               "nature_reserve", "recreation_ground"}) or \
             ("landuse" in tags and tags["landuse"] in
              {"grass", "forest", "meadow", "cemetery", "recreation_ground",
               "village_green", "conservation"}) or \
             (etype == "relation" and tags.get("leisure") == "park"):
            poly = _extract_polygon(el, geom, tags)
            if poly:
                parks.append({
                    "id": el.get("id"),
                    "name": tags.get("name", ""),
                    "kind": tags.get("leisure", tags.get("landuse", "")),
                    "polygons": _localize_polygon(poly, lat0, lon0),
                    "polygons_latlon": poly,
                })

        # ---- DISTRICTS / NEIGHBOURHOODS (relations, multipolygon) ----
        elif (tags.get("boundary") == "administrative" and etype == "relation") or \
             (tags.get("place") in {"neighbourhood", "suburb"} and etype == "relation"):
            poly = _extract_polygon(el, geom, tags)
            if poly:
                admin_l = tags.get("admin_level", "")
                districts.append({
                    "id": el.get("id"),
                    "name": tags.get("name", ""),
                    "admin_level": int(admin_l) if admin_l.isdigit() else None,
                    "place": tags.get("place", ""),
                    # Raw OSM landuse/leisure tag — the importer maps this to a
                    # NYC-style zoning class (R8 / C6 / M1 / PARK) for height caps.
                    "zoning_tag": tags.get("landuse", tags.get("leisure", tags.get("place", ""))),
                    "polygons": _localize_polygon(poly, lat0, lon0),
                    "polygons_latlon": poly,
                })

    return {
        "streets": streets,
        "buildings": buildings,
        "water": water,
        "parks": parks,
        "districts": districts,
        "signals": signals,
        "_anchor": (lat0, lon0),
    }


def haversine_chain(pts):
    """Sum of segment lengths for a polyline of (lon,lat)."""
    total = 0.0
    for i in range(len(pts) - 1):
        total += haversine_m(pts[i][1], pts[i][0], pts[i + 1][1], pts[i + 1][0])
    return total


def _localize_polygon(poly, lat0, lon0):
    out = []
    for ring in poly:
        out.append([[round(x, 3), round(y, 3)]
                    for x, y in (local_meters(lat, lon, lat0, lon0)
                                 for lon, lat in ring)])
    return out


def _extract_polygon(el, geom, tags):
    """Return list-of-rings [[(lon,lat)...], ...] from a way or relation.

    For `out geom;` ways: `geometry` is the ring.
    For relations: `geometry` is a flat list of member geometries; member role
    ('outer'/'inner') is carried in each member's `role` field by Overpass.
    """
    etype = el.get("type")
    if etype == "way" and geom:
        pts = _geom_to_points(geom)
        if len(pts) >= 3:
            # ensure closed ring
            if pts[0] != pts[-1]:
                pts = pts + [pts[0]]
            return [pts]
    if etype == "relation" and geom:
        outer, inner = [], []
        for m in geom:
            role = m.get("role", "")
            mpts = _geom_to_points(m.get("geometry", []))
            if len(mpts) < 3:
                continue
            if mpts[0] != mpts[-1]:
                mpts = mpts + [mpts[0]]
            if role == "inner":
                inner.append(mpts)
            else:
                outer.append(mpts)
        if outer:
            return outer + inner
    return None


# --------------------------------------------------------------------------- #
# Output assembly
# --------------------------------------------------------------------------- #

def build_output(parsed: dict, bbox: dict, query: str) -> dict:
    lat0, lon0 = parsed["_anchor"]
    meta = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "source": "OpenStreetMap (Overpass API)",
        "attribution": "© OpenStreetMap contributors (https://www.openstreetmap.org/copyright)",
        "crs": "EPSG:4326 (WGS84)",
        "bounds": {
            "south": bbox["south"], "north": bbox["north"],
            "west": bbox["west"], "east": bbox["east"],
        },
        "anchor": {"lat": round(lat0, 7), "lon": round(lon0, 7)},
        "coordinate_frames": {
            "latlon": "GeoJSON-style [lon, lat] degrees (WGS84).",
            "local_m": ("Tangent-plane metres relative to `anchor`: "
                        "[east_m, north_m]. X=east, Y=north. Use for UE5 "
                        "placement to avoid float drift."),
        },
        "ue_scale_cm_per_m": UE_CM_PER_METRE,
        "stats": {
            "streets": len(parsed["streets"]),
            "buildings": len(parsed["buildings"]),
            "water": len(parsed["water"]),
            "parks": len(parsed["parks"]),
            "districts": len(parsed["districts"]),
        },
        "use_breakdown": _use_breakdown(parsed["buildings"]),
    }
    return {
        "metadata": meta,
        "streets": parsed["streets"],
        "buildings": parsed["buildings"],
        "water": parsed["water"],
        "parks": parsed["parks"],
        "districts": parsed["districts"],
        # The raw query is embedded for reproducibility / re-runs.
        "_overpass_query": query.strip(),
    }


def _use_breakdown(buildings):
    out = {}
    for b in buildings:
        out[b["use"]] = out.get(b["use"], 0) + 1
    return out


# --------------------------------------------------------------------------- #
# CLI
# --------------------------------------------------------------------------- #

def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Convert Manhattan (OSM) to a UE5-ready city JSON.")
    ap.add_argument("--south", type=float, default=DEFAULT_BBOX["south"])
    ap.add_argument("--north", type=float, default=DEFAULT_BBOX["north"])
    ap.add_argument("--west", type=float, default=DEFAULT_BBOX["west"])
    ap.add_argument("--east", type=float, default=DEFAULT_BBOX["east"])
    ap.add_argument("--out", default=None,
                    help="Output JSON path (default: <scriptdir>/manhattan_slice.json)")
    ap.add_argument("--pretty", action="store_true",
                    help="Pretty-print JSON (indent=2). Default is compact.")
    ap.add_argument("--no-cache", action="store_true",
                    help="Force a fresh Overpass download.")
    args = ap.parse_args(argv)

    bbox = {"south": args.south, "north": args.north,
            "west": args.west, "east": args.east}

    script_dir = os.path.dirname(os.path.abspath(__file__))
    slug = f"manhattan_{bbox['south']}_{bbox['north']}_{bbox['west']}_{bbox['east']}"
    slug = slug.replace(".", "p").replace("-", "m")
    cache_path = os.path.join(script_dir, "cache", f"{slug}.json")
    out_path = args.out or os.path.join(script_dir, "manhattan_slice.json")

    print(f"[init] bbox={bbox}")
    print(f"[init] out={out_path}")

    query = build_overpass_query(bbox)
    obj = download_overpass(query, cache_path, force=args.no_cache)
    parsed = parse_osm(obj, bbox)
    out = build_output(parsed, bbox, query)

    with open(out_path, "w", encoding="utf-8") as f:
        if args.pretty:
            json.dump(out, f, indent=2, ensure_ascii=False)
        else:
            json.dump(out, f, ensure_ascii=False, separators=(",", ":"))

    print("[done] wrote", out_path)
    print("[stats]", json.dumps(out["metadata"]["stats"], indent=2))
    print("[use]  ", json.dumps(out["metadata"]["use_breakdown"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
