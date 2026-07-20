# TropoLink — Model References

Every displayed number traces to a published method. This document lists every
equation implemented in the core, with its source and section. Where models disagree,
TropoLink shows the disagreement as the design uncertainty; it never averages it away.

## 1. Geodesy (`core/geo/`)

| Quantity | Method | Source |
|---|---|---|
| Distance, azimuths | WGS-84 geodesic inverse/direct (Karney's method) | C. F. F. Karney, *Algorithms for geodesics*, J. Geodesy 87, 43–55 (2013); GeographicLib 2.x |
| MGRS / UTM conversion | GeographicLib `MGRS` / `UTMUPS` | NGA.STND.0037 (MGRS); GeographicLib |
| Effective earth radius factor | k = 157 / (157 − ΔN) | ITU-R P.452 (ΔN positive-lapse convention) |
| ΔN from Ns | ΔN = 7.32 exp(0.005577 Ns) | NBS TN101, eq. (4.3) (sign per P.452) |
| Exponential atmosphere | N(h) = N0 exp(−h/hb), hb = 7.35 km | ITU-R P.617-5 §4.1 Step 4; P.453 |
| N0, ΔN digital maps | N050.TXT / DN50.TXT, 1.5° grid, bilinear | ITU-R P.452-17 supplement; integral part of P.617-5 (§2, Table 1) |

## 2. Terrain (`core/terrain/`)

| Quantity | Method | Source |
|---|---|---|
| Elevation sampling | Bilinear on DTED 0/1/2, SRTM HGT, GeoTIFF | GDAL; MIL-PRF-89020B (DTED) |
| Profile step | finest covering dataset resolution, clamped to [30 m, 90 m], then widened as needed to cap the profile at 16384 samples | engineering limit: beyond ~16k samples a finer step adds nothing to horizon/mean-elevation accuracy (still < 70 m posts at 1000 km) while cost grows without bound |
| Void handling | NODATA interpolated linearly between valid neighbours and **flagged** | design rule: never silently invented terrain |
| Horizon/takeoff angle | θ_t = max_i[(h_i − h_a)/d_i − d_i/(2ka)] | NBS TN101 §6 convention; identical construction in NTIA ITM `FindHorizons` |
| Earth bulge | c(x) = x(d−x)/(2ka) | standard effective-earth transform |
| First Fresnel radius | F1 = √(λ d1 d2 / d) | standard; clearance criterion 0.6 F1 |

## 3. Scatter geometry (`core/tropo/scatter_geometry.*`)

| Quantity | Method | Source |
|---|---|---|
| Scatter angle | θ = θe + θt + θr, θe = d·10³/(k·a), a = 6370 km | ITU-R P.617-5 eq. (1)–(2) |
| Common-volume height | ray-crossover in the flat-earth frame: x* = (h_r − h_t + θ_r d + d²/2ka)/θ; h0 = h_t + θ_t x* + x*²/2ka | equivalent small-angle form of P.617-5 eq. (7a); robust for negative (smooth-earth) horizon angles |
| Half-power beamwidth from gain | θ3dB(deg) ≈ √(27000/g) | classic parabolic-aperture estimate (e.g. Balanis, *Antenna Theory*); used only to draw the lens top |

## 4. Loss models (`core/tropo/`)

### 4.1 FSPL (reference baseline)
`FSPL = 32.45 + 20 log f(MHz) + 20 log d(km)` — ITU-R P.525-4.

### 4.2 ITU-R P.617-5 (primary)
Implemented step-by-step from Rec. ITU-R P.617-5 (08/2019), Annex 1 §4.1–4.2:

- eq. (3) coupling: `Lc = 0.07 exp[0.055 (Gt + Gr)]`
- eq. (4) loss: `L(p) = F + 22 log f + 35 log θ + 17 log d + Lc − Y(p)`
- eq. (5) `F = 0.18 N0 exp(−hs/hb) − 0.23 ΔN`
- eq. (6) `Y(p) = ±0.035 N0 exp(−h0/hb) (−log(p'/50))^0.67`
- eq. (7a) common-volume height h0 (see §3 above)
- N0, ΔN from the integral digital maps at the path midpoint.
- Validity envelope enforced: 100–1000 km, 200–5000 MHz, θ > 0. Outside it the model
  reports *out of validity range* — no extrapolated fiction.

### 4.3 NBS TN101 (classic anchor)
Rice, Longley, Norton, Barsis, *Transmission Loss Predictions for Tropospheric
Communication Circuits*, NBS Technical Note 101 (rev. 1967), Vol. 1–2:

- Median scatter attenuation per Ch. 9: attenuation function F(θd) (Fig. 9.1),
  frequency-gain function H0 (eq. 9.4–9.5), scattering-efficiency ηs (eq. 9.3a),
  crossover height (eq. 9.3b), evaluated through their published computerized fits in
  G. A. Hufford, *The ITS Irregular Terrain Model, version 1.2.2: The Algorithm*
  (1982), §4.63–4.67, 6.9, 6.13 — the exact code vendored in `third_party/ntia_itm`
  (`TroposcatterLoss.cpp`, `H0Function.cpp`).
- Ns dependence: −0.1(Ns − 301) exp(−θd/40 km) [Algorithm 4.63].
- Long-term variability V(50,de), Y(q,de) per TN101 Ch. 10 / Vol. 2 eq. III.69–70, via
  the Algorithm §5 climate-parameterized curves (`Variability.cpp`), with location and
  situation variability disabled (engineered point-to-point link, mdvar 13).
- Atmospheric absorption after TN101 §3: γ ≈ 0.0067 + 5·10⁻⁵ f²(GHz) dB/km (surface
  oxygen + water vapour estimate).
- Validity: 100 MHz–10 GHz, 50–1000 km, θ > 0, terrain data present.

### 4.4 ITM / Longley-Rice (cross-check)
The official NTIA C++ implementation (public domain), vendored unmodified in
`third_party/ntia_itm`, run in point-to-point TLS mode over the real extracted
profile: `ITM_P2P_TLS_Ex(h_tx, h_rx, pfl, climate, N0, f, pol, ε, σ, mdvar=13,
time, 50, 50)`. ITM performs its own horizon finding, terrain roughness Δh and
climate variability. The propagation mode it selects (LOS / diffraction /
troposcatter) is reported; a non-scatter mode flags the row as a mixed-mechanism
cross-check.

### 4.5 Coupling loss
`Lc = 0.07 exp[0.055(Gt+Gr)]` (P.617-5 eq. (3)) applied to all scatter models and
**shown as its own line** — at 2 × 39.1 dBi it is ≈ 5.2 dB, the classic beginner trap.

## 5. Statistics (`core/tropo/statistics.*`, `core/budget/availability.*`)

| Quantity | Method | Source |
|---|---|---|
| Inverse complementary normal | rational approximation 26.2.23 | Abramowitz & Stegun; same approximation the ITS Algorithm uses |
| Annual ↔ worst month | pw = Q(p)·p, Q per eq. (2); trans-horizon Q1 = 5.8 − 0.03 exp(Ns/75), β = 0.13, Q ∈ [1,12] | ITU-R P.841-5 §2, Table 1 |
| Long-term fading | hourly-median distribution L(p) from the selected model (log-normal-like) | P.617-5 §4 |
| Short-term fading | Rayleigh within the hour (exponential power around hourly median) | P.617-5 Attachment 1 §2 |
| Diversity | selection combining of independent branches (short-term only): outage = ∫ q1(u)^n du; none = 1, space/freq/angle = 2, quad = 4 | standard selection-combining statistics; angle ≈ vertical space per P.617-5 §7.3 |
| Availability ↔ margin inversion | monotone bisection, fixed iteration count | true inverse, round-trip tested |
| Space diversity spacing | Δh = 0.634 √(D² + Ih²), Δv = 0.634 √(D² + Iv²); Ih = 20 m, Iv = 15 m | ITU-R P.617-5 §7.1 eq. (44)–(45) |
| Frequency diversity separation | Δf = 1.44 f θ / (d √(D² + Iv²)) MHz (f MHz, θ mrad, d km, D m) | ITU-R P.617-5 §7.2 eq. (46); evaluated with the aperture term in the denominator for dimensional consistency (the published typesetting is ambiguous; this reading reproduces sane tens-of-MHz separations) |
| Angle diversity spacing | Δr = arctan(Δv / 500d) | ITU-R P.617-5 §7.3 eq. (47) |

## 6. Link budget (`core/budget/`)

| Quantity | Method |
|---|---|
| EIRP | P_tx − L_line,tx + G_tx |
| Noise floor | −174 dBm/Hz + 10 log B + NF (kT at 290 K) |
| Required SNR | Eb/N0 + 10 log(bits/symbol / (1+rolloff)); library values are uncoded theoretical references at BER 1e-6 (Proakis) — editable JSON |
| Waterfall | sums exactly by construction; regression-tested |

## 7. Map cartography (`ui/map/MapSources.cpp`)

The offline basemap is a rendering, not a data product — it never feeds computation.
It is documented here so its appearance is not mistaken for surveyed cartography.

| Element | Method |
|---|---|
| Relief shading | Horn's method (3×3 Sobel gradients) on the DEM, sun at 315° azimuth / 45° elevation, multiplied over the tint; vertical exaggeration 1.3–5.0 growing as the view zooms out |
| Hypsometric tint | 10-stop ramp (woodland green → cream → tan → brown → grey) interpolated on elevation; separate day/night ramps |
| Contours | Level set of the smoothed DEM, anti-aliased by distance-to-level measured in screen pixels through the local gradient; every 5th is an index contour, labelled and rotated along the contour with a paper halo. Interval per zoom: 200 m (z7) → 5 m (z≥15) |
| Pre-filter | Two passes of 3×3 binomial smoothing (≈ Gaussian σ 1) to suppress DEM posting steps; a seam guard suppresses ink where a pixel is further than 0.4 × interval from its level, so an elevation step between adjacent datasets cannot etch a false contour |
| Coverage | Pixels with no DEM coverage are left transparent (blank map paper), never drawn as sea level |

Water is shown only where the DEM is at or below datum; inland hydrography and
culture (roads, settlements) are not in a DEM — use an online source or an MBTiles
pack when that detail is needed.

## 8. Determinism

Identical inputs produce bit-identical outputs: fixed grids and iteration counts, no
order-dependent parallel reductions (profile samples land in preassigned slots), and
the report content is hashed with SHA-256 (FIPS 180-4) — the hash printed in the
report footer is reproducible across runs.
