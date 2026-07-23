# CodexMeter 架构说明

## 调研结论

- Codex 用量通过 `codex app-server --listen stdio://` 的 JSON-RPC 获取：初始化后读取 `account/read`、`account/rateLimits/read`，并把可选的 `account/usage/read` 每日 Token 桶合并到同一快照。
- Clawdmeter 的硬件目标是 Waveshare ESP32-S3-Touch-AMOLED-2.16，屏幕为 480x480 CO5300，PMU 为 AXP2101；CodexMeter 当前支持同一硬件。
- Waveshare ESP32-S3-Touch-AMOLED-2.16 板载 QMI8658 六轴 IMU，固件通过加速度计判断重力方向；CO5300 驱动不可靠提供 90/270 度硬件旋转，因此当前实现采用 CPU 侧局部刷新旋转。
- BLE 沿用 Clawdmeter 的自定义数据服务 UUID：service `...0001`，RX write `...0002`，TX notify `...0003`。CodexMeter 额外保留 `...0004` 作为设备主动请求刷新信号。
- 固件保持 PlatformIO、Arduino_GFX、LVGL、ArduinoJson、NimBLE 技术栈，以降低硬件适配风险。
- 当前只面向 macOS + Waveshare ESP32-S3-Touch-AMOLED-2.16 + Codex 订阅余量，但协议中的 `src` 和模块边界为未来其他订阅来源预留了空间。

## 模块职责

- `codexmeter.app_server`：负责 stdio JSON-RPC 进程管理、初始化握手和请求响应匹配。
- `codexmeter.provider`：负责调用 Codex App Server 并转换为订阅用量快照。
- `codexmeter.limits`：负责限额桶归一化与每日 Token 桶聚合，当前默认只读取 `codex` 桶，将 300 分钟映射为 `5h`，10080 分钟映射为 `7d`。
- `codexmeter.local_usage`：当服务端尚未返回当天桶时，增量读取本机 Codex session 的 `token_count` 累计值；fork 初始历史回放只作为基线，不重复计入今日用量。
- `codexmeter.payloads`：负责 BLE JSON payload 生成、摘要清洗、设备字库字符过滤和 512 bytes 长度约束。
- `codexmeter.device_registry`：负责读取和维护 `~/.codexmeter/devices.json`，用稳定短 ID 管理已登记设备。
- `codexmeter.multi_ble`：负责合并 BLE 广播和 macOS CoreBluetooth 已连接外设、为每台已登记设备创建独立 worker，并把上游 payload fan-out 到每台设备队列。
- `codexmeter.ble`：负责单个 BLE 外设会话的连接、notify 订阅、写入、ACK 校验和健康检查。
- `codexmeter.events`：负责本地 Unix socket 事件入口，供 Codex hooks 与 `codexmeterctl` 使用；同时维护运行中任务集合、独立活动租约和 TTL 清理循环。
- `codexmeter.transcripts`：按路径和字节偏移增量监听当前任务 transcript，以 `turn_id` 识别完成或中断；该模块是可静默降级的活动状态加速层。
- `codexmeter.screen_policy`：负责 macOS 锁屏状态轮询、自动亮屏/关屏状态机和 BLE 重连亮屏策略。
- `hooks/codexmeter_start_hook.py`：负责接收 Codex `UserPromptSubmit` hook 输入，通知 daemon 有任务开始运行。
- `hooks/codexmeter_stop_hook.py`：负责接收 Codex `Stop` hook 输入、生成短摘要并静默通知 daemon；异常时仍成功退出，不阻塞 Codex。
- `scripts/install_hook.py`：负责合并 CodexMeter `Stop` hook 到 `~/.codex/hooks.json`，并在覆盖前备份旧文件。
- `firmware/src/model.*`：负责解析 BLE JSON 为固件内部模型，并记录 `usage` payload 接收时的 `millis()`，用于后续倒计时计算。
- `firmware/src/dashboard_view_model.*`：把用量、电量、任务活动和连接状态转换为主题只读的统一仪表盘模型与格式化文本。
- `firmware/src/theme.h` / `theme_registry.*` / `theme_runtime.*`：定义主题契约、编译期注册表，以及当前主题的挂载、更新、动画 tick、卸载和安全回退生命周期。
- `firmware/src/classic_theme.*` / `cyberpunk_theme.*`：当前内置的两套独立 LVGL 仪表盘实现。
- `firmware/src/theme_rotation.*`：只在仪表盘真实可见时累计的自动换肤策略。
- `firmware/src/device_settings.*`：保存主题、亮度、预留音量和自动换肤配置；使用带版本和 CRC 的 NVS 记录以及延迟写入。
- `firmware/src/ui.*`：负责主题运行时、设置页、系统浮层、防烧屏漂移、红黄绿闪屏和任务完成视图之间的场景协调。
- `firmware/src/ble_service.*`：负责 GATT 服务、RX/TX、ACK/NACK 与刷新通知。
- `firmware/src/power.*`：负责 AXP2101 电量和中间 PKEY 事件。
- `firmware/src/imu.*`：负责 QMI8658 初始化、加速度采样、方向防抖、自动/手动旋转模式和 IMU 串口状态输出。
- `firmware/src/display_rotation.*`：负责把 LVGL 局部刷新区域按当前方向旋转后写入 CO5300，并让 USB 截图输出当前物理方向。
- `firmware/src/main.cpp`：负责板级初始化、主循环调度、串口调试命令、LVGL flush、AMOLED 亮屏/关屏、亮度控制和方向变化重绘。
- `firmware/src/device_log.*`：负责 ESP32 端关键事件环形日志、实时串口打印和按需日志 dump。
- `tools/capture_screenshot.py`：负责通过 USB 串口触发固件截图命令、读取 RGB565 帧缓冲并编码为 PNG，用于本地视觉 QA。
- `tools/read_device_logs.py`：负责通过 USB 串口查询、清空或跟随 ESP32 设备日志。
- `screenshot.sh`：截图工具入口，自动选择 Python 并转发参数。
- `logs.sh`：日志工具入口，自动选择 Python 并转发参数。

