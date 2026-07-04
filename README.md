# CodexMeter

CodexMeter 是一个基于 ESP32 小屏幕的 Codex 订阅余量与任务完成提醒显示器。

首版支持：

- macOS 后台 daemon 定时读取 Codex 订阅剩余用量。
- 通过 BLE 蓝牙把用量和提醒发送到 Waveshare ESP32-S3-Touch-AMOLED-2.16。
- 正常状态显示 5h/7d 剩余百分比、电量和重置倒计时。
- 正常状态底部用小蓝点显示当前有几个 Codex 任务正在运行。
- Codex 任务完成后触发红、黄、绿全屏闪动，然后显示“任务完成！”和任务摘要。
- 中间按键可切换屏幕关闭和亮屏，左/右按键可调节屏幕亮度。
- Mac 锁屏或 BLE 通信断开 5 分钟后可自动关屏；Mac 解锁或未锁屏时 BLE 恢复会自动亮屏。

## 项目结构

- `codexmeter/`：macOS 端核心代码，包括 Codex App Server 客户端、BLE 通信、daemon 和 payload 构造。
- `hooks/codexmeter_start_hook.py`：Codex `UserPromptSubmit` hook，用于标记任务开始。
- `hooks/codexmeter_stop_hook.py`：Codex `Stop` hook，用于标记任务结束并触发完成提醒。
- `firmware/`：ESP32 固件，使用 PlatformIO、Arduino、LVGL、Arduino_GFX、ArduinoJson 和 NimBLE。
- `scripts/`：安装 hook 的辅助脚本。
- `tools/capture_screenshot.py`：USB 串口截图采集工具，将 RGB565 帧缓冲转换为 PNG。
- `tools/read_device_logs.py`：USB 串口日志查询工具，用于读取 ESP32 环形日志。
- `install-mac.sh`：安装 macOS 端应用、LaunchAgent 和 Codex hooks。
- `flash-mac.sh`：编译并烧录 ESP32 固件。
- `screenshot.sh`：截图工具入口，默认自动寻找 USB 串口。
- `logs.sh`：ESP32 日志查询工具入口。
- `codex_limits_demo.py`：此前已验证过的 Codex 用量获取 demo，本项目以它作为额度读取逻辑的参考来源。
- `docs/architecture.md`：架构说明。
- `NOTICE.md`：第三方项目参考与许可说明。
- `CONTRIBUTING.md`：贡献指南。
- `SECURITY.md`：安全问题报告说明。
- `CHANGELOG.md`：版本更新日志。
- `.github/`：GitHub issue 模板、PR 模板和 Python 测试 workflow。

## 硬件

首版目标硬件与 Clawdmeter 保持一致：

- Waveshare ESP32-S3-Touch-AMOLED-2.16
- 480x480 AMOLED 屏幕
- AXP2101 电源管理芯片
- 通过 BLE 与 macOS 通信

按键：

- 左键：GPIO0，降低亮度。
- 中键：AXP2101 PKEY，切换亮屏/关屏。
- 右键：GPIO18，增加亮度。

板级引脚配置集中放在 `firmware/include/config.h`。如果硬件版本不同，优先修改这个文件。

## macOS 安装

在项目目录执行：

```bash
./install-mac.sh
```

如果 `codex` 不在默认 `PATH` 中，可以显式指定：

```bash
CODEX_BIN=/path/to/codex ./install-mac.sh
```

安装脚本会完成这些事情：

- 创建或复用 `.venv/` 虚拟环境。
- 安装 `codexmeterd` 和 `codexmeterctl`。
- 写入用户级 LaunchAgent：`com.user.codexmeter`。
- 安装 Codex `UserPromptSubmit` 和 `Stop` hook 到 `~/.codex/hooks.json`，安装前会备份已有文件。

常用命令：

以下命令默认在项目根目录执行；如果已经激活 `.venv`，也可以直接使用 `codexmeterctl`。

```bash
tail -F ~/.codexmeter/codexmeter.log ~/.codexmeter/codexmeter.out.log ~/.codexmeter/codexmeter.err.log
.venv/bin/codexmeterctl once
.venv/bin/codexmeterctl status
.venv/bin/codexmeterctl demo-alert "构建任务已完成"
.venv/bin/codexmeterctl demo-usage --h5 72 --d7 84
.venv/bin/codexmeterctl demo-activity --count 2
.venv/bin/codexmeterctl screen-on
.venv/bin/codexmeterctl screen-off
```

