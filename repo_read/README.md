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
- `WarningType`: has main types (`WARNING_MAIN_LOADER`, `WARNING_MAIN_FLOW`) and subtypes (`WARNING_DISPLACED_BALE`, `WARNING_BROKEN_BALE`, `WARNING_SENSOR_STATUS`, `WARNING_FEED_TIMEOUT`).
  - Placeholder types: `WARNING_UNDEFINED` (when subtype is unclear) and `WARNING_NO_SUB` (when main type has no subtype).
- `warning_main_type()`: maps a subtype back to its main class; handles misclassifications.
- `warning_is_main_type()`, `warning_is_loader_subtype()`, `warning_is_flow_subtype()`: helpers for type checking.
- `WarnStatus`: runtime struct holding warning type, main type, severity, previous `WorkStatus`, timestamps and message.
- `WarnStatusGroup`: fixed-size warning list used to keep more than one warning active at the same time.
- `WarningSeverity`: severity buckets, independent from warning type.

Behavioral notes:
- Warnings do not automatically change `WorkStatus`; they are recorded in the warning group and can be acted on by higher-level logic.
- The module exposes `set_warn_status()`, `clear_warn_status()`, `clear_all_warn_status()` and `get_warn_status_group()` for warning management.
- Warning log output is throttled so the same warning does not spam the serial monitor every loop.

Status / warning pair examples:
- `STATUS_ON_WORK` + `WARNING_NONE`: normal running state.
- `STATUS_ON_WORK` + `WARNING_MAIN_LOADER`: loader-level warning, may contain subwarnings.
- `STATUS_ON_WORK` + `WARNING_MAIN_FLOW`: flow-related warning, such as delayed end detection.
- `STATUS_END_DETECTION` + `WARNING_FEED_TIMEOUT`: end-stage timeout while the wedger still has not confirmed completion.

## Refactor Notes (warning separation)
The warning system was separated into its own header to keep warning types and APIs independent from the sensor module.

Key changes:
- `warning.h` now owns `WorkStatus`, `WarningType`, `WarningSeverity`, `WarnStatus`, and `WarnStatusGroup`.
- `sensor.h` includes `warning.h` and focuses on sensor configuration and public APIs.
- `sensor.cpp` keeps all implementations and explicitly includes `warning.h` for clarity.

## Runtime Flow (Current)
1. Program starts in not_start.
2. When any loader sensor first detects straw (raw < threshold), status becomes on_work.
3. Loader sensors are grouped as near/far when enough configured sensors are available.
4. Each loader sensor runs warning logic:
   - raw > threshold for 2500 ms -> warning
   - then raw < threshold for 1000 ms -> warning cleared
5. When all loader sensors report no straw, feeder timer starts.
6. After FEED_PROCESS_TIME_MS (currently 18000 ms):
   - if wedger sensor raw < wedger_threshold -> warning_work and continue
   - otherwise -> end

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

## Notes
- With only loader_1 configured today, auto grouping is partial by design.
- Once all pins are known, set them directly in sensor.h and rerun calibration.
