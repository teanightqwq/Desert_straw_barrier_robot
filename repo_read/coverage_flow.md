# coverage_flow.md (new draft based on current sensor placement constraints)

## 0) Definitions

### Sensor boolean
Each sensor uses AO analog value and a per-sensor threshold:

- `det_i = (raw_i < thr_i)`  // AO low means detected (blocked by straw)

> If your AO polarity is reversed on some modules, normalize it here first.

### Two grouping views (per current design)

**A. Same distance / same direction (NEAR vs FAR)**
- `nearPair = (S1, S2)`
- `farPair  = (S3, S4)`

**B. Same zone / same straw block (zone consistency)**
- `zone1 = (S1, S3)`  // both observe the same straw zone
- `zone2 = (S2, S4)`  // both observe the same straw zone

### Coverage score (reuse old idea)
- `coverage = det1 + det2 + det3 + det4`  // 0..4

### Placeholder timers (tunable later)
- `T_DEBOUNCE`            = [x] ms
- `T_START_HOLD`          = [x] ms   // enter ON_WORK
- `T_LOW_HOLD`            = [x] ms   // low coverage persists -> warning
- `T_MISMATCH_HOLD`       = [x] ms   // zone mismatch persists -> warning
- `T_RANGE_BLIND_HOLD`    = [x] ms   // near/far blind persists -> warning
- `T_GAP_HOLD`            = [x] ms   // sudden drop persists -> gap warning
- `T_END_DETECT_HOLD`     = [x] ms   // coverage==0 -> end detection
- `T_FEED_PROCESS`        = [x] ms   // after end detection -> wait for S5
- `T_S5_CONFIRM_HOLD`     = [x] ms   // S5 confirm planting end

---

## 1) What can be diagnosed vs what cannot

### Cannot reliably diagnose now
- left/right displacement ("too left/too right") because **no edge sensors**.

### Can diagnose now
- coverage quality: `coverage` (0..4)
- zone mismatch: `S1 != S3` or `S2 != S4`
- range-layer blind: `(S1==0 && S2==0)` or `(S3==0 && S4==0)` for long time
- temporal gap events: sudden `coverage>=3 -> coverage<=1`
- flow end / feed timeout (future S5 integration)

---

## 2) Coverage -> Warning mapping table (for later severity assignment)

> Notes:
> - This table describes **primary** warnings. Multiple warnings can coexist.
> - Time holds should be applied (do not trigger on a single sample).

### coverage = 4
- Expected: all zones detect straw.
- Warning: none (unless mismatch/range-blind triggers, which is unlikely).

### coverage = 3
- Expected: mostly OK.
- Warning:
  - `WARNING_LOW_COVERAGE` if it stays 3 but always missing the same sensor (optional)
  - also check `zone mismatch` (section 3)

### coverage = 2
Two sub-cases matter a lot:

1) **2 sensors are the same zone** (zone1==2, zone2==0 OR zone1==0, zone2==2)
   - Example: `det1=1, det3=1, det2=0, det4=0`  -> zone1 full, zone2 empty
   - Warning candidates:
     - `WARNING_LOW_COVERAGE` (base)
     - optionally a more specific message "zone missing"
   - Possible causes: straw only covers one zone, or misplacement, or uneven straw.

2) **2 sensors are split across zones but within-zone mismatch exists**
   - Example: `det1=1, det3=0` -> zone1 mismatch
   - Warning candidates:
     - `WARNING_SENSOR_MISCONFIG` (zone mismatch)

Also check range-group blind:
- If `nearPair == (0,0)` for long time -> `WARNING_RANGE_GROUP_BLIND_NEAR`
- If `farPair  == (0,0)` for long time -> `WARNING_RANGE_GROUP_BLIND_FAR`

### coverage = 1
- Warning:
  - `WARNING_LOW_COVERAGE` (strong)
  - If on_work and previously coverage was high -> `WARNING_GAP_DETECTED` (gap / broken continuity)

### coverage = 0
- Warning:
  - If on_work and persists -> `WARNING_FLOW_END_DETECTING` (enter END_DETECTION)
  - If end_detecting and S5 not confirmed by `T_FEED_PROCESS` -> `WARNING_FEED_TIMEOUT`
  - If on_work and drops suddenly -> `WARNING_GAP_DETECTED`

---

## 3) Zone mismatch and range-blind rules (independent of coverage)

### 3.1 Zone mismatch (same zone should agree)
- `zone1 mismatch` if `det1 != det3` for >= `T_MISMATCH_HOLD`
- `zone2 mismatch` if `det2 != det4` for >= `T_MISMATCH_HOLD`

Trigger:
- `WARNING_SENSOR_MISCONFIG` (or reuse existing `WARNING_SENSOR_STATUS`)

