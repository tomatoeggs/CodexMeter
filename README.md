# CodexMeter

[![Python Tests](https://github.com/tomatoeggs/CodexMeter/actions/workflows/python-tests.yml/badge.svg)](https://github.com/tomatoeggs/CodexMeter/actions/workflows/python-tests.yml)
[![Latest tag](https://img.shields.io/github/v/tag/tomatoeggs/CodexMeter?sort=semver)](https://github.com/tomatoeggs/CodexMeter/tags)
[![License: MIT](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-macOS%20%2B%20ESP32-24292f)](#兼容性与前置条件)

CodexMeter 是一个基于 ESP32 AMOLED 屏幕的 Codex 订阅余量与任务完成提醒显示器。macOS 后台服务读取本机 Codex 用量，通过 BLE 同步到一台或多台桌面设备。

> A compact macOS + ESP32 display for Codex usage limits and task-completion alerts.

当前稳定版本为 [`v2.0.1`](CHANGELOG.md)。

| 用量主页 | 任务完成提醒 |
| --- | --- |
| ![CodexMeter 用量主页](docs/assets/dashboard.png) | ![CodexMeter 四行任务完成摘要](docs/assets/task-complete.png) |

## 主要功能

- macOS 后台 daemon 定时读取 Codex 订阅剩余用量。
- 通过 BLE 蓝牙把用量和提醒发送到 Waveshare ESP32-S3-Touch-AMOLED-2.16。
- 支持一台 Mac 同时驱动多台已登记的 CodexMeter；设备按稳定短 ID 自动发现和重连。
- 正常状态显示 5h/7d 剩余百分比、电量和重置倒计时。
- 正常状态底部用小蓝点显示当前有几个 Codex 任务正在运行。
- Codex 任务完成后触发红、黄、绿全屏闪动，然后显示“任务完成！”和任务摘要。
- 中间按键可切换屏幕关闭和亮屏，左/右按键可调节屏幕亮度。
- Mac 锁屏或 BLE 通信断开 5 分钟后可自动关屏；Mac 解锁或未锁屏时 BLE 恢复会自动亮屏。
- 正常页会定时在 2px 范围内轻微漂移，降低 AMOLED 固定元素长期停留在同一批像素上的风险。
- 通过板载 QMI8658 IMU 感知重力方向，自动旋转屏幕显示方向。

## 兼容性与前置条件

- 一台运行 macOS 的 Mac；暂不支持多台 Mac 共同驱动同一设备。
- Python 3.11 或更高版本。
- 已安装并登录的 [Codex CLI](https://developers.openai.com/codex/cli/)。
- 已开启蓝牙，并允许启动服务访问蓝牙。
- 一台或多台 Waveshare ESP32-S3-Touch-AMOLED-2.16。
- 编译固件时需要 PlatformIO；`install-mac.sh` 会安装宿主端 Python 依赖，但不会安装 PlatformIO。

## 快速开始

克隆项目并安装 macOS 后台服务与 Codex hooks：

```bash
git clone https://github.com/tomatoeggs/CodexMeter.git
cd CodexMeter
./install-mac.sh
```

首次给开发板烧录 CodexMeter 固件时，确认串口无误后执行：

```bash
.venv/bin/python -m pip install platformio
./flash-mac.sh waveshare_amoled_216 /dev/cu.usbmodemXXXX --force
```

扫描并登记设备；将示例短 ID 替换为扫描结果：

```bash
.venv/bin/codexmeterctl devices scan
.venv/bin/codexmeterctl devices adopt A3F91C --alias Desk
launchctl kickstart -k gui/$(id -u)/com.user.codexmeter
.venv/bin/codexmeterctl status
```

`--force` 只用于尚未运行 CodexMeter 固件、因而无法通过 identity 校验的开发板，或明确的 bootloader 恢复场景。后续升级应优先使用 `./flash-mac.sh --all`。

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
- `LICENSE`：项目使用的 MIT License。
- `NOTICE.md`：第三方项目参考与许可说明。
- `CONTRIBUTING.md`：贡献指南。
- `SECURITY.md`：安全问题报告说明。
- `CHANGELOG.md`：版本更新日志。
- `.github/`：GitHub issue 模板、PR 模板和 Python 测试 workflow。

## 硬件

当前支持的目标硬件与 Clawdmeter 保持一致：

- Waveshare ESP32-S3-Touch-AMOLED-2.16
- 480x480 AMOLED 屏幕
- AXP2101 电源管理芯片
- QMI8658 六轴 IMU，用于屏幕方向自适应
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
.venv/bin/codexmeterctl devices scan
.venv/bin/codexmeterctl devices adopt A3F91C --alias Home
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

当前只处理 `codex` 限额桶：

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
- 正常页每 10 分钟在 2px 范围内整体轻微漂移；任务完成提醒和亮度浮层不参与漂移。
- 设备旋转时，固件会读取 QMI8658 加速度计并在 0/90/180/270 度之间自动切换显示方向；切换时会短暂压暗屏幕并重绘，减少半帧撕裂感。

## 多设备

每台固件会用 ESP32 稳定芯片 ID 生成身份，并以短名广播：

```text
CodexMeter-A3F91C
```

Mac 端只会自动连接已登记设备。首次使用一台新设备时：

```bash
.venv/bin/codexmeterctl devices scan
.venv/bin/codexmeterctl devices adopt A3F91C --alias Home
launchctl kickstart -k gui/$(id -u)/com.user.codexmeter
```

`alias` 只是本机显示标签，不参与连接判断。daemon 运行后会持续扫描附近 CodexMeter；当你带 Mac 从公司回家，办公室设备会离线，家里的已登记设备被扫描到后会由独立 worker 连接。每台设备有独立队列、ACK、重连 backoff 和健康状态；单台设备假活或断电不会阻塞其他设备。首次连接后，daemon 会读取 identity characteristic，把短 ID 登记记录原子升级为完整芯片 ID；后续连接必须通过完整 identity 校验。

状态类 payload 会保存每台设备的最新期望值并在重连后恢复。告警在收到设备 ACK 前保持为 in-flight；ACK 丢失时会重试，固件按告警 ID 去重，因此不会因为瞬时断连而漏提醒或重复闪屏。

查看登记设备：

```bash
.venv/bin/codexmeterctl devices list
```

任务完成提醒：

- 先红、黄、绿全屏闪动。
- 闪动结束后显示“任务完成！”。
- 下方最多显示四行 Codex 任务摘要，超出部分在第四行省略；正文使用 LittleFS 中的 TTF 字体经 LVGL TinyTTF 本地渲染。
- 摘要在 macOS 端只移除控制字符并保留 Unicode 文本，中文覆盖主要由设备端 TTF 字体负责。
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

同时烧录所有已通过 identity 校验的 CodexMeter：

```bash
./flash-mac.sh --all
```

自动发现和 `--all` 不会选择其他 USB 串口。首次给尚未运行 CodexMeter 固件的开发板烧录，或设备停在 bootloader、无法回应 identity 查询的恢复场景，才应在确认端口后显式追加 `--force`：

```bash
./flash-mac.sh waveshare_amoled_216 /dev/cu.usbmodem211201 --force
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
identity
screen_on
screen_off
screen_toggle
brightness_down
brightness_up
brightness 80
imu
rotate auto
rotate 0
rotate 90
rotate 180
rotate 270
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

固件支持通过 USB 串口执行 `screenshot` 命令，返回当前屏幕物理方向的 RGB565 快照。项目根目录提供宿主侧工具，可直接保存 PNG：

```bash
./screenshot.sh out.png
```

如果机器上有多个串口，可以显式指定设备：

```bash
./screenshot.sh out.png /dev/cu.usbmodem211201
./screenshot.sh out.png --device A3F91C
./screenshot.sh --list
```

截图工具使用纯 Python 实现串口读取和 PNG 编码，不依赖 `ffmpeg` 或 `pyserial`。当前目标板带 PSRAM，可支持 480x480 全帧截图；未来若适配无 PSRAM 板，固件会返回 `SCREENSHOT_UNSUPPORTED`。当 USB 同时接入多台 CodexMeter 时，未指定 `--device` 或 port 会报错并要求明确目标，避免误操作。

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
./logs.sh -n 20 --device A3F91C
./logs.sh --list
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

macOS daemon 日志使用轮转文件，当前文件和一个备份合计最多约 100MB。

## BLE 协议

设备名：

```text
CodexMeter-<short_id>
```

GATT UUID：

- service：`4c41555a-4465-7669-6365-000000000001`
- RX write：`4c41555a-4465-7669-6365-000000000002`
- TX notify：`4c41555a-4465-7669-6365-000000000003`
- refresh notify：`4c41555a-4465-7669-6365-000000000004`
- identity read：`4c41555a-4465-7669-6365-000000000005`

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
- `src`：用量来源，当前为 `codex`，预留给未来其他订阅来源。
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
.venv/bin/codexmeterctl devices scan
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

如果 CoreBluetooth 连接处于假活状态，daemon 会在 BLE notify 订阅、BLE 写入或设备 ACK 超时后主动断开本次会话，并重新扫描连接。默认写入和 ACK 超时都是 10 秒，notify 订阅超时为 5 秒，应用层 heartbeat 间隔为 45 秒，可通过 `codexmeterd --ble-write-timeout`、`--ble-ack-timeout`、`--ble-notify-timeout` 和 `--ble-healthcheck-interval` 调整。`codexmeterctl status` 会额外显示 BLE 连接状态、队列深度、最近写入/ACK 时间和连续失败次数。

如果 7d 用量短暂跳到 `100%` 且显示 `7d 后重置`，通常是 Codex App Server 返回了临时空窗口。daemon 会过滤这类单次异常快照，并在日志中记录 `Rejected transient usage sample`、原始 `h5/d7/h5r/d7r` 和最终排队的可信值。

如果任务完成提醒没有出现，优先检查：

- `~/.codex/hooks.json` 中是否存在 CodexMeter `UserPromptSubmit` 和 `Stop` hook。
- `~/.codexmeter/codexmeter.log` 中是否出现 `Queued local alert`。

## 参考与许可

CodexMeter 参考了 [HermannBjorgvin/Clawdmeter](https://github.com/HermannBjorgvin/Clawdmeter) 的硬件方向和 BLE JSON 通信思路，并沿用兼容的自定义 GATT UUID 形状。

当前实现没有直接引入大量 Clawdmeter 源码。若未来复制上游源码文件，应在对应文件旁保留原始版权与许可说明。

CodexMeter 使用 [MIT License](LICENSE) 开源。第三方参考与许可说明见 [NOTICE.md](NOTICE.md)，参与开发前请阅读 [CONTRIBUTING.md](CONTRIBUTING.md)，安全问题请按 [SECURITY.md](SECURITY.md) 私密报告。
