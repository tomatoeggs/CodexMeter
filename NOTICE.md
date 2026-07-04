# 说明

CodexMeter 参考了 HermannBjorgvin/Clawdmeter 的产品形态和通信思路。
Clawdmeter 是一个用于显示 Claude Code 用量的 ESP32 桌面小屏项目。

本项目沿用了 BLE JSON 通道的整体设计，并使用兼容的自定义 GATT UUID，
从而让硬件通信模型保持一致。

当前首版 CodexMeter 没有直接引入大段 Clawdmeter 源码。若未来直接复制
Clawdmeter 文件，请在相邻位置保留上游版权和许可证说明。
