#include "sensor.h"
#include "logger.h"

struct LoaderSensorState {
  int pin;
  const char* name;
  bool enabled;
  int threshold;
  int raw;
  bool det;
};

LoaderSensorState loaderSensors[LOADER_SENSOR_COUNT] = {
    {LOADER_PINS[0], "loader_1", false, NEAR_THRESHOLD, -1, false},
    {LOADER_PINS[1], "loader_2", false, NEAR_THRESHOLD, -1, false},
    {LOADER_PINS[2], "loader_3", false, NEAR_THRESHOLD, -1, false},
    {LOADER_PINS[3], "loader_4", false, NEAR_THRESHOLD, -1, false},
};

WorkStatus workStatus = STATUS_NOT_START;
int wedgerRaw = -1;
bool wedgerDet = false;

static WarnStatusGroup g_warn_group = {};

static int coverage = 0;
static int nearCount = 0;
static int farCount = 0;
static bool zone1Mismatch = false;
static bool zone2Mismatch = false;
static int pairA_raw = -1;
static int pairB_raw = -1;
static bool pairA_enabled = false;
static bool pairB_enabled = false;
static bool nearPairIsA = true;
static bool nearPairAssigned = false;
static bool nearFarAmbiguous = false;
static int prevCoverage = 0;

static uint32_t startHoldMs = 0;
static uint32_t lowHold2Ms = 0;
static uint32_t lowHold1Ms = 0;
static uint32_t gapHoldMs = 0;
static uint32_t zone1MismatchHoldMs = 0;
static uint32_t zone2MismatchHoldMs = 0;
static uint32_t rangeBlindNearHoldMs = 0;
static uint32_t rangeBlindFarHoldMs = 0;
static uint32_t endDetectHoldMs = 0;
static uint32_t endDetectStartMs = 0;
static uint32_t s5ConfirmHoldMs = 0;
static uint32_t nearFarAmbigHoldMs = 0;

bool hasValidPin(int pin) {
  return pin >= 0;
}

bool wedgerEnabled() {
  return hasValidPin(WEDGER_SENSOR_PIN);
}

bool updateHold(bool condition, uint32_t now, uint32_t holdMs, uint32_t* startMs) {
  if (!condition) {
    *startMs = 0;
    return false;
  }
  if (*startMs == 0) {
    *startMs = now;
  }
  return (now - *startMs) >= holdMs;
}

bool inNearBand(int raw) {
  return raw >= 0 && raw <= NEAR_THRESHOLD;
}

bool inFarBand(int raw) {
  return raw >= FAR_THRESHOLD_MIN && raw <= FAR_THRESHOLD_MAX;
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
  char msg[128] = {0};
  if (reason != nullptr) {
    snprintf(msg, sizeof(msg), "status=%s reason=%s", sensors_work_status_name(workStatus), reason);
  } else {
    snprintf(msg, sizeof(msg), "status=%s", sensors_work_status_name(workStatus));
  }
  logger_event("status", msg);
}

WorkStatus sensors_get_work_status() {
  return workStatus;
}

void log_warn_status(const char* action, const WarnStatus& warn) {
  char msg[192] = {0};
  if (warn.message != nullptr) {
    snprintf(msg, sizeof(msg), "warn=%s main=%s sev=%d prev=%s msg=%s",
             warning_type_name(warn.type),
             warning_type_name(warn.mainType),
             static_cast<int>(warn.severity),
             sensors_work_status_name(warn.prevWorkStatus),
             warn.message);
  } else {
    snprintf(msg, sizeof(msg), "warn=%s main=%s sev=%d prev=%s",
             warning_type_name(warn.type),
             warning_type_name(warn.mainType),
             static_cast<int>(warn.severity),
             sensors_work_status_name(warn.prevWorkStatus));
  }
  logger_event(action, msg);
}

