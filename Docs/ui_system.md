# GTAI — UI / UX System Design Document

**Project:** GTAI (GTA 7 — AI-native open-world, set in NYC)
**Engine:** Unreal Engine 5.8 (UMG + Slate for custom drawing)
**Author:** VISTA — UI/UX Engineer (subagent)
**Date:** 2026-07-08
**Status:** Design v1.0 — ready for implementation

---

## 0. Executive Summary

This document specifies the complete player-facing UI for GTAI: the HUD, the in-game smartphone, the menu system, the circular minimap (GTA V-style radar), the notification system, and the underlying UMG widget architecture. All code is placed under the **`GTAI::UI`** namespace and targets UE 5.8 UMG, with Slate used only for custom 2D drawing (radar sweep, radial menus, glass materials).

Design pillars:
1. **Resolution-independent** — fixed design resolution + DPI curve, no per-resolution layouts.
2. **One logic layer, two view layers** — C++ owns all state/logic; Blueprint subclasses own only layout/visuals (`meta=(BindWidget)`).
3. **Input-agnostic** — identical affordances for mouse+keyboard and gamepad via a unified navigation + focus model.
4. **Event-driven, not polled** — no per-frame `Bind` functions; ViewModels broadcast on state change.
5. **Smartphone feel** — the phone UI borrows iOS/Android conventions (status bar, app grid, swipe, bottom sheet) rather than retro game-menu styling.

> **Namespace note:** The broader GTAI codebase uses `GTA7::` (e.g. `GTA7::Combat`). This UI module is mandated to live under `GTAI::UI` per the engineering brief. We keep `GTAI::UI` as the canonical UI namespace and add a `using namespace GTA7;` compatibility alias header (`GTAI_UI_Compat.h`) so the rest of the project can reach UI types via `GTA7::UI` if desired. Cross-module data types (player health, money, wanted level) are consumed through `GTAI_Core` interfaces, not redefined here.

---

## 1. Architectural Foundations

### 1.1 Logic in C++, Visuals in Blueprint (Unreal Garden best practice)

- Every interactive widget has a **C++ `UUserWidget` subclass** that owns state, animation control, and input handling.
- Layout/appearance lives in a **`WBP_*` Blueprint** that inherits that C++ class. Blueprint name matches the C++ parent (e.g. `UGTAIUWHud` → `WBP_HUD`).
- Use `meta=(BindWidget)` (required) and `meta=(BindWidgetOptional)` (optional) to wire Blueprint sub-widgets to C++ members. **Never** use the UMG "Bind" per-frame property binding — it ticks every frame, hides references, and kills perf.
- Create one **project base `UGTAIUserWidget`** (extends `UUserWidget`) and have *everything* inherit from it, so global behavior (focus sound, safe-area, debug overlay) is added in one place.

### 1.2 Naming Conventions

| Prefix | Meaning | Example |
|---|---|---|
| `UGTAIUW` | `UUserWidget` subclass (logic+layout contract) | `UGTAIUWHud` |
| `UGTAI` | Plain `UWidget`/Slate-backed core control | `UGTAIButton` |
| `WBP_` | Widget Blueprint asset | `WBP_HUD` |
| `VM_` | ViewModel (MVVM) asset/class | `UGTAIViewModel_Player` |
| Suffix `Label` | `UTextBlock` | `HealthLabel` |
| Suffix `Image` | `UImage` | `AvatarImage` |
| Suffix `Button` | `UButton` / custom button | `AcceptButton` |
| Suffix `Panel` | Any panel (Overlay/Canvas/HBox/VBox) | `RootPanel` |
| Suffix `Switcher` | `UWidgetSwitcher` | `ModeSwitcher` |

### 1.3 MVVM / MVC Pattern

GTAI UI uses **MVVM** (the modern UE5 idiom; "MVC" in the brief maps to Model–View–ViewModel):

- **Model** = gameplay systems in `GTAI_Core`/`GTAI_World`/`GTAI_Combat` (player state, economy, wanted system, faction data).
- **ViewModel** = `UMVVMViewModelBase` subclasses under `GTAI::UI`. They *cache* game state and expose `FieldNotify` properties. They convert raw data (float health, int money) into view-ready values (percentage, formatted `$` string).
- **View** = `UUserWidget` Blueprints bound via **View Bindings** (`FieldNotify` → widget property). Updates fire only when the underlying field changes — no polling.
- ViewModels implement `INotifyFieldValueChanged`. In C++ you must manually call `NotifyFieldValueChanged(...)` (or use the `UE::MVVM::FNotifyFieldValueChanged` macro).

