# TODO

## Hardware Mapping
- [x] Confirm loader_1 pin is GPIO10
- [ ] Fill loader_2 pin in sensor.cpp
- [ ] Fill loader_3 pin in sensor.cpp
- [ ] Fill loader_4 pin in sensor.cpp
- [ ] Fill wedger_sensor pin in sensor.cpp

## Sensor Logic Validation
- [ ] Measure real far-group baseline and tune FAR_THRESHOLD_SCALE
- [ ] Validate whether wedger completion condition should be raw < threshold or raw > threshold
- [ ] Tune WEDGER_THRESHOLD on real machine
- [ ] Tune FEED_PROCESS_TIME_MS from measured transport timing

## Workflow Integration
- [ ] Add real e_stop trigger source (GPIO or bus command)
- [ ] Integrate completion signal with future wedger.cpp
- [ ] Define final handoff protocol between feeder and wedger modules

## Warning System Work
- [x] Formalize `WarnStatus` struct in `sensor.h` (type, mainType, severity, prevWorkStatus, startMs, lastLogMs, message, active)
- [x] Replace `STATUS_WARNING_*` states with the warning system and update code paths in `sensor.cpp`
- [x] Add grouped warning storage with `WarnStatusGroup` so multiple warnings can coexist
- [x] Add serial log formatting for `WarnStatus` events to simplify debugging
- [x] Add main/subtype classification helpers (`warning_main_type()`, `warning_is_loader_subtype()`, `warning_is_flow_subtype()`)
- [x] Add placeholder types `WARNING_UNDEFINED` and `WARNING_NO_SUB` for ambiguous/main-class-only warnings
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
- [ ] Test loader warning trigger and clear timing
- [ ] Test feeder timeout warning branch
- [ ] Test successful completion branch to end
- [ ] Capture serial logs for all branches and archive test records
