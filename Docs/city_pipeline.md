# GTAI City Generation Pipeline — Design Doc

> Agent: ATLAS (City Architect)
> Date: 2026-07-08
> Status: Phase 1 complete (script + data delivered, doc written by orchestrator)

## Overview

The city generation pipeline converts real-world OpenStreetMap data for Manhattan into a structured JSON format that UE5 can import and place as World Partition sectors.

## Pipeline Architecture

```
Overpass API (OSM data)
  ↓
osm_to_json.py (Python stdlib only, no pip install)
  ↓
manhattan_slice.json (streets, buildings, water, parks, districts)
  ↓
UE5 C++ Importer (GTAI_World module)
  ↓
World Partition sectors (streaming cells)
```

## Data Flow

### 1. OSM Download (Overpass API)
- Script queries Overpass API for the bounded Manhattan area (40.74-40.78 lat, -74.02 to -73.97 lon)
- Three Overpass endpoints with automatic fallback
- Raw response cached to `cache/` for idempotent reruns
- Zero third-party dependencies — uses only Python stdlib (urllib, json, xml.etree)

### 2. Parsing & Enrichment
- Streets: Extracted as polylines with width (from OSM highway tags), lane count (from tags or heuristic by road class), direction (oneway flag), highway classification
- Buildings: Footprint polygons with height estimation (from `height` tag → `building:levels` tag → heuristic by building class + use type), use classification (commercial/residential/industrial via rule-based mapping)
- Water: Water-body polygons (rivers, bays, reservoirs)
- Parks: Green-space polygons (leisure=park, landuse=grass, etc.)
- Districts: Administrative/neighborhood boundaries

### 3. Coordinate System
- Input: WGS84 lat/lon (from OSM)
- Output: Both raw lat/lon AND local metric frame (X east, Y north) relative to area centroid
- Projection: Equirectangular tangent plane — error <1cm for ~4km area, perfectly reproducible
- UE5 scale: `ue_scale` hint in metadata (100 cm per metre default)

### 4. UE5 Import
The JSON is parsed by C++ in the `GTAI_World` module:
- Streets → Spline meshes or static road geometry
- Buildings → Procedural meshes from footprint polygons + extruded by height
- Water → Planes with water material
- Parks → Flat areas with foliage
- Each category placed into appropriate World Partition streaming cells

## Output JSON Schema

```json
{
  "metadata": {
    "crs": "WGS84",
    "bounds": {"south": 40.74, "north": 40.78, "west": -74.02, "east": -73.97},
    "centroid": {"lat": 40.76, "lon": -73.995},
    "ue_scale": 100.0,
    "generated": "2026-07-08T20:28:00Z",
    "stats": {"streets": N, "buildings": N, "water": N, "parks": N}
  },
  "streets": [
    {
      "id": "way/123456",
      "name": "5th Avenue",
      "highway": "primary",
      "lanes": 4,
      "width_m": 14.0,
      "oneway": false,
      "points": [[lat, lon, x_m, y_m], ...]
    }
  ],
  "buildings": [
    {
      "id": "way/789012",
      "name": "Empire State Building",
      "use": "commercial",
      "height_m": 381.0,
      "levels": 102,
      "footprint": [[lat, lon, x_m, y_m], ...]
    }
  ],
  "water": [...],
  "parks": [...],
  "districts": [...]
}
```

## World Partition Integration

The Manhattan slice (~4km x 5km) will be divided into World Partition cells:
- Cell size: 500m x 500m (configurable)
- Each cell streams in/out based on player position
- Loading budget: 3x3 grid centered on player (9 cells loaded)
- Buildings, roads, props placed into their respective cells based on centroid position

## Verified Artifacts

| File | Size | Status |
|------|------|--------|
| `Tools/CityGenerator/osm_to_json.py` | 655 lines, 26KB | Complete — stdlib only, Overpass with fallback, height estimation, lane detection |
| `Tools/CityGenerator/cache/manhattan_*.json` | 18MB | Complete — raw Overpass data for Manhattan slice |
| `Tools/CityGenerator/manhattan_slice.json` | 18MB | Complete — processed city data ready for UE5 import |

## Next Steps

1. Write UE5 C++ importer in `GTAI_World` module that reads the JSON and spawns geometry
2. Integrate with FORGE's asset pipeline to place AI-generated building meshes at footprint locations
3. Set up World Partition cells with proper streaming bounds
4. Add traffic spawn points along street network
5. Add NPC spawn points at building entrances and park locations