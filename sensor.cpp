#include <Arduino.h>

static const size_t LOADER_SENSOR_COUNT = 4;

// Loader Sensor
// Sensor 1: GPIO10.
// Other loader pins are TBD until hardware mapping is finalized.
static const int LOADER_PIN_1 = 10;
static const int LOADER_PIN_2 = -1; // TBD
static const int LOADER_PIN_3 = -1; // TBD
static const int LOADER_PIN_4 = -1; // TBD

// 5th sensor between feeder and wedger.
static const int WEDGER_SENSOR_PIN = -1; // TBD

static const int LOADER_PINS[LOADER_SENSOR_COUNT] = {
    LOADER_PIN_1,
    LOADER_PIN_2,
    LOADER_PIN_3,
    LOADER_PIN_4,
};

// Preset Value 预设值
static const uint32_t PRINT_PERIOD_MS = 100;    // Time between print 检测间隔时间值
static const uint32_t SAMPLE_INTERVAL_MS = 5;
static const uint32_t CALIBRATION_MS = 1500;
static const uint32_t CALIBRATION_INTERVAL_MS = 5;
static const uint32_t ERROR_HOLD_MS = 2500;
static const uint32_t CLEAR_HOLD_MS = 1000;
static const uint32_t FEED_PROCESS_TIME_MS = 18000; // Feeding total time 总运输时间
static const uint32_t FLOW_WARNING_REPEAT_MS = 2000;

static const int NEAR_THRESHOLD = 300;
static const int FAR_THRESHOLD_MIN = 400;
static const int FAR_THRESHOLD_MAX = 700;
static const int FAR_THRESHOLD_DEFAULT = 550;
static const float FAR_THRESHOLD_SCALE = 0.85f;

// After feed_process_time, wedger_raw < WEDGER_THRESHOLD means warning but keep running.
static const int WEDGER_THRESHOLD = 300;

static const float ADC_FULL_SCALE_V = 3.3f;

enum SensorGroup {
  GROUP_NEAR,
  GROUP_FAR,
  GROUP_UNKNOWN,
};

enum WorkStatus {
  STATUS_ON_WORK,
  STATUS_WARNING_WORK,
  STATUS_E_STOP,
  STATUS_END,
  STATUS_NOT_START,
};

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
bool flowWarningActive = false;
uint32_t loaderAllClearStartMs = 0;
uint32_t lastFlowWarningMs = 0;
int wedgerRaw = 0;

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

const char* workStatusName(WorkStatus status) {
  if (status == STATUS_ON_WORK) {
    return "on_work";
  }
  if (status == STATUS_WARNING_WORK) {
    return "warning_work";
  }
  if (status == STATUS_E_STOP) {
    return "e_stop";
  }
  if (status == STATUS_END) {
    return "end";
  }
  return "not_start";
}

void setWorkStatus(WorkStatus next, const char* reason) {
  if (workStatus == next) {
    return;
  }

  workStatus = next;
  Serial.print("[STATUS] ");
  Serial.print(workStatusName(workStatus));
  if (reason != nullptr) {
    Serial.print(" | ");
    Serial.println(reason);
  } else {
    Serial.println();
  }
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

void warnFlowIfNeeded(uint32_t now, const char* message) {
  if (lastFlowWarningMs == 0 || now - lastFlowWarningMs >= FLOW_WARNING_REPEAT_MS) {
    Serial.print("[WARN] ");
    Serial.println(message);
    lastFlowWarningMs = now;
  }
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

  bool noStrawAtLoader = allLoaderNoStraw();
  if (noStrawAtLoader) {
    if (loaderAllClearStartMs == 0) {
      loaderAllClearStartMs = now;
      Serial.println("[FLOW] Loader sensors all clear. Start feeder timer.");
    }
  } else {
    if (loaderAllClearStartMs != 0) {
      loaderAllClearStartMs = 0;
      flowWarningActive = false;
      Serial.println("[FLOW] Straw returned at loader. Feeder timer reset.");
    }
    return;
  }

  if (loaderAllClearStartMs == 0 ||
      now - loaderAllClearStartMs < FEED_PROCESS_TIME_MS) {
    flowWarningActive = false;
    return;
  }

  if (!wedgerEnabled()) {
    flowWarningActive = true;
    warnFlowIfNeeded(now, "Wedger sensor pin is TBD; cannot verify feed completion.");
    return;
  }

  if (wedgerRaw < WEDGER_THRESHOLD) {
    flowWarningActive = true;
    warnFlowIfNeeded(now, "Feeder timeout reached, wedger sensor still < threshold.");
  } else {
    flowWarningActive = false;
    setWorkStatus(STATUS_END, "Feed process completed and wedger check passed.");
  }
}

void refreshWorkStatusByWarnings() {
  if (workStatus == STATUS_NOT_START || workStatus == STATUS_END ||
      workStatus == STATUS_E_STOP) {
    return;
  }

  bool loaderWarn = anyLoaderWarningActive();
  bool hasWarning = loaderWarn || flowWarningActive;

  if (hasWarning) {
    if (flowWarningActive) {
      setWorkStatus(STATUS_WARNING_WORK, "Flow warning active, system keeps running.");
    } else {
      setWorkStatus(STATUS_WARNING_WORK, "Loader warning active, system keeps running.");
    }
  } else {
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
  Serial.print(workStatusName(workStatus));
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

void setup() {
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

void loop() {
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
