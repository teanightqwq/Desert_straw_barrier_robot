# IR Sensor Test Analysis (2026-05-07)

## Test Context
- Data source: dis_test/dist_link_07052026 (PuTTY log)
- Sensor: MH series IR sensor (star label)
- Motion: manual movement (not perfectly uniform)
- Sampling interval: 0.5 s
- Distance span: 0 mm to 90 mm

## Derived Speed Estimate (Assuming Uniform Motion)
Let:
- N = number of numeric samples
- dt = 0.5 s
- D = 90 mm

Then:
- Total duration: T = (N - 1) * dt
- Speed estimate: v = D / T

For this log:
- N = 162
- T = 80.5 s
- v = 1.118 mm/s

Note: Because the motion is manual, v is only an approximate reference for mapping samples to distance.

## Low Point (Valley) Detection
The raw values decrease first, reach a minimum, then rise.
- Minimum raw value: 101
- Minimum index (0-based): 37

Distance at minimum (uniform assumption):
- d_min = index * dt * v = 20.68 mm

This minimum should be treated as a breakpoint for piecewise calibration.

## Suggested Piecewise Calibration Logic
Use two monotonic models split by the minimum point:
- Near segment: d <= d_min (raw decreases with distance)
- Far segment: d >= d_min (raw increases with distance)

Common monotonic fit candidates (per segment):
1) d = a / (raw - b) + c
2) d = a * raw^b + c

Segment selection during runtime:
- If motion direction is known, pick the corresponding segment.
- Otherwise use trend (raw decreasing => near segment, raw increasing => far segment).
- If raw is saturated near max, treat as far-segment saturation region.

## Fitted Coefficients (This Log, Uniform Assumption)
Model used:
- d = a / (raw - b) + c

Fitting notes:
- Fitted separately for near/far segments split at d_min.
- Points with raw >= 4090 were dropped to reduce saturation bias.

Near segment (d <= d_min):
- a = 186.789
- b = 86.007
- c = 6.260

Far segment (d >= d_min):
- a = -167338.199
- b = -1899.000
- c = 94.294

Important: These coefficients are approximate because the motion was manual. For accuracy, repeat the sweep with uniform motion and re-fit.

## Notes
- The sensor shows a U-shaped response across 0-90 mm for this test.
- A dedicated calibration sweep with uniform motion would improve model accuracy.
