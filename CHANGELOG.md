# 更新日志

## 未发布

## v2.1.0

- 新增 Token 活动主页：当 Codex 只返回有效 7d 限额窗口时，自动显示今日/近7天 Token 消耗和 7d 额度；5h 窗口恢复时自动切回原有双余量布局。
- macOS daemon 新增可选 `account/usage/read` 读取、近 7 个自然日聚合和缓存回退；旧版 Codex 或接口失败不会阻断限额刷新。
- 修复服务端当天 Token 桶尚未生成时误显示 `0`：改用本机 session `token_count` 增量作为实时回退，并过滤 fork 的历史事件回放。
- `usage` BLE payload 以兼容方式新增 `td` / `t7` 字段，并增加宿主与串口 Token 布局预览命令。
- 完善 Token 主页视觉样式：统一标题和数字字体、字号与间距，移除多余滚动条和标点，并使用可配置的 Montserrat TTF 渲染 Token 数值。

## v2.0.2

- 修复任务完成摘要中 `“”‘’—–…·` 等常见智能标点无法渲染的问题：这些符号现在会进入内置 UI 字体，作为 TinyTTF 字体缺字时的 fallback。
- 修复 macOS CoreBluetooth 残留连接下设备停止广播后，多设备 discovery 无法重新接管连接的问题；补充发现来源与扫描健康状态。
- 为 BLE 连接和断开清理增加独立超时，避免 CoreBluetooth 卡住单台设备 worker 并阻止后续重连。
- 完善公开仓库文档、快速开始、项目截图、安全说明和 Python 包元数据，并以 MIT License 开源。
- README 增加 CodexMeter 实物运行照片。

## v2.0.1

- 任务完成摘要改为固定四行正文区域，并把宿主摘要上限提高到 64 字符。
- BLE 告警在收到 ACK 前保持为 in-flight，断连后重试，并由固件按告警 ID 去重。
- BLE 连接后校验完整芯片 identity，并用原子写入持久化注册表升级。
- daemon 增加单实例锁、关键任务 fail-fast 监督和约 100MB 的日志总上限。
- 自动烧录仅选择通过 identity 校验的 CodexMeter 串口，恢复模式需要显式 `--force`。
- 对齐宿主与固件的 512 字节 payload 边界，并锁定已验证的依赖版本。

## v2.0.0

- 新增多 CodexMeter 支持：daemon 可同时驱动多台已登记设备，每台设备拥有独立 BLE 队列、ACK、健康状态和重连 backoff。
- 新增设备登记与发现命令：`codexmeterctl devices scan/list/adopt/rename/enable/disable`。
- 固件新增稳定设备身份，BLE 广播名改为 `CodexMeter-<short_id>`，并提供 BLE/USB identity 查询。
- USB 日志和截图工具支持 `--device` 与 `--list`，多设备连接时避免误选设备。
- `flash-mac.sh` 支持多串口保护和 `--all` 批量烧录。

- 新增 AMOLED 防烧屏像素漂移：正常页每 10 分钟在 2px 范围内整体轻微移动，任务完成提醒和亮度浮层保持固定。
- 强化 macOS 端 BLE 健康检查：TX ACK notify 订阅失败时不再降级为“写入成功”，并增加 45 秒应用层 heartbeat，避免 CoreBluetooth 假活导致设备端持续 `stale`。
- Codex 用量读取失败时，会用上一次成功数据发送 `stale` heartbeat，保持 BLE 链路探活并明确区分数据源异常和蓝牙异常。
- `codexmeterctl status` 会返回 BLE 健康字段，包括连接状态、队列深度、最近写入/ACK 时间和连续失败次数。
- 增加 Codex 用量快照稳定器：过滤 App Server 偶发返回的 7d 空窗口，避免屏幕短暂显示 `100%` 和 `7d 后重置`。

## v0.5.1

- 修复 macOS CoreBluetooth 连接假活时，daemon 持续排队但设备端用量变为 `stale` 的问题：BLE 写入或设备 ACK 超时后会主动断开并自动重连。
- 新增 `codexmeterd --ble-write-timeout` 和 `--ble-ack-timeout` 参数，用于调整 BLE 自动恢复超时。

## v0.5.0

- 新增 QMI8658 IMU 屏幕方向自适应：支持 0/90/180/270 度自动旋转，并在方向切换时压暗重绘。
- 新增 `imu`、`rotate auto`、`rotate 0/90/180/270` 串口调试命令，方便查看 IMU 读数和手动锁定方向。
- USB 截图会按当前物理方向输出，便于自动旋转场景下做视觉 QA。

## v0.4.0

- 新增中间按键切换 AMOLED 亮屏/关屏能力，并提供 `screen_on`、`screen_off`、`screen_toggle` 串口调试命令。
- 新增自动亮屏/关屏策略：Mac 锁屏 5 分钟或 BLE 断连 5 分钟后关屏，Mac 解锁或未锁屏时 BLE 恢复后亮屏。
- 新增 `control` BLE payload 和 `codexmeterctl screen-on/screen-off` 调试命令。
- 新增左/右按键亮度调节：左键降低亮度，右键增加亮度，调整后显示 3 秒亮度进度条。

## v0.3.0

- 新增 ESP32 设备端环形日志和 `./logs.sh` 查询工具，支持日志查询、清空和实时跟随。
- 修复连续 BLE 写入时固件单槽 RX 缓冲被覆盖，导致完成提醒出现但小蓝点偶发不消失的问题。

## v0.2.0

- 新增运行中任务指示：Codex 任务开始后在正常页底部显示对应数量的小蓝点，任务完成后自动隐藏。
- 新增 `activity` BLE payload 和 `codexmeterctl demo-activity` 调试命令。
- 安装脚本会同时安装 `UserPromptSubmit` 与 `Stop` hook，用于跟踪任务开始和结束。
- 修复 start/stop hook 输入 ID 不一致时，任务完成后小蓝点可能残留的问题。
- Stop hook 改为发送单个 `task_complete` 事件，避免完成提醒已显示但运行中任务数未清零。
- 新增 USB 串口截图工具：固件支持 `screenshot` 命令，`./screenshot.sh` 可抓取当前屏幕并保存为 PNG，用于视觉 QA。

## v0.1.0

首个可用里程碑版本。

- macOS daemon：通过 Codex App Server 读取 `codex` 订阅 5h/7d 剩余用量。
- BLE：向设备发送 `usage` 和 `alert` payload，payload 限制在 512 bytes 内。
- Codex 完成提醒：安装 `Stop` hook，任务完成后显示提醒摘要。
- ESP32 固件：支持 Waveshare ESP32-S3-Touch-AMOLED-2.16。
- UI：显示 Codex 标识、剩余用量、电池状态、5h/7d 剩余百分比和重置倒计时。
- 重置倒计时：5h 显示为 `HH:MM 后重置`，7d 显示为 `xd 后重置`。
- 提醒页：红、黄、绿全屏闪动后显示“任务完成！”和任务摘要。
- 字体与文本：加入中文子集字体和设备字库清洗，避免摘要乱码。
- 开发体验：提供安装脚本、刷机脚本、串口 demo 命令和 Python 测试。
