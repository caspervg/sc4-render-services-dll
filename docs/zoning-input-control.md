# Zoning Input Control Notes

## Current conclusion

It appears feasible to implement a fully custom zoning view input control in a DLL and inject it into the active 3D view.

This is supported by the public SDK surface in `vendor/gzcom-dll`:

- `cISC4ViewInputControl` defines the required view input control interface.
- `cSC4BaseViewInputControl` provides a reusable base implementation.
- `cISC4View3DWin` exposes `SetCurrentViewInputControl`, `GetCurrentViewInputControl`, and `RemoveCurrentViewInputControl`.
- `SC4UI::GetView3DWin()` already provides access to the active 3D view window.

The current repo already contains working examples of DLL-side custom view input controls:

- `src/sample/camera-view-input`
- `src/sample/road-decal`

## Zoning-specific feasibility

The SDK and reverse-engineering work together appear sufficient to build a real replacement for `cSC4ViewInputControlZone`.

Relevant public interfaces:

- `cISC4ZoneManager`
  - `PlaceZone`
  - `GetZoneType`
  - `GetZoneDragColor`
  - `GetMinZoneSize`
  - `GetMaxZoneSize`
- `cISC4ZoneDeveloper`
  - `HighlightParcels`
  - `DoParcellization`
  - `SetOptions`
- `cISC4City`
  - access to `ZoneManager`
  - access to `ZoneDeveloper`
  - access to `LotManager`
  - access to `Terrain`
  - access to `EffectsManager`

Reverse-engineering coverage for `cSC4ViewInputControlZone` now includes:

- constructor / init / shutdown
- mouse and key handlers
- `ComputeAndMarkSelectedArea`
- `DoCursorUpdate`
- `FloodFillRegion`
- `TestCell`
- `PlaceZone`
- cursor / highlight / flood-fill flow

## Important distinction

There are two different goals:

1. Implement and inject a custom zoning tool.
2. Make the stock zoning UI create that custom tool instead of the Maxis one.

Goal 1 appears feasible with current knowledge and public interfaces.

Goal 2 likely still requires hooking or patching the stock tool activation path, such as the zone tool command or `InvokeZoningTool`, because the stock UI currently instantiates the engine tool.

## Practical implementation shape

A custom DLL-side zoning tool would likely:

- derive from `cSC4BaseViewInputControl`
- use `PickTerrain` and mouse handlers from `cISC4View3DWin`
- use `SetCapture` and `ReleaseCapture` through the base class
- compute its own drag rectangle or flood-filled region
- use `cISC4ZoneDeveloper` for parcel highlighting / parcellization
- use `cISC4ZoneManager::PlaceZone` for final commit
- use `cISC4View3DWin::SetCursorText` and related UI helpers for status text
- optionally recreate the stock `local_tile_outline` effect through `cISC4EffectsManager`

## Current caveat

Pure SDK access appears sufficient for a functional custom zoning tool.

Exact stock parity still depends on how closely we want to mirror:

- the stock highlight/parcellization behavior
- the stock visual effect behavior
- the stock zone button activation flow

## Next research targets

- `cSC4ZoneDeveloper::HighlightParcels`
- `cSC4ZoneDeveloper::DoParcellization`
- helper routines used by those methods in both the Mac and Windows binaries

## ZoneDeveloper behavior notes

The public `cISC4ZoneDeveloper` entry points are thin wrappers.

`HighlightParcels(region, zoneType, point, outRegion)`:

- stores temporary mode state in the developer object
- checks whether the zone type is treated as an RCI-style zone
- clears previous highlight state
- calls `OnZone`
- calls `UpdateHighlightTextures`
- restores temporary mode state

`DoParcellization(region, zoneType, bool)`:

- stores the requested zone type
- checks whether the zone type is treated as an RCI-style zone
- calls `OnDezone` for non-RCI / dezone flow
- otherwise clears previous highlight state and calls `OnZone`
- restores temporary zone-type state

## OnZone pipeline

The heavy zoning behavior lives in `OnZone`, not in the public wrapper.

The Mac symbolized binary shows this sequence:

- `DeferOccupantOperations`
- `CreateParcellizableRegion`
- `CompressRegion`
- `DetermineLotSize`
- optional street extension and jog-street layout
- `SubdivideTheZone`
- per-subregion boundary examination
- either `Parcellize` or `SimpleParcellize`
- optional `ExtractConnectedCells`
- `UpdateZoneType`
- `DeferOccupantOperations(false)`

Important helper roles:

