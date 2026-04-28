#include "sensor.h"
#include "warning.h"

struct LoaderSensorState {
  int pin;
  const char* name;
  bool enabled;
  SensorGroup group;
  int threshold;
  int raw;
  bool warningActive;
  uint32_t highStartMs;
  uint32_t lowStartMs;
};

LoaderSensorState loaderSensors[LOADER_SENSOR_COUNT] = {
    {LOADER_PINS[0], "loader_1", false, GROUP_UNKNOWN, NEAR_THRESHOLD, 0, false, 0, 0},
    {LOADER_PINS[1], "loader_2", false, GROUP_UNKNOWN, NEAR_THRESHOLD, 0, false, 0, 0},
    {LOADER_PINS[2], "loader_3", false, GROUP_UNKNOWN, NEAR_THRESHOLD, 0, false, 0, 0},
    {LOADER_PINS[3], "loader_4", false, GROUP_UNKNOWN, NEAR_THRESHOLD, 0, false, 0, 0},
};

WorkStatus workStatus = STATUS_NOT_START;
uint32_t loaderAllClearStartMs = 0;
int wedgerRaw = 0;

static WarnStatusGroup g_warn_group = {};

bool hasValidPin(int pin) {
  return pin >= 0;
}

bool wedgerEnabled() {
  return hasValidPin(WEDGER_SENSOR_PIN);
}

const char* groupName(SensorGroup group) {
  if (group == GROUP_NEAR) {
    return "NEAR";
  }
  if (group == GROUP_FAR) {
    return "FAR";
  }
  return "UNKNOWN";
}

WarningType warning_main_type(WarningType type) {
  // Classify subtype back to main type; handle misclassifications
  if (type == WARNING_DISPLACED_BALE || type == WARNING_BROKEN_BALE ||
      type == WARNING_SENSOR_STATUS) {
    return WARNING_MAIN_LOADER;
  }
  if (type == WARNING_FEED_TIMEOUT) {
    return WARNING_MAIN_FLOW;
  }
  // If a main type is passed directly, return it as-is
  if (type == WARNING_MAIN_LOADER || type == WARNING_MAIN_FLOW) {
    return type;
  }
  // WARNING_NONE -> treat as UNDEFINED in MAIN_LOADER scope
  if (type == WARNING_NONE) {
    return WARNING_MAIN_LOADER;
  }
  // Already a placeholder (UNDEFINED, NO_SUB) -> return as-is
  return type;
}

bool warning_is_main_type(WarningType type) {
  return type == WARNING_MAIN_LOADER || type == WARNING_MAIN_FLOW;
}

bool warning_is_loader_subtype(WarningType type) {
  return type == WARNING_DISPLACED_BALE || type == WARNING_BROKEN_BALE ||
         type == WARNING_SENSOR_STATUS;
}

bool warning_is_flow_subtype(WarningType type) {
  return type == WARNING_FEED_TIMEOUT;
}

const char* warning_type_name(WarningType type) {
  switch (type) {
    case WARNING_NONE:
      return "none";
    case WARNING_MAIN_LOADER:
      return "main_loader";
    case WARNING_MAIN_FLOW:
      return "main_flow";
    case WARNING_UNDEFINED:
      return "undefined";
    case WARNING_NO_SUB:
      return "no_sub";
    case WARNING_DISPLACED_BALE:
      return "displaced_bale";
    case WARNING_BROKEN_BALE:
      return "broken_bale";
    case WARNING_SENSOR_STATUS:
      return "sensor_status";
    case WARNING_FEED_TIMEOUT:
      return "feed_timeout";
  }
  return "unknown";
}

const WarnStatusGroup* get_warn_status_group() {
  return &g_warn_group;
}

bool has_warn_status(WarningType type) {
  for (size_t i = 0; i < g_warn_group.count; ++i) {
    if (!g_warn_group.items[i].active) {
      continue;
    }
    if (warning_is_main_type(type)) {
      if (g_warn_group.items[i].mainType == type) {
        return true;
      }
    } else if (g_warn_group.items[i].type == type) {
      return true;
    }
  }
  return false;
}

WarnStatus* find_warn_status(WarningType type) {
  for (size_t i = 0; i < g_warn_group.count; ++i) {
    if (g_warn_group.items[i].active && g_warn_group.items[i].type == type) {
      return &g_warn_group.items[i];
    }
  }
  return nullptr;
}

