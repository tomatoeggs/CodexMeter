# 更新日志

## v0.1.0

首个可用里程碑版本。

- macOS daemon：通过 Codex App Server 读取 `codex` 订阅 5h/7d 剩余用量。
- BLE：向设备发送 `usage` 和 `alert` payload，payload 限制在 512 bytes 内。
- Codex 完成提醒：安装 `Stop` hook，并通过 `notify` 包装器提供兜底链路。
- ESP32 固件：支持 Waveshare ESP32-S3-Touch-AMOLED-2.16。
- UI：显示 Codex 标识、剩余用量、电池状态、5h/7d 剩余百分比和重置倒计时。
- 重置倒计时：5h 显示为 `HH:MM 后重置`，7d 显示为 `xd 后重置`。
- 提醒页：红、黄、绿全屏闪动后显示“任务完成！”和任务摘要。
- 字体与文本：加入中文子集字体和设备字库清洗，避免摘要乱码。
- 开发体验：提供安装脚本、刷机脚本、串口 demo 命令和 Python 测试。