- `CreateParcellizableRegion`
  - builds the candidate cell region used for parcelization
  - calls `DoesCellHaveLot`
- `Parcellize`
  - full parcel-layout path
  - calls `CreateLotRemnants` and `DrawStreet`
- `SimpleParcellize`
  - simplified parcel-layout path
  - also calls `CreateLotToFaceRoad` and `DrawStreet`
- `UpdateHighlightTextures`
  - paints the preview/highlight overlay based on parcel boundaries and zone color

## UpdateHighlightTextures notes

`UpdateHighlightTextures`:

- fetches the zone drag color from `ZoneManager`
- walks parcel boundary data already built by `OnZone`
- chooses boundary textures using per-edge tests
- emits preview/highlight geometry through the developer-owned overlay/effect object

This means the visible parcel highlight is not just a flat color fill. It is driven by parcel-boundary metadata and texture selection.

## SetOptions and slot usage

`SetOptions(bool alternateOrientation, bool placeStreets, bool customZoneSize)` only writes three mode bytes in the developer object.

The VIC steers ZoneDeveloper mostly by:

- calling `SetOptions`
- calling `HighlightParcels`
- calling `DoParcellization`

Confirmed interface slot usage in the stripped Windows binary:

- `spZoneDeveloper->vtable + 0x14` = `HighlightParcels`
- `spZoneDeveloper->vtable + 0x18` = `DoParcellization`
- `spZoneDeveloper->vtable + 0x1C` = `SetOptions`
- `spZoneDeveloper->vtable + 0x20` = `ExistsNetworkOfType`
- `spZoneDeveloper->vtable + 0x24` = `IsCellWater`

Even before the concrete stripped implementation addresses are identified, these slots are already viable hook/interposition points once `spZoneDeveloper` has been initialized during city load.

## ZoneDeveloper address map

Mac anchors:

- `0x000e100c` `cSC4ZoneDeveloper::Init`
- `0x000e0c18` `cSC4ZoneDeveloper::Shutdown`
- `0x000e5daa` `cSC4ZoneDeveloper::HighlightParcels`
- `0x000e5e2e` `cSC4ZoneDeveloper::DoParcellization`
- `0x00647126` `cSC4ZoneDeveloper::SetOptions`
- `0x000dce1c` `cSC4ZoneDeveloper::ExistsNetworkOfType`
- `0x000dacae` `cSC4ZoneDeveloper::IsCellWater`
- `0x000dd426` `cSC4ZoneDeveloper::ClearHighlight`
- `0x000e5668` `cSC4ZoneDeveloper::OnZone`
- `0x000e2a6c` `cSC4ZoneDeveloper::OnDezone`
- `0x000e14a6` `cSC4ZoneDeveloper::UpdateHighlightTextures`
- `0x000e1a36` `cSC4ZoneDeveloper::CreateParcellizableRegion`
- `0x000e1c60` `cSC4ZoneDeveloper::SubdivideTheZone`
- `0x000e365a` `cSC4ZoneDeveloper::Parcellize`
- `0x000e5386` `cSC4ZoneDeveloper::SimpleParcellize`
- `0x000e4310` `cSC4ZoneDeveloper::LayInJogStreets`
- `0x000e5182` `cSC4ZoneDeveloper::ExtendAsStreets`
- `0x000df7ac` `cSC4ZoneDeveloper::DetermineLotSize`
- `0x000dd8d2` `cSC4ZoneDeveloper::ExtractConnectedCells`
- `0x000dc9ce` `cSC4ZoneDeveloper::CompressRegion`
- `0x000db64e` `cSC4ZoneDeveloper::UpdateZoneType`

Windows anchors:

- vtable `0x00ab383c`
- `0x00734030` `cSC4ZoneDeveloper::Init`
- `0x007335a0` `cSC4ZoneDeveloper::Shutdown`
- `0x00733f30` `cSC4ZoneDeveloper::HighlightParcels`
- `0x00733fa0` `cSC4ZoneDeveloper::DoParcellization`
- `0x007347c0` `cSC4ZoneDeveloper::SetOptions`
- `0x0072fe10` `cSC4ZoneDeveloper::ExistsNetworkOfType`
- `0x0072a030` `cSC4ZoneDeveloper::IsCellWater`
- `0x0072c0e0` `ClearHighlight_`
- `0x00733770` `OnZone_`
- `0x0072e6a0` `OnDezone_`
- `0x0072db60` `UpdateHighlightTextures_`
- `0x0072cb90` `CreateParcellizableRegion_`
- `0x0072ec60` `SubdivideTheZone_`
- `0x0072edd0` `Parcellize_`
- `0x0072e9b0` `SimpleParcellize_`
- `0x00730d30` `LayInJogStreets_`
- `0x00730ba0` `ExtendAsStreets_`
- `0x00732bf0` `DetermineLotSize_`
- `0x0072cda0` `ExtractConnectedCells_`
- `0x0072af30` `CompressRegion_`
- `0x0072b180` likely `UpdateZoneType`
- `0x0072a2a0` `DrawStreet`
- `0x00731f60` `ConnectRoadToAnyAdjacentStreets_`
- `0x00732450` `ConnectStreetsAcrossRail_`
- `0x0072fca0` `GetNetwork_`
- `0x0072fe80` edge-connection helper used by street passes

