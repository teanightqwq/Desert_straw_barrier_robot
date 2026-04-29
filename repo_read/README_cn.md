# Desert Straw Barrier Robot

## 目的
本仓库包含草料输送检测的早期上电与验证代码和笔记。
当前重点是 ESP32-S3 上的红外模拟传感，用于 loader-to-feeder 和 feeder-to-wedger 的流程检查。

## 当前文件
### Planting System
- bake.ino: ESP32-S3 统一固件入口
- sensor.h: 传感器模块接口（GPIO 映射、枚举、公开 API）
- sensor.cpp: 传感器模块实现（流程与状态机）
- warning.h: warning 系统类型与 API（与传感器模块解耦）
- emb_system.md: 系统级硬件说明
- TODO.md: 下一步工程任务清单

## 当前引脚规划
目前仅确定一个 loader 传感器引脚。

| 传感器 | 角色 | 引脚 |
|---|---|---|
| loader_1 | Loader 传感器（已确定） | GPIO10 |
| loader_2 | Loader 传感器 | TBD |
| loader_3 | Loader 传感器 | TBD |
| loader_4 | Loader 传感器 | TBD |
| wedger_sensor | Feeder-to-wedger 检测传感器 | TBD |

## Work Status 枚举
sensor.cpp 内的运行状态机包含：
- not_start
- on_work
- e_stop
- end
- end_detection

## Warning 系统（新增）
Warning 已与 `WorkStatus` 解耦，使用分组的 `WarnStatus` 系统建模。这样可以让传感器问题、流程超时等短暂警告不影响主状态机，同时允许多个 warning 并行存在。

关键概念：
- `WarningType`: 包含主类（`WARNING_MAIN_LOADER`, `WARNING_MAIN_FLOW`）和子类（`WARNING_DISPLACED_BALE`, `WARNING_BROKEN_BALE`, `WARNING_SENSOR_STATUS`, `WARNING_FEED_TIMEOUT`）。
  - 占位类型：`WARNING_UNDEFINED`（子类不明确）与 `WARNING_NO_SUB`（主类无子类）。
- `warning_main_type()`: 子类映射回主类，并处理误分类情况。
- `warning_is_main_type()`, `warning_is_loader_subtype()`, `warning_is_flow_subtype()`: 类型判断辅助函数。
- `WarnStatus`: 运行时结构体，记录 warning 类型、主类、严重性、上一次 `WorkStatus`、时间戳与消息。
- `WarnStatusGroup`: 固定容量的 warning 列表，支持多个 warning 并行存在。
- `WarningSeverity`: warning 严重级别，与 warning 类型独立。

行为说明：
- warning 不会自动改变 `WorkStatus`，只记录在 warning 组中，供上层逻辑处理。
- 模块提供 `set_warn_status()`, `clear_warn_status()`, `clear_all_warn_status()`, `get_warn_status_group()` 进行管理。
- warning 日志具有节流机制，避免每个 loop 都刷屏。

状态/警告示例：
- `STATUS_ON_WORK` + `WARNING_NONE`: 正常运行。
- `STATUS_ON_WORK` + `WARNING_MAIN_LOADER`: loader 层 warning，可包含子类。
- `STATUS_ON_WORK` + `WARNING_MAIN_FLOW`: 流程相关 warning，例如 end detection 延迟。
- `STATUS_END_DETECTION` + `WARNING_FEED_TIMEOUT`: 末端超时但 wedger 仍未确认完成。

## 运行流程（当前）
1. 程序从 not_start 启动。
2. 任意 loader 传感器第一次检测到草料（raw < threshold）后进入 on_work。
3. 在配置足够传感器时，loader 传感器会被分为 near/far 分组。
4. 每个 loader 传感器执行 warning 逻辑：
   - raw > threshold 持续 2500 ms -> warning
   - raw < threshold 持续 1000 ms -> warning 清除
5. 当所有 loader 传感器都检测到无草料，喂料计时开始。
6. 超过 FEED_PROCESS_TIME_MS（当前 18000 ms）后：
   - 如果 wedger 传感器 raw < wedger_threshold -> warning_work 并继续
   - 否则 -> end

## 模块接口
- bake.ino 保留 Arduino 的 setup/loop。
- sensor.cpp 提供：
   - sensors_setup()
   - sensors_loop()
- sensor.h 暴露枚举、GPIO 配置与公开函数声明。

## 重要配置参数
在 sensor.h 的 Runtime Config 部分修改以下常量以适配测试台：
- LOADER_PIN_1..4
- WEDGER_SENSOR_PIN
- NEAR_THRESHOLD
- FAR_THRESHOLD_MIN / FAR_THRESHOLD_MAX
- WEDGER_THRESHOLD
- FEED_PROCESS_TIME_MS

## 重构说明（warning 分离）
warning 系统已拆分为独立头文件，减少与传感器模块的耦合。

关键点：
- `warning.h` 负责 `WorkStatus`, `WarningType`, `WarningSeverity`, `WarnStatus`, `WarnStatusGroup`。
- `sensor.h` 通过 include 引入 warning 类型，聚焦传感器配置与公开接口。
- `sensor.cpp` 保留全部实现，并显式包含 `warning.h` 以保证依赖清晰。

## 备注
- 目前仅配置了 loader_1，因此自动分组是部分模式。
- 一旦全部引脚确定，请直接在 sensor.h 中设置并重新校准。
