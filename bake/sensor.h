#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include "warning.h"

// Loader sensor count and GPIO mapping.
// Known pin: loader_1 -> GPIO10.
constexpr size_t LOADER_SENSOR_COUNT = 4;

constexpr int LOADER_PIN_1 = 10;
constexpr int LOADER_PIN_2 = -1; // TBD
constexpr int LOADER_PIN_3 = -1; // TBD
constexpr int LOADER_PIN_4 = -1; // TBD

// 5th sensor between feeder and wedger.
constexpr int WEDGER_SENSOR_PIN = -1; // TBD

constexpr int LOADER_PINS[LOADER_SENSOR_COUNT] = {
    LOADER_PIN_1,
    LOADER_PIN_2,
    LOADER_PIN_3,
    LOADER_PIN_4,
};

// ============================
// Runtime Config
// ============================

// Timing (ms)
constexpr uint32_t PRINT_PERIOD_MS = 500;
constexpr uint32_t SAMPLE_INTERVAL_MS = 5;
constexpr uint32_t CALIBRATION_MS = 1500;
constexpr uint32_t CALIBRATION_INTERVAL_MS = 5;
constexpr uint32_t ERROR_HOLD_MS = 2500;
constexpr uint32_t CLEAR_HOLD_MS = 1000;
constexpr uint32_t FLOW_END_WARNING_HOLD_MS = 2000;
constexpr uint32_t FLOW_END_DETECTION_HOLD_MS = 5000;
constexpr uint32_t FEED_PROCESS_TIME_MS = 18000;
constexpr uint32_t FLOW_WARNING_REPEAT_MS = 2000;

// Thresholds
constexpr int NEAR_THRESHOLD = 300;
constexpr int FAR_THRESHOLD_MIN = 400;
constexpr int FAR_THRESHOLD_MAX = 700;
constexpr int FAR_THRESHOLD_DEFAULT = 550;
constexpr float FAR_THRESHOLD_SCALE = 0.85f;
constexpr int WEDGER_THRESHOLD = 300;

// ADC conversion
constexpr float ADC_FULL_SCALE_V = 3.3f;

enum SensorGroup {
  GROUP_NEAR,
  GROUP_FAR,
  GROUP_UNKNOWN,
};

// Forward declaration for LoaderSensorState (defined in sensor.cpp)
struct LoaderSensorState;

// ============================
// Sensor Interface & Initialization
// ============================

void sensors_setup();
void sensors_loop();

WorkStatus sensors_get_work_status();
const char* sensors_work_status_name(WorkStatus status);

// ============================
// Internal Sensor Processing Functions (declared for completeness)
// ============================

void setWorkStatus(WorkStatus next, const char* reason);
bool anyLoaderWarningActive();
bool anyLoaderHasStraw();
bool allLoaderNoStraw();
void calibrateAndAssignGroups();
void updateSingleLoaderWarning(LoaderSensorState* sensor, uint32_t now);
void updateLoaderSensors(uint32_t now);
void updateWedgerSensor();
void resetFlowTracking();
bool isFlowTransitionStatus(WorkStatus status);
void processWorkFlow(uint32_t now);
void refreshWorkStatusByWarnings();
void printStatus(uint32_t now);

#endif // SENSOR_H
