#ifndef WARNING_H
#define WARNING_H

#include <Arduino.h>

// WorkStatus enum (also defined in sensor.h, included here for warning system completeness)
enum WorkStatus {
  STATUS_ON_WORK,
  STATUS_E_STOP,
  STATUS_END,
  STATUS_END_DETECTION,
  STATUS_NOT_START,
};

enum WarningType {
  WARNING_NONE,
  WARNING_LOW_COVERAGE,            // sustained low coverage
  WARNING_GAP_DETECTED,            // coverage drop event
  WARNING_SENSOR_MISCONFIG,        // zone mismatch / misconfig
  WARNING_RANGE_GROUP_BLIND_NEAR,  // near layer blind
  WARNING_RANGE_GROUP_BLIND_FAR,   // far layer blind
  WARNING_FLOW_END_DETECTING,      // end detecting (coverage==0)
  WARNING_FEED_TIMEOUT,            // timeout waiting for S5 confirm
};

enum WarningSeverity {
  SEVERITY_INFO,
  SEVERITY_NORMAL,
  SEVERITY_IMPORTANT,
  SEVERITY_SEVERE,
};

struct WarnStatus {
  WarningType type;
  WarningSeverity severity;
  WorkStatus prevWorkStatus;
  uint32_t startMs;
  uint32_t lastLogMs;
  const char* message;
  bool active;
};

constexpr size_t WARN_STATUS_CAPACITY = 8;
constexpr uint32_t WARN_LOG_REPEAT_MS = 3000;

struct WarnStatusGroup {
  WarnStatus items[WARN_STATUS_CAPACITY];
  size_t count;
};

// Warning API
const char* warning_type_name(WarningType type);
const WarnStatusGroup* get_warn_status_group();
bool has_warn_status(WarningType type);
void set_warn_status(WarningType type, WarningSeverity severity, const char* message);
void clear_warn_status(WarningType type, const char* reason);
void clear_all_warn_status(const char* reason);

#endif // WARNING_H