## 数据流

1. daemon 每 60 秒拉取 Codex App Server 限额，并尝试读取每日 Token 活动。
2. daemon 将 `codex` 限额归一化为 `UsageSnapshot`，按自然日汇总今日/近7天 Token。服务端当天桶缺失时，以当前 Mac 的 session token 增量补齐今日和近7天；服务端当天桶出现后与本机今日值比较，差异同时达到 100 万 Token 和 25% 时优先本机值，并替换近7天合计中的当天部分。每日桶或数据源选择变化时记录 UTC/本地观察时间、最近桶、差值和最终数据源。可选数据源失败时复用缓存值，再生成 `usage` payload 放入发送队列。
3. BLE discovery 按 service UUID 和 `CodexMeter-<short_id>` 广播名持续发现附近设备；macOS 上还会合并 `retrieveConnectedPeripheralsWithServices` 返回的已连接外设，以覆盖残留连接导致设备停止广播的情况。worker 连接后先读取 identity characteristic，校验完整芯片 ID，再写入 RX characteristic。
4. 固件解析 `usage`；存在 5h 窗口时显示原有双余量卡片，只返回有效 7d 窗口时显示 Token 活动与 7d 余量卡片。重置倒计时用 `t` 加设备本地经过时间计算：
   - 5h 显示为 `HH:MM 后重置`
   - 7d 显示为 `xd 后重置`