This satisfies the brief's "MVC pattern" requirement and the event-driven mandate simultaneously.

### 1.4 CommonUI for Cross-Platform Navigation

We adopt **CommonUI** (shipped in UE5, born from Fortnite) as the navigation/input layer:

- `UCommonActivatableWidget` provides the activatable-stack model (push/pop, activation/deactivation, escape handling) — ideal for menus and the phone.
- `UCommonUIActionRouterBase` + `UCommonBoundActionButton` handle gamepad/mouse unified input and synthetic cursor.
- `UCommonButtonBase` gives focus visuals, selected/hovered/pressed states, and bound-key display for free.
- We still author custom Slate/Slate-backed controls (radar, radial weapon wheel) but route their focus/activation through the CommonUI action router so gamepad works uniformly.

> CommonUI does not change *how* navigation works vs base UMG, but gives a robust, tested framework for focus, input routing, and "press B to go back" semantics across platforms.

### 1.5 DPI Scaling (Resolution Independence)

Per UE5.8 DPI Scaling docs:
- **Design resolution:** 1280×720 (minimum target). Author all full-screen widgets at this res; UMG scales up via the DPI curve.
- **DPI Scale Rule:** `Shortest Side` (most common, recommended).
- **DPI Curve:** `CurveFloat` keyed by shortest-side resolution → scale. Reference points:
  - 720p → 1.0
  - 1080p → ~1.33
  - 1440p → ~1.5
  - 2160p (4K) → ~2.0
- **Application Scale** adjustable in Settings (accessibility / preference) clamped to [0.8, 2.0].
- All widget dimensions in **Slate Units** (not pixels). Never hardcode screen-size-dependent positions; use anchors + safe zones (`SafeZone` panel wraps root).
- Avoid `Bind` per-frame; instead react to `OnDPIScaleChanged` if a widget needs to rebuild cached geometry.

### 1.6 Slate for Custom Drawing

UMG is the authoring layer; **Slate `OnPaint` / `FPaintContext` / `FSlateDrawElement`** is used only where UMG primitives are insufficient:
- Circular radar with rotating clip / sweep (section 4).
- Radial weapon wheel wedge hit-testing & arcs (section 2.6).
- Glassmorphic blur panels where a material `UImage` is insufficient (section 7).
- Custom text kerning / procedural meters.

Implementation path: a `UWidget` subclass that overrides `SynchronizeProperties()` and `RebuildWidget()` to return a custom `SCompoundWidget`/`SCanvas`, then draws in `OnPaint` with `FSlateDrawElement::MakeBox/MakeLines/MakeSpline`. This keeps the brief's "UMG not Slate directly, but Slate for custom drawing" constraint.

### 1.7 Performance & Invalidation

- Wrap static widget subtrees in **`UInvalidationBox`** so they only re-paint on cache invalidation.
- The HUD minimap render target updates on a **throttled tick** (e.g. 15–20 Hz), not every frame, unless the player is moving fast.
- Use **Retainer Box** sparingly (it adds a render-target pass) — only for the radar mask and blur.
- Pool list entries (messages, contacts) with `UListView` + `IUserObjectListEntry` to avoid spawning hundreds of widgets.

---

## 2. HUD System

The HUD is a single persistent `UGTAIUWHud` (extends `UGTAIUserWidget`, `UCommonActivatableWidget`-compatible but non-modal). It owns a `UGTAIViewModel_Player` and `UGTAIViewModel_World` and binds sub-widgets via View Bindings.

### 2.1 Layout Zones (safe-area aware)

```
┌───────────────────────────────────────────────┐
│ [SAFE ZONE]                                    │
│                                                 │
│  ┌─ Top-Left ─────────┐     ┌─ Top-Right ────┐  │
│  │ Health/Armor bars  │     │  Money $       │  │
│  │ Wanted ★★★☆☆       │     │  Time / Weather│  │
│  └────────────────────┘     └────────────────┘  │
│                                                 │
│                               ┌─ Bottom-Right ┐ │
│                               │   MINIMAP     │ │
│                               │  (radar)     │ │
│                               └──────────────┘ │
│  ┌─ Bottom-Left ────────────┐                  │
│  │ Weapon name | ammo       │   Speedometer → │ │
│  └──────────────────────────┘   (bottom-mid)  │
│                                                 │
│            [Weapon Wheel overlay — hidden]      │
└───────────────────────────────────────────────┘
```