如果修改了 macOS 端代码，可以重启 daemon：

```bash
launchctl kickstart -k gui/$(id -u)/com.user.codexmeter
```

## Codex 用量读取

daemon 通过本地 Codex App Server 读取订阅余量：

1. 启动 `codex app-server --listen stdio://`
2. 发送 `initialize`
3. 发送 `initialized`
4. 读取 `account/read`
5. 读取 `account/rateLimits/read`

首版只处理 `codex` 限额桶：

- 300 分钟窗口映射为 `5h`
- 10080 分钟窗口映射为 `7d`

daemon 不读取、不打印 Codex 登录 token。

## 屏幕显示

正常状态：

- 顶部左侧显示 Codex 标识。
- 顶部中间显示“剩余用量”。
- 顶部右侧显示电量百分比和电池图标，电池填充色会根据电量变为绿色、黄色或红色。
- 第一张卡片显示 `5h 剩余` 百分比，以及 `HH:MM 后重置`。
- 第二张卡片显示 `7d 剩余` 百分比，以及 `xd 后重置`。
- 底部空白区域显示运行中 Codex 任务数：1 个任务显示 1 个小蓝点，2 个任务显示 2 个小蓝点；没有运行中任务时隐藏。
- 中间按键短按可切换 AMOLED 亮屏和关闭；屏幕关闭时 BLE、任务计数、日志和用量刷新仍会继续运行。
- 左/右按键短按分别降低/增加亮度；亮度范围为 10%-100%，每次调整 10%，调整后会显示 3 秒亮度进度条。
- Mac 锁屏持续 5 分钟后，daemon 会发送关屏控制；Mac 解锁后会立即发送亮屏控制。
- ESP32 与 Mac 的 BLE 通信断开持续 5 分钟后，固件会本地关屏；通信恢复时，只有 Mac 当前未锁屏才会由 daemon 发送亮屏控制。

任务完成提醒：

- 先红、黄、绿全屏闪动。
- 闪动结束后显示“任务完成！”。
- 下方显示 Codex 任务摘要，正文使用 28px 专用中文字体。
- 摘要会在 macOS 端做设备字库清洗，emoji 和少见字符会被移除，避免屏幕乱码或方框。
- 默认在文字出现后停留 8 秒；提醒显示时关屏会同时关闭当前提醒。

## 固件编译与烧录

项目优先使用 `.venv/bin/pio`，如果不存在再回退到系统 `pio`。PlatformIO 缓存默认放在项目内的 `.platformio/`。

如果没有安装 PlatformIO，可以先安装：

```bash
.venv/bin/python -m pip install platformio
```

连接设备后执行：

```bash
./flash-mac.sh waveshare_amoled_216
```

如果需要指定串口：

```bash
./flash-mac.sh waveshare_amoled_216 /dev/cu.usbmodem211201
```

只编译不烧录：

```bash
PLATFORMIO_CORE_DIR="$PWD/.platformio" \
UV_CACHE_DIR="$PWD/.platformio/.cache/uv" \
.venv/bin/pio run -d firmware -e waveshare_amoled_216
```

## 串口调试

固件支持通过串口输入 demo 命令：

```text
demo_usage
demo_alert
demo_activity
demo_idle
screen_on
screen_off
screen_toggle
brightness_down
brightness_up
brightness 80
screenshot
logs
logs 20
log_clear
```

也可以直接发送 BLE JSON payload 形状的消息：

```json
{"v":1,"k":"usage","src":"codex","h5":72,"h5r":1783093200,"d7":84,"d7r":1783545600,"st":"ok","t":1783070000}
```

```json
{"v":1,"k":"alert","id":"demo","title":"任务完成！","body":"Codex 已完成测试任务","t":1783070000}
```

```json
{"v":1,"k":"activity","src":"codex","run":2,"t":1783070000}
```

屏幕控制 payload：

```json
{"v":1,"k":"control","cmd":"screen","on":true,"why":"mac_unlocked","t":1783070000}
```

## USB 截图与视觉 QA

固件支持通过 USB 串口执行 `screenshot` 命令，返回当前 LVGL 屏幕的 RGB565 快照。项目根目录提供宿主侧工具，可直接保存 PNG：

```bash
./screenshot.sh out.png
```