5. 若 180 秒没有新用量，固件将状态标记为 `stale`，并等待下一次 daemon 更新或设备刷新请求。
6. Codex `UserPromptSubmit` hook 触发时，start hook 通过 Unix socket 向 daemon 发送 `task_start`，并附带 Codex 提供的 `transcript_path`。
7. daemon 优先用 `turn_id`，其次用 `task_id`、`session_id` 或 `conversation_id` 维护运行中任务集合；计数变化时生成 `activity` payload。
8. daemon 从注册时的文件末尾增量读取受信任的 Codex transcript。任何新增内容都会刷新对应任务的 `last_seen_at`；发现当前 `turn_id` 的 `turn_aborted` 或 `task_complete` 时，只提前清除活动状态，不生成完成提醒。路径、读取或解析失败会静默降级到 hook 和 TTL 路径。
9. 固件收到 `activity` 后，在正常页底部居中显示对应数量的小蓝点；计数为 0 时隐藏。
10. Codex turn 完成时触发用户级 `Stop` hook；Stop hook 从 `last_assistant_message` 中生成不超过 96 字符的短摘要。
11. Stop hook 只发送一次 `task_complete` 事件。daemon 在同一个事件处理中先幂等地清除运行中任务，再构造 `alert` payload。
12. daemon 会同时发送 `activity` payload，并把完成后的 `run` 数量写入 `alert` payload，避免 BLE 连续写入丢失时小蓝点残留。
13. daemon 构造 `alert` payload 时会清洗 Markdown、过滤设备字库不支持的字符，并限制 payload 在 512 bytes 内。
14. 固件 BLE RX 使用小队列缓存连续 payload；收到携带 `run` 的 `alert` 时，会在显示提醒前同步更新活动任务数。
15. daemon 在收到设备 ACK 前保留告警 in-flight，断连后自动重试；固件缓存最近的告警 ID 并对重试去重。
16. 固件收到新 `alert` 后先隐藏文字并红、黄、绿全屏闪动；闪动结束后显示“任务完成！”和 28px 正文摘要，文字出现后默认停留 8 秒。
17. 用户可短按中间键进入设置页，并用左/右键选择或调值；长按中间键切换 AMOLED 亮屏和关闭，关屏会同时结束提醒或取消未确认编辑。
18. 仪表盘中左/右键可快捷降低/增加亮度，固件显示 3 秒亮度进度条。

## 屏幕电源控制链路

1. 固件在 `main.cpp` 维护 `screen_on` 状态，启动后默认为亮屏。
2. AXP2101 PKEY 长按进入按键处理逻辑并切换 `screen_on`；同一次长按伴随的短按 IRQ 会被抑制。PKEY 短按只在亮屏状态下进入设置或确认当前设置。
3. 关屏时调用 CO5300 驱动的 `displayOff()`，并让 LVGL flush 直接完成，不继续向屏幕写入像素。
4. 亮屏时调用 `displayOn()`，随后 invalidate 当前活动屏幕并强制刷新一次，确保显示的是最新 UI。
5. 串口调试支持 `screen_on`、`screen_off` 和 `screen_toggle`，设备日志会记录 `screen on` / `screen off`。
6. daemon 通过 `ioreg -n Root -d1 -r` 轮询 macOS `IOConsoleLocked` 状态。锁屏持续 5 分钟后发送 `control/screen off`，解锁时发送 `control/screen on`。
7. 固件本地监测 BLE 连接状态。BLE 断开持续 5 分钟后，即使 Mac 端没有机会发送控制消息，固件也会本地关屏。
8. BLE 恢复连接时，固件不会自行亮屏；daemon 只有在 Mac 当前未锁屏时才发送 `control/screen on`，避免锁屏期间重连点亮屏幕。
9. daemon 队列只保留最新一条 `control` payload；BLE 写入前还会跳过锁屏期间遗留的过期亮屏控制，避免状态反转。
10. 屏幕关闭只影响面板显示；BLE、任务计数、用量刷新、截图和日志链路继续运行。

## 主题与设置链路

1. BLE 数据仍按既有协议写入唯一的固件业务模型，不包含主题配置。
2. `DashboardViewModelBuilder` 统一计算 Token、7d 额度、重置窗口、电量、连接状态和任务数，主题不能重新解释业务数据。
3. `ThemeRuntime` 根据稳定字符串 ID 从注册表选择主题，只挂载当前主题；未知 ID、挂载失败或持久化数据无效时安全回退 `classic`。
4. 主题切换会卸载旧主题、清理根节点、挂载新主题并立即重放完整 ViewModel，不需要等待下一条 BLE 消息。
5. 中间键短按打开设置页；Browse 状态下左/右移动选项，中间键进入编辑或执行开关；Edit 状态下左/右预览或调值，中间键确认。
6. 设置页包括主题、亮度、预留音量、自动换肤、1–1440 分钟切换间隔和退出；30 秒无操作、任务提醒或关屏会退出，并恢复未确认编辑。
7. 自动换肤仅在亮屏、仪表盘可见且没有设置页、提醒、亮度或主题提示浮层时累计；暂停期间不丢失已累计的有效展示时间。
8. 手动确认的主题和其他设置延迟约 2 秒写入 NVS；自动轮换不会更新持久化的首选主题，避免 Flash 写放大。
9. 主题系统属于 ESP32 展现层，macOS daemon 与 BLE payload 无需感知主题。