## Private state model

The Windows object layout around the active zoning/parcellization state is now largely clear:

- `+0x18` message server used for begin/end deferred-occupant messages
- `+0x1c` renderer
- `+0x20` building development simulator / lot-pattern source
- `+0x24` city
- `+0x28`, `+0x2c` city width/height
- `+0x30` occupant manager
- `+0x34` terrain
- `+0x3c` traffic sim
- `+0x40` zone manager
- `+0x44`, `+0x48`, `+0x4c` network drawing or network helper services
- `+0x50` overlay/highlight renderer
- `+0x54` cached lot map sized to city width x height
- `+0x74` sim-grid used by boundary-rule logic
- `+0x98` preferred split/orientation flag chosen by `ExamineBoundaries_`
- `+0x99` temporary highlight mode
- `+0x9a` place-streets option
- `+0x9b` alternate-orientation option
- `+0x9c` custom-zone-size option
- `+0xa0` current zone type
- `+0xa4` current working `SC4CellRegion`
- `+0xa8` current focus point for highlight filtering
- `+0xac` .. `+0xb8` four directional boundary scores
- `+0xbc` .. `+0xc8` directional boundary-rule state
- `+0xcc`, `+0xd0` chosen primary and secondary parcel axes
- `+0xd4` texture or highlight base index from renderer state
- `+0xd8` tree/map of parcel-boundary descriptors used by `UpdateHighlightTextures_`

## Street logic summary

Street insertion is layered rather than handled in one place.

1. `DetermineLotSize_` chooses target lot dimensions and stores them in the developer object.
2. `ExtendAsStreets_` looks just outside the candidate region for compatible road, avenue, street, rail-adjacent street, and viaduct-style networks and pulls those alignments inward.
3. `LayInJogStreets_` optionally places boundary streets along edges when there is a single clean connection on the outside of an edge and no conflicting external network on that edge.
4. `Parcellize_` and `SimpleParcellize_` may then insert internal streets based on the selected parcel axis, current lot size, and the `placeStreets` option.
5. `DrawStreet` emits the actual network segments and also handles clipped ranges, single-tile cases, and endpoint filling when the caller requested a hard boundary pass.

The important consequence is that "streets in zoning" are not only decorative preview lines. They change the actual subdivision result before lot placement occurs.

## CreateParcellizableRegion notes

`CreateParcellizableRegion_` produces the real candidate mask used by all later steps.

Cells survive only if they pass all of the following:

- they are inside the selected region
- they are not blocked by network flags `0x1f4f`, unless the current zone type is `0`
- `IsCellWater` returns false
- either highlight mode is active, or `DoesCellHaveLot_` accepts the cell
- existing lot configuration is compatible with the requested zone type

This is why the visible dragged rectangle and the parcelizable footprint can diverge before highlighting is even generated.

## Boundary and orientation notes

`ExamineBoundaries_` scans just outside the working region and scores all four directions. It then chooses:

- the primary split axis
- the secondary split axis
- whether the region should be treated as horizontally or vertically favored

Two important modifiers apply:

- low-density RCI zones with `placeStreets` enabled may invert the preferred orientation for large enough regions
- `alternateOrientation` swaps the chosen primary and secondary axes and flips the orientation flag

This means `SetOptions(alternateOrientation, placeStreets, customZoneSize)` is not superficial; it materially changes the geometry that `Parcellize_` will generate.

## Zone type category map

Useful higher-level grouping for the raw zone type values:

- category `0` = Residential
  - raw types `0x01` to `0x03`
- category `1` = Commercial
  - raw types `0x04` to `0x06`
- category `2` = Industrial
  - raw types `0x07` to `0x09`
- category `3` = Plopped
  - raw type `0x0F`
- category `4` = None
  - raw type `0x00`