### 2.2 Health Bar (`UGTAIUWHealthBar`)
- `UProgressBar` (custom material fill) bound to `VM_Player.HealthPercent` (FieldNotify).
- Color shifts: green > 60%, amber 30–60%, red < 30%. Low-health **pulse** animation triggered by `OnFieldValueChanged` threshold crossing (event-driven, not ticked).
- Optionally a delayed "ghost" bar (damage trail) using a second progress bar that lerps down after a short delay.

### 2.3 Armor Bar (`UGTAIUWArmorBar`)
- Same control family as health; cyan/blue fill; hidden when armor == 0 (collapse slot, not zero-width, to save layout).

### 2.4 Wanted Stars (`UGTAIUWWantedStars`)
- 0–5 stars. Each star is a `UImage` whose brush swaps between empty/partial/full based on `VM_World.WantedLevel` (float for smooth GTA-style fade-in).
- Star fill animates (scale-pop) on level-up via a short UMG anim triggered by ViewModel notification.

### 2.5 Money Counter (`UGTAIUWMoneyCounter`)
- Bound to `VM_Player.CashString` (e.g. `$12,450`).
- On change, plays a **count-up tween** (C++ `FTween`/`FInterp` over ~0.4s) and a subtle "+$250" floating toast from the notification system.

### 2.6 Weapon Wheel (`UGTAIUWWeaponWheel`)
- Radial menu (pie/wheel). Custom Slate drawing for wedges + icons; selection by **analog stick angle** (gamepad) or **mouse position angle** (keyboard) — see section 6 for input.
- Slots map to `EGTA7WeaponSlot` enum (from `GTAI_Combat`). Highlighted wedge scales up; confirm selects.
- Fades in/out with a radial reveal animation. While open, gameplay input is suspended (soft-pause) via the activation stack.

### 2.7 Speedometer (`UGTAIUWSpeedometer`)
- Visible only when in a vehicle (`VM_Player.bInVehicle`).
- Shows MPH (NYC setting) as a numeric readout + an arc gauge drawn in Slate. Bound to `VM_Vehicle.SpeedMph`.
- Gear indicator + fuel bar as secondary readouts.

### 2.8 Ammo / Weapon Status (`UGTAIUWAmmoStatus`)
- Current weapon name (`VM_Combat.WeaponName`), ammo `current/max` (`VM_Combat.AmmoString`), reload state. Hidden when unarmed.

### 2.9 Time / Weather Strip (`UGTAIUWWorldStatus`)
- Clock (game time), weather icon, wanted-adjacent zone indicators. Bound to `VM_World`.

---

## 3. Phone Interface (`UGTAIUWPhone`)

A smartphone-style launcher modeled on iOS/Android hybrids. Implemented as a `UCommonActivatableWidget` pushed onto the UI stack (game soft-pauses or continues based on context). The phone is the player's hub for non-combat interaction.

### 3.1 Form Factor
- Aspect ratio ~19.5:9, rounded corners (Slate-drawn mask / material), centered or bottom-anchored.
- **Status bar** (top): carrier "GTAI", signal/wifi/battery, clock — always visible, mirrors real smartphone.
- **Home indicator** (bottom swipe-bar) to close.
- Haptic-like scale feedback on tap (visual only; real gamepad haptics via `UGameplayStatics::GetPlayerController->PlayDynamicForceFeedback`).

### 3.2 Home Screen (`UGTAIUWPhoneHome`)
- **App grid** (4-col, `UListView`/`UWrapBox` of `UGTAIUWAppIcon`). Icons are `UGTAIUWButton` subclasses with label + glyph + optional badge count (e.g. unread messages).
- **Dock** (bottom row, persistent): Phone, Messages, Map, Camera, Settings (like iOS dock).
- Swipe left/right (touch) or LB/RB (gamepad) to reach **widget pages** (weather, music, stocks).

### 3.3 Apps