## 正常页防烧屏漂移链路

1. UI 初始化时创建 `theme_host` 承载当前主题的仪表盘。
2. 任务完成提醒层、设置页和系统浮层挂在屏幕根节点上，不参与正常页漂移。
3. `ui_tick()` 每隔 10 分钟从固定偏移序列中选择下一个位置，并把 `theme_host` 移动到 2px 范围内的新坐标。
4. 漂移只改变正常页容器坐标，不改变 BLE 协议、用量模型或各控件内部布局。
5. 任务完成提醒显示期间暂停漂移，避免全屏闪动和提醒文本出现额外位移。

## 屏幕方向自适应链路

1. 固件启动时在共享 I2C 总线上初始化 QMI8658，并启用加速度计低功耗采样。
2. `imu_tick()` 每 100ms 读取一次三轴加速度，使用简单低通滤波和阈值排除平放、倒扣等方向不明确状态。
3. 当某个候选方向持续稳定超过 350ms 后，IMU 模块才更新当前方向，避免拿起设备时画面抖动。
4. 方向以 0/1/2/3 表示，分别对应 0/90/180/270 度顺时针旋转。
5. LVGL 仍以 480x480 逻辑坐标渲染；`display_rotation_draw()` 在 flush 出口把局部 RGB565 区域旋转到物理坐标后再写入 CO5300。
6. 方向变化时，主循环先把屏幕亮度降为 0，强制整屏 invalidate 和刷新，然后用 4 个小步恢复到用户设置的亮度。
7. 屏幕关闭时仍会更新方向状态；再次亮屏时会刷新到最新方向。
8. 串口调试支持 `imu` 查看加速度读数和当前方向，`rotate auto` 恢复自动旋转，`rotate 0/90/180/270` 手动锁定方向。
9. `screenshot` 命令会在输出前按当前方向旋转快照，因此 USB 视觉 QA 看到的画面与设备物理显示一致。

## 亮度控制链路

1. 仪表盘中左键 GPIO0 短按降低亮度，右键 GPIO18 短按增加亮度；设置页编辑亮度时，两键调整预览值。
2. 固件维护逻辑亮度百分比，默认 60%，范围 10%-100%，每次调整 10%。
3. 亮度百分比会映射到 CO5300 `setBrightness()` 的 0-255 硬件亮度值。
4. 屏幕关闭时左/右键不生效，需先长按中间键亮屏。
5. 每次调整后，UI 顶层显示一个 3 秒亮度进度条和百分比；该浮层不进入 BLE 协议。
6. 串口调试支持 `brightness_down`、`brightness_up` 和 `brightness <pct>`，设备日志会记录亮度百分比和触发来源。

## USB 截图链路

1. 用户或自动化脚本运行 `./screenshot.sh out.png [port]`。
2. 宿主侧工具自动寻找 USB CDC 串口，或使用显式传入的 `/dev/cu.usbmodem...`。
3. 工具向固件发送一行 `screenshot`。
4. 固件刷新一次 LVGL，然后用 `lv_snapshot_take_to_draw_buf()` 将当前活动屏幕渲染到 PSRAM 中的 RGB565 缓冲区。
5. 固件会按当前屏幕方向旋转快照，再通过串口输出 `SCREENSHOT_START <width> <height> <bytes>`，随后写入原始 RGB565LE 字节，最后输出 `SCREENSHOT_END`。
6. 宿主侧工具校验尺寸和字节数，把 RGB565LE 转为 RGB888，并使用 Python 标准库写出 PNG。

该链路只用于调试和视觉 QA，不参与 BLE 数据协议，也不影响 macOS daemon 的运行职责。

## ESP32 日志链路