- category `5` = Other special zones
  - raw types `0x0A` to `0x0E`
  - Military, Airport, Seaport, Spaceport, Landfill

This grouping matches the control-flow splits seen in the ZoneDeveloper routines:

- `DoParcellization` treats RCI-like types differently from non-RCI types.
- `OnZone` uses the raw zone type to choose between `Parcellize_` and `SimpleParcellize_`.
- `UpdateHighlightTextures_` suppresses the normal parcel-edge logic for raw types `0x07` to `0x09`, which lines up with industrial behaving differently in preview generation.
- `OnDezone_` has special handling for raw types `0x0B` and `0x0C`, which fall into the "Other special zones" bucket.

The main practical takeaway is that the engine does not have just one "zone parcelization" algorithm. It has at least three behavioral families:

- standard R/C parcelization
- industrial / special-case parcelization and highlighting
- dezone / special-zone cleanup

## Category-based algorithm view

With the higher-level category map applied, the internal behavior reads more cleanly:

- category `0` Residential and category `1` Commercial:
  - use the main RCI parcelization path
  - favor `Parcellize_` for normal zoning
  - use parcel-edge-driven highlight generation
- category `2` Industrial:
  - still uses zoning/parcellization flow
  - but highlight and parcel logic diverge from R/C
  - this matches the special handling seen for raw types `0x07` to `0x09`
- category `5` Other special zones:
  - can pass through some zoning infrastructure
  - but are closer to special-case cleanup and simplified handling than to R/C parcel growth
- category `3` Plopped:
  - outside the normal zoning parcelization family
- category `4` None:
  - sentinel used by dezone / no-zone behavior

Category-oriented pseudocode:

```text
GetZoneCategory(rawZoneType):
  if rawZoneType in [0x01, 0x02, 0x03]:
    return Residential
  if rawZoneType in [0x04, 0x05, 0x06]:
    return Commercial
  if rawZoneType in [0x07, 0x08, 0x09]:
    return Industrial
  if rawZoneType == 0x0F:
    return Plopped
  if rawZoneType == 0x00:
    return None
  return OtherSpecial
```

```text
HighlightParcels(region, rawZoneType, focusPoint, outRegion):
  category = GetZoneCategory(rawZoneType)

  save temporary state
  currentZoneType = rawZoneType
  currentFocusPoint = focusPoint
  tempHighlightMode = false

  if category is Residential or Commercial or Industrial:
    ClearHighlight()
    OnZone(region, outRegion)
    UpdateHighlightTextures(region)
  else:
    no full parcel-highlight build

  restore temporary state
```

```text
DoParcellization(region, rawZoneType, allowNetworkCleanup):
  category = GetZoneCategory(rawZoneType)
  currentZoneType = rawZoneType

  if category is None or OtherSpecial:
    OnDezone(region, allowNetworkCleanup)
  else if category is Residential or Commercial or Industrial:
    ClearHighlight()
    OnZone(region, null)
  else:
    do nothing or use separate non-zoning behavior

  currentZoneType = 0
```

```text
OnZone(region, outRegion):
  category = GetZoneCategory(currentZoneType)

  if customZoneSize:
    create a road-facing lot directly
    return

  defer occupant operations = true

  working = CreateParcellizableRegion(region)
  CompressRegion(working)
  DetermineLotSize(working)

  if placeStreets:
    ExtendAsStreets and LayInJogStreets around working

  subregions = SubdivideTheZone(working)

  for each subregion:
    currentWorkingRegion = subregion
    ExamineBoundaries()

    if focusPoint filtering rejects this subregion:
      subtract subregion from outRegion
      continue

    if category is Residential or Commercial:
      Parcellize_()
    else if category is Industrial:
      SimpleParcellize_(16)
    else if category is None:
      SimpleParcellize_(6)
    else:
      SimpleParcellize_(6)

  if focusPoint exists and outRegion exists:
    outRegion = connected component under focusPoint

  if tempHighlightMode:
    UpdateZoneType(region)

  defer occupant operations = false
```

```text
UpdateHighlightTextures(region):
  category = GetZoneCategory(currentZoneType)
  color = ZoneManager.GetZoneDragColor(currentZoneType)

  for each parcel descriptor built during OnZone:
    if category is Residential or Commercial:
      compute per-edge boundary textures against adjacent parcels
    else if category is Industrial:
      use simplified / special industrial boundary behavior
    draw overlay geometry with selected texture set and drag color

  for each selected cell bordering a foreign lot:
    if cell is not blocked by zoning-network flags:
      draw neighbor-boundary highlight
```

