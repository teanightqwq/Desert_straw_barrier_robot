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

## Test Plan
- [ ] Test startup transition: not_start -> on_work
- [ ] Test loader warning trigger and clear timing
- [ ] Test feeder timeout warning branch
- [ ] Test successful completion branch to end
- [ ] Capture serial logs for all branches and archive test records
