# 安全说明

CodexMeter 会调用本机 Codex App Server 获取订阅用量，但项目代码不应读取、打印或提交 Codex 登录 token。

## 支持范围

安全修复优先覆盖 `main` 和最新的 `2.0.x` 版本，范围包括：

- macOS host daemon
- Codex `UserPromptSubmit` 和 `Stop` hooks
- 多设备发现、身份校验和本地设备注册表
- Waveshare ESP32-S3-Touch-AMOLED-2.16 固件
- BLE 本地通信

## 报告安全问题

如果发现可能导致凭据泄露、任意代码执行、BLE 未授权写入或其他安全问题，请优先通过 GitHub 仓库的 **Security > Report a vulnerability** 私密联系维护者。若仓库未启用该功能，请先私下联系维护者，不要在公开 issue 中披露漏洞细节、凭据或个人日志。

报告时请尽量包含：

- 受影响的版本或 commit
- 复现步骤
- 风险说明
- 建议修复方向

## 凭据处理

- 不要把 `~/.codex/`、`~/.codexmeter/`、shell 历史、token、cookie 或个人日志提交到仓库。
- `codexmeterd` 日志应只记录状态和摘要，不应记录认证凭据。
- `codex_limits_demo.py` 仅作为额度读取参考，不应包含个人密钥。