1. 固件启动后，关键事件会写入 48 条容量的环形日志，并同步打印到 USB 串口，格式为 `LOG <seq> <ms> <level> <message>`。
2. 日志覆盖启动、PMU 初始化、BLE 广播/连接/断开、BLE RX 队列溢出、JSON 解析错误、usage/activity/alert payload、stale 状态和截图结果。
3. 用户或自动化脚本运行 `./logs.sh [-n count] [port]`。
4. 宿主侧工具向固件发送 `logs` 或 `logs <count>`。
5. 固件输出 `LOGS_START <count>`、日志行和 `LOGS_END`。
6. `./logs.sh --clear [port]` 发送 `log_clear`，清空设备端环形日志。
7. `./logs.sh --follow [port]` 不发送查询命令，只跟随实时串口输出。

日志链路与截图链路共用 USB CDC 串口，不进入 BLE 协议。日志只保存运行事件和状态摘要，不保存 Codex 登录凭据或完整 payload 正文。

macOS daemon 使用 50MB 当前日志加一个 50MB 轮转备份，总占用上限约 100MB。设备注册表和用量缓存使用同目录临时文件、`fsync` 和原子替换；注册表额外保留一份 last-known-good 备份。

## 显示模型

正常页：

- `Classic` 与 `Cyberpunk` 主题使用独立 LVGL 对象树，但读取同一份 `DashboardViewModel`。
- Codex 返回 5h 窗口时展示 5h/7d 余量语义；只返回有效 7d 数据时展示今日/近7天 Token、7d 额度和重置窗口。具体位置、颜色、字体和图形由主题决定。
- 运行中任务数最多显示 6 个指示点；没有运行中任务时不显示。
- 按键：中间短按进入设置或确认，中间长按切换亮屏/关屏；仪表盘中左/右短按快捷调节亮度。
- 亮度浮层：调整亮度时显示百分比和进度条，3 秒后自动隐藏。
- 防烧屏：正常页每 10 分钟在 2px 范围内整体轻微漂移；提醒和亮度浮层保持固定。
- 方向：设备竖放、横放或倒置时，整个正常页会随重力方向自动旋转。

提醒页：

- 闪动阶段：只显示红、黄、绿全屏背景，不显示文字。
- 内容阶段：固定区域显示标题和最多四行正文，超出部分在第四行省略，避免动态摘要与标题重叠。
- 标题和正文优先使用 LittleFS 中的 TTF 字体经 LVGL TinyTTF 本地渲染；TTF 缺失的常见智能标点由内置 UI 字体兜底。Token 数值使用固件内嵌的 Montserrat TTF，确保数字、单位与小数点均按配置字号渲染。
- 提醒显示时关屏会同时关闭当前提醒。

## 协议

设备名：

```text
CodexMeter-<short_id>
```

GATT UUID：

- service：`4c41555a-4465-7669-6365-000000000001`
- RX write：`4c41555a-4465-7669-6365-000000000002`
- TX notify：`4c41555a-4465-7669-6365-000000000003`
- refresh notify：`4c41555a-4465-7669-6365-000000000004`
- identity read：`4c41555a-4465-7669-6365-000000000005`

用量 payload：

```json
{"v":1,"k":"usage","src":"codex","h5":72,"h5r":1783093200,"d7":84,"d7r":1783545600,"td":18600000,"t7":236000000,"st":"ok","t":1783070000}
```

提醒 payload：

```json
{"v":1,"k":"alert","id":"...","title":"任务完成！","body":"摘要","t":1783070000}
```

`task_complete` 提醒会额外带上完成后的运行中任务数：

```json
{"v":1,"k":"alert","id":"...","title":"任务完成！","body":"摘要","run":0,"t":1783070000}
```

运行中任务 payload：

```json
{"v":1,"k":"activity","src":"codex","run":2,"t":1783070000}
```

屏幕控制 payload：

```json
{"v":1,"k":"control","cmd":"screen","on":true,"why":"mac_unlocked","t":1783070000}
```

字段说明：