如果机器上有多个串口，可以显式指定设备：

```bash
./screenshot.sh out.png /dev/cu.usbmodem211201
```

截图工具使用纯 Python 实现串口读取和 PNG 编码，不依赖 `ffmpeg` 或 `pyserial`。当前首版目标板带 PSRAM，可支持 480x480 全帧截图；未来若适配无 PSRAM 板，固件会返回 `SCREENSHOT_UNSUPPORTED`。

## ESP32 日志查询

固件会把关键运行事件写入一个 48 条的设备端环形日志，包括启动、PMU、BLE 连接/断开、payload 解析、用量更新、活动任务数、提醒和截图状态。日志仍会实时打印到 USB 串口，也可以按需查询：

```bash
./logs.sh
```

只看最近 20 条：

```bash
./logs.sh -n 20
```

指定串口：

```bash
./logs.sh -n 20 /dev/cu.usbmodem211201
```

清空设备端日志：

```bash
./logs.sh --clear
```

跟随实时串口日志：

```bash
./logs.sh --follow
```

查询输出格式为：

```text
LOGS_START 2
LOG 12 3401 INFO BLE connected xx:xx:xx:xx:xx:xx
LOG 13 3550 INFO usage h5=72 d7=64 status=ok
LOGS_END
```

## BLE 协议

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
{"v":1,"k":"alert","id":"abc","title":"任务完成！","body":"摘要","t":1783070000}
```

`task_complete` 生成的提醒 payload 会额外携带当前运行中任务数，固件会在显示提醒前同步更新底部小蓝点：

```json
{"v":1,"k":"alert","id":"abc","title":"任务完成！","body":"摘要","run":0,"t":1783070000}
```

运行中任务 payload：

```json
{"v":1,"k":"activity","src":"codex","run":2,"t":1783070000}
```

字段说明：

- `v`：协议版本。
- `k`：payload 类型，当前支持 `usage`、`alert`、`activity` 和 `control`。
- `src`：用量来源，首版为 `codex`，预留给未来其他订阅来源。
- `h5` / `d7`：剩余百分比。
- `h5r` / `d7r`：重置时间戳，单位为秒。
- `st`：状态，例如 `ok` 或 `stale`。
- `t`：payload 生成时间戳，固件用它计算重置倒计时。
- `title` / `body`：提醒标题和正文。
- `run`：当前正在运行的 Codex 任务数量。固件用它在底部居中显示小蓝点。
- `cmd` / `on` / `why`：控制命令、屏幕目标状态和触发原因；当前 `control` 只支持 `cmd:"screen"`。

payload 会控制在 512 bytes 以内。

## 测试

运行 Python 测试：

```bash
.venv/bin/python tests/run_tests.py
```

常用端到端验证：

```bash
.venv/bin/codexmeterctl status
.venv/bin/codexmeterctl demo-usage --h5 72 --d7 84
.venv/bin/codexmeterctl demo-alert "任务完成提醒测试"
.venv/bin/codexmeterctl demo-activity --count 3
.venv/bin/codexmeterctl screen-off --reason test
.venv/bin/codexmeterctl screen-on --reason test
```

## 故障排查

查看 daemon 日志：

```bash
tail -F ~/.codexmeter/codexmeter.log ~/.codexmeter/codexmeter.out.log ~/.codexmeter/codexmeter.err.log
```

确认 daemon 是否运行：

```bash
.venv/bin/codexmeterctl status
```

重启 daemon：

```bash
launchctl kickstart -k gui/$(id -u)/com.user.codexmeter
```

如果设备刚烧录完成，BLE 会短暂断开，daemon 通常会自动扫描并重连。

如果任务完成提醒没有出现，优先检查：

- `~/.codex/hooks.json` 中是否存在 CodexMeter `UserPromptSubmit` 和 `Stop` hook。
- `~/.codexmeter/codexmeter.log` 中是否出现 `Queued local alert`。

## 参考与许可

CodexMeter 参考了 [HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) 的硬件方向和 BLE JSON 通信思路，并沿用兼容的自定义 GATT UUID 形状。

当前实现没有直接引入大量 Clawdmeter 源码。若未来复制上游源码文件，应在对应文件旁保留原始版权与许可说明。

当前仓库尚未指定开源许可证。若需要公开开源发布，请先补充明确的 `LICENSE` 文件。
