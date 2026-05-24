# LBJ Receiver Module Overview

本文档记录当前重构后的模块边界、数据流和配置项。目标是让后续添加功能时先找到正确入口，避免重新把逻辑堆回 `main.cpp` 或格式化任务里。

## 数据流程

1. `main.cpp` 负责主循环、接收状态机、任务创建和兜底处理。
2. LoRa/Pager 接收到足够数据后，`main.cpp` 调用 `pager.readDataMSA(...)` 收集原始 POCSAG 数据。
3. `formatDataTask()` 作为格式化任务入口，只负责阶段调度：
   - `collectRawPagerData(...)` 汇总原始 pager 文本。
   - `decodeLbjData(...)` 调用 LBJ 解码。
   - `dispatchDecodedOutputs(...)` 分发 Serial、SD log、CSV、Telnet 输出。
   - `requestDecodedDisplayUpdate(...)` 提交 OLED 显示请求。
4. `processPendingDisplayUpdate()` 在主循环中执行真正的 OLED 刷新。
5. `flushDecodedCsvOutputIfDue()` 在主循环尾部处理可选的 CSV 延迟 flush。

## 主要模块职责

### `main.cpp`

- 负责硬件初始化、主循环、接收状态机、任务创建和超时兜底。
- 负责调用 `handleSerialInput()`、网络轮询、蜂鸣器更新、显示请求处理和 CSV 尾部 flush。
- 不应继续添加具体输出格式、CSV 字段、Telnet 输出细节或 OLED 绘制细节。

### `data_formatter`

- 负责格式化任务内的数据处理阶段。
- `collectRawPagerData(...)` 收集非空 POCSAG 文本，保留原始调试输出。
- `decodeLbjData(...)` 调用 `readDataLBJ(...)` 生成 `lbj_data`。
- 不直接实现 Serial、SD、CSV、Telnet 输出细节，也不直接刷 OLED。

适合修改：
- 新增解码前后的轻量数据整理。
- 新增不会改变输出通道语义的格式化阶段。

不适合修改：
- SD 写入策略。
- Telnet 连接策略。
- OLED 绘制内容。

### `data_outputs`

- 负责 decoded 结果的输出分发。
- 统一入口是 `dispatchDecodedOutputs(...)`。
- 当前调度顺序：
  1. Serial 输出阶段。
  2. Storage 输出阶段：SD log 后 CSV。
  3. Network 输出阶段：Telnet。
- 管理 CSV 批量 flush 的计数和超时边界。
- 管理 decoded Telnet 输出开关、离线跳过边界和默认关闭的限频边界。

适合修改：
- Serial/Telnet 输出开关策略。
- SD/CSV flush 策略。
- 输出阶段计时日志。

不适合修改：
- `printDataSerial(...)` 内部业务输出格式。
- `appendDataCSV(...)` 字段定义。
- `telPrintf(...)` 底层发送实现。

### `status_display`

- 负责 OLED 显示。
- `requestDecodedDisplayUpdate(...)` 只记录最新 pending LBJ 显示请求。
- 多次 pending 请求不会排队，最新请求覆盖旧请求。
- `processPendingDisplayUpdate()` 根据 `OLED_DECODED_DISPLAY_MIN_INTERVAL_MS` 执行实际刷新。
- 刷新期间仍会进入 `TASK_RUNNING_SCREEN`，保持原任务状态语义。

适合修改：
- OLED 显示内容。
- 显示刷新节流策略。
- 低电压或 OLED 休眠相关显示行为。

不适合修改：
- 解码、SD、Telnet 输出逻辑。

### `serial_commands`

- 负责串口命令交互。
- `$ ...` 开头的响应是用户交互输出，不应被 `DEBUG_LOG_LEVEL` 关闭。

适合修改：
- 新增串口命令。
- 调整命令响应文本。

不适合修改：
- 高频调试日志。
- decoded LBJ 业务输出格式。

### `receiver_control`

- 负责接收和 AFC 辅助逻辑。
- 包含频偏计算、同步/载波/前导处理和频率回退。

适合修改：
- AFC 策略。
- 同步、载波、前导检测相关处理。

不适合修改：
- 输出格式。
- SD/CSV/Telnet/OLED 策略。

### `task_state.hpp`

- 定义格式化任务状态枚举和任务句柄外部声明。
- `TASK_RUNNING_SCREEN` 表示显示刷新占用任务状态语义。

### `debug_log`