void set_warn_status(WarningType type, WarningSeverity severity, const char* message) {
  uint32_t now = millis();
  WarnStatus* warn = alloc_warn_status(type);
  if (warn == nullptr) {
    char msg[96] = {0};
    snprintf(msg, sizeof(msg), "warn=%s drop=group_full", warning_type_name(type));
    logger_event("warn_drop", msg);
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
    *warn = g_warn_group.items[g_warn_group.count - 1];
    g_warn_group.count -= 1;
  }
}

void clear_all_warn_status(const char* reason) {
  if (g_warn_group.count == 0) {
    return;
  }
  if (reason != nullptr) {
    char msg[128] = {0};
    snprintf(msg, sizeof(msg), "reason=%s", reason);
    logger_event("warn_clear_all", msg);
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

void updateLoaderSensors(uint32_t now) {
  (void)now;
  for (size_t i = 0; i < LOADER_SENSOR_COUNT; ++i) {
    LoaderSensorState* sensor = &loaderSensors[i];
    if (!sensor->enabled) {
      sensor->raw = -1;
      sensor->det = false;
      continue;
    }

    sensor->raw = analogRead(sensor->pin);
    sensor->det = (sensor->raw < sensor->threshold);
  }
}

void updateWedgerSensor() {
  if (!wedgerEnabled()) {
    wedgerRaw = -1;
    wedgerDet = false;
    return;
  }
  wedgerRaw = analogRead(WEDGER_SENSOR_PIN);
  wedgerDet = (wedgerRaw < WEDGER_THRESHOLD);
}

void computeDerivedSignals(uint32_t now) {
  (void)now;
  coverage = 0;
  for (size_t i = 0; i < LOADER_SENSOR_COUNT; ++i) {
    if (loaderSensors[i].enabled && loaderSensors[i].det) {
      coverage += 1;
    }
  }

  zone1Mismatch = loaderSensors[0].enabled && loaderSensors[2].enabled &&
                  (loaderSensors[0].det != loaderSensors[2].det);
  zone2Mismatch = loaderSensors[1].enabled && loaderSensors[3].enabled &&
                  (loaderSensors[1].det != loaderSensors[3].det);

  auto computePairRaw = [&](int idx1, int idx2, int* outRaw, bool* outEnabled) {
    bool en1 = loaderSensors[idx1].enabled;
    bool en2 = loaderSensors[idx2].enabled;
    if (!en1 && !en2) {
      *outRaw = -1;
      *outEnabled = false;
      return;
    }
    *outEnabled = true;
    if (en1 && en2) {
      *outRaw = (loaderSensors[idx1].raw < loaderSensors[idx2].raw)
                    ? loaderSensors[idx1].raw
                    : loaderSensors[idx2].raw;
    } else if (en1) {
      *outRaw = loaderSensors[idx1].raw;
    } else {
      *outRaw = loaderSensors[idx2].raw;
    }
  };

  computePairRaw(0, 1, &pairA_raw, &pairA_enabled);
  computePairRaw(2, 3, &pairB_raw, &pairB_enabled);

  bool pairA_near = pairA_enabled && inNearBand(pairA_raw);
  bool pairA_far = pairA_enabled && inFarBand(pairA_raw);
  bool pairB_near = pairB_enabled && inNearBand(pairB_raw);
  bool pairB_far = pairB_enabled && inFarBand(pairB_raw);

  nearFarAmbiguous = false;
  if (pairA_near && pairB_far) {
    nearPairIsA = true;
    nearPairAssigned = true;
    nearFarAmbigHoldMs = 0;
  } else if (pairB_near && pairA_far) {
    nearPairIsA = false;
    nearPairAssigned = true;
    nearFarAmbigHoldMs = 0;
  } else {
    nearFarAmbiguous = true;
  }

  if (!nearPairAssigned) {
    nearPairIsA = true;
  }

  nearCount = 0;
  farCount = 0;
  if (nearPairIsA) {
    if (loaderSensors[0].enabled && loaderSensors[0].det) {
      nearCount += 1;
    }
    if (loaderSensors[1].enabled && loaderSensors[1].det) {
      nearCount += 1;
    }
    if (loaderSensors[2].enabled && loaderSensors[2].det) {
      farCount += 1;
    }
    if (loaderSensors[3].enabled && loaderSensors[3].det) {
      farCount += 1;
    }
  } else {
    if (loaderSensors[2].enabled && loaderSensors[2].det) {
      nearCount += 1;
    }
    if (loaderSensors[3].enabled && loaderSensors[3].det) {
      nearCount += 1;
    }
    if (loaderSensors[0].enabled && loaderSensors[0].det) {
      farCount += 1;
    }
    if (loaderSensors[1].enabled && loaderSensors[1].det) {
      farCount += 1;
    }
  }
}

void resetFlowTracking() {
  endDetectHoldMs = 0;
  endDetectStartMs = 0;
  s5ConfirmHoldMs = 0;
  clear_warn_status(WARNING_MAIN_FLOW, "flow reset");
  clear_warn_status(WARNING_FEED_TIMEOUT, "flow reset");
}

void processWorkFlow(uint32_t now) {
  if (workStatus == STATUS_END || workStatus == STATUS_E_STOP) {
    return;
  }

  if (workStatus == STATUS_NOT_START) {
    if (updateHold(coverage >= 2, now, T_START_HOLD, &startHoldMs)) {
      setWorkStatus(STATUS_ON_WORK, "start: coverage>=2");
      clear_all_warn_status("start");
      startHoldMs = 0;
    } else {
      return;
    }
  }

  bool sensorStatusActive = false;
  const char* sensorStatusMsg = nullptr;

  bool zone1Hold = updateHold(zone1Mismatch, now, T_MISMATCH_HOLD, &zone1MismatchHoldMs);
  bool zone2Hold = updateHold(zone2Mismatch, now, T_MISMATCH_HOLD, &zone2MismatchHoldMs);
  bool ambigHold = updateHold(nearFarAmbiguous, now, T_MISMATCH_HOLD, &nearFarAmbigHoldMs);

  bool nearEnabled = nearPairIsA ? pairA_enabled : pairB_enabled;
  bool farEnabled = nearPairIsA ? pairB_enabled : pairA_enabled;
  bool nearBlindHold = false;
  bool farBlindHold = false;

  if (nearEnabled) {
    nearBlindHold = updateHold(nearCount == 0, now, T_RANGE_BLIND_HOLD, &rangeBlindNearHoldMs);
  } else {
    rangeBlindNearHoldMs = 0;
  }

  if (farEnabled) {
    farBlindHold = updateHold(farCount == 0, now, T_RANGE_BLIND_HOLD, &rangeBlindFarHoldMs);
  } else {
    rangeBlindFarHoldMs = 0;
  }

  if (zone1Hold) {
    sensorStatusActive = true;
    sensorStatusMsg = "zone1 mismatch (S1!=S3)";
  } else if (zone2Hold) {
    sensorStatusActive = true;
    sensorStatusMsg = "zone2 mismatch (S2!=S4)";
  } else if (ambigHold) {
    sensorStatusActive = true;
    sensorStatusMsg = "near/far ambiguous";
  } else if (nearBlindHold) {
    sensorStatusActive = true;
    sensorStatusMsg = "near layer blind";
  } else if (farBlindHold) {
    sensorStatusActive = true;
    sensorStatusMsg = "far layer blind";
  }

  if (sensorStatusActive) {
    set_warn_status(WARNING_SENSOR_STATUS, SEVERITY_NORMAL, sensorStatusMsg);
  } else {
    clear_warn_status(WARNING_SENSOR_STATUS, "sensor status cleared");
  }

  bool brokenActive = false;
  const char* brokenMsg = nullptr;

  bool low2Hold = updateHold(coverage == 2, now, T_LOW_HOLD, &lowHold2Ms);
  bool low1Hold = updateHold(coverage <= 1, now, T_LOW_HOLD, &lowHold1Ms);

  bool gapCond = (workStatus == STATUS_ON_WORK && prevCoverage >= 3 && coverage <= 1);
  bool gapHold = updateHold(gapCond, now, T_GAP_HOLD, &gapHoldMs);

  if (gapHold) {
    brokenActive = true;
    brokenMsg = "gap detected";
  } else if (low1Hold) {
    brokenActive = true;
    brokenMsg = "low coverage<=1";
  } else if (low2Hold) {
    brokenActive = true;
    brokenMsg = "low coverage=2";
  }

  if (brokenActive) {
    set_warn_status(WARNING_BROKEN_BALE, SEVERITY_NORMAL, brokenMsg);
  } else {
    clear_warn_status(WARNING_BROKEN_BALE, "coverage recovered");
  }

  if (workStatus == STATUS_ON_WORK) {
    if (updateHold(coverage == 0, now, T_END_DETECT_HOLD, &endDetectHoldMs)) {
      setWorkStatus(STATUS_END_DETECTION, "end detecting (coverage==0)");
      set_warn_status(WARNING_MAIN_FLOW, SEVERITY_NORMAL, "end detecting (coverage==0)");
      endDetectStartMs = now;
      s5ConfirmHoldMs = 0;
    }
  }

  if (workStatus == STATUS_END_DETECTION) {
    if (endDetectStartMs == 0) {
      endDetectStartMs = now;
    }

    if (wedgerEnabled()) {
      if (updateHold(wedgerDet, now, T_S5_CONFIRM_HOLD, &s5ConfirmHoldMs)) {
        setWorkStatus(STATUS_END, "S5 confirmed end");
        clear_warn_status(WARNING_MAIN_FLOW, "S5 confirmed");
        clear_warn_status(WARNING_FEED_TIMEOUT, "S5 confirmed");
      }
    } else {
      s5ConfirmHoldMs = 0;
    }

    if (now - endDetectStartMs >= T_FEED_PROCESS) {
      set_warn_status(WARNING_FEED_TIMEOUT, SEVERITY_NORMAL, "timeout waiting for S5 confirm");
    }
  } else {
    endDetectStartMs = 0;
    s5ConfirmHoldMs = 0;
    if (coverage > 0) {
      clear_warn_status(WARNING_MAIN_FLOW, "coverage restored");
      clear_warn_status(WARNING_FEED_TIMEOUT, "coverage restored");
    }
  }

  prevCoverage = coverage;
}

void printStatus(uint32_t now) {
  static uint32_t lastPrintMs = 0;
  if (now - lastPrintMs < PRINT_PERIOD_MS) {
    return;
  }
  lastPrintMs = now;

    int raw[4] = {loaderSensors[0].raw, loaderSensors[1].raw,
          loaderSensors[2].raw, loaderSensors[3].raw};
    int thr[4] = {
      loaderSensors[0].enabled ? loaderSensors[0].threshold : -1,
      loaderSensors[1].enabled ? loaderSensors[1].threshold : -1,
      loaderSensors[2].enabled ? loaderSensors[2].threshold : -1,
      loaderSensors[3].enabled ? loaderSensors[3].threshold : -1,
    };
    bool enabled[4] = {
      loaderSensors[0].enabled,
      loaderSensors[1].enabled,
      loaderSensors[2].enabled,
      loaderSensors[3].enabled,
    };

  logger_snapshot(sensors_work_status_name(workStatus),
                  coverage,
                  nearCount,
                  farCount,
                  zone1Mismatch,
                  zone2Mismatch,
                  raw,
                  thr,
                  get_warn_status_group(),
                  warning_type_name);

    logger_csv_snapshot(sensors_work_status_name(workStatus),
              coverage,
              nearPairIsA,
              nearCount,
              farCount,
              zone1Mismatch,
              zone2Mismatch,
              pairA_raw,
              pairB_raw,
              raw,
              thr,
              enabled,
              get_warn_status_group(),
              warning_type_name);
}

void sensors_setup() {
  Serial.begin(115200);
  delay(300);

  logger_session_header(SYSTEM_NAME);
  logger_csv_header();

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

  logger_event("boot", "IR AO loader/wedger monitor start");
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
  computeDerivedSignals(now);
  processWorkFlow(now);
  printStatus(now);
}