```text
OnDezone(region, allowNetworkCleanup):
  for each selected cell:
    lot = lookup lot at cell
    if no lot:
      continue

    category = GetZoneCategory(lot.zoneType)
    if category is Residential or Commercial or Industrial:
      remove lot
      CreateLotRemnants()
    else if lot.zoneType is one of the handled special raw values:
      remove lot
      CreateLotRemnants()

    if allowNetworkCleanup and cell has transport flags 0x808 and not 0x1747:
      add cell to demolition mask

  if demolition mask not empty:
    invoke demolition service
```

These category-based forms are intentionally slightly higher level than the raw decompile. They are a better representation of the engine's real policy decisions.

## DrawStreet, preview mode, and the real network path

One correction to the earlier working model is important:

- `cSC4ZoneDeveloper::DrawStreet` at `0x0072A2A0` does **not** directly choose a network type by itself.
- The byte at `this+0x99` is **not** a network ID. It is a preview/commit mode flag.

### Confirmed mode behavior

- `cSC4ZoneDeveloper::HighlightParcels` at `0x00733F30` sets:
  - `this+0xA8 = focusPoint`
  - `this+0x99 = 0`
  - `this+0xA0 = zoneType`
  - then runs `ClearHighlight_`, `OnZone_`, and `UpdateHighlightTextures_`
  - and finally restores `this+0x99 = 1`
- `cSC4ZoneDeveloper::DoParcellization` at `0x00733FA0` sets `this+0xA0 = zoneType`, then runs `ClearHighlight_` and `OnZone_` or `OnDezone_`, but does **not** touch `this+0x99`
- The constructor `FUN_007348D0` initializes `this+0x99 = 1`

So the intended interpretation is:

- `this+0x99 == 0`: preview/highlight zoning pass
- `this+0x99 == 1`: normal placement/commit-side zoning pass

### What DrawStreet actually does

`DrawStreet` walks a line between two cells, clips that path against the current parcelizable mask in `this+0xA4`, and then delegates the actual low-level work to the object at `this+0x48`:

```text
DrawStreet(x1, y1, x2, y2, fillEndpoints):
  if the path does not intersect the current allowed region:
    return

  for each contiguous allowed run along the line:
    call (this+0x48)->vfunc_1C(
      startX, startY, endX, endY,
      0x0C49,
      this+0x48,
      this+0x99,
      -1)
```

That means `DrawStreet` is a corridor/path emitter, not the policy layer that decides which network family is being emitted.

### Real preview vs commit split

The street path is used in both preview and commit:

- Preview:
  - `HighlightParcels -> OnZone_ -> ExtendAsStreets_/LayInJogStreets_/Parcellize_/SimpleParcellize_ -> DrawStreet -> (this+0x48)->vfunc_1C(..., mode=0, ...)`
- Commit:
  - `DoParcellization -> OnZone_ -> ExtendAsStreets_/LayInJogStreets_/Parcellize_/SimpleParcellize_ -> DrawStreet -> (this+0x48)->vfunc_1C(..., mode=1, ...)`

`UpdateHighlightTextures_` is separate. It is the parcel/overlay highlight renderer, not the internal-street path emitter.

`ClearHighlight_` also supports this split:

- it clears parcel highlight sets
- then calls `(this+0x48)->vfunc_14()`
- then calls `(this+0x4C)->vfunc_14()`
- then clears the texture renderer at `this+0x38`

So the low-level network preview/build backend is tied to `this+0x48` and `this+0x4C`, while overlay parcel textures are handled elsewhere.

### Consequence for modding

This invalidates the earlier idea of writing a chosen network ID into `this+0x99`.

The correct next target is:

- identify the class/service stored at `this+0x48`
- identify what object state or argument actually selects the network family
- trace how `ExtendAsStreets_`, `LayInJogStreets_`, `Parcellize_`, and `SimpleParcellize_` decide which network family to request before they call `DrawStreet`

### Street-helper observations already visible

The helper functions already show that network-family policy exists above `DrawStreet`:

- `ExtendAsStreets_` and `LayInJogStreets_` use `param_2` plus `DAT_00AB3680[param_2]` as a network-family mask
- they probe adjacency with `GetNetwork_` and directional checks
- `LayInJogStreets_` has explicit family-specific checks for masks such as:
  - `0x40`
  - `0x1`
  - `0x400`
  - `0x8`
  - `0x800`
- in preview mode (`this+0x99 == 0`) it also consults the lower services at `this+0x48` and `this+0x44` through vfunc `+0x24` to ask whether preview connectivity already exists

