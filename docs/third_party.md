# TropoLink — Third-Party Components

TropoLink is fully offline-capable; none of these components phones home. No
telemetry anywhere in the product. The only outbound traffic that can ever occur is
operator-initiated map/terrain fetching (see *Optional online map services* below),
and it does not exist at all in the Air-Gap flavor.

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

## Optional online map services

The default basemap is rendered locally from the loaded DEMs and needs no network.
The operator may instead select an online raster tile source, or build an offline
`.mbtiles` pack from one. Requests are sent only while such a source is selected or
a download is running; tiles are cached on disk so an area is fetched once.

| Service | Endpoint | Terms | Attribution shown in-app |
|---|---|---|---|
| OpenTopoMap | `{s}.tile.opentopomap.org` | Tiles CC-BY-SA; data ODbL. Non-commercial, low-volume use per the project's tile-usage policy | © OpenStreetMap contributors, SRTM \| style © OpenTopoMap (CC-BY-SA) |
| OpenStreetMap standard | `tile.openstreetmap.org` | OSMF Tile Usage Policy; data ODbL | © OpenStreetMap contributors |
| AWS Terrain Tiles (SRTM) | `elevation-tiles-prod.s3.amazonaws.com` | Public-domain NASA SRTM, hosted on the AWS Open Data registry | Marked *downloaded* in the terrain store |

Compliance measures: an identifying `User-Agent`, at most 4–6 concurrent requests,
paced dispatch (≥ 40 ms) while downloading, a 30,000-tile ceiling per download job,
a persistent disk cache so tiles are never re-fetched, and mandatory on-map
attribution for whichever source is active. **These endpoints are compiled out of the
Air-Gap flavor** — the Air-Gap binary contains no tile URL and no HTTP code (verified
by scanning the linked executable for remote endpoints in both ASCII and UTF-16).

## Optional local AI

The report commentary talks to a **local Ollama instance on 127.0.0.1 only** (the
host is hard-coded; no remote endpoint exists in the source). Absent from the
Air-Gap flavor entirely.