- 统一调试、提示、错误和阶段计时日志入口。
- `debugLogError(...)` 用于错误提示。
- `debugLogInfo(...)` 用于启动、网络、SD 等用户可见状态提示。
- `debugLogVerbose(...)` 用于 `[D]`、内部状态和高频开发调试。
- `debugLogStageTiming(...)` 用于阶段耗时输出。

注意：
- LBJ 解码结果 Serial 输出是业务输出，不受 `DEBUG_LOG_LEVEL` 控制。
- 串口命令 `$ ...` 响应是用户交互输出，不受 `DEBUG_LOG_LEVEL` 控制。
- coredump 十六进制正文保留为原始输出。

### `runtime_config`

- 集中管理编译期策略开关。
- 所有配置使用 `#ifndef / #define`，可以通过 PlatformIO build flags 覆盖。
- 不引入运行时配置文件、EEPROM 或 NVS 配置。

### `sdlog`

- 负责 SD log、CSV 和 coredump 文件管理。
- `sendBufferLOG(...)`、`sendBufferCSV(...)` 支持 `flushAfterWrite` 参数。
- `flushCSV()` 提供 CSV 延迟 flush 的明确边界。

适合修改：
- SD 文件打开、轮转、flush 底层实现。
- CSV 文件可用性保护。

不适合修改：
- decoded 输出调度顺序。

### `networks`

- 负责 WiFi、Telnet、时间同步和底层 decoded 输出函数。
- `isTelnetOutputAvailable()` 封装当前 Telnet 可用性判断，当前无输出副作用。
- `telPrintf(...)` 内部仍保留原有 Telnet 在线保护。
- `printDataSerial(...)` 和 `printDataTelnet(...)` 保留业务输出内容。
- `appendDataLog(...)` 和 `appendDataCSV(...)` 保留 SD/CSV 写入内容。

适合修改：
- WiFi/Telnet 连接状态处理。
- Telnet 底层输出行为。
- LBJ 业务输出格式本身。

不适合修改：
- 输出阶段是否调用 Telnet 的策略；优先放在 `data_outputs`。

## 关键配置项

配置项位于 `src/runtime_config.hpp`。

| 配置项 | 默认值 | 说明 |
| --- | --- | --- |
| `ENABLE_BATCHED_CSV_FLUSH` | `0` | 默认关闭 CSV 批量 flush。开启后可减少 SD flush 阻塞，但断电时可能丢失最近 CSV 数据。 |
| `CSV_FLUSH_EVERY_N_WRITES` | `5` | 批量 CSV flush 开启后，每累计多少条 CSV 写入触发 flush。小于 1 时会被钳制为 1。 |
| `CSV_FLUSH_INTERVAL_MS` | `5000UL` | 批量 CSV flush 开启后，最长延迟 flush 时间。小于 1 时会被钳制为 1。 |
| `OLED_DECODED_DISPLAY_MIN_INTERVAL_MS` | `150UL` | decoded OLED 显示最小刷新间隔。间隔内的新请求会合并为最新一条。 |
| `ENABLE_DECODED_SERIAL_OUTPUT` | `1` | decoded Serial 业务输出开关，默认开启。 |
| `ENABLE_DECODED_TELNET_OUTPUT` | `1` | decoded Telnet 输出开关，默认开启。 |
| `SKIP_DECODED_TELNET_WHEN_OFFLINE` | `0` | 默认关闭离线跳过，保持旧行为；开启后 Telnet 离线时跳过 `printDataTelnet(...)`。 |
| `ENABLE_DECODED_TELNET_RATE_LIMIT` | `0` | 默认关闭 decoded Telnet 限频。 |
| `DECODED_TELNET_MIN_INTERVAL_MS` | `150UL` | Telnet 限频开启后的最小输出间隔。小于 1 时会被钳制为 1。 |
| `DEBUG_LOG_LEVEL` | `DEBUG_LOG_VERBOSE` | 调试日志等级，默认保持现有输出尽量不变。 |
| `ENABLE_STAGE_TIMING_LOGS` | `1` | 阶段计时日志开关，默认开启。 |

调试等级：

| 配置项 | 值 | 说明 |
| --- | --- | --- |
| `DEBUG_LOG_NONE` | `0` | 关闭 debug log helper 控制的日志。 |
| `DEBUG_LOG_ERROR` | `1` | 仅错误日志。 |
| `DEBUG_LOG_INFO` | `2` | 错误和用户可见状态提示。 |
| `DEBUG_LOG_VERBOSE` | `3` | 包含开发调试和高频内部状态。 |