Inference:

- `DrawStreet` is downstream of the real family choice
- the actual network-family selector likely lives in helper-side family/mask logic, or in lower state owned by the `this+0x48` backend, not in `DrawStreet` itself

### Helper family codes now confirmed

`OnZone_` does not pass a user-selected network ID into `DrawStreet`. Instead, when internal streets are enabled, it invokes the helper stages in this hardcoded order:

- `ExtendAsStreets_(..., 0x0B)`
- `ExtendAsStreets_(..., 3)`
- `ExtendAsStreets_(..., 10)`
- `ExtendAsStreets_(..., 0)`
- `ExtendAsStreets_(..., 6)`

and then, for sufficiently large regions:

- `LayInJogStreets_(..., 0x0B)`
- `LayInJogStreets_(..., 3)`
- `LayInJogStreets_(..., 10)`
- `LayInJogStreets_(..., 0)`
- `LayInJogStreets_(..., 6)`

The helper argument is used as an index into the mask table at `0x00AB3680`, which expands as powers of two:

```text
index 0  -> 0x00000001
index 1  -> 0x00000002
index 2  -> 0x00000004
index 3  -> 0x00000008
index 4  -> 0x00000010
index 5  -> 0x00000020
index 6  -> 0x00000040
index 7  -> 0x00000080
index 8  -> 0x00000100
index 9  -> 0x00000200
index 10 -> 0x00000400
index 11 -> 0x00000800
```

So the family codes currently confirmed in zoning are:

- family `0`  -> mask `0x00000001`
- family `3`  -> mask `0x00000008`
- family `6`  -> mask `0x00000040`
- family `10` -> mask `0x00000400`
- family `11` -> mask `0x00000800`

### What LayInJogStreets_ proves

`LayInJogStreets_` makes the family resolution very explicit. It checks for surrounding network occupancy using the masks above, then resolves the concrete network object with `GetNetwork_(x, y, mask)`, and finally asks that object whether the relevant side is compatible via vfunc `+0xBC`.

The currently visible family-specific checks are:

- mask `0x40` with side tests using family code `6`
- mask `0x1` with side tests using family code `0`
- mask `0x400` with side tests using family code `10`
- mask `0x8` with side tests using family code `3`
- mask `0x800` with side tests using family code `0x0B`

This is strong evidence that the real internal-street family is decided by the helper stage and adjacency policy, not by a single mutable byte near `DrawStreet`.

### Family-specific backend objects at +0x44 / +0x48 / +0x4C

The next layer below the helper functions is now partially visible.

`ConnectRoadToAnyAdjacentStreets_` and `ConnectStreetsAcrossRail_` both choose one of three backend objects before calling the common low-level line emitter at vfunc `+0x1C`:

- `this+0x44`
- `this+0x48`
- `this+0x4C`

Those backends are selected from neighboring network family information, not from `DrawStreet` itself.

#### Evidence from ConnectRoadToAnyAdjacentStreets_

`ConnectRoadToAnyAdjacentStreets_` examines the neighboring network occupant and tests its family via vfunc `+0x58`:

- if the neighbor reports family `0` or `10`, it chooses backend `this+0x4C`
- else if the neighbor reports family `3`, it chooses backend `this+0x48`
- else if the neighbor reports family `0x0B`, it chooses backend `this+0x44`

Then it emits the connecting segment by calling:

```text
(selectedBackend)->vfunc_1C(x1, y1, x2, y2, 0, 0, 1, -1)
```

This is a very strong indication that the backend object itself is family-specific.

#### Evidence from ConnectStreetsAcrossRail_

`ConnectStreetsAcrossRail_` uses the same split:

- if the far-side network is family `3`, use `this+0x48`
- if the far-side network is family `0x0B`, use `this+0x44`

and then emits the connecting segment through that backend's vfunc `+0x1C`.

#### Current best interpretation

The most defensible model now is:

- `this+0x44` = family-specific line emitter/backend for family `0x0B`
- `this+0x48` = family-specific line emitter/backend for family `3`
- `this+0x4C` = family-specific line emitter/backend for family `0` and/or `10`

This does not yet prove the exact human-readable family names, but it does show that the concrete emitted network is probably controlled by **which backend object is chosen**, not by a single integer written just before `DrawStreet`.

### What meth_0x72fe80 adds

`meth_0x72fe80` confirms the selection logic above `DrawStreet`:

