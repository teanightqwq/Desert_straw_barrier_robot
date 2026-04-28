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
  // 主类
  WARNING_NONE,
  WARNING_MAIN_LOADER,      // 载草系统警告(未细分)
  WARNING_MAIN_FLOW,        // 结束流程时正常报错
  // 占位符
  WARNING_UNDEFINED,        // 暂时未定义
  WARNING_NO_SUB,           // 主类警告无子类项
  // 子类 (LOADER)
  WARNING_DISPLACED_BALE,   // 草捆偏移导致的左右草量不同
  WARNING_BROKEN_BALE,      // 草捆毁坏导致的部分传感器识别错误
  WARNING_SENSOR_STATUS,    // 传感器未正确设置/传感器未完全设置完毕/传感器毁坏
  // 子类 (FLOW)
  WARNING_FEED_TIMEOUT,     // 结束流程较预设值慢
};

enum WarningSeverity {
  SEVERITY_INFO,
  SEVERITY_NORMAL,
  SEVERITY_IMPORTANT,
  SEVERITY_SEVERE,
};

struct WarnStatus {
  WarningType type;
  WarningType mainType;
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
WarningType warning_main_type(WarningType type);
bool warning_is_main_type(WarningType type);
bool warning_is_loader_subtype(WarningType type);
bool warning_is_flow_subtype(WarningType type);
const char* warning_type_name(WarningType type);
const WarnStatusGroup* get_warn_status_group();
bool has_warn_status(WarningType type);
void set_warn_status(WarningType type, WarningSeverity severity, const char* message);
void clear_warn_status(WarningType type, const char* reason);
void clear_all_warn_status(const char* reason);

// Loader subwarning handlers (skeleton only)
void handle_displaced_bale_warning();
void handle_broken_bale_warning();
void handle_sensor_status_warning();

#endif // WARNING_H
