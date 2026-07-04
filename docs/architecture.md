# CodexMeter 架构说明

## 调研结论

- Codex 剩余用量以本仓库的 `codex_limits_demo.py` 为准：通过 `codex app-server --listen stdio://` 走 JSON-RPC，初始化后读取 `account/read` 与 `account/rateLimits/read`。
- Clawdmeter 的硬件目标是 Waveshare ESP32-S3-Touch-AMOLED-2.16，屏幕为 480x480 CO5300，PMU 为 AXP2101；CodexMeter 首版保持同一硬件。
- BLE 沿用 Clawdmeter 的自定义数据服务 UUID：service `...0001`，RX write `...0002`，TX notify `...0003`。CodexMeter 额外保留 `...0004` 作为设备主动请求刷新信号。
- 固件保持 PlatformIO、Arduino_GFX、LVGL、ArduinoJson、NimBLE 技术栈，以降低硬件适配风险。
- 首版只面向 macOS + Waveshare ESP32-S3-Touch-AMOLED-2.16 + Codex 订阅余量，但协议中的 `src` 和模块边界为未来其他订阅来源预留了空间。

## 模块职责

- `codexmeter.app_server`：负责 stdio JSON-RPC 进程管理、初始化握手和请求响应匹配。
- `codexmeter.provider`：负责调用 Codex App Server 并转换为订阅用量快照。
- `codexmeter.limits`：负责限额桶归一化，当前默认只读取 `codex` 桶，将 300 分钟映射为 `5h`，10080 分钟映射为 `7d`。
- `codexmeter.payloads`：负责 BLE JSON payload 生成、摘要清洗、设备字库字符过滤和 512 bytes 长度约束。
- `codexmeter.ble`：负责发现、连接和写入 `CodexMeter` BLE 外设；macOS 上会优先尝试找回已连接的 CoreBluetooth 外设。
- `codexmeter.events`：负责本地 Unix socket 事件入口，供 Codex hooks 与 `codexmeterctl` 使用；同时维护运行中任务集合。
- `codexmeter.screen_policy`：负责 macOS 锁屏状态轮询、自动亮屏/关屏状态机和 BLE 重连亮屏策略。
- `hooks/codexmeter_start_hook.py`：负责接收 Codex `UserPromptSubmit` hook 输入，通知 daemon 有任务开始运行。
- `hooks/codexmeter_stop_hook.py`：负责接收 Codex `Stop` hook 输入、生成短摘要并静默通知 daemon；异常时仍成功退出，不阻塞 Codex。
- `scripts/install_hook.py`：负责合并 CodexMeter `Stop` hook 到 `~/.codex/hooks.json`，并在覆盖前备份旧文件。
- `firmware/src/model.*`：负责解析 BLE JSON 为固件内部模型，并记录 `usage` payload 接收时的 `millis()`，用于后续倒计时计算。
- `firmware/src/ui.*`：负责 LVGL 显示、余量卡片、电池图标、重置倒计时、红黄绿闪屏和任务完成视图。
- `firmware/src/ble_service.*`：负责 GATT 服务、RX/TX、ACK/NACK 与刷新通知。
- `firmware/src/power.*`：负责 AXP2101 电量和中间 PKEY 事件。
- `firmware/src/main.cpp`：负责板级初始化、主循环调度、串口调试命令、LVGL flush、AMOLED 亮屏/关屏和亮度控制。
- `firmware/src/device_log.*`：负责 ESP32 端关键事件环形日志、实时串口打印和按需日志 dump。
- `tools/capture_screenshot.py`：负责通过 USB 串口触发固件截图命令、读取 RGB565 帧缓冲并编码为 PNG，用于本地视觉 QA。
- `tools/read_device_logs.py`：负责通过 USB 串口查询、清空或跟随 ESP32 设备日志。
- `screenshot.sh`：截图工具入口，自动选择 Python 并转发参数。
- `logs.sh`：日志工具入口，自动选择 Python 并转发参数。

## 数据流

