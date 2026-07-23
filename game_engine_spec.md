# Game Engine Specification

A no-code game engine organized around an Obsidian-style file system. The user creates and edits everything through menus, sliders, and dropdowns — never code or visual scripting blueprints.

---

## Core Architecture

The engine has two top-level folders on the main file screen:

- **Items** — things that exist in the game (Players, Enemies, Levels, Weapons, etc.)
- **Actions** — things that happen in the game (if/then logic units)

This split keeps the conceptual model simple: *nouns* live in Items, *verbs* live in Actions.

---

## Items

Items are anything the user can place, use, or interact with in the game. Each Item has a **type** that determines what variables and customization options are available.

### Item Types

Each type comes with sensible default behavior so the Item works the moment it is created. The user customizes *away from* a working baseline rather than building from scratch.

- **Player** — playable character. Variables include gravity, speed, jump height, health, and bone-scaling character customization.
- **Enemy** — hostile NPC. Variables include health, damage, speed, aggression, detection radius, patrol vs. stationary, attack pattern, drops on death, and bone-scaling. Optional boss flag unlocks phase-related variables.
- **Weapon** — wieldable damage source. Variables include damage, fire rate, ammo, reload time, range, and which Projectile (if any) it fires.
- **Projectile** — separate from Weapon because one Weapon may fire different Projectiles. Variables include speed, gravity-affected toggle, homing, pierce count, AoE radius, and visual trail.
- **Level** — playable space. Holds its own scoped Actions, environment settings, spawn points, and references to placed Items.
- **NPC** — non-hostile character. Dialogue, vendor, quest-giver, or companion roles. Shares the bone-scaling system with Player.
- **Item/Pickup** — collectible or usable object in-world. Variables include stack size, weight, use-effect, and inventory vs. instant-use flag.
- **Vehicle/Mount** — controllable transport. Variables include seat count, top speed, turn radius, and damage handling.
- **Camera** — defines view behavior. Variables include perspective (first-person, third-person, fixed, side-scroll, top-down), follow target, smoothing, and FOV.
- **UI/HUD** — heads-up display configuration. Variables include which elements are visible (health bar, ammo, minimap, etc.) and their layout.
- **Audio** — music tracks, ambient loops, and SFX banks tied to levels or zones.
- **Effect** — particles, screen shakes, hit flashes. Reusable across Weapons, Enemies, and environment.
- **Construction Piece** — placeable building element. Variables include snap-to-grid toggle, material/texture, destructibility (indestructible, HP-based, instant-break), collision toggle, and stack rules.
- **Trigger Zone** — invisible volume that detects entry/exit. Used as a trigger source for Actions.

### Item Folder Organization

Inside the **Items** folder, sub-folders are auto-organized by type (Players, Enemies, Weapons, etc.) so the user never sees a flat list of hundreds of mixed Items.

---

## Actions

Actions are the engine's logic layer. Every behavior in the game — doors opening, enemies spawning, cutscenes playing — is an Action.

### Action Model

Actions are **strict if/then** entities. They cannot be copied or duplicated. Each Action is a unique, explicit logic unit. This means complex games will accumulate many Actions, so the engine must support strong organization, search, and naming tools (see Action Management below).

### Action Subtypes

When the user creates an Action and clicks create, they classify it by subtype, which determines which effect-specific variables appear:

- Door (open/close)
- Spawn / Despawn
- Cutscene
- Camera Change
- Give Item / Take Item
- Modify Variable (set, add, subtract, multiply)
- Play Sound
- Trigger Effect (particles, screen shake, etc.)
- Teleport
- Damage / Heal
- Lock / Unlock
- Dialogue
- Scene Transition
- Boss Phase

### Triggers and Effects (Multi-Select)

Each Action supports **multiple triggers** and **multiple effects** via multi-select.

**Triggers** are the "when" — what causes the Action to fire:
- Item interaction (button pressed, zone entered, NPC talked to)
- Variable threshold reached
- Item destroyed (enemy killed)
- Timer elapsed
- Game state change

**Effects** are the "what" — what happens when the Action fires:
- All listed effects execute when the Action fires
- Each effect supports an optional **per-effect delay** (in seconds) so multi-step sequences can be staggered without needing a separate chaining system

### Trigger Logic

When an Action has multiple triggers, a slider appears asking **how many triggers must be active** for the Action to fire:
- Slider at 1 = OR logic (any trigger fires the Action)
- Slider at max = AND logic (all triggers must be active simultaneously)
- Anywhere in between = N-of-M logic (e.g., "2 of 3 pressure plates pressed")

This single control covers the full range of trigger-combination logic without a separate AND/OR toggle.

### Numeric (Non-Boolean) Triggers

When a trigger is based on a numeric value rather than a boolean event (e.g., player health, score, enemy count), the user is prompted for:

1. **Threshold value** — the number to compare against
2. **Comparison operator** — dropdown with the following options:
   - `≥` (greater than or equal — inclusive above)
   - `>` (greater than — exclusive above)
   - `≤` (less than or equal — inclusive below)
   - `<` (less than — exclusive below)
   - `=` (equal)
   - `≠` (not equal)