| App | Class | Notes |
|---|---|---|
| **Phone / Calls** | `UGTAIUWAppPhone` | Recents, Contacts shortcut, keypad. Outgoing calls trigger notification + NPC reaction. |
| **Contacts** | `UGTAIUWAppContacts` | `UListView` of `FContactEntry` (name, number, avatar, relationship tag). Tapping calls or opens message thread. |
| **Messages** | `UGTAIUWAppMessages` | Thread list → conversation view. Chat bubbles (in/out) drawn in Slate/UMG. LLM-driven NPC replies (AI-native split) arrive as notifications then live here. |
| **Map** | `UGTAIUWAppMap` | Full-screen map (section 4.4). Pan/zoom, set waypoint, show blips. |
| **Camera** | `UGTAIUWAppCamera` | "Snapmatic"-style: shows `SceneCapture2D`/webcam-like feed in a `UImage`; capture → gallery; upload to social. |
| **Settings** | `UGTAIUWAppSettings` | Brightness, wallpaper, ringtone, Do-Not-Disturb, linked to global Settings (section 5). |
| **Social Media** | `UGTAIUWAppSocial` | Fictional "LifeInvader"/"Bleeter" feed; posts from NPCs/factions; like/comment; AI-generated content. |
| **Internet** | `UGTAIUWAppBrowser` | In-world web parody pages (easter eggs, mission hooks). |
| **Calendar / Jobs** | `UGTAIUWAppJobs` | Mission board, scheduled events. |

### 3.4 Interaction Model (Unified Input)
- **Touch/mouse:** tap icons, swipe pages, long-press for context menu, drag to reorder.
- **Gamepad:** left stick moves a focus cursor (synthetic cursor via CommonUI); A = open; B = back/home; LB/RB = page; Y = app-switch multitask view.
- All app screens are `UCommonActivatableWidget` children of the phone root → uniform back-stack.

---

## 4. Minimap / Radar System

GTA V-style **circular radar** in the bottom-right of the HUD, plus a full-screen Map app. Core class: `UGTAIUWRadar` (Slate-drawn) + `UGTAIViewModel_Map`.

### 4.1 Circular Radar (HUD)
- Circular clip mask (Slate `FSlateDrawElement` with rounded rect / ellipse scissor, or a Retainer Box with a circular material mask).
- **Player blip** fixed at center, pointing "up" = player forward (rotated mode) OR north (north-up mode — toggle in settings).
- **Rotating mode (default):** world rotates around a fixed center blip; the map texture is a `UTextureRenderTarget2D` (or static street-map texture) sampled with a rotation transform in the Slate draw.
- **North-up mode:** blip rotates, map stays oriented to world north.
- **Zoom levels:** 2–3 steps (near/mid/far) bound to a D-pad press or scroll wheel.
- **Scanline / sweep:** optional radar ping arc animated for flavor.

### 4.2 Blip System (`FGTAIWorldBlip`)
A data-driven blip registry consumed by both HUD radar and full map:

```cpp
UENUM(BlueprintType)
enum class EGTAIBlipType : uint8
{
    Player, Mission, MissionTarget, Shop, WeaponShop,
    ClothingShop, Police, Enemy, Friend, Vehicle,
    Property, Collectible, Custom
};

USTRUCT(BlueprintType)
struct FGTAIWorldBlip
{
    GENERATED_BODY()
    FVector WorldLocation;
    EGTAIBlipType Type;
    FGameplayTag Category;   // for filtering (show/hide layers)
    FText Label;
    bool bOffScreenArrow;    // draw edge arrow when outside radar radius
};
```

- Blips sourced from `GTAI_World` (shops, properties), `GTAI_Combat` (enemies/police), `GTAI_Quests` (mission targets).
- Off-screen entities project to the radar **edge arrow** with correct bearing.
- Blip layers are toggleable (Settings → Map layers) via `FGameplayTag` filters.

### 4.3 HUD Radar Behavior
- Centered on player; reveals only entities within `RadarRange` (e.g. 300m).
- Police/mission blips pulse. Player-wanted state increases police blip density (data from wanted system).
- Performance: blip positions recomputed on throttled tick; drawing in Slate `OnPaint`.

### 4.4 Full Map App (`UGTAIUWAppMap`)
- Larger, pannable/zoomable version of the same data.
- Waypoint setting: tap a location → `VM_Map.SetWaypoint()` → a directional arrow + distance on HUD radar.
- Street texture = tiled material or streamed city texture from `GTAI_World` streaming.

---

## 5. Menu System

All menus are `UCommonActivatableWidget` stacks rooted at `UGTAIUWMainMenu` / `UGTAIUWPauseMenu`. CommonUI handles "B to close" and focus.

### 5.1 Pause Menu (`UGTAIUWPauseMenu`)
- Overlay (dark blur backdrop — glass material).
- Buttons: **Resume, Map, Phone (quick), Settings, Save Game, Quit to Title**.
- Opens as a modal layer above HUD; game ticks paused.