- `v`：协议版本。
- `k`：payload 类型，当前支持 `usage`、`alert`、`activity` 和 `control`。
- `src`：用量来源，当前为 `codex`。
- `h5` / `d7`：剩余百分比，整数；没有 5h 窗口时 `h5` 为 `null`，固件据此选择 Token 活动布局。
- `h5r` / `d7r`：重置时间戳，单位为秒。
- `td` / `t7`：今日与包含今日在内的近 7 个自然日 Token 数；缺失时为 `null`，设备显示 `--`。
- `st`：状态，例如 `ok`、`stale` 或 Codex App Server 返回的限额状态。
- `t`：payload 生成时间戳；固件用它和本地 `millis()` 推算当前时间，避免依赖 ESP32 自身联网校时。
- `id`：提醒事件 ID，默认由 daemon 生成短 UUID。
- `title` / `body`：提醒标题和正文，发送前会做设备字库清洗和长度约束。
- `run`：当前正在运行的 Codex 任务数量。
- `cmd` / `on` / `why`：控制命令、屏幕目标状态和触发原因；当前 `control` 只支持 `cmd:"screen"`。

## 安装与运行

- `install-mac.sh` 会创建 `.venv/`，安装 Python 包，写入 `~/Library/LaunchAgents/com.user.codexmeter.plist`，安装 CodexMeter `UserPromptSubmit` / `Stop` hooks，并加载 LaunchAgent。
- LaunchAgent 默认运行 `.venv/bin/codexmeterd --codex-bin <codex>`。
- 如果 `codex` 不在默认 `PATH`，安装时可用 `CODEX_BIN=/path/to/codex ./install-mac.sh` 指定。
- daemon 主日志写入 `~/.codexmeter/codexmeter.log`，LaunchAgent stdout/stderr 分别写入 `~/.codexmeter/codexmeter.out.log` 和 `~/.codexmeter/codexmeter.err.log`。
- `codexmeterctl` 默认通过 `~/.codexmeter/events.sock` 与 daemon 通信。
- `codexmeterctl devices scan/adopt/list/rename/enable/disable` 维护本机设备登记表；alias 只用于展示，不参与 BLE 匹配。

## 关键约束

- BLE payload 控制在 512 bytes 内。
- 多设备场景下，每台设备拥有独立 BLE 队列、ACK 状态、失败计数和重连 backoff；任一设备异常不阻塞其他设备。
- transcript 监听只接受 `CODEX_HOME` 内路径，从注册时的文件末尾开始增量读取，并限制单次读取和单行缓冲大小；任何异常都不影响事件服务主路径。
- 活动任务 TTL 默认为 60 分钟无活动，检查间隔为 30 秒；使用单调时钟，任务独立过期，延迟到达的结束事件按已结束历史幂等处理。`--activity-ttl 0` 可关闭。
- alert 正文在 macOS 端先限制为 210 bytes，再由整体 payload 编码器做最终兜底裁剪。
- 固件 alert 正文缓冲区为 240 bytes，标题缓冲区为 48 bytes。
- 固件 BLE RX 队列可缓存 6 个 payload，避免连续写入时后一个 payload 覆盖前一个。
- 固件设备日志保留最近 48 条，每条消息最多 143 bytes；超过容量时覆盖最旧日志。
- 固件 180 秒无新 usage 更新时进入 stale 状态。
- Mac 锁屏和 BLE 断连的自动关屏阈值均为 5 分钟；Mac 锁屏轮询间隔默认为 5 秒。
- 固件亮度范围为 10%-100%，默认 60%，步进 10%，亮度提示浮层停留 3 秒。
- 固件自动换肤间隔范围为 1–1440 分钟，默认 10 分钟且默认关闭；设置页无操作超时为 30 秒。
- 设备设置使用版本化、带 CRC 的 NVS 记录，并在最后一次修改约 2 秒后合并写入。
- alert 闪动步长为 180 ms，共 6 步；文字出现后停留 8 秒。
- ESP32 端不联网校时，重置倒计时依赖 daemon payload 的 `t` 字段和设备本地运行时钟。
- 480x480 RGB565 全帧截图约 450 KiB，当前仅在带 PSRAM 的目标板启用；无 PSRAM 固件会返回 `SCREENSHOT_UNSUPPORTED`。
