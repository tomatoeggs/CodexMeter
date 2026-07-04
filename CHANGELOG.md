# 更新日志

## 未发布

- 新增运行中任务指示：Codex 任务开始后在正常页底部显示对应数量的小蓝点，任务完成后自动隐藏。
- 新增 `activity` BLE payload 和 `codexmeterctl demo-activity` 调试命令。
- 安装脚本会同时安装 `UserPromptSubmit` 与 `Stop` hook，用于跟踪任务开始和结束。
- 修复 start/stop hook 输入 ID 不一致时，任务完成后小蓝点可能残留的问题。
- Stop hook 改为发送单个 `task_complete` 事件，避免完成提醒已显示但运行中任务数未清零。
- 新增 USB 串口截图工具：固件支持 `screenshot` 命令，`./screenshot.sh` 可抓取当前屏幕并保存为 PNG，用于视觉 QA。
- 修复连续 BLE 写入时固件单槽 RX 缓冲被覆盖，导致完成提醒出现但小蓝点偶发不消失的问题。
- 新增 ESP32 设备端环形日志和 `./logs.sh` 查询工具，支持日志查询、清空和实时跟随。

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