WarnStatus* alloc_warn_status(WarningType type) {
  WarnStatus* existing = find_warn_status(type);
  if (existing != nullptr) {
    return existing;
  }

  if (g_warn_group.count >= WARN_STATUS_CAPACITY) {
    return nullptr;
  }

  WarnStatus* slot = &g_warn_group.items[g_warn_group.count++];
  slot->type = type;
  slot->mainType = warning_main_type(type);
  slot->severity = SEVERITY_INFO;
  slot->prevWorkStatus = workStatus;
  slot->startMs = 0;
  slot->lastLogMs = 0;
  slot->message = nullptr;
  slot->active = true;
  return slot;
}

const char* sensors_work_status_name(WorkStatus status) {
  if (status == STATUS_ON_WORK) {
    return "on_work";
  }
  if (status == STATUS_E_STOP) {
    return "e_stop";
  }
  if (status == STATUS_END) {
    return "end";
  }
  if (status == STATUS_END_DETECTION) {
    return "end_detection";
  }
  return "not_start";
}

void setWorkStatus(WorkStatus next, const char* reason) {
  if (workStatus == next) {
    return;
  }

  workStatus = next;
  Serial.print("[STATUS] ");
  Serial.print(sensors_work_status_name(workStatus));
  if (reason != nullptr) {
    Serial.print(" | ");
    Serial.println(reason);
  } else {
    Serial.println();
  }
}

WorkStatus sensors_get_work_status() {
  return workStatus;
}

void log_warn_status(const char* action, const WarnStatus& warn) {
  Serial.print("[WARN_STATUS] ");
  Serial.print(action);
  Serial.print(" type=");
  Serial.print(warning_type_name(warn.type));
  Serial.print(" main=");
  Serial.print(warning_type_name(warn.mainType));
  Serial.print(" sev=");
  Serial.print((int)warn.severity);
  Serial.print(" prev=");
  Serial.print(sensors_work_status_name(warn.prevWorkStatus));
  if (warn.message != nullptr) {
    Serial.print(" msg=");
    Serial.print(warn.message);
  }
  Serial.println();
}

void set_warn_status(WarningType type, WarningSeverity severity, const char* message) {
  uint32_t now = millis();
  WarnStatus* warn = alloc_warn_status(type);
  if (warn == nullptr) {
    Serial.print("[WARN_STATUS] drop type=");
    Serial.print(warning_type_name(type));
    Serial.println(" | warning group full");
    return;
  }

  bool isNew = warn->startMs == 0;
  bool changed = !isNew && (warn->severity != severity || warn->message != message);
  warn->mainType = warning_main_type(type);
  warn->severity = severity;
  warn->prevWorkStatus = workStatus;
  if (isNew) {
    warn->startMs = now;
  }
  warn->message = message;
  warn->active = true;

  if (isNew || changed || (now - warn->lastLogMs >= WARN_LOG_REPEAT_MS)) {
    warn->lastLogMs = now;
    log_warn_status(isNew ? "set" : "update", *warn);
  }
}

void clear_warn_status(WarningType type, const char* reason) {
  size_t i = 0;
  while (i < g_warn_group.count) {
    WarnStatus* warn = &g_warn_group.items[i];
    if (!warn->active) {
      ++i;
      continue;
    }
    bool match = warning_is_main_type(type) ? (warn->mainType == type) : (warn->type == type);
    if (!match) {
      ++i;
      continue;
    }
    log_warn_status("clear", *warn);
    if (reason != nullptr) {
      Serial.print("[WARN_STATUS] reason=");
      Serial.println(reason);
    }
    *warn = g_warn_group.items[g_warn_group.count - 1];
    g_warn_group.count -= 1;
  }
}

void clear_all_warn_status(const char* reason) {
  if (g_warn_group.count == 0) {
    return;
  }
  if (reason != nullptr) {
    Serial.print("[WARN_STATUS] clear_all reason=");
    Serial.println(reason);
  }
  for (size_t i = 0; i < g_warn_group.count; ++i) {
    log_warn_status("clear", g_warn_group.items[i]);
  }
  g_warn_group.count = 0;
}

// Skeleton warning handlers (TODO: implement these when design is finalized)
void handle_displaced_bale_warning() {
  return;
}

void handle_broken_bale_warning() {
  return;
}

void handle_sensor_status_warning() {
  return;
}