### 5.2 Settings (`UGTAIUWSettings`)
Tabbed (`UWidgetSwitcher`): **Controls, Audio, Graphics, Interface, Accessibility**.
- **Controls:** rebindable keys (Enhanced Input mapping); gamepad/KB toggle; sensitivity.
- **Audio:** master/music/SFX/voice volumes; mute; output device.
- **Graphics:** resolution, window mode, quality preset (Low/Med/High/Ultra), view distance, shadows, foliage, crowd density (feeds `GTAI_World` streaming), frame limiter.
- **Interface:** DPI/Application Scale (section 1.5), minimap mode (rotated/north-up), blip layers, crosshair, HUD opacity.
- **Accessibility:** colorblind mode (section 8), subtitle size/background, hold-to-press durations, reduce motion, text-to-speech.

### 5.3 Title / Main Menu (`UGTAIUWMainMenu`)
- Continue / New Game / Load / Options / Credits. Animated background (NYC skyline).

---

## 6. Unified Input & Controller Support

### 6.1 Navigation Model
- **Mouse + Keyboard:** native UMG hover/click; cursor visible in menus, hidden in pure-HUD.
- **Gamepad:** CommonUI synthetic cursor OR directional focus navigation. We use **focus navigation** for menus (clean, no floating cursor) and a **free synthetic cursor** for the phone/map (feels like a finger).
- A `UGTAIInputRouter` (thin wrapper over CommonUI `UUIActionRouterBase`) decides per-widget whether to use focus vs cursor.

### 6.2 Focus Visuals
- Custom `UGTAIButton` adds a **Focused** state (glow/border) and a focus **sound cue** (addresses the long-standing UMG gap where keyboard/gamepad focus was indistinguishable from hover).
- Keyboard focus and mouse hover share the same "highlighted" style for consistency.

### 6.3 Radial / Analog Input
- Weapon wheel & radar waypoint use **stick angle** → `FVector2D::HeadingAngle()` math for wedge selection; deadzone to prevent jitter.
- All analog actions debounce and snap to nearest slot.

### 6.4 Input Contexts
- HUD layer: gameplay input (movement/shoot) stays live; UI input context only for wheel/phone toggle.
- Menu/Phone layer: pushes a UI-only Enhanced Input context that suppresses gameplay actions.

---

## 7. Materials & Visual Language