Message examples:
- "zone1 mismatch: S1!=S3 (same block, different result)"
- "zone2 mismatch: S2!=S4 (same block, different result)"

Possible causes (typical):
- thresholds not aligned (VR1 / code threshold)
- installation offset/angle difference
- one sensor dirty/blocked
- straw is much closer to one side (unknown side), the other side is out of effective range
- strong ambient light on one sensor

### 3.2 Range-group blind (same distance layer both fail)
- `near blind` if `det1==0 && det2==0` for >= `T_RANGE_BLIND_HOLD`
- `far blind`  if `det3==0 && det4==0` for >= `T_RANGE_BLIND_HOLD`

Trigger:
- `WARNING_RANGE_GROUP_BLIND_NEAR` / `WARNING_RANGE_GROUP_BLIND_FAR`
(or map both into `WARNING_SENSOR_STATUS` with message)

Possible causes:
- that distance layer is not positioned correctly (too far/too close)
- thresholds too strict for that layer
- both sensors in that layer are dirty/obstructed
- straw happens to be on the opposite side (plausible since direction is unknown)
- shared wiring power/ground issue affecting that pair

---

## 4) Draft runtime flow (if-else, no severity yet)

```cpp
// State placeholders
enum WorkStatus { NOT_START, ON_WORK, END_DETECTION, END };

// Inputs each tick
bool det1, det2, det3, det4;  // from AO thresholds
bool det5;                   // future: S5 (wedger confirm), same AO logic

int coverage = det1 + det2 + det3 + det4;

// Derived group signals
int nearCount = det1 + det2;
int farCount  = det3 + det4;

bool zone1Mismatch = (det1 != det3);
bool zone2Mismatch = (det2 != det4);

// 1) Start condition
if (status == NOT_START) {
  if (coverage >= 2 for T_START_HOLD) {
    status = ON_WORK;
    clear_all_warnings();
  } else {
    // still idle
    return;
  }
}

// 2) Always-on consistency warnings (independent)
if (zone1Mismatch for T_MISMATCH_HOLD) warn(WARNING_SENSOR_MISCONFIG, "zone1 mismatch");
if (zone2Mismatch for T_MISMATCH_HOLD) warn(WARNING_SENSOR_MISCONFIG, "zone2 mismatch");

if (nearCount == 0 for T_RANGE_BLIND_HOLD) warn(WARNING_RANGE_GROUP_BLIND_NEAR, "near layer blind");
if (farCount  == 0 for T_RANGE_BLIND_HOLD) warn(WARNING_RANGE_GROUP_BLIND_FAR,  "far layer blind");

// 3) Coverage-based warnings
if (coverage >= 3) {
  clear(WARNING_LOW_COVERAGE);
} else if (coverage == 2 for T_LOW_HOLD) {
  warn(WARNING_LOW_COVERAGE, "coverage=2 sustained");
} else if (coverage <= 1 for T_LOW_HOLD) {
  warn(WARNING_LOW_COVERAGE, "coverage<=1 sustained");
}

// 4) Gap detection (broken continuity)
if (status == ON_WORK) {
  if (previousCoverage >= 3 && coverage <= 1 for T_GAP_HOLD) {
    warn(WARNING_GAP_DETECTED, "coverage drop event");
  }
}

// 5) End detection (flow end)
if (status == ON_WORK) {
  if (coverage == 0 for T_END_DETECT_HOLD) {
    status = END_DETECTION;
    warn(WARNING_FLOW_END_DETECTING, "coverage=0 sustained, entering end detection");
    start_end_timer();
  }
}

// 6) Feed process + S5 confirm
if (status == END_DETECTION) {
  if (det5 == true for T_S5_CONFIRM_HOLD) {
    status = END;
    clear(WARNING_FEED_TIMEOUT);
  } else if (end_timer_elapsed >= T_FEED_PROCESS) {
    warn(WARNING_FEED_TIMEOUT, "timeout waiting for S5 confirm");
  }
}
```

---

## 5) Suggested WARNING list (no severity yet)

Minimal set:
- `WARNING_LOW_COVERAGE`
- `WARNING_SENSOR_MISCONFIG` (zone mismatch / inconsistent sensors)
- `WARNING_RANGE_GROUP_BLIND_NEAR`
- `WARNING_RANGE_GROUP_BLIND_FAR`
- `WARNING_GAP_DETECTED`
- `WARNING_FLOW_END_DETECTING`
- `WARNING_FEED_TIMEOUT`

You can later map these to existing enums if you want to avoid adding new ones:
- MISCONFIG / RANGE_BLIND -> `WARNING_SENSOR_STATUS`
- GAP / LOW_COVERAGE -> `WARNING_BROKEN_BALE` (optional)
- FLOW_END_DETECTING -> `WARNING_MAIN_FLOW`