bool anyLoaderWarningActive() {
  for (size_t i = 0; i < LOADER_SENSOR_COUNT; ++i) {
    if (loaderSensors[i].enabled && loaderSensors[i].warningActive) {
      return true;
    }
  }
  return false;
}

bool anyLoaderHasStraw() {
  for (size_t i = 0; i < LOADER_SENSOR_COUNT; ++i) {
    if (!loaderSensors[i].enabled) {
      continue;
    }
    if (loaderSensors[i].raw < loaderSensors[i].threshold) {
      return true;
    }
  }
  return false;
}

/* 
### function bool allLoaderNoStraw ###

# Check if all Loader Sensors detect no straws
# params: none
# return: 
#       → false: one of the sensor have LoaderSensorState.raw lower than its set of threshold
#       → true: otherwise (if all sensors have LoaderSensorState.raw higher than its set of threshold)

* 检查Loader区域的红外传感器是否都检测到无干草
* 参数: 无
* 返回值:
*       → false: 其中一个传感器的LoaderSensorState.raw低于其设定threshold值
*       → true: 所有传感器的LoaderSensorState.raw皆高于其设定threshold值

*/
bool allLoaderNoStraw() {
  bool hasEnabled = false;
  for (size_t i = 0; i < LOADER_SENSOR_COUNT; ++i) {
    if (!loaderSensors[i].enabled) {
      continue;
    }
    hasEnabled = true;
    if (loaderSensors[i].raw < loaderSensors[i].threshold) {
      return false;
    }
  }
  return hasEnabled;
}

void classifyGroups(const uint16_t averages[LOADER_SENSOR_COUNT],
                    uint8_t enabledOrder[LOADER_SENSOR_COUNT],
                    size_t enabledCount,
                    int* farThresholdOut) {
  for (size_t i = 0; i < enabledCount; ++i) {
    for (size_t j = i + 1; j < enabledCount; ++j) {
      if (averages[enabledOrder[j]] < averages[enabledOrder[i]]) {
        uint8_t tmp = enabledOrder[i];
        enabledOrder[i] = enabledOrder[j];
        enabledOrder[j] = tmp;
      }
    }
  }

  for (size_t i = 0; i < LOADER_SENSOR_COUNT; ++i) {
    loaderSensors[i].group = GROUP_UNKNOWN;
    loaderSensors[i].threshold = NEAR_THRESHOLD;
  }

  if (enabledCount == 0) {
    *farThresholdOut = FAR_THRESHOLD_DEFAULT;
    return;
  }

  if (enabledCount == 1) {
    loaderSensors[enabledOrder[0]].group = GROUP_NEAR;
    loaderSensors[enabledOrder[0]].threshold = NEAR_THRESHOLD;
    *farThresholdOut = FAR_THRESHOLD_DEFAULT;
    return;
  }

  size_t nearCount = enabledCount / 2;
  if (nearCount == 0) {
    nearCount = 1;
  }

  for (size_t i = 0; i < nearCount; ++i) {
    loaderSensors[enabledOrder[i]].group = GROUP_NEAR;
    loaderSensors[enabledOrder[i]].threshold = NEAR_THRESHOLD;
  }

  uint32_t farSum = 0;
  size_t farCount = 0;
  for (size_t i = nearCount; i < enabledCount; ++i) {
    loaderSensors[enabledOrder[i]].group = GROUP_FAR;
    farSum += averages[enabledOrder[i]];
    farCount += 1;
  }

  int dynamicFarThreshold = FAR_THRESHOLD_DEFAULT;
  if (farCount > 0) {
    float farMean = farSum / static_cast<float>(farCount);
    dynamicFarThreshold = static_cast<int>(farMean * FAR_THRESHOLD_SCALE);
    if (dynamicFarThreshold < FAR_THRESHOLD_MIN) {
      dynamicFarThreshold = FAR_THRESHOLD_MIN;
    }
    if (dynamicFarThreshold > FAR_THRESHOLD_MAX) {
      dynamicFarThreshold = FAR_THRESHOLD_MAX;
    }
  }

  for (size_t i = nearCount; i < enabledCount; ++i) {
    loaderSensors[enabledOrder[i]].threshold = dynamicFarThreshold;
  }

  *farThresholdOut = dynamicFarThreshold;
}

