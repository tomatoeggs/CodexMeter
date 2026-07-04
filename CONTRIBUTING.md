# 贡献指南

感谢你愿意改进 CodexMeter。这个项目同时包含 macOS daemon、Codex hook 和 ESP32 固件，修改时请尽量保持模块职责清晰。

## 本地开发

安装 macOS 端依赖：

```bash
python3 -m venv .venv
.venv/bin/python -m pip install --upgrade pip
.venv/bin/python -m pip install -e .
```

运行测试：

```bash
.venv/bin/python tests/run_tests.py
```

编译固件：

```bash
PLATFORMIO_CORE_DIR="$PWD/.platformio" \
UV_CACHE_DIR="$PWD/.platformio/.cache/uv" \
.venv/bin/pio run -d firmware -e waveshare_amoled_216
```

烧录固件：

```bash
./flash-mac.sh waveshare_amoled_216 /dev/cu.usbmodem211201
```

## 代码约定

- macOS 端保持单一职责：App Server、限额归一化、BLE、payload、事件入口分别独立。
- 固件端保持硬件、BLE、模型解析、UI、电源管理分层。
- BLE payload 必须控制在 512 bytes 以内。
- alert 文本需要经过设备字库清洗，避免屏幕乱码。
- 不提交 `.venv/`、`.platformio/`、`.npm-cache/`、`__pycache__/`、构建产物和日志。
- 不提交 token、cookie、Codex 登录态、GitHub token 或任何个人凭据。

## 提交前检查

```bash
.venv/bin/python tests/run_tests.py
PLATFORMIO_CORE_DIR="$PWD/.platformio" UV_CACHE_DIR="$PWD/.platformio/.cache/uv" .venv/bin/pio run -d firmware -e waveshare_amoled_216
```

如果只修改文档，可以在 PR 或提交说明里注明未运行测试。
