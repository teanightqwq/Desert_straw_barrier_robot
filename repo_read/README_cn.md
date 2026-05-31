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
- logger.h: 结构化日志接口（session/event/snapshot/CSV）
- logger.cpp: 结构化日志实现
- motor.h: 送料电机控制接口（A/B/C）
- motor.cpp: 送料电机控制实现（analogWrite PWM）
- wheels.h: 轮子控制接口（H 桥驱动、主动/被动组、速度优先逻辑）
- wheels.cpp: 轮子控制实现（前进/后退/停止框架）
- emb_system.md: 系统级硬件说明
- TODO.md: 下一步工程任务清单

## 当前引脚规划
loader 传感器引脚因电机接线冲突暂时禁用。

| 传感器 | 角色 | 引脚 |
|---|---|---|
| loader_1 | Loader 传感器（TOF，Route A 不用） | -1 |
| loader_2 | Loader 传感器（IR1 AO） | GPIO20 |
| loader_3 | Loader 传感器（IR2 AO） | GPIO0 |
| loader_4 | Loader 传感器（TOF，Route A 不用） | -1 |
| wedger_sensor | Feeder-to-wedger 检测传感器 | TBD |

## 送料电机控制（motor）
电机模块使用 ESP32-S3 的 analogWrite PWM 驱动三路电机：
- Motor A: 上右（主调速）
- Motor B: 上左（跟随 A，比例缩放）
- Motor C: 下方输送（仅 CW，比例缩放）

启动兼容规则：
- 当 STATUS_NOT_START -> STATUS_ON_WORK 后，如果至少一个 loader GPIO 启用且 coverage 在 START_COMPATIBLE_TIME_MS 内仍为 0，则 Motor A 方向仅翻转一次（Motor B 跟随 A）。

| 电机 | IN1 | IN2 | PWM |
|---|---|---|---|
| A | GPIO4 | GPIO5 | GPIO6 |
| B | GPIO9 | GPIO10 | GPIO11 |
| C | GPIO15 | GPIO16 | GPIO17 |

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
- `WarningType`: 六个独立类别：
   - `WARNING_LOW_COVERAGE`
   - `WARNING_GAP_DETECTED`
   - `WARNING_SENSOR_MISCONFIG`
   - `WARNING_RANGE_GROUP_BLIND_NEAR`
   - `WARNING_RANGE_GROUP_BLIND_FAR`
   - `WARNING_FLOW_END_DETECTING`
   - `WARNING_FEED_TIMEOUT`
- `WarnStatus`: 运行时结构体，记录 warning 类型、严重性、上一次 `WorkStatus`、时间戳与消息。
- `WarnStatusGroup`: 固定容量的 warning 列表，支持多个 warning 并行存在。
- `WarningSeverity`: warning 严重级别，与 warning 类型独立。

行为说明：
- warning 不会自动改变 `WorkStatus`，只记录在 warning 组中，供上层逻辑处理。
- 模块提供 `set_warn_status()`, `clear_warn_status()`, `clear_all_warn_status()`, `get_warn_status_group()` 进行管理。
- warning 日志具有节流机制，避免每个 loop 都刷屏。

状态/警告示例：
- `STATUS_ON_WORK` + `WARNING_NONE`: 正常运行。
- `STATUS_ON_WORK` + `WARNING_SENSOR_MISCONFIG`: zone mismatch 或 near/far 歧义。
- `STATUS_ON_WORK` + `WARNING_RANGE_GROUP_BLIND_NEAR`: 近层盲区。
- `STATUS_END_DETECTION` + `WARNING_FEED_TIMEOUT`: 末端超时但 wedger 仍未确认完成。

## 日志输出（session/event/snapshot/CSV）
日志采用结构化输出，便于从 Serial Monitor 复制到本地 `log/` 文件夹保存。

格式说明：
- `[SESSION]` 启动头，包含构建时间与推荐文件名。
- `[EVT]` 状态变化与 warning set/clear 事件。
- `[SNAP]` 周期性状态快照。
- `[CSV_HEADER]` + `[CSV]` 用于表格化存档。

## 轮子控制（wheels）
轮子模块用于 H 桥驱动的双组马达控制，分为主动组与被动组：
- 主动组：速度优先，控制整体速度与方向。
- 被动组：只需能转即可，默认用较低 PWM 保持转动。

目前代码仅提供前进/后退/停止框架，GPIO 与电机接口为 TBD：
- 头文件： [bake/wheels.h](bake/wheels.h)
- 实现文件： [bake/wheels.cpp](bake/wheels.cpp)

后续需要补充：
- H 桥驱动引脚映射
- 主动/被动组具体轮位（前/后 或 左/右）
- 被动组最低可转 PWM 与方向一致性验证

## 运行流程（当前）
1. 程序从 not_start 启动。
2. coverage >= 2 持续 T_START_HOLD 后进入 on_work。
3. 每次采样计算 coverage 和 zone 一致性：
   - coverage = det1 + det2 + det3 + det4
   - zone mismatch: (S1 vs S3), (S2 vs S4)
4. 物理配对固定，但 near/far 动态判断：
   - pairA = (S1,S2), pairB = (S3,S4)
   - 使用阈值带判断 near/far（NEAR_THRESHOLD / FAR_THRESHOLD_MIN/MAX）
5. warning 由持续条件触发：
   - zone mismatch、near/far 歧义、range blind
   - coverage 过低 / gap 检测
6. end 检测流程：
   - coverage == 0 持续 T_END_DETECT_HOLD -> STATUS_END_DETECTION + WARNING_FLOW_END_DETECTING
   - END_DETECTION 内等待 S5（如可用），超过 T_FEED_PROCESS 则超时警告

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
- T_START_HOLD / T_LOW_HOLD / T_MISMATCH_HOLD / T_RANGE_BLIND_HOLD
- T_GAP_HOLD / T_END_DETECT_HOLD / T_FEED_PROCESS / T_S5_CONFIRM_HOLD

## 重构说明（warning 分离）
warning 系统已拆分为独立头文件，减少与传感器模块的耦合。

关键点：
- `warning.h` 负责 `WorkStatus`, `WarningType`, `WarningSeverity`, `WarnStatus`, `WarnStatusGroup`。
- `sensor.h` 通过 include 引入 warning 类型，聚焦传感器配置与公开接口。
- `sensor.cpp` 保留全部实现。

## 备注
- 目前仅配置了 loader_1，因此 coverage 逻辑是部分模式。
- 一旦全部引脚确定，请直接在 sensor.h 中设置并重新测试。