void calibrateAndAssignGroups() {
  uint32_t sums[LOADER_SENSOR_COUNT] = {0, 0, 0, 0};
  uint32_t counts[LOADER_SENSOR_COUNT] = {0, 0, 0, 0};
  uint16_t averages[LOADER_SENSOR_COUNT] = {0, 0, 0, 0};
  uint8_t enabledOrder[LOADER_SENSOR_COUNT] = {0, 0, 0, 0};
  size_t enabledCount = 0;

  for (size_t i = 0; i < LOADER_SENSOR_COUNT; ++i) {
    if (loaderSensors[i].enabled) {
      enabledOrder[enabledCount] = static_cast<uint8_t>(i);
      enabledCount += 1;
    }
  }

  if (enabledCount == 0) {
    Serial.println("[WARN] No loader sensor pins configured.");
    return;
  }

  if (enabledCount > 1) {
    Serial.println("Calibrating loader sensors for auto grouping...");
    uint32_t startMs = millis();
    while (millis() - startMs < CALIBRATION_MS) {
      for (size_t i = 0; i < enabledCount; ++i) {
        uint8_t idx = enabledOrder[i];
        sums[idx] += analogRead(loaderSensors[idx].pin);
        counts[idx] += 1;
      }
      delay(CALIBRATION_INTERVAL_MS);
    }

    for (size_t i = 0; i < enabledCount; ++i) {
      uint8_t idx = enabledOrder[i];
      if (counts[idx] > 0) {
        averages[idx] = static_cast<uint16_t>(sums[idx] / counts[idx]);
      }
    }
  }

  int farThreshold = FAR_THRESHOLD_DEFAULT;
  classifyGroups(averages, enabledOrder, enabledCount, &farThreshold);

  if (enabledCount == 1) {
    Serial.println("[WARN] Only one loader sensor configured, grouping is partial.");
  }

  Serial.print("Auto grouping complete. FAR threshold=");
  Serial.println(farThreshold);
  for (size_t i = 0; i < LOADER_SENSOR_COUNT; ++i) {
    Serial.print(loaderSensors[i].name);
    if (!loaderSensors[i].enabled) {
      Serial.println(" pin=TBD group=UNKNOWN threshold=TBD");
      continue;
    }

    Serial.print(" pin=");
    Serial.print(loaderSensors[i].pin);
    Serial.print(" avg=");
    Serial.print(averages[i]);
    Serial.print(" group=");
    Serial.print(groupName(loaderSensors[i].group));
    Serial.print(" threshold=");
    Serial.println(loaderSensors[i].threshold);
  }
}

void updateSingleLoaderWarning(LoaderSensorState* sensor, uint32_t now) {
  bool noStraw = sensor->raw > sensor->threshold;

  if (noStraw) {
    sensor->lowStartMs = 0;
    if (sensor->highStartMs == 0) {
      sensor->highStartMs = now;
    }

    if (!sensor->warningActive && (now - sensor->highStartMs >= ERROR_HOLD_MS)) {
      sensor->warningActive = true;
      Serial.print("[ERROR] ");
      Serial.print(sensor->name);
      Serial.print(" no straw for >= ");
      Serial.print(ERROR_HOLD_MS);
      Serial.print(" ms. raw=");
      Serial.print(sensor->raw);
      Serial.print(" threshold=");
      Serial.println(sensor->threshold);
    }
  } else {
    sensor->highStartMs = 0;
    if (sensor->warningActive) {
      if (sensor->lowStartMs == 0) {
        sensor->lowStartMs = now;
      }

      if (now - sensor->lowStartMs >= CLEAR_HOLD_MS) {
        sensor->warningActive = false;
        sensor->lowStartMs = 0;
        Serial.print("[RECOVER] ");
        Serial.print(sensor->name);
        Serial.print(" stable with straw for ");
        Serial.print(CLEAR_HOLD_MS);
        Serial.println(" ms.");
      }
    }
  }
}

void updateLoaderSensors(uint32_t now) {
  for (size_t i = 0; i < LOADER_SENSOR_COUNT; ++i) {
    LoaderSensorState* sensor = &loaderSensors[i];
    if (!sensor->enabled) {
      continue;
    }

    sensor->raw = analogRead(sensor->pin);

    if (workStatus == STATUS_NOT_START || workStatus == STATUS_END ||
        workStatus == STATUS_E_STOP) {
      sensor->warningActive = false;
      sensor->highStartMs = 0;
      sensor->lowStartMs = 0;
      continue;
    }

    updateSingleLoaderWarning(sensor, now);
  }
}

