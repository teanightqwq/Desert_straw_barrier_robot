#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include "warning.h"

constexpr const char* SYSTEM_NAME = "loader_ir_test";

void logger_session_header(const char* systemName);
void logger_event(const char* type, const char* msg);
void logger_snapshot(const char* status,
                     int coverage,
                     int nearCount,
                     int farCount,
                     bool zone1Mismatch,
                     bool zone2Mismatch,
                     const int raw[4],
                     const int thr[4],
                     const WarnStatusGroup* warnGroup,
                     const char* (*warn_name)(WarningType));

#endif // LOGGER_H