1. daemon 每 60 秒拉取 Codex App Server 限额。
2. daemon 将 `codex` 限额归一化为 `UsageSnapshot`，生成 `usage` payload 并放入发送队列。
3. BLE transport 发现或找回 `CodexMeter` 外设，连接后写入 RX characteristic。
4. 固件解析 `usage`，更新 5h/7d 剩余百分比，并用 `t` 加设备本地经过时间计算重置倒计时：
   - 5h 显示为 `HH:MM 后重置`
   - 7d 显示为 `xd 后重置`
5. 若 180 秒没有新用量，固件将状态标记为 `stale`，并等待下一次 daemon 更新或设备刷新请求。
6. Codex `UserPromptSubmit` hook 触发时，start hook 通过 Unix socket 向 daemon 发送 `task_start`。
7. daemon 用 `session_id`、`turn_id` 或 `cwd` 推导任务 key，维护运行中任务集合；计数变化时生成 `activity` payload。
8. 固件收到 `activity` 后，在正常页底部居中显示对应数量的小蓝点；计数为 0 时隐藏。
9. Codex turn 完成时触发用户级 `Stop` hook；Stop hook 从 `last_assistant_message` 中生成不超过 96 字符的短摘要。
10. Stop hook 只发送一次 `task_complete` 事件。daemon 在同一个事件处理中先清除运行中任务，再构造 `alert` payload。
11. daemon 会同时发送 `activity` payload，并把完成后的 `run` 数量写入 `alert` payload，避免 BLE 连续写入丢失时小蓝点残留。
12. daemon 构造 `alert` payload 时会清洗 Markdown、过滤设备字库不支持的字符，并限制 payload 在 512 bytes 内。
13. 固件 BLE RX 使用小队列缓存连续 payload；收到携带 `run` 的 `alert` 时，会在显示提醒前同步更新活动任务数。
14. 固件收到 `alert` 后先隐藏文字并红、黄、绿全屏闪动；闪动结束后显示“任务完成！”和 28px 正文摘要，文字出现后默认停留 8 秒。
15. 用户可按中间按键切换 AMOLED 亮屏和关闭；提醒显示时关屏会同时关闭当前提醒。
16. 用户可按左/右按键降低/增加亮度，固件显示 3 秒亮度进度条。

## 屏幕电源控制链路

1. 固件在 `main.cpp` 维护 `screen_on` 状态，启动后默认为亮屏。
2. AXP2101 PKEY 短按进入按键处理逻辑，切换 `screen_on`。
3. 关屏时调用 CO5300 驱动的 `displayOff()`，并让 LVGL flush 直接完成，不继续向屏幕写入像素。
4. 亮屏时调用 `displayOn()`，随后 invalidate 当前活动屏幕并强制刷新一次，确保显示的是最新 UI。
5. 串口调试支持 `screen_on`、`screen_off` 和 `screen_toggle`，设备日志会记录 `screen on` / `screen off`。
6. daemon 通过 `ioreg -n Root -d1 -r` 轮询 macOS `IOConsoleLocked` 状态。锁屏持续 5 分钟后发送 `control/screen off`，解锁时发送 `control/screen on`。
7. 固件本地监测 BLE 连接状态。BLE 断开持续 5 分钟后，即使 Mac 端没有机会发送控制消息，固件也会本地关屏。
8. BLE 恢复连接时，固件不会自行亮屏；daemon 只有在 Mac 当前未锁屏时才发送 `control/screen on`，避免锁屏期间重连点亮屏幕。
9. daemon 队列只保留最新一条 `control` payload；BLE 写入前还会跳过锁屏期间遗留的过期亮屏控制，避免状态反转。
10. 屏幕关闭只影响面板显示；BLE、任务计数、用量刷新、截图和日志链路继续运行。

## 亮度控制链路

1. 左键 GPIO0 短按降低亮度，右键 GPIO18 短按增加亮度，中间 AXP2101 PKEY 不参与亮度调节。
2. 固件维护逻辑亮度百分比，默认 80%，范围 10%-100%，每次调整 10%。
3. 亮度百分比会映射到 CO5300 `setBrightness()` 的 0-255 硬件亮度值。
4. 若屏幕处于关闭状态，按左/右键会先亮屏，再应用新的亮度，保证用户能看到反馈。
5. 每次调整后，UI 顶层显示一个 3 秒亮度进度条和百分比；该浮层不进入 BLE 协议。
6. 串口调试支持 `brightness_down`、`brightness_up` 和 `brightness <pct>`，设备日志会记录亮度百分比和触发来源。