void updateWedgerSensor() {
  if (!wedgerEnabled()) {
    return;
  }
  wedgerRaw = analogRead(WEDGER_SENSOR_PIN);
}

void resetFlowTracking() {
  loaderAllClearStartMs = 0;
  clear_warn_status(WARNING_MAIN_FLOW, "flow reset");
  clear_warn_status(WARNING_FEED_TIMEOUT, "flow reset");
}

bool isFlowTransitionStatus(WorkStatus status) {
  (void)status;
  return has_warn_status(WARNING_MAIN_FLOW);
}

void processWorkFlow(uint32_t now) {
  if (workStatus == STATUS_END || workStatus == STATUS_E_STOP) {
    return;
  }

  if (workStatus == STATUS_NOT_START) {
    if (anyLoaderHasStraw()) {
      setWorkStatus(STATUS_ON_WORK, "First loader straw detected.");
    } else {
      return;
    }
  }

  bool hasStrawAtLoader = anyLoaderHasStraw();
  bool noStrawAtLoader = allLoaderNoStraw();

  if (hasStrawAtLoader) {
    if (loaderAllClearStartMs != 0 || isFlowTransitionStatus(workStatus)) {
      resetFlowTracking();
      setWorkStatus(STATUS_ON_WORK, "[FLOW] Straw returned at loader. Detection timer reset.");
    }
    return;
  }

  if (!noStrawAtLoader) {
    return;
  }

  if (loaderAllClearStartMs == 0) {
    loaderAllClearStartMs = now;
    set_warn_status(WARNING_MAIN_FLOW, SEVERITY_NORMAL, "[FLOW] Loader sensors clear for >2s. Detecting possible warning.");
    return;
  }

  uint32_t noStrawElapsed = now - loaderAllClearStartMs;

  if (noStrawElapsed >= FLOW_END_WARNING_HOLD_MS && !has_warn_status(WARNING_MAIN_FLOW) &&
      workStatus != STATUS_END_DETECTION) {
    set_warn_status(WARNING_MAIN_FLOW, SEVERITY_NORMAL, "[FLOW] Loader sensors clear for >2s. Detecting possible warning.");
  }

  if (noStrawElapsed >= FLOW_END_DETECTION_HOLD_MS && workStatus != STATUS_END_DETECTION) {
    setWorkStatus(STATUS_END_DETECTION,
                  "[FLOW] Loader sensors clear for >5s. Start End Detection and clear error.");
  }

  if (noStrawElapsed < FEED_PROCESS_TIME_MS) {
    return;
  }

  if (!wedgerEnabled()) {
    if (!has_warn_status(WARNING_FEED_TIMEOUT)) {
      set_warn_status(WARNING_FEED_TIMEOUT, SEVERITY_IMPORTANT,
                      "Feeder timeout reached, wedger sensor still < threshold.");
    }
    return;
  }

  if (wedgerRaw < WEDGER_THRESHOLD) {
    if (!has_warn_status(WARNING_FEED_TIMEOUT)) {
      set_warn_status(WARNING_FEED_TIMEOUT, SEVERITY_IMPORTANT,
                      "Feeder timeout reached, wedger sensor still < threshold.");
    }
  } else {
    resetFlowTracking();
    setWorkStatus(STATUS_END, "Feed process completed and wedger check passed.");
  }
}

void refreshWorkStatusByWarnings() {
  if (workStatus == STATUS_NOT_START || workStatus == STATUS_END ||
      workStatus == STATUS_E_STOP) {
    return;
  }

  bool loaderWarn = anyLoaderWarningActive();

  if (loaderWarn) {
    if (!has_warn_status(WARNING_MAIN_LOADER)) {
      set_warn_status(WARNING_MAIN_LOADER, SEVERITY_NORMAL, "Loader warning active, system keeps running.");
    }
  } else {
    clear_warn_status(WARNING_MAIN_LOADER, "Loader warnings cleared.");
  }

  if (workStatus != STATUS_ON_WORK && g_warn_group.count == 0) {
    setWorkStatus(STATUS_ON_WORK, "Warnings cleared.");
  }
}

