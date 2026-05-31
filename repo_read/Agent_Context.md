# Agent Context: Desert_straw_barrier_robot

> This file is intended to be read by AI/agents BEFORE doing any edits.
> It describes project intent, architecture, runtime flow, and non-negotiable constraints.
> For Project background and current status, see `README.md` and `README_cn.md`.
> For team communication and task tracking, see `TODO.md`.

## TL;DR (must follow)
- ESP32-S3 + Arduino; main logic in `bake/`.
- IR AO sensors are active-low: det = (raw < threshold).
- Disabled pin = -1; must not trigger warnings.
- Runtime: sample every SAMPLE_INTERVAL_MS; print every PRINT_PERIOD_MS.
- Do not break log formats ([SESSION]/[EVT]/[SNAP]/CSV).
- Prefer minimal changes; edit `sensor.h` for constants, `sensor.cpp` for logic, `logger.*` for logs.

## 1) Project background / goal
- Repo: teanightqwq/Desert_straw_barrier_robot
- Purpose: Self-navigating robot that constructs grid sand barriers using straw mats (ISDN3001-2).
- Current focus (bring-up stage): ESP32-S3 reads IR AO sensors to infer straw presence/flow state.
- Non-goals for now: motors/wheels/wedger control are stubbed or not integrated.

## 2) Build & runtime environment
- Platform: Arduino IDE / ESP32-S3 (Arduino core).
- Language: C++ (Arduino style).
- Serial baud: 115200.
- Logging is saved on PC via Serial Monitor / PuTTY/TeraTerm (device does NOT write PC files).

## 3) Hardware assumptions (critical)
### 3.1 Loader sensors (S1..S4)
- Type: IR reflective AO sensors (ADC).
- Direction: **active-low detection**
  - `det = (raw < threshold)` means straw detected / blocked.
- Thresholds are empirical (tested on real installation).
- Some sensors may be disabled (pin = -1). Disabled sensors must not cause false warnings.

### 3.2 Wedger sensor (S5)
- Future AO sensor (may be not wired). Must be guarded by pin validity checks.

## 4) High-level architecture (file map)
### Entry point
- `bake/bake.ino`
  - `setup()` -> `sensors_setup()`
  - `loop()`  -> `sensors_loop()`
  - (wedger/wheels/motor loops are currently commented out)

### Sensor / flow logic
- `bake/sensor.h`: pins, thresholds, timing constants, public APIs.
- `bake/sensor.cpp`:
  - Sampling: analogRead loader & wedger
  - Derived signals: coverage, zone mismatch, pair raw, near/far assignment
  - Hold timers (debounce)
  - WorkStatus state machine
  - Warning registry (set/clear/has)
  - Calls logger for [SESSION]/[EVT]/[SNAP]/(optional CSV)

### Warnings
- `bake/warning.h`: enums/types for WarningType, WorkStatus, WarnStatusGroup, etc.

### Logging
#### Logging compatibility rule
- Never rename existing fields/columns; only append new fields at the end.
#### Log types:
- `bake/logger.h` / `bake/logger.cpp`:
  - `[SESSION]` header (system name, build date/time, suggested filename)
  - `[EVT]` for state changes & warning changes
  - `[SNAP]` periodic snapshot for analysis
  - (optional) `[CSV_HEADER]` + `[CSV]` for Excel/Pandas analysis

### Design documents (source of truth)
- `repo_read/coverage_flow.md`: logic spec for coverage-based flow
- `repo_read/warning_mapping.md`: mapping from derived conditions -> WarningType
- `repo_read/planting_flow.md`: flow summary

## 5) Runtime flow (what runs each tick)
- Sampling tick: gated by `SAMPLE_INTERVAL_MS` (e.g. 5ms)
- Print tick: gated by `PRINT_PERIOD_MS` (e.g. 500ms)

Pseudo-flow (simplified):
1. Read loader sensors raw -> det
2. Read wedger raw -> det (if enabled)
3. Compute derived signals:
   - coverage
   - zone mismatch (S1 vs S3, S2 vs S4)
   - pairA_raw = min(S1,S2), pairB_raw = min(S3,S4)
   - near/far assignment by threshold bands
   - nearCount/farCount
