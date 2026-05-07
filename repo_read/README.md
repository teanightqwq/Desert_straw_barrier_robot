# Desert Straw Barrier Robot

## Purpose
This repository contains early bring-up code and notes for straw checkerboard planting detection.
The current focus is infrared analog sensing on ESP32-S3 for loader-to-feeder and feeder-to-wedger flow checks.

## Current Files
### Planting System
- bake.ino: Unified firmware entrypoint for ESP32-S3
- sensor.h: Sensor module interface (GPIO map, enums, public APIs)
- sensor.cpp: Sensor module implementation (workflow and state machine)
- warning.h: Warning system types and APIs (separated from sensor module)
- logger.h: Structured logging helpers (session/event/snapshot/CSV)
- logger.cpp: Structured logging implementation
- wheels.h: Wheel control interface (H-bridge driver, active/passive groups, speed priority)
- wheels.cpp: Wheel control implementation (forward/backward/stop framework)
- emb_system.md: System-level hardware notes
- TODO.md: Action list for next engineering steps

## Current Pin Plan
Only one loader sensor pin is fixed right now.

| Sensor | Role | Pin |
|---|---|---|
| loader_1 | Loader sensor (known) | GPIO10 |
| loader_2 | Loader sensor | TBD |
| loader_3 | Loader sensor | TBD |
| loader_4 | Loader sensor | TBD |
| wedger_sensor | Feeder-to-wedger check sensor | TBD |

## Work Status Enum
The runtime status machine in sensor.cpp uses:
- not_start
- on_work
- e_stop
- end
- end_detection

## Warning System (new)
Warnings are now modeled separately from `WorkStatus` using a grouped `WarnStatus` system. This keeps transient warnings (sensor issues, flow timeouts) decoupled from the main work-state machine and allows multiple warnings to coexist.

Key concepts:
- `WarningType`: six concrete classes:
   - `WARNING_LOW_COVERAGE`
   - `WARNING_GAP_DETECTED`
   - `WARNING_SENSOR_MISCONFIG`
   - `WARNING_RANGE_GROUP_BLIND_NEAR`
   - `WARNING_RANGE_GROUP_BLIND_FAR`
   - `WARNING_FLOW_END_DETECTING`
   - `WARNING_FEED_TIMEOUT`
- `WarnStatus`: runtime struct holding warning type, severity, previous `WorkStatus`, timestamps and message.
- `WarnStatusGroup`: fixed-size warning list used to keep more than one warning active at the same time.
- `WarningSeverity`: severity buckets, independent from warning type.

Behavioral notes:
- Warnings do not automatically change `WorkStatus`; they are recorded in the warning group and can be acted on by higher-level logic.
- The module exposes `set_warn_status()`, `clear_warn_status()`, `clear_all_warn_status()` and `get_warn_status_group()` for warning management.
- Warning log output is throttled so the same warning does not spam the serial monitor every loop.

Status / warning pair examples:
- `STATUS_ON_WORK` + `WARNING_NONE`: normal running state.
- `STATUS_ON_WORK` + `WARNING_SENSOR_MISCONFIG`: zone mismatch or near/far ambiguity.
- `STATUS_ON_WORK` + `WARNING_RANGE_GROUP_BLIND_NEAR`: near layer blind.
- `STATUS_END_DETECTION` + `WARNING_FEED_TIMEOUT`: end-stage timeout while the wedger still has not confirmed completion.

## Refactor Notes (warning separation)
The warning system was separated into its own header to keep warning types and APIs independent from the sensor module.

Key changes:
- `warning.h` now owns `WorkStatus`, `WarningType`, `WarningSeverity`, `WarnStatus`, and `WarnStatusGroup`.
- `sensor.h` includes `warning.h` and focuses on sensor configuration and public APIs.
- `sensor.cpp` keeps all implementations.

## Logging (session/event/snapshot/CSV)
The logger emits structured lines so you can copy/paste Serial Monitor output into a local `log/` folder on Windows.

Log formats:
- `[SESSION]` header at boot with build date/time and suggested filename.
- `[EVT]` for status changes and warning set/clear.
- `[SNAP]` periodic structured snapshot.
- `[CSV_HEADER]` + `[CSV]` for spreadsheet-friendly snapshots.

## Wheel Control (wheels)
The wheel module controls two motor groups over an H-bridge:
- Active group: speed priority, sets overall speed and direction.
- Passive group: only needs to spin, default PWM keeps it moving.

The current code is a forward/backward/stop framework with TBD GPIO mapping:
- Header: [bake/wheels.h](bake/wheels.h)
- Source: [bake/wheels.cpp](bake/wheels.cpp)

Next steps:
- Define H-bridge pin mapping
- Clarify active/passive wheel placement (front/rear or left/right)
- Tune passive minimum PWM and verify direction alignment

## Runtime Flow (Current)
1. Program starts in not_start.
2. When coverage >= 2 holds for T_START_HOLD, status becomes on_work.
3. Coverage and zone consistency are computed every sample:
   - coverage = det1 + det2 + det3 + det4
   - zone mismatches: (S1 vs S3) and (S2 vs S4)
4. Pair grouping is fixed physically, but near/far is decided dynamically by band rules:
   - pairA = (S1,S2), pairB = (S3,S4)
   - near/far determined by raw bands (NEAR_THRESHOLD / FAR_THRESHOLD_MIN/MAX)
5. Warnings are driven by sustained conditions:
   - zone mismatch, near/far ambiguity, range blind
   - low coverage / gap detection
6. End detection:
   - coverage == 0 for T_END_DETECT_HOLD -> STATUS_END_DETECTION + WARNING_FLOW_END_DETECTING
   - in END_DETECTION, wait T_FEED_PROCESS for S5 confirm (if enabled)
   - timeout -> WARNING_FEED_TIMEOUT

## Module Interface
- bake.ino keeps the global Arduino setup/loop.
- sensor.cpp provides:
   - sensors_setup()
   - sensors_loop()
- sensor.h exposes enums, GPIO configuration, and public function declarations.

## Important Config Values
Edit these constants in sensor.h (Runtime Config section) for your test bench:
- LOADER_PIN_1..4
- WEDGER_SENSOR_PIN
- NEAR_THRESHOLD
- FAR_THRESHOLD_MIN / FAR_THRESHOLD_MAX
- WEDGER_THRESHOLD
- FEED_PROCESS_TIME_MS
- T_START_HOLD / T_LOW_HOLD / T_MISMATCH_HOLD / T_RANGE_BLIND_HOLD
- T_GAP_HOLD / T_END_DETECT_HOLD / T_FEED_PROCESS / T_S5_CONFIRM_HOLD

## Notes
- With only loader_1 configured today, coverage-based logic is partial by design.
- Once all pins are known, set them directly in sensor.h and rerun tests.
