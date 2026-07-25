# CodexMeter WALL-E 系列主题

CodexMeter 内置两套彼此独立的 WALL-E 仪表盘：原有的黄黑工程终端
`WALL-E`，以及以地球复育和夕阳废土为核心视觉的 `WALL-E EARTH`。
两者拥有不同对象树和资源，但都只读取统一 `DashboardViewModel`；
macOS 程序和 BLE 协议没有主题专用字段。

## WALL-E EARTH

![WALL-E EARTH 最终设计稿](assets/walle-theme-v10.png)

`WALL-E EARTH` 是第七套设备端主题，内部稳定 ID 为 `walle_v10`。
480×480 界面保留夕阳城市剪影、幼苗靴、WALL-E 主体和机械面板纹理：
左右屏幕显示今日与近 7 天 Token，十枚叶片表达 7d 剩余额度，磁带区域
承载 RESET 倒计时，履带状态栏显示任务数、BLE 和同步状态。

### 实现与资源

- `firmware/src/walle_v10_theme.*` 实现主题生命周期、动态标签、叶片更新和
  LittleFS / PSRAM 资源管理。
- `walle_v10_bg.rgb565` 是移除动态文字后的静态场景；设计稿和干净运行时
  PNG 分别保存在 `assets/walle-theme-v10.png` 与
  `assets/walle-theme-v10-clean.png`。
- `walle_v10_leaves_active.rgb565`、
  `walle_v10_leaves_inactive.rgb565` 与 `walle_v10_leaves_mask.bin`
  保存十枚叶子的亮态、灰态和像素掩码。
- Jersey 10 绘制像素显示器文字；字体按 SIL Open Font License 1.1
  随固件资源分发。

运行时资源可从最终设计参考确定性生成：

```bash
python3 tools/generate_walle_v10_theme_assets.py
```

生成器会移动顶部标题和 RESET 静态标签，清理所有运行时数值与任务数，
提取十枚叶片并统一各叶槽的亮暗色阶，然后输出 RGB565 小端资源。

### 动态与像素校准

- Token 模式显示 `TODAY TOKEN` / `7 DAYS`；配额模式显示
  `5H REMAINS` / `7D REMAINS`。
- 叶片按剩余百分比向下取整：90%–99% 为九亮一灰，100% 才十枚全亮。
- 7 DAYS 数值和单位组成一个整体，以 `x=399` 为中心；不同位数自动收缩，
  数值与单位保持共同可见底边。
- RESET 的天 / 小时数值分别与 `D` / `H` 对齐；底部任务数与静态
  `ACTIVE TASKS` 文本分层渲染。
- 顶部标题、电池、额度百分比和状态栏均经过 480×480 真机截图校准。

| 最终设计稿 | 480×480 真机截图 |
| --- | --- |
| ![WALL-E EARTH 最终设计稿](assets/walle-theme-v10.png) | ![WALL-E EARTH 真机主题](verification/walle-v10-secondary-center-x399.png) |

## WALL-E 工程终端

![WALL-E 工程终端最终设计稿](assets/walle-theme-v9.png)

原有 `WALL-E` 主题内部 ID 为 `walle`。整个界面被组织为一台黄黑工程
机器人仪表：双目镜头和电池位于顶部，警示带划分信息层级，Token 数据进入
左右遥测区，额度由九片幼苗叶表达，RESET 使用磁带计时器，底部履带区域
承载任务、BLE 与同步状态。

- `firmware/src/walle_theme.*` 实现独立 LVGL 对象树与主题生命周期。
- `walle_bg.rgb565` 保存无动态文字的机械背景；
  `walle_leaves_active.rgb565` 与 `walle_leaves_mask.bin` 保存九片叶轮廓。
- Teko SemiBold 负责数值与单位，D-DIN Condensed Bold 负责仪表标签。

旧主题资源可通过以下命令重新生成：

```bash
python3 tools/generate_walle_theme_assets.py
```

| 最终设计稿 | 480×480 真机截图 |
| --- | --- |
| ![WALL-E 工程终端设计稿](assets/walle-theme-v9.png) | ![WALL-E 工程终端真机主题](verification/walle-theme-final.png) |

边界截图：[近 7 天用量 1.14B](verification/walle-theme-1.14b.png)
