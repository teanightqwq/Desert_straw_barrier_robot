# Desert Straw Barrier Robot

## Purpose
This repository contains early bring-up code and notes for straw feeding detection.
The current focus is infrared analog sensing on ESP32-S3 for loader-to-feeder and feeder-to-wedger flow checks.

## Current Files
- sensor.cpp: ESP32-S3 IR sensor workflow test code
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
- warning_work
- e_stop
- end

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

## Important Config Values
Edit these constants in sensor.cpp for your test bench:
- LOADER_PIN_1..4
- WEDGER_SENSOR_PIN
- NEAR_THRESHOLD
- FAR_THRESHOLD_MIN / FAR_THRESHOLD_MAX
- WEDGER_THRESHOLD
- FEED_PROCESS_TIME_MS

## Notes
- With only loader_1 configured today, auto grouping is partial by design.
- Once all pins are known, set them directly in sensor.cpp and rerun calibration.