## 默认关闭的稳定性策略

### CSV 批量 flush

- 默认关闭，CSV 每次写入后仍立即 flush，保持旧行为和断电可靠性。
- 开启 `ENABLE_BATCHED_CSV_FLUSH=1` 后：
  - SD log 仍每条立即 flush。
  - CSV 可以按条数或时间延迟 flush。
  - 最多可能丢失 `CSV_FLUSH_EVERY_N_WRITES` 条以内，或 `CSV_FLUSH_INTERVAL_MS` 时间窗口内尚未 flush 的 CSV 数据。

### Telnet 离线跳过

- 默认关闭，仍调用 `printDataTelnet(...)`，由 `telPrintf(...)` 内部保护离线状态。
- 开启 `SKIP_DECODED_TELNET_WHEN_OFFLINE=1` 后，离线时不调用 `printDataTelnet(...)`，也不会打印 Telnet 完成计时。

### Telnet 限频

- 默认关闭，保持旧输出频率。
- 开启 `ENABLE_DECODED_TELNET_RATE_LIMIT=1` 后：
  - 第一次 decoded Telnet 输出立即通过。
  - 后续输出必须间隔至少 `DECODED_TELNET_MIN_INTERVAL_MS`。
  - 限频跳过时不会打印 `telprint complete...`。
  - 时间判断使用 `millis()` 无符号差值，支持回绕。

## 输出边界

### 业务输出

- `printDataSerial(...)` 中的 LBJ 解码结果是业务输出。
- `printDataTelnet(...)` 中的 LBJ 解码结果是业务输出。
- 这些输出的内容、顺序和字段不要随意改成 debug log。

### 用户交互输出

- `serial_commands` 中 `$ ...` 响应是用户交互输出。
- 用户通过串口命令依赖这些响应，不应被 `DEBUG_LOG_LEVEL` 误关。

### 调试和阶段计时日志

- `[D]`、内部状态、阶段耗时等应使用 `debug_log`。
- 阶段计时日志由 `ENABLE_STAGE_TIMING_LOGS` 控制。

### 错误和状态提示

- SD、WiFi、Telnet、低压、初始化等提示优先使用 `debugLogInfo(...)` 或 `debugLogError(...)`。
- 错误提示默认仍会输出。

## 当前仍保留的同步阻塞风险

- SD log 和 CSV 写入仍在 decoded 输出路径同步执行。
- 默认 CSV 每条写入后立即 flush，慢 SD 卡时仍可能阻塞。
- Telnet 输出默认同步执行；限频和离线跳过默认关闭。
- Serial 业务输出仍同步执行；慢串口或高频输出时可能拖慢任务。
- OLED 实际刷新仍同步执行，只是已经移动到主循环并增加 pending 合并和最小刷新间隔。

## 添加新功能时优先修改哪里

| 目标 | 优先修改模块 |
| --- | --- |
| 新增串口命令 | `serial_commands` |
| 调整 LBJ 解码逻辑 | `networks` 中 `readDataLBJ(...)`，必要时再整理到新解码模块 |
| 调整 decoded Serial/Telnet/SD/CSV 输出策略 | `data_outputs` |
| 调整业务输出格式 | `networks` 中 `printDataSerial(...)`、`printDataTelnet(...)`、`appendDataLog(...)`、`appendDataCSV(...)` |
| 调整 CSV flush 策略 | `runtime_config` + `data_outputs` + `sdlog` |
| 调整 OLED 显示内容或刷新节流 | `status_display` |
| 调整 AFC/频偏/同步处理 | `receiver_control` |
| 调整调试日志等级或阶段计时 | `runtime_config` + `debug_log` |
| 调整 WiFi/Telnet 连接生命周期 | `networks` |
| 调整任务状态语义 | `task_state.hpp` + `main.cpp`，需非常保守 |

## 后续建议

1. 先保持当前结构稳定运行一段时间，观察 SD、Telnet、OLED 三类同步路径的实际卡顿情况。
2. 如果 Telnet 客户端慢或信号密集，优先试验 `SKIP_DECODED_TELNET_WHEN_OFFLINE=1` 和 `ENABLE_DECODED_TELNET_RATE_LIMIT=1`。
3. 如果 SD 卡慢，再评估开启 `ENABLE_BATCHED_CSV_FLUSH=1`，并接受明确的数据丢失窗口。
4. 如果串口业务输出造成阻塞，再考虑新增 decoded Serial 输出限频；注意不要影响用户需要实时查看的业务数据。
