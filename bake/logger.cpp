#include "logger.h"

static int month_to_number(const char* mon) {
  if (strncmp(mon, "Jan", 3) == 0) return 1;
  if (strncmp(mon, "Feb", 3) == 0) return 2;
  if (strncmp(mon, "Mar", 3) == 0) return 3;
  if (strncmp(mon, "Apr", 3) == 0) return 4;
  if (strncmp(mon, "May", 3) == 0) return 5;
  if (strncmp(mon, "Jun", 3) == 0) return 6;
  if (strncmp(mon, "Jul", 3) == 0) return 7;
  if (strncmp(mon, "Aug", 3) == 0) return 8;
  if (strncmp(mon, "Sep", 3) == 0) return 9;
  if (strncmp(mon, "Oct", 3) == 0) return 10;
  if (strncmp(mon, "Nov", 3) == 0) return 11;
  if (strncmp(mon, "Dec", 3) == 0) return 12;
  return 0;
}

static void build_date_compact(char out[7]) {
  // __DATE__ format: "Mmm dd yyyy"
  const char* date = __DATE__;
  int mon = month_to_number(date);
  int day = (date[4] == ' ') ? (date[5] - '0') : ((date[4] - '0') * 10 + (date[5] - '0'));
  int year = (date[9] - '0') * 10 + (date[10] - '0');
  snprintf(out, 7, "%02d%02d%02d", day, mon, year);
}

static void build_time_compact(char out[7]) {
  // __TIME__ format: "hh:mm:ss"
  const char* t = __TIME__;
  snprintf(out, 7, "%.2s%.2s%.2s", t, t + 3, t + 6);
}

void logger_session_header(const char* systemName) {
  char dateCompact[7] = {0};
  char timeCompact[7] = {0};
  build_date_compact(dateCompact);
  build_time_compact(timeCompact);

  Serial.print("[SESSION] system=");
  Serial.print(systemName);
  Serial.print(" build_date=");
  Serial.print(__DATE__);
  Serial.print(" build_time=");
  Serial.print(__TIME__);
  Serial.print(" start_ms=");
  Serial.println(millis());

  Serial.print("[SESSION] filename=log_");
  Serial.print(systemName);
  Serial.print("_");
  Serial.print(dateCompact);
  Serial.print("_");
  Serial.print(timeCompact);
  Serial.println(".log");

  Serial.println("[SESSION] note=copy Serial Monitor output into log/ folder on PC");
}

void logger_event(const char* type, const char* msg) {
  Serial.print("[EVT] t=");
  Serial.print(millis());
  Serial.print(" type=");
  Serial.print(type);
  Serial.print(" msg=");
  if (msg) {
    Serial.print(msg);
  } else {
    Serial.print("-");
  }
  Serial.println();
}

void logger_snapshot(const char* status,
                     int coverage,
                     int nearCount,
                     int farCount,
                     bool zone1Mismatch,
                     bool zone2Mismatch,
                     const int raw[4],
                     const int thr[4],
                     const WarnStatusGroup* warnGroup,
                     const char* (*warn_name)(WarningType)) {
  Serial.print("[SNAP] t=");
  Serial.print(millis());
  Serial.print(" status=");
  Serial.print(status);
  Serial.print(" coverage=");
  Serial.print(coverage);
  Serial.print(" near=");
  Serial.print(nearCount);
  Serial.print(" far=");
  Serial.print(farCount);
  Serial.print(" zone1_mis=");
  Serial.print(zone1Mismatch ? 1 : 0);
  Serial.print(" zone2_mis=");
  Serial.print(zone2Mismatch ? 1 : 0);
  Serial.print(" loader_raw=[");
  for (int i = 0; i < 4; ++i) {
    if (i > 0) Serial.print(",");
    Serial.print(raw[i]);
  }
  Serial.print("] loader_thr=[");
  for (int i = 0; i < 4; ++i) {
    if (i > 0) Serial.print(",");
    Serial.print(thr[i]);
  }
  Serial.print("] warn=");
  if (!warnGroup || warnGroup->count == 0) {
    Serial.print("none");
  } else {
    bool printed = false;
    for (size_t i = 0; i < warnGroup->count; ++i) {
      const WarnStatus& warn = warnGroup->items[i];
      if (!warn.active) {
        continue;
      }
      if (printed) {
        Serial.print(";");
      }
      printed = true;
      Serial.print(warn_name ? warn_name(warn.type) : "warn");
    }
  }
  Serial.println();
}
