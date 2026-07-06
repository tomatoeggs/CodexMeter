# 更新日志

## 未发布

暂无。

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
