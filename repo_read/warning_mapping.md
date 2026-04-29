# warning_mapping.md

This file maps the **proposed new warnings** (from `coverage_flow.md`) to the **current repo enums** in `bake/warning.h`.

Goal:
- Keep the flow / detection logic readable and physically meaningful.
- Avoid forcing "left/right" diagnosis when edge sensors are missing.
- Minimize enum churn: prefer reusing existing `WarningType` where possible.

> Current repo enums (from `bake/warning.h`):
> - Main: `WARNING_MAIN_LOADER`, `WARNING_MAIN_FLOW`
> - Loader subtypes: `WARNING_DISPLACED_BALE`, `WARNING_BROKEN_BALE`, `WARNING_SENSOR_STATUS`
> - Flow subtype: `WARNING_FEED_TIMEOUT`

---

## 1) Proposed warnings (design intent)

From `coverage_flow.md`, the proposed warning concepts are:

1) `WARNING_LOW_COVERAGE`
- Meaning: sustained low detection coverage (coverage=1/2 or always missing one zone).
- Indicates: straw amount too low, straw not covering expected area, or sensor thresholds too strict.

2) `WARNING_GAP_DETECTED`
- Meaning: sudden drop event (coverage high -> coverage very low) while ON_WORK.
- Indicates: discontinuity / hole / broken flow.

3) `WARNING_SENSOR_MISCONFIG`
- Meaning: mismatch between paired sensors that should observe the same zone (S1!=S3 or S2!=S4) for long enough.
- Indicates: threshold mismatch, installation offset, dirt/obstruction, sensor failure.

4) `WARNING_RANGE_GROUP_BLIND_NEAR` / `WARNING_RANGE_GROUP_BLIND_FAR`
- Meaning: the whole NEAR layer (S1+S2) or FAR layer (S3+S4) sees no straw for long enough.
- Indicates: range/angle/threshold problem on that layer, or straw is entirely on the opposite side.

5) `WARNING_FLOW_END_DETECTING`
- Meaning: coverage==0 sustained -> system transitions into end detection stage.
- Indicates: loader area appears empty (potential end of bale feed).

6) `WARNING_FEED_TIMEOUT`
- Meaning: end detection stage exceeded allowed time without S5 confirmation.

---

## 2) Mapping to current `WarningType`

### Loader-side warnings (map to `WARNING_MAIN_LOADER` scope)

#### A) LOW_COVERAGE
**Recommended mapping:**
- Use `WARNING_BROKEN_BALE` when coverage is low but not strictly a "broken" physical bale.
  - Rationale: current enum set has no "low coverage" type; `BROKEN_BALE` is the closest existing bucket.
- Alternatively (more conservative), map LOW_COVERAGE to `WARNING_SENSOR_STATUS` if you want to treat it as sensor/threshold issue first.

**Suggested implementation choice:**
- If you want LOW_COVERAGE to mean "material condition": map to `WARNING_BROKEN_BALE`.
- If you want LOW_COVERAGE to mean "uncertain root cause": map to `WARNING_UNDEFINED` (not currently used) or `WARNING_SENSOR_STATUS`.

**Message templates:**
- "LOW_COVERAGE sustained: coverage=2" / "coverage=1" / "zone missing"

#### B) GAP_DETECTED
**Recommended mapping:**
- Map to `WARNING_BROKEN_BALE`

**Message templates:**
- "GAP detected: coverage drop >=3 -> <=1"

#### C) SENSOR_MISCONFIG
**Recommended mapping:**
- Map to `WARNING_SENSOR_STATUS`

**Message templates:**
- "zone1 mismatch (S1!=S3)"
- "zone2 mismatch (S2!=S4)"

#### D) RANGE_GROUP_BLIND_NEAR / RANGE_GROUP_BLIND_FAR
**Recommended mapping:**
- Map to `WARNING_SENSOR_STATUS`

**Message templates:**
- "near layer blind: S1,S2 both not detecting"
- "far layer blind: S3,S4 both not detecting"

#### E) (Optional) DISPLACED_BALE
Because current hardware lacks edge sensors, **avoid** producing "Too left" / "Too right".

If you still want a displaced bucket, use it only as a **neutral** label:
- Map some patterns (e.g. zone missing for long time) to `WARNING_DISPLACED_BALE`
- But keep message neutral, e.g. "possible displacement / uneven coverage" (no left/right).

---

### Flow-side warnings (map to `WARNING_MAIN_FLOW` scope)

#### F) FLOW_END_DETECTING
**Recommended mapping:**
- Map to `WARNING_MAIN_FLOW`

**Message templates:**
- "END_DETECTING: coverage==0 sustained"

#### G) FEED_TIMEOUT
**Recommended mapping:**
- Map to existing `WARNING_FEED_TIMEOUT`

**Message templates:**
- "timeout waiting for S5 confirmation"

---

## 3) Summary table (quick reference)

| Proposed concept | Recommended current enum | Main type | Notes |
|---|---|---|---|
| LOW_COVERAGE | `WARNING_BROKEN_BALE` (or `WARNING_SENSOR_STATUS`) | `WARNING_MAIN_LOADER` | choose by whether you blame material vs sensor |
| GAP_DETECTED | `WARNING_BROKEN_BALE` | `WARNING_MAIN_LOADER` | continuity drop event |
| SENSOR_MISCONFIG | `WARNING_SENSOR_STATUS` | `WARNING_MAIN_LOADER` | zone mismatch (S1!=S3 or S2!=S4) |
| RANGE_GROUP_BLIND_NEAR | `WARNING_SENSOR_STATUS` | `WARNING_MAIN_LOADER` | near layer both not detecting |
| RANGE_GROUP_BLIND_FAR | `WARNING_SENSOR_STATUS` | `WARNING_MAIN_LOADER` | far layer both not detecting |
| FLOW_END_DETECTING | `WARNING_MAIN_FLOW` | `WARNING_MAIN_FLOW` | end detection stage |
| FEED_TIMEOUT | `WARNING_FEED_TIMEOUT` | `WARNING_MAIN_FLOW` | already exists |

---

## 4) Practical advice for later implementation

1) Keep detection logic separate from warning naming:
   - compute `coverage`, `zone mismatch`, `near/far blind`, `gap event` first
   - then map to `WarningType` in a single place (so it is easy to adjust later)

2) Treat `WARNING_SENSOR_STATUS` as "sensor/threshold/installation" bucket.

3) Avoid left/right-specific messages until edge sensors exist.