4. Apply hold timers to generate stable events
5. Update WorkStatus state machine
6. Set/clear warnings based on mapping
7. Log `[EVT]` on changes, `[SNAP]` periodically

## 6) Derived signals definitions (important)
- coverage = count(det_i == true for enabled sensors)
- Physical pairs:
  - pairA = (S1,S2)
  - pairB = (S3,S4)
- Zone pairs:
  - zone1 = (S1,S3)
  - zone2 = (S2,S4)
- Representative pair raw:
  - pairX_raw = min(raw in that pair among enabled sensors)
- Near/Far semantic assignment:
  - Use threshold bands (no long-average calibration):
    - nearBand: raw <= NEAR_THRESHOLD
    - farBand:  FAR_THRESHOLD_MIN <= raw <= FAR_THRESHOLD_MAX
  - If ambiguous, keep last assignment and (optionally) warn with hold.

## 7) Warnings & state machine summary
- WorkStatus:
  - NOT_START -> ON_WORK: coverage >= 2 persists T_START_HOLD
  - ON_WORK -> END_DETECTION: coverage == 0 persists T_END_DETECT_HOLD
  - END_DETECTION -> END: wedger det persists T_S5_CONFIRM_HOLD (future)
- Warning mapping (see warning_mapping.md):
  - WARNING_SENSOR_STATUS: zone mismatch, near/far ambiguous, near/far layer blind
  - WARNING_BROKEN_BALE: low coverage / gap
  - WARNING_MAIN_FLOW: end detecting stage
  - WARNING_FEED_TIMEOUT: timeout waiting for S5 confirm

## 8) Non-negotiable rules for edits (agents must follow)
1) Keep `det = (raw < threshold)` (active-low). Do NOT invert without explicit instruction.
2) Disabled sensors (pin invalid) must not break logic nor spam warnings.
3) Avoid dynamic allocations (`String`) in tight loops; prefer fixed buffers + snprintf.
4) Preserve sampling and printing cadence; do not print every 5ms.
5) Prefer minimal changes; do not refactor across many files unless asked.

## 9) Common tasks & where to change
- Change thresholds / pins / hold times: `bake/sensor.h`
- Change warning mapping logic: `bake/sensor.cpp` (processWorkFlow)
- Add new log fields: `bake/logger.cpp` + `bake/sensor.cpp::printStatus`
- Add new warning types: `bake/warning.h` (only if required)

## 10) Known limitations / TODO
- Only sensor/flow monitoring is implemented; actuators are not integrated.
- Wedger sensor may be unwired; end confirmation is future work.
- CSV output may be optional depending on logging needs.

## 11) Agent Workflow suggestions
- It is a MUST to read this context before making any code changes.
- Most of the logic coding is done in `bake` folder. `repo_read` focuses on documentations.
- *Note* hidden files (e.g. `log` for storing logs, `dis_test` for distance approximation for IR sensors) exists locally but are hidden by `.gitignore`. Changes might be needed in future. In that case, the agent will be explicitly asked to make changes to those files and remove them from `.gitignore` if necessary. The agent should add them back to `.gitignore` after making changes if they are not intended to be tracked.
- Do changes according to the context:
    - For newly added features, follow the architecture and patterns in existing code. Add suitable comments and documentation for future maintainings.
    - For bug fixes, identify the relevant module and make minimal changes.
    - For debug activity, use the existing logging framework. If new logs are needed, add them in the same structured format. Remember to add comments for faster location in the future.
- After making changes, do changes to the TODO list in `repo_read/TODO.md` to keep track of completed and pending tasks.
- After making changes, do changes to the README files (`README.md`, `README_cn.md`) if there are any updates to the architecture, flow, or important notes that future maintainers should be aware of. 
- DO NOT make any changes to this context file unless there are significant updates to the project background, architecture, or non-negotiable constraints, unless such updates are required by the same file(`repo_read/Agent_Context.md`).