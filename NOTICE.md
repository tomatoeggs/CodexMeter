# 说明

CodexMeter 参考了 HermannBjorgvin/Clawdmeter 的产品形态和通信思路。
Clawdmeter 是一个用于显示 Claude Code 用量的 ESP32 桌面小屏项目。

本项目沿用了 BLE JSON 通道的整体设计，并使用兼容的自定义 GATT UUID，
从而让硬件通信模型保持一致。

当前 CodexMeter 没有直接引入大段 Clawdmeter 源码。若未来直接复制
Clawdmeter 文件，请在相邻位置保留上游版权和许可证说明。

USB 截图能力参考了 Clawdmeter 的串口协议和 LVGL snapshot 思路：
设备接收 `screenshot` 命令后输出 `SCREENSHOT_START`、RGB565 帧缓冲和
`SCREENSHOT_END`。CodexMeter 的宿主侧 PNG 转换器为本项目重新实现，避免额外依赖。

屏幕方向自适应参考了 Clawdmeter 在 Waveshare AMOLED 2.16 上的实现思路：
使用 QMI8658 加速度计判断 0/90/180/270 度方向，并在 CO5300 不依赖硬件旋转的前提下
对 LVGL 局部刷新区域做 CPU 侧 RGB565 旋转。CodexMeter 的 `imu.*` 和
`display_rotation.*` 按本项目模块边界重新实现。