## USB 截图链路

1. 用户或自动化脚本运行 `./screenshot.sh out.png [port]`。
2. 宿主侧工具自动寻找 USB CDC 串口，或使用显式传入的 `/dev/cu.usbmodem...`。
3. 工具向固件发送一行 `screenshot`。
4. 固件刷新一次 LVGL，然后用 `lv_snapshot_take_to_draw_buf()` 将当前活动屏幕渲染到 PSRAM 中的 RGB565 缓冲区。
5. 固件通过串口输出 `SCREENSHOT_START <width> <height> <bytes>`，随后写入原始 RGB565LE 字节，最后输出 `SCREENSHOT_END`。
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

日志链路与截图链路共用 USB CDC 串口，不进入 BLE 协议。日志只保存运行事件和状态摘要，不保存 Codex token 或完整 payload 正文。

## 显示模型

正常页：

- 顶部左侧：Codex 标识。
- 顶部中间：`剩余用量`。
- 顶部右侧：电量百分比和电池图标，填充色按电量变为绿色、黄色或红色。
- 第一张卡片：`5h 剩余:`、剩余百分比、`HH:MM 后重置`。
- 第二张卡片：`7d 剩余:`、剩余百分比、`xd 后重置`。
- 底部：运行中 Codex 任务数指示。1 个任务显示 1 个小蓝点，2 个任务显示 2 个小蓝点；没有运行中任务时不显示。
- 按键：中间短按切换 AMOLED 亮屏和关闭；左/右短按调节亮度；关屏不影响后台数据更新。
- 亮度浮层：调整亮度时显示百分比和进度条，3 秒后自动隐藏。

提醒页：

- 闪动阶段：只显示红、黄、绿全屏背景，不显示文字。
- 内容阶段：固定区域显示标题和正文，避免动态摘要与标题重叠。
- 标题使用 30px 中文字体，正文使用 28px 专用中文字库。
- 提醒显示时关屏会同时关闭当前提醒。

## 协议

设备名：

```text
CodexMeter
```

GATT UUID：

- service：`4c41555a-4465-7669-6365-000000000001`
- RX write：`4c41555a-4465-7669-6365-000000000002`
- TX notify：`4c41555a-4465-7669-6365-000000000003`
- refresh notify：`4c41555a-4465-7669-6365-000000000004`

用量 payload：

```json
{"v":1,"k":"usage","src":"codex","h5":72,"h5r":1783093200,"d7":84,"d7r":1783545600,"st":"ok","t":1783070000}
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
- `src`：用量来源，首版为 `codex`。
- `h5` / `d7`：剩余百分比，整数。
- `h5r` / `d7r`：重置时间戳，单位为秒。
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

## 关键约束

- BLE payload 控制在 512 bytes 内。
- alert 正文在 macOS 端先限制为 210 bytes，再由整体 payload 编码器做最终兜底裁剪。
- 固件 alert 正文缓冲区为 240 bytes，标题缓冲区为 48 bytes。
- 固件 BLE RX 队列可缓存 6 个 payload，避免连续写入时后一个 payload 覆盖前一个。
- 固件设备日志保留最近 48 条，每条消息最多 143 bytes；超过容量时覆盖最旧日志。
- 固件 180 秒无新 usage 更新时进入 stale 状态。
- Mac 锁屏和 BLE 断连的自动关屏阈值均为 5 分钟；Mac 锁屏轮询间隔默认为 5 秒。
- 固件亮度范围为 10%-100%，默认 80%，步进 10%，亮度提示浮层停留 3 秒。
- alert 闪动步长为 180 ms，共 6 步；文字出现后停留 8 秒。
- ESP32 端不联网校时，重置倒计时依赖 daemon payload 的 `t` 字段和设备本地运行时钟。
- 480x480 RGB565 全帧截图约 450 KiB，当前仅在带 PSRAM 的目标板启用；无 PSRAM 固件会返回 `SCREENSHOT_UNSUPPORTED`。
