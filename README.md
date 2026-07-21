# TropoLink

**Made by [LukiBox](https://github.com/LukiBox)** · Apache-2.0

A professional desktop instrument for the design of **troposcatter (tropospheric
scatter) radio links**. Drop two pins on an offline map (or type DD / DMS / MGRS /
UTM), enter the radio parameters, and receive a complete, defensible link design:
path geometry over real terrain, common-volume geometry, free-space and troposcatter
losses from **multiple published models side by side**, full link budget, fade
margin, diversity gain, predicted availability — finished with a formal bilingual
(PL/EN) PDF link-design report carrying a reproducible SHA-256 content hash.

Where models disagree — and troposcatter models famously disagree by several dB —
TropoLink shows the disagreement honestly as the design uncertainty instead of
hiding it behind one figure.

**Fully offline.** No telemetry, no phone-home, ever. An Air-Gap build flavor
compiles the optional SRTM downloader and local-AI commentary out entirely.

## Models

| Model | Role | Source |
|---|---|---|
| FSPL | reference baseline | ITU-R P.525-4 |
| **ITU-R P.617-5** | primary | Rec. P.617-5 (08/2019) §4, with the integral N0/ΔN digital maps |
| **NBS TN101** | classic sanity anchor | Tech Note 101 (1967) Ch. 9–10, computerized per the ITS Algorithm (1982) |
| **ITM (Longley-Rice)** | independent cross-check | official NTIA public-domain C++, P2P mode over the real profile |

Aperture-to-medium coupling loss (P.617-5 eq. 3) is applied explicitly and shown as
its own line. Validity envelopes are enforced: outside its envelope a model says
*out of validity range* rather than extrapolating. Every displayed value carries its
formula/source on hover; `docs/model_references.md` cites every equation.

## Maps

Three basemap layers, chosen from the **Basemap** selector on the map bar. Whatever
is selected, the link line, common volume, horizon fans and MGRS grid draw on top.

| Layer | Needs network | What it is |
|---|---|---|
| **Terrain (offline)** — default | never | A topographic sheet rendered from the loaded DEMs themselves: hypsometric tints, Horn relief shading, and elevation contours with labelled index contours (every 5th). Contour interval follows the zoom (200 m → 5 m). |
| **OpenTopoMap / OSM** | first view only | Online XYZ raster tiles, cached permanently on disk — an area browsed once stays available offline. Standard flavor only. |
| **MBTiles pack** | never | Any standard raster `.mbtiles` (yours, one from MOBAC/QGIS, or one built by the in-app downloader). Over-zooms gracefully past the pack's max zoom. |

**Download offline maps…** (map bar, standard flavor) packs an area — the current
view or a corridor along the A–B path — into a `.mbtiles` file over a chosen zoom
range, with a live tile-count/size estimate, a 30,000-tile ceiling, polite paced
requests, and reuse of already-cached tiles. The finished pack becomes the active
basemap and can be copied to an air-gapped machine.

The **?** button on every panel header opens a detailed bilingual guide for that
panel; the **Help** toolbar button opens the full manual (14 topics, PL/EN).

## Building (Windows-first, core portable)

Prerequisites: Visual Studio 2022 Build Tools (MSVC v143 + Windows 11 SDK),
CMake ≥ 3.24, Ninja, Qt 6.8.x (`msvc2022_64`), vcpkg at `C:\vcpkg`.

```powershell
# vcpkg deps (manifest mode installs automatically at configure)
cmake --preset windows-release        # standard flavor
cmake --build build\windows-release
ctest --test-dir build\windows-release

cmake --preset windows-airgap         # Air-Gap flavor (no network code at all)
cmake --build build\windows-airgap

C:\Qt\6.8.3\msvc2022_64\bin\windeployqt --qmldir ui\qml build\windows-release\bin\TropoLink.exe
```

Non-ASCII checkout paths: build through an ASCII junction
(`mklink /J C:\dev\TropoLink <checkout>`) — several Windows toolchain components
(rc.exe env, gtest discovery, qmlimportscanner) still mishandle such paths.

### Headless report / reproducibility check

```powershell
TropoLink.exe --report out.pdf --lang pl
```

loads the reference project, computes, writes the PDF and logs the SHA-256 content
hash (identical runs produce identical hashes).

## Repository layout

```
app/           main(), QML bootstrap, i18n, headless report mode
core/          pure C++20, no Qt UI types, unit-typed quantities
  geo/         GeographicLib wrappers, DD/DMS/MGRS/UTM, k-factor, ITU maps
  terrain/     GDAL ingestion, terrain store, profile extraction, horizons
  tropo/       scatter geometry, P.617, TN101, ITM, coupling, statistics
  budget/      link budget, availability, diversity, design solver
  project/     .tlk files (JSON-in-zip), KML/CSV export
  report/      bilingual report content + SHA-256 hash
  ai/          optional loopback-only Ollama client (absent in Air-Gap)
ui/            Qt Quick UI: tile map (paper-topo DEM rendering, MBTiles, optional
               online tiles + downloader), scene-graph profile view with the
               common-volume lens, panels, bilingual help, PDF renderer
resources/     ITU N0/ΔN maps, modulation & equipment libraries, bundled DTED-0
tools/         make_dted — derive spec-correct DTED 0/1/2 from any GDAL DEM;
               render_topo_preview — offscreen map-cartography preview (dev aid)
tests/         GoogleTest suite (51 tests): geodesy, terrain, tropo, budget, project
docs/          model_references.md (every equation cited), third_party.md
```

## Reference scenario

Preloaded on first run: Site A 51.50609699 N 15.33150851 E, Site B 52.43470597 N
15.21931198 E, both 4 m AGL; 4.4 GHz, 500 W (57.0 dBm), 39.1 dBi both ends.
Pinned in tests: distance ≈ 103.5 km, FSPL ≈ 145.6 dB, azimuths ≈ 355.8°/175.7°.
Bundled SRTM-derived DTED-0 covers the path, so the full demo — terrain profile,
lens, model table, budget, availability, PDF — works with the cable unplugged.

## Author & licence

**TropoLink is made by [LukiBox](https://github.com/LukiBox).**

Licensed under the **Apache License 2.0** — see [LICENSE](LICENSE). Third-party
components keep their own licences; [NOTICE](NOTICE) and
[docs/third_party.md](docs/third_party.md) list every one, with linkage and terms.
Note in particular that **Qt 6 is used under LGPL v3 and linked dynamically**, so
the Qt DLLs ship beside the executable and remain replaceable, as LGPL v3 §4
requires.
