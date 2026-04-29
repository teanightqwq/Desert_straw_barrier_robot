# draft_logic_flow.md (from flowchart image)

> Source: flowchart image (S1–S5).
> Assumption (as written in the flowchart):
> - **Nothing = 1**
> - **Blocked = 0**
>
So for the diagram below, `Si == 0` means **sensor is blocked / detects straw**, and `Si == 1` means **nothing detected**.

## Variables / timers (conceptual)
- `S1..S5`: digital sensor states (0/1)
- `coverage = S1 + S2 + S3 + S4`
- `startFlag`: whether system has entered "Start = True"

Timers used by the flowchart:
- `T_COV_HOLD = 2s` (used for `coverage == 2 for 2s?` and `coverage == 1 for 2s?`)
- `T_S5_END = [time]s` (used for `S5 = 1 for [time]s?` -> planting end)
- `T_S5_REVERSE = [time]s` (used for `S5 = 0 after [time]s?` -> reverse motor)

> Note: `T_S5_END` and `T_S5_REVERSE` are left as **placeholders** per request.

---

## Main loop (if-else style, mapped to the flowchart)

```cpp
// System Start
initialize S1..S5
// Diagram explicitly states:
// Nothing = 1
// Blocked = 0

bool startFlag = false;

while (true) {
  // "Coverage = S1 + S2 + S3 + S4"
  int coverage = S1 + S2 + S3 + S4;

  // Decision: "Coverage >= 3?"
  if (coverage >= 3) {
    // Decision: "Start?"
    if (!startFlag) {
      // Parallelogram: "Start = True"
      startFlag = true;
    }

    // After started: Decision: "S5 = 0 after [time]s?"
    if (startFlag) {
      if (S5 == 0 for T_S5_REVERSE) {
        // Action: "Reverse Motor Direction"
        reverse_motor_direction();
        // Then: "Warning Solved" (flowchart routes back)
      } else {
        // Action: "Continue check delay .05s"
        delay(50);
      }
    }
  } else {
    // coverage < 3

    // Decision: "Coverage == 2 for 2s?"
    if (coverage == 2 continuously for T_COV_HOLD) {
      // Diagnosis branch: check which sensors are blocked
      if (S1 == 0) {
        if (S2 == 0) {
          // Box: "S1, S2 = 0" -> Warning: Bale Displaced (Too left)
          warning("Bale Displaced (Too left)");
        } else if (S3 == 0) {
          // Box: "S1, S3 = 0" -> Warning: Bale Broken (Hole)
          warning("Bale Broken (Hole)");
        } else {
          // Box: "S1, S4 = 0" -> Warning: Bale Broken (Hollowed)
          warning("Bale Broken (Hollowed)");
        }
      } else if (S2 == 0) {
        if (S3 == 0) {
          // Box: "S2, S3 = 0" -> Warning: Bale Broken (Too short)
          warning("Bale Broken (Too short)");
        } else {
          // Box: "S2, S4 = 0" -> Warning: Bale Broken (Hole)
          warning("Bale Broken (Hole)");
        }
      } else {
        // Box: "S3, S4 = 0" -> Warning: Bale Displaced (Too right)
        warning("Bale Displaced (Too right)");
      }

      // Flowchart routes all warnings to "Warning Solved" then back to monitoring.
      warning_solved();
    }

    // Decision: "Coverage == 1 for 2s?"
    else if (coverage == 1 continuously for T_COV_HOLD) {
      // Right branch in flowchart
      // Decision: "S5 = 1 for [time]s?"
      if (S5 == 1 continuously for T_S5_END) {
        // Terminal: "Bale planting end"
        planting_end();
        break;
      } else {
        // Warning: "Bale Stucked"
        warning("Bale Stucked");
        warning_solved();
      }
    }

    // Otherwise: "Measurement error" (transient/unstable)
    else {
      measurement_error();
      delay(50);
    }
  }
}
```

---

## Notes / ambiguities
- The flowchart uses multiple time-based stability checks; in code these become timestamp-based "hold" timers.
- `measurement_error()` in the flowchart seems to mean "coverage is not stable long enough" or "reading is transient".
- The flowchart uses **digital** sensor readings (0/1). Your current repo implementation uses **analog AO** values and thresholds.
