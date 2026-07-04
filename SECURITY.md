# 安全说明

CodexMeter 会调用本机 Codex App Server 获取订阅用量，但项目代码不应读取、打印或提交 Codex 登录 token。

## 支持范围

当前首版支持：

- macOS host daemon
- Codex `Stop` hook 和 `notify` 包装器
- Waveshare ESP32-S3-Touch-AMOLED-2.16 固件
- BLE 本地通信

## 报告安全问题

如果发现可能导致凭据泄露、任意代码执行、BLE 未授权写入或其他安全问题，请优先通过 GitHub 私密漏洞报告功能联系维护者。若仓库未启用该功能，请先私下联系维护者，再公开 issue。

报告时请尽量包含：

- 受影响的版本或 commit
- 复现步骤
- 风险说明
- 建议修复方向

## 凭据处理

- 不要把 `~/.codex/`、`~/.codexmeter/`、shell 历史、token、cookie 或个人日志提交到仓库。
- `codexmeterd` 日志应只记录状态和摘要，不应记录认证凭据。
- `codex_limits_demo.py` 仅作为额度读取参考，不应包含个人密钥。