- it maps edge index `0..3` into a directional side code
- it resolves a real placed network with `GetNetwork_(x, y, mask)`
- it asks the returned network occupant whether that side is compatible using vfunc `+0xBC(direction, familyCode)`
- only if there is no real network and we are in preview mode (`this+0x99 == 0`) does it fall back to the preview/query backends at `this+0x48` and `this+0x44` via vfunc `+0x24`

So there are really two different concepts:

- helper-side family selection and side compatibility
- backend object selection for actual segment emission

### Current hypothesis about "where the network type is encoded"

The concrete network type for zoning streets is likely encoded by one or both of:

- the helper family code sequence used by `OnZone_`
- the chosen backend object among `this+0x44`, `this+0x48`, and `this+0x4C`

It is increasingly unlikely that there is one standalone `currentInternalNetworkType` byte analogous to the earlier mistaken `this+0x99` theory.

## What cSC4NetworkManager::Init adds

The Mac `cSC4NetworkManager::Init()` decompile gives a strong anchor for the network-tool namespace that zoning is using.

It constructs and stores a family of tool objects in this order:

```text
offset +0x8C  -> cSC4NetworkTool(0)
offset +0x90  -> cSC4NetworkTool(1)
offset +0x94  -> cSC4NetworkTool(2)
offset +0x98  -> cSC4NetworkTool(3)
offset +0x9C  -> cSC4PipeTool()
offset +0xA0  -> cSC4PowerLineTool()
offset +0xA4  -> cSC4NetworkTool(6)
offset +0xA8  -> cSC4SubwayTool()
offset +0xAC  -> cSC4NetworkTool(8)
offset +0xB0  -> cSC4NetworkTool(9)
offset +0xB4  -> cSC4NetworkTool(10)
offset +0xB8  -> cSC4NetworkTool(11)
offset +0xBC  -> cSC4NetworkTool(12)
```

So the engine definitely has a network-tool family parameterized by a compact integer type code, and the zoning helper codes now line up with real constructed tool variants:

- zoning family `0`
- zoning family `3`
- zoning family `6`
- zoning family `10`
- zoning family `11`

Those are not arbitrary constants. They correspond to actual tool instances the network manager creates.

### What this likely means for zoning

This makes two models plausible:

1. `cSC4ZoneDeveloper` holds direct references to a subset of the `cSC4NetworkManager` tool instances.
2. `cSC4ZoneDeveloper` holds its own smaller set of wrappers/adapters that delegate into those network-tool instances.

The second model still fits the current Windows evidence slightly better, because zoning only shows three lower backend pointers:

- `this+0x44`
- `this+0x48`
- `this+0x4C`

while `OnZone_` and the street helpers operate over at least five family codes:

- `0`
- `3`
- `6`
- `10`
- `11`

So the most likely structure is:

- helper-side family/mask logic works in the full network-family namespace
- zoning then maps those families onto a smaller set of lower preview/build backends
- those backends are either preconfigured `cSC4NetworkTool`-like instances or adapters over them

### Why the family grouping matters

In Windows, `ConnectRoadToAnyAdjacentStreets_` groups the families this way:

- family `0` or `10` -> backend `this+0x4C`
- family `3` -> backend `this+0x48`
- family `0x0B` -> backend `this+0x44`

With the `cSC4NetworkManager::Init()` evidence, that grouping now looks less like random magic numbers and more like a zoning-specific reduction from the network-manager tool set into a few backend categories.

That still leaves open whether:

- family `10` is drawn through the same backend object as family `0` but with different internal state
- or family `10` is only used for side-compatibility/orientation and not for a distinct emitted segment type in zoning

Either way, the network-manager constructor table is now a strong reason to stop treating the zoning family codes as guesses. They are part of the game's real network-tool namespace.

## Final backend result

The most useful practical result from the live Windows experiments is now clear:

- `cSC4ZoneDeveloper +0x48` is the main emitted internal-network backend worth targeting.
- Repointing `+0x48` changes what zoning draws internally.
- `+0x44` still matters for other family/connection cases, but `+0x48` is the primary modding leverage point for the visible in-zone network.

### What the live tool objects are

Using the Windows binary and the in-game debug panel:

- `+0x44` is a private `cSC4NetworkTool` instance with tool type `11`
- `+0x48` is a private `cSC4NetworkTool` instance with tool type `3`
- `+0x4C` should not currently be assumed to be a plain `cSC4NetworkTool` in every state

The important correction here is:

- the `ZoneDeveloper` backend objects are not just aliases of `NetworkManager::GetNetworkTool(type, false)`
- they are private tool instances or equivalent private backend objects

### Shared tools vs fresh tools

