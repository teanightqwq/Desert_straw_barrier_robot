# TODO

## Hardware Mapping
- [x] Resolve loader sensor pins after motor wiring (GPIO10/11 conflict)
- [] Fill loader_1 pin in sensor.h
- [x] Fill loader_2 pin in sensor.h
- [x] Fill loader_3 pin in sensor.h
- [] Fill loader_4 pin in sensor.h
- [ ] Fill wedger_sensor pin in sensor.cpp
- [ ] Define wheels H-bridge pin mapping in wheels.h
- [ ] Confirm active/passive wheel placement (front/rear or left/right)

## Sensor Logic Validation
- [ ] Validate near/far band thresholds (NEAR_THRESHOLD / FAR_THRESHOLD_MIN / FAR_THRESHOLD_MAX)
- [ ] Validate whether wedger completion condition should be raw < threshold or raw > threshold
- [ ] Tune WEDGER_THRESHOLD on real machine
- [ ] Tune FEED_PROCESS_TIME_MS from measured transport timing
- [ ] Tune hold timers (T_START_HOLD / T_LOW_HOLD / T_MISMATCH_HOLD / T_RANGE_BLIND_HOLD)
- [ ] Tune hold timers (T_GAP_HOLD / T_END_DETECT_HOLD / T_FEED_PROCESS / T_S5_CONFIRM_HOLD)

## Workflow Integration
- [ ] Add real e_stop trigger source (GPIO or bus command)
- [ ] Integrate completion signal with future wedger.cpp
- [ ] Define final handoff protocol between feeder and wedger modules
- [ ] Add wheels control calls in bake.ino (setup/loop integration)

## Motor Control
- [x] Add basic PWM motor driver (A/B/C) with LEDC
- [x] Integrate motor setup/loop in bake.ino
- [x] Add start-compatible direction flip for Motor A
- [x] Set motor C as master speed; A/B follow 0.8x

## Warning System Work
- [x] Formalize `WarnStatus` struct in `warning.h` (type, mainType, severity, prevWorkStatus, startMs, lastLogMs, message, active)
- [x] Replace `STATUS_WARNING_*` states with the warning system and update code paths in `sensor.cpp`
- [x] Add grouped warning storage with `WarnStatusGroup` so multiple warnings can coexist
- [x] Add serial log formatting for `WarnStatus` events to simplify debugging
- [x] Add main/subtype classification helpers (`warning_main_type()`, `warning_is_loader_subtype()`, `warning_is_flow_subtype()`)
- [x] Add placeholder types `WARNING_UNDEFINED` and `WARNING_NO_SUB` for ambiguous/main-class-only warnings
- [x] Add structured logs (session/event/snapshot/CSV)
- [ ] Implement loader subwarning handlers:
  - [ ] `handle_displaced_bale_warning()` - skeleton only
  - [ ] `handle_broken_bale_warning()` - skeleton only
  - [ ] `handle_sensor_status_warning()` - skeleton only
- [ ] Define decision algorithm for when to use `WARNING_UNDEFINED` vs `WARNING_NO_SUB` in actual flow
- [ ] Implement severity decision rules for each warning subtype
- [ ] Add unit/runtime tests that simulate loader warnings, flow warnings and feed-timeout warnings
- [ ] Tune warning log repeat interval if serial output is still too noisy

## Test Plan
- [ ] Test startup transition: not_start -> on_work
- [ ] Test coverage-based warnings (low coverage, gap, mismatch, range blind)
- [ ] Test end detection and feed timeout behavior
- [ ] Test successful completion branch to end
- [ ] Capture serial logs for all branches and archive test records
- [ ] Test wheels forward/backward/stop with H-bridge driver