This gives full directional and inclusivity control on every numeric trigger.

### Fire Once vs. Continuously

Each Action has a toggle:
- **Fire once** — Action triggers a single time when its conditions are met, then deactivates (e.g., a button that opens a door once)
- **Fire continuously** — Action re-fires every frame/tick its conditions remain met (e.g., a damage zone that hurts the player as long as they are inside)

### Action Folder Organization

Actions are scoped **per-level by default**. When the user is editing Level 3, only Level 3's Actions appear in dropdowns and lists.

A **Global** Actions folder also exists for cross-level Actions (pause menu, save game, scene transitions, etc.).

Inside a level's Actions folder, the user can create their own sub-folders (e.g., "Combat," "Doors," "Cutscenes") for further manual organization.

### Reusing Actions Across Levels

Because Actions cannot be copied:
- The user can **promote** any per-level Action to the Global folder via right-click, making it available across all levels.
- For Actions that are similar but not identical across levels, the user creates a new Action per level. This is by design — the strict if/then model trades reusability for explicitness.

---

## Creation Flow (Uniform Across All Types)

The creation flow is identical for every Item type and every Action subtype:

1. **Select the type** (Player, Enemy, Door Action, Spawn Action, etc.)
2. **Name the Item or Action** in the naming box
3. **Click Create**
4. **Customization screen opens** with:
   - Type-specific variables on one side (sliders, dropdowns, toggles)
   - Trigger and effect wiring on the other side (for Actions; for Items, this is where the user sees which Actions list this Item as a trigger)

New Items and Actions load with working defaults so they function immediately upon creation.

---

## Item-Action Relationships

Authoring relationships is one-directional, but visibility is two-directional:

- **Authoring** — the user creates an Action and assigns Items as its triggers from the Action's customization screen.
- **Visibility on the Item** — when an Item is assigned as a trigger for an Action, that Action's name appears as a "related" entry on the Item, showing the user which Actions reference this Item.

This means the user always edits relationships from the Action side, but can audit them from either side.

---

## Action Management Features

Because the strict if/then model produces many Actions in complex projects, the engine provides robust tools for managing them:

- **Search** — substring search across Action names within the current scope (level or global)
- **Filter** — filter Actions by subtype, by triggering Item, or by tag
- **Sort options** — alphabetical, by creation date, by subtype, by triggering Item
- **Tags** — Actions (and Items) can be tagged with user-defined labels (e.g., "boss-fight," "tutorial," "ambient") for grouping and filtering
- **Description field** — every Action has an optional one-line description box for the user to leave a note to themselves
- **Naming convention placeholder** — the Action name field shows faint placeholder text suggesting a format like `Level_Trigger_Effect` (e.g., `L3_ButtonA_OpensGate`) to nudge consistent naming without enforcing it
- **"Used by" display** — each Item shows the list of Actions that use it as a trigger; each Action shows the list of Items it references
- **Orphan detection** — Actions that are not triggered by any Item are flagged visually on the main screen
- **Broken link warnings** — if an Item that an Action references is deleted, the Action is flagged visually with a red indicator

---

## Tagging System

Any Item or Action can be tagged with one or more user-defined labels. Tags enable:

- Triggers and effects that reference Items by tag rather than by specific instance ("damage all Items tagged 'flammable'")
- Filtering and grouping in the file browser
- Cross-cutting organization independent of folder structure

---

## Global Variables

The engine maintains a list of user-defined global variables (score, currency, story flags, lives remaining, etc.). Any Action can:

- Read a global variable as a trigger condition (with operator and threshold)
- Modify a global variable as an effect (set, add, subtract, multiply)

Global variables are visible and editable from the main file screen.

---

## Test / Playtest

Every Item and Action customization screen has a **Test** button that drops the user into an instant playtest of just that object. This provides tight feedback loops for tuning sliders without needing to launch the full game.

---

## Summary of Key Design Decisions

- **Two top-level folders**: Items and Actions
- **Items organized by type sub-folders**; **Actions scoped per-level with a Global folder and user-defined sub-folders**
- **Uniform creation flow**: type → name → create → customize, with working defaults
- **Strict if/then Actions**: no copying, no duplication, multi-select for both triggers and effects
- **Trigger count slider**: handles OR, AND, and N-of-M trigger logic in one control
- **Numeric triggers**: threshold value plus comparison operator dropdown (≥, >, ≤, <, =, ≠)
- **Fire once vs. continuously toggle** on every Action
- **Per-effect delays** for staggered sequences without chaining
- **One-directional authoring, two-directional visibility** for Item-Action relationships
- **Heavy investment in Action management**: search, filter, sort, tags, descriptions, naming placeholders, "used by" displays, orphan detection, broken link warnings
- **Tagging and global variables** for cross-cutting logic
- **Test button** on every customization screen for instant playtesting
