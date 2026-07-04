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
- `codexmeter.events`：负责本地 Unix socket 事件入口，供 Codex hook、notify 包装器与 `codexmeterctl` 使用。
- `hooks/codexmeter_stop_hook.py`：负责接收 Codex `Stop` hook 输入、生成短摘要并静默通知 daemon；异常时仍成功退出，不阻塞 Codex。
- `hooks/codexmeter_notify.py`：负责包装 Codex `notify`，作为 hook 未被信任或未执行时的 turn-ended 兜底提醒，同时继续调用原通知命令。
- `scripts/install_hook.py`：负责合并 CodexMeter `Stop` hook 到 `~/.codex/hooks.json`，并在覆盖前备份旧文件。
- `scripts/install_notify.py`：负责包装 `~/.codex/config.toml` 中的 `notify` 配置，并保留原 notify 命令链。
- `firmware/src/model.*`：负责解析 BLE JSON 为固件内部模型，并记录 `usage` payload 接收时的 `millis()`，用于后续倒计时计算。
- `firmware/src/ui.*`：负责 LVGL 显示、余量卡片、电池图标、重置倒计时、红黄绿闪屏和任务完成视图。
- `firmware/src/ble_service.*`：负责 GATT 服务、RX/TX、ACK/NACK 与刷新通知。
- `firmware/src/power.*`：负责 AXP2101 电量和 PWR/按键事件。

## 数据流

1. daemon 每 60 秒拉取 Codex App Server 限额。
2. daemon 将 `codex` 限额归一化为 `UsageSnapshot`，生成 `usage` payload 并放入发送队列。
3. BLE transport 发现或找回 `CodexMeter` 外设，连接后写入 RX characteristic。
4. 固件解析 `usage`，更新 5h/7d 剩余百分比，并用 `t` 加设备本地经过时间计算重置倒计时：
   - 5h 显示为 `HH:MM 后重置`
   - 7d 显示为 `xd 后重置`
5. 若 180 秒没有新用量，固件将状态标记为 `stale`，并等待下一次 daemon 更新或设备刷新请求。
6. Codex turn 完成时优先触发用户级 `Stop` hook；若 hook 未被信任或未执行，Codex `notify` 包装器作为兜底。
7. hook 或 notify 包装器从 `last_assistant_message`、`message`、`summary` 或 `text` 中生成不超过 96 字符的短摘要；没有摘要时使用默认完成文案。
8. 摘要经 Unix socket 发送给 daemon。daemon 构造 `alert` payload 时会清洗 Markdown、过滤设备字库不支持的字符，并限制 payload 在 512 bytes 内。
9. 固件收到 `alert` 后先隐藏文字并红、黄、绿全屏闪动；闪动结束后显示“任务完成！”和 28px 正文摘要，文字出现后默认停留 8 秒。
10. 用户可按 BOOT/侧边按键提前关闭提醒。

## 显示模型

正常页：

- 顶部左侧：Codex 标识。
- 顶部中间：`剩余用量`。
- 顶部右侧：电量百分比和电池图标，填充色按电量变为绿色、黄色或红色。
- 第一张卡片：`5h 剩余:`、剩余百分比、`HH:MM 后重置`。
- 第二张卡片：`7d 剩余:`、剩余百分比、`xd 后重置`。

提醒页：

- 闪动阶段：只显示红、黄、绿全屏背景，不显示文字。
- 内容阶段：固定区域显示标题和正文，避免动态摘要与标题重叠。
- 标题使用 30px 中文字体，正文使用 28px 专用中文字库。

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

字段说明：

- `v`：协议版本。
- `k`：payload 类型，当前支持 `usage` 和 `alert`。
- `src`：用量来源，首版为 `codex`。
- `h5` / `d7`：剩余百分比，整数。
- `h5r` / `d7r`：重置时间戳，单位为秒。
- `st`：状态，例如 `ok`、`stale` 或 Codex App Server 返回的限额状态。
- `t`：payload 生成时间戳；固件用它和本地 `millis()` 推算当前时间，避免依赖 ESP32 自身联网校时。
- `id`：提醒事件 ID，默认由 daemon 生成短 UUID。
- `title` / `body`：提醒标题和正文，发送前会做设备字库清洗和长度约束。

## 安装与运行

- `install-mac.sh` 会创建 `.venv/`，安装 Python 包，写入 `~/Library/LaunchAgents/com.user.codexmeter.plist`，并加载 LaunchAgent。
- LaunchAgent 默认运行 `.venv/bin/codexmeterd --codex-bin <codex>`。
- 如果 `codex` 不在默认 `PATH`，安装时可用 `CODEX_BIN=/path/to/codex ./install-mac.sh` 指定。
- daemon 主日志写入 `~/.codexmeter/codexmeter.log`，LaunchAgent stdout/stderr 分别写入 `~/.codexmeter/codexmeter.out.log` 和 `~/.codexmeter/codexmeter.err.log`。
- `codexmeterctl` 默认通过 `~/.codexmeter/events.sock` 与 daemon 通信。

## 关键约束

- BLE payload 控制在 512 bytes 内。
- alert 正文在 macOS 端先限制为 210 bytes，再由整体 payload 编码器做最终兜底裁剪。
- 固件 alert 正文缓冲区为 240 bytes，标题缓冲区为 48 bytes。
- 固件 180 秒无新 usage 更新时进入 stale 状态。
- alert 闪动步长为 180 ms，共 6 步；文字出现后停留 8 秒。
- ESP32 端不联网校时，重置倒计时依赖 daemon payload 的 `t` 字段和设备本地运行时钟。