Two direct experiments were run:

1. assign `NetworkManager::GetNetworkTool(type, false)` into the zoning backend slots
2. assign a freshly constructed `cSC4NetworkTool(type)` into the zoning backend slots

Result:

- shared `NetworkManager` tools crashed when used as zoning backends
- a raw freshly constructed tool also crashed if only constructor state was used

That means matching vtable and matching tool type are not enough by themselves.

### Required lifecycle for fresh tools

The working result came from this sequence:

1. `cSC4NetworkTool(type)` constructor
2. `Init()`
3. `Reset()`
4. assign the resulting tool to `cSC4ZoneDeveloper +0x48`

With that lifecycle, the fresh tool worked as a replacement backend for `+0x48`, provided the selected tool type was one of the safe network tools.

So the practical modding rule is:

```text
fresh zoning backend tool = ctor(type) + Init() + Reset()
```

Constructor-only and shared-manager-tool substitution are not sufficient.

### Confirmed tool-type observations

The currently confirmed tool IDs from live testing are:

- `0` = road
- `1` = rail
- `2` = elevated highway
- `3` = street
- `4` = pipe tool
- `5` = powerline tool
- `6` = avenue
- `7` = subway tool
- `8` = elevated rail
- `9` = monorail
- `10` = one-way road
- `11` = likely ANT / RHW
- `12` = ground highway

Tool IDs above `12` should currently be treated as invalid for this path.

### Practical implication for the DLL

The cleanest current implementation strategy is:

- let the zoning DLL choose a desired internal network tool type
- construct a fresh `cSC4NetworkTool(type)`
- call `Init()`
- call `Reset()`
- store it into `cSC4ZoneDeveloper +0x48`

That is now a demonstrated working path for changing the internal network drawn by zoning, as long as the chosen tool type is one of the compatible surface-network tools.

## Intersection placement from a DLL

The Windows `cSC4ViewInputControlNetworkIntxTool` path is now mapped well enough to reproduce intersection placement from a DLL without going through the full stock VIC.

### Windows function overview

Strong Windows candidates confirmed from the stripped binary:

- `0x00660CA0` `cSC4ViewInputControlNetworkIntxTool::Init`
- `0x00660D30` `cSC4ViewInputControlNetworkIntxTool::Shutdown`
- `0x00660E80` `cSC4ViewInputControlNetworkIntxTool::OnKeyDown`
- `0x00661010` `cSC4ViewInputControlNetworkIntxTool::OnMouseDownL`
- `0x00660C30` `cSC4ViewInputControlNetworkIntxTool::OnMouseUpL`
- `0x00661260` `cSC4ViewInputControlNetworkIntxTool::OnMouseMove`
- `0x00625380` `cSC4NetworkTool::GetIntersectionRule`
- `0x006097E0` `nSC4Networks::cIntRule::DoAutoComplete`
- `0x0062CD50` `cSC4NetworkTool::InsertIsolatedHighwayIntersection`
- `0x0062C4B0` `cSC4NetworkTool::InsertHighwayIntersectionPieces`

### Placement algorithm

The stock click path is:

1. `city->GetNetworkManager()`
2. `GetNetworkTool(2, false)`
3. `tool->Init()`
4. `tool->Reset()`
5. `GetIntersectionRule(ruleId)`
6. `DoAutoComplete(rule, x, z, tool)`
7. `InsertIsolatedHighwayIntersection(tool, x, z, ruleId, commit)`

This was implemented in the sample DLL as `PlaceIntersectionByRuleId(...)`.

### Important ownership/lifetime finding

The helper originally crashed on commit even though:

- `DoAutoComplete(...)` returned
- `InsertIsolatedHighwayIntersection(..., commit=1)` returned success
- `GetCostOfSolution()` returned a valid cost

Step-by-step logging showed the crash happened immediately after that at `tool->Release()`.

So for this path:

- `GetNetworkTool(2, false)` must currently be treated as a borrowed/shared tool
- calling `Release()` on it after placement is wrong in this usage

The working rule is:

```text
GetNetworkTool(type, false) in the intersection-placement path:
  borrow it
  use it
  do not Release() it in the DLL helper
```

### Practical implication

It is now possible to place an intersection from a DLL by rule ID with the stock solver path, as long as the helper:

- uses network tool type `2`
- runs `Init()` and `Reset()`
- runs `DoAutoComplete(...)`
- calls `InsertIsolatedHighwayIntersection(...)`
- does not `Release()` the borrowed shared tool returned by `GetNetworkTool(2, false)`
