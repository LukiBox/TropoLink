# TropoLink — Third-Party Components

TropoLink is fully offline-capable; none of these components phones home. No
telemetry anywhere in the product.

| Component | Version | License | Linkage | Use |
|---|---|---|---|---|
| Qt 6 (Core, Gui, Quick, QuickControls2, Svg, Network*) | 6.8.x LTS | **LGPL v3** | **dynamic** (LGPL compliance) | UI, scene graph, PDF writer (QPdfWriter), i18n |
| GDAL | 3.x (vcpkg) | MIT/X | dynamic | DTED / SRTM HGT / GeoTIFF ingestion, DTED derivation tool |
| GeographicLib | 2.x (vcpkg) | MIT | dynamic | WGS-84 geodesics (Karney), MGRS/UTM |
| NTIA ITM (Longley-Rice) | v1.x | **Public domain** (US Government work) | static, vendored unmodified in `third_party/ntia_itm` | ITM cross-check model; computerized TN101 F(θd)/H0/variability functions |
| nlohmann/json | 3.x (vcpkg) | MIT | header-only | project files, libraries, terrain index |
| libzip | 1.x (vcpkg) | BSD-3 | dynamic | `.tlk` container (JSON-in-zip) |
| SQLite | 3.x (vcpkg) | Public domain | dynamic | MBTiles basemap reading |
| zlib | 1.x (vcpkg) | zlib | dynamic | SRTM downloader gunzip (non-air-gap flavor only) |
| GoogleTest | 1.x (vcpkg) | BSD-3 | dev only | unit suite |

\* Qt Network is linked only in the standard flavor; the Air-Gap flavor compiles the
downloader and the AI client out entirely.

## Qt LGPL note

Qt is used under LGPL v3 with **dynamic linking** (`windeployqt` ships the Qt DLLs
beside the executable). No Qt sources are modified. Re-linking against a different
Qt build is possible by replacing those DLLs — the LGPL§4 requirement.

## Bundled data

| Data | Source | Terms |
|---|---|---|
| `resources/data/N050.TXT`, `DN50.TXT` | ITU-R P.452-17 supplement (integral digital products of ITU-R P.617-5) | ITU-R Recommendation data files, redistributed as required for implementing the Recommendation |
| `resources/terrain/*.dt0` | Derived from NASA SRTM 1-arc-second (public domain) via `tools/make_dted`; provenance marked in-app | Public domain source; derivation documented |
| `resources/data/modulations.json`, `equipment.json` | TropoLink originals (generic engineering placeholder values) | project license |

## Optional local AI

The report commentary talks to a **local Ollama instance on 127.0.0.1 only** (the
host is hard-coded; no remote endpoint exists in the source). Absent from the
Air-Gap flavor entirely.