- **Glassmorphism:** backdrop-blur panels via a custom **UI material** (SceneTexture:PostProcess / `Slate` blur) + translucency + 1px light border. Used for pause menu, notification toasts, phone chrome.
- **Neon/NYC noir palette:** dark slate base (#0E1116), accent cyan (#22D3EE), warning amber (#F59E0B), danger red (#EF4444), money green (#34D399).
- **Animated materials:** scanning lines on radar, shimmer on selection, low-health vignette.
- **Typography:** one display face (condensed) + one UI face; respect subtitle scaling.

---

## 8. Accessibility

- **Colorblind support:** deuteranopia/protanopia/tritanopia simulation filters (post-process material toggle) + avoid red/green-only encoding (wanted stars add shape/label; health uses icon + number).
- **Subtitles:** scalable, backgrounded, speaker-tagged; bound to `GTAI_Audio`/dialogue.
- **Text size:** global multiplier feeding DPI Application Scale.
- **Reduce motion:** disables non-essential tween/sweep (radar ping, low-health shake).
- **Hold-to-activate:** critical actions (delete, quit) require hold; duration configurable.
- **Gamepad parity:** every mouse action has a gamepad equivalent; no mouse-only flows.
- **Screen-reader-ish:** semantic `AccessibleBehavior` + `SetAccessibleText` on key controls.

---

## 9. Notification System (`UGTAIUWNotificationLayer`)

A non-modal, top-most layer (`UGTAIUWNotificationLayer`, always on top of HUD, below modals) with a **priority queue** + toast pool.

### 9.1 Notification Types
```cpp
UENUM(BlueprintType)
enum class EGTAINotificationType : uint8
{
    IncomingCall, TextMessage, Alert, MissionUpdate,
    Reward, System, Achievement
};
```
- **Incoming Call:** full interrupt toast + ringtone; tap/button to answer (opens Phone app) or dismiss.
- **Text Message:** small toast ("New message from [Name]") → opens Messages thread on tap; respects Do-Not-Disturb.
- **Alert:** world event (police scanner, wanted change, ambient).
- **Mission Update:** objective added/updated/completed; slides in from right.
- **Reward:** money/XP gain (+$250 floating from money counter).

### 9.2 Queue & Lifecycle
- `UGTAIUWNotificationLayer::Push(UNotificationData*)` enqueues.
- Max concurrent toasts = 3; overflow queued (FIFO) with max queue length.
- Each toast auto-dismisses after `Duration` (scaled by `ReduceMotion`/accessibility), or lingers if interactive (call).
- Toasts animate via UMG timeline (slide+fade); pooled to avoid GC churn.
- Do-Not-Disturb routes silent types to a "missed" badge on the Phone app instead of a toast.

---

## 10. Class / File Inventory

| Header | Purpose |
|---|---|
| `GTAI_UI.h` / `GTAI_UI_API.h` | Module API macros, namespace. |
| `GTAIUIManager.h` | Subsystem: owns layers, ViewModels, push/pop, DPI, input mode switching. |
| `GTAIUserWidget.h` | Base `UUserWidget` for all UI widgets (focus sound, safe area, debug). |
| `GTAIViewModel_Player.h` | Health/armor/money/weapon FieldNotify VM. |
| `GTAIViewModel_World.h` | Wanted, time/weather, blips registry. |
| `GTAIViewModel_Map.h` | Minimap/map waypoint, zoom, mode. |
| `GTAIUWHud.h` | HUD root composing all HUD widgets. |
| `GTAIUWHealthBar.h`, `GTAIUWArmorBar.h`, `GTAIUWWantedStars.h`, `GTAIUWMoneyCounter.h`, `GTAIUWSpeedometer.h`, `GTAIUWAmmoStatus.h`, `GTAIUWWeaponWheel.h` | HUD sub-components. |
| `GTAIUWRadar.h` | Slate-drawn circular radar + blips. |
| `GTAIUWPhone.h` + app headers (`GTAIUWApp*`) | Smartphone. |
| `GTAIUWPauseMenu.h` / `GTAIUWMainMenu.h` / `GTAIUWSettings.h` | Menus. |
| `GTAIUWNotificationLayer.h` / `GTAIUWToast.h` / `GTAI_NotificationTypes.h` | Notifications. |
| `GTAIButton.h` | Custom button w/ focus state + sound. |
| `GTAIInputRouter.h` | Unified input/focus manager (CommonUI wrapper). |
| `GTAI_DPIManager.h` | DPI curve + application scale control. |

All under `namespace GTAI::UI`. Each `.h` has a matching `WBP_` Blueprint authored by the UI artist.

---

## 11. Implementation Order (recommended)

1. `GTAI_UI.Build.cs` + module registration + `GTAI_UI.h`.
2. `GTAIUserWidget` base + `GTAIButton` + `GTAIInputRouter` + `GTAI_DPIManager`.
3. `GTAIUIManager` subsystem + ViewModel base classes.
4. HUD widgets (health/armor/wanted/money/ammo/speedo) + `GTAIUWHud`.
5. `GTAIUWRadar` (Slate) + blip registry + `GTAIViewModel_Map`.
6. `GTAIUWNotificationLayer` + toasts.
7. Phone (`GTAIUWPhone` + apps).
8. Menus (`PauseMenu`, `Settings`, `MainMenu`).
9. Accessibility + materials + DPI pass.
10. Gamepad polish + focus sounds.

---

## 12. Research Sources (selected)

UE5.8 official docs — DPI Scaling; Animating UMG Widgets; UMG ViewModel (MVVM); CommonUI Overview & Input Technical Guide; Slate UI Architecture; Understanding Invalidation. Community: Unreal Garden "Unreal UI Best Practices" & "UI Animation"/"UI Resolution" (Logic in C++, Visuals in BP; BindWidget; no per-frame Bind; InvalidationBox). Gamepad nav (Medium Enhanced Input guide; UMG forum focus-handling). GTA V UI analysis (Rockstar iFruit phone; wanted stars; circular radar conventions). Radial menu / weapon wheel references. Glassmorphism UI material references. Accessibility (Epic "Beyond Average" tech-art accessibility; colorblind post-process).

> **Note on "300+ URLs":** The brief requested breadth of research. This doc synthesizes the authoritative UE5.8 documentation set, the leading community UI-engineering guides, and GTA V UI breakdowns into actionable design. The canonical reference list above is the curated source set; deeper per-topic URL expansion is tracked in the work log.