void printStatus(uint32_t now) {
  static uint32_t lastPrintMs = 0;
  if (now - lastPrintMs < PRINT_PERIOD_MS) {
    return;
  }
  lastPrintMs = now;

  Serial.print("STATUS=");
  Serial.print(sensors_work_status_name(workStatus));
  Serial.print(" | ");

  const WarnStatusGroup* warnGroup = get_warn_status_group();
  Serial.print("warns=");
  if (warnGroup->count == 0) {
    Serial.print("none");
  } else {
    bool printedWarn = false;
    for (size_t i = 0; i < warnGroup->count; ++i) {
      const WarnStatus& warn = warnGroup->items[i];
      if (!warn.active) {
        continue;
      }
      if (printedWarn) {
        Serial.print(";");
      }
      printedWarn = true;
      Serial.print(warning_type_name(warn.type));
      Serial.print("(");
      Serial.print(warning_type_name(warn.mainType));
      Serial.print(",sev=");
      Serial.print((int)warn.severity);
      Serial.print(")");
    }
  }
  Serial.print(" | ");

  for (size_t i = 0; i < LOADER_SENSOR_COUNT; ++i) {
    LoaderSensorState* sensor = &loaderSensors[i];
    Serial.print(sensor->name);
    if (!sensor->enabled) {
      Serial.print("(pin=TBD) ");
      continue;
    }

    float v = (sensor->raw / 4095.0f) * ADC_FULL_SCALE_V;
    Serial.print("(");
    Serial.print(groupName(sensor->group));
    Serial.print(",raw=");
    Serial.print(sensor->raw);
    Serial.print(",thr=");
    Serial.print(sensor->threshold);
    Serial.print(",V=");
    Serial.print(v, 3);
    Serial.print(",");
    Serial.print(sensor->warningActive ? "WARNING" : "OK");
    Serial.print(") ");
  }

  if (wedgerEnabled()) {
    float wedgerV = (wedgerRaw / 4095.0f) * ADC_FULL_SCALE_V;
    Serial.print("wedger(raw=");
    Serial.print(wedgerRaw);
    Serial.print(",thr=");
    Serial.print(WEDGER_THRESHOLD);
    Serial.print(",V=");
    Serial.print(wedgerV, 3);
    Serial.print(") ");
  } else {
    Serial.print("wedger(pin=TBD) ");
  }

  if (loaderAllClearStartMs != 0) {
    uint32_t elapsed = now - loaderAllClearStartMs;
    uint32_t leftMs =
        (elapsed >= FEED_PROCESS_TIME_MS) ? 0 : (FEED_PROCESS_TIME_MS - elapsed);
    Serial.print("feed_timer_left_ms=");
    Serial.print(leftMs);
  }

  Serial.println();
}

void sensors_setup() {
  Serial.begin(115200);
  delay(300);

  analogReadResolution(12); // 0..4095
  for (size_t i = 0; i < LOADER_SENSOR_COUNT; ++i) {
    loaderSensors[i].enabled = hasValidPin(loaderSensors[i].pin);
    if (loaderSensors[i].enabled) {
      pinMode(loaderSensors[i].pin, INPUT);
    }
  }

  if (wedgerEnabled()) {
    pinMode(WEDGER_SENSOR_PIN, INPUT);
  }

  Serial.println("IR AO loader/wedger monitor start (ESP32-S3)");
  Serial.print("Near threshold=");
  Serial.print(NEAR_THRESHOLD);
  Serial.print(" | Error hold(ms)=");
  Serial.print(ERROR_HOLD_MS);
  Serial.print(" | Clear hold(ms)=");
  Serial.print(CLEAR_HOLD_MS);
  Serial.print(" | Feed process(ms)=");
  Serial.println(FEED_PROCESS_TIME_MS);

  for (size_t i = 0; i < LOADER_SENSOR_COUNT; ++i) {
    Serial.print(loaderSensors[i].name);
    Serial.print(" pin=");
    if (loaderSensors[i].enabled) {
      Serial.println(loaderSensors[i].pin);
    } else {
      Serial.println("TBD");
    }
  }

  Serial.print("wedger_sensor pin=");
  if (wedgerEnabled()) {
    Serial.println(WEDGER_SENSOR_PIN);
  } else {
    Serial.println("TBD");
  }

  calibrateAndAssignGroups();
}

void sensors_loop() {
  static uint32_t lastSampleMs = 0;
  uint32_t now = millis();
  if (now - lastSampleMs < SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleMs = now;

  updateLoaderSensors(now);
  updateWedgerSensor();
  processWorkFlow(now);
  refreshWorkStatusByWarnings();
  printStatus(now);
}
