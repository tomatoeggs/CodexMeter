# CodexMeter 游戏世界主题提案 V1

![CodexMeter 红白机、动森与旷野之息主题预览](assets/themes-games.svg)

四套主题都会重新组织信息架构和控件形态，不是在同一布局上替换颜色。当前 `Famicom`、`Animal Crossing` 与 `Gundam` 已完成 ESP32 端实现和真机视觉验收；旷野之息仍处于设计阶段。

## 01 / 任天堂红白机·硬件面板

- 核心隐喻：整个屏幕是一台正在运行的复古主机，数据区是嵌入式显示窗，底部是手柄操作面板。
- 色彩：老化象牙白外壳、酒红面板、黑褐数据屏和少量金色铭牌。
- 信息结构：今日 Token 和 7 天总量位于主显示窗；剩余额度装入“卡带标签”；重置时间放入 SELECT / START 胶囊键。
- 状态表达：顶部 `BATTERY 87%` 与红色电源灯结合；任务数与十字键、A/B 键形成一条手柄状态轨。
- 进度精度：今日 / 7 天占比使用 18 格点亮 5 格，对应 27.8%；剩余额度使用 12 格点亮 10 格，对应 83.3%。
- 完成动效：POWER 灯闪烁 → 数据窗像素扫光 → A/B 键依次弹起 → `STAGE CLEAR`。

记忆点：不需要出现游戏角色，只看红白机外壳、嵌入屏与手柄几何就能识别主题。

实现状态：已注册为设备端主题 `famicom`，沿用统一 `DashboardViewModel`，不修改 macOS 程序或 BLE 协议。当前仪表盘已实现 POWER 呼吸灯和任务运行时的 A/B 键轻微交替呼吸；完整任务完成动效仍使用系统默认场景，等待 Completion 主题接口启用。

| 设计稿裁切 | 480×480 真机截图 |
| --- | --- |
| ![Famicom 设计稿](verification/famicom-design-reference.png) | ![Famicom 真机主题](verification/famicom-theme-final.png) |

边界截图：[100% 剩余额度](verification/famicom-theme-100-percent.png) · [6 个运行任务](verification/famicom-theme-six-tasks.png)

## 02 / 动森·岛屿日报

- 核心隐喻：数据不再进入常规卡片，而是成为一座正在营业的小岛营地。
- 色彩：天空蓝、奶油白、薄荷绿、河水青、木质棕和少量珊瑚红。
- 信息结构：今日 Token 进入黄色帐篷；7 天总量是右侧悬挂便笺；剩余额度位于叶片形岛屿徽章；重置时间放入低矮的木制箭头路牌。
- 场景化：顶部悬挂 `CODEX ISLAND` 木牌，电量藏进右上角叶片；狸克位于路牌右侧，底部游客卡片承载任务数、BLE 与同步状态。
- 动态表达：额度叶片使用 10 枚小叶进度格；底部任务区域直接显示空闲状态或运行任务数，保持透明且不遮挡游客卡片。
- 双模式：Token 模式显示 `TODAY TOKEN` / `7 DAYS`；配额模式在相同场景内切换为 `5H REMAINS` / `7D REMAINS`。

记忆点：整个仪表盘像小岛上的一幅早报，每条数据都是场景中的物件。

实现状态：已注册为设备端主题 `animal_crossing`，继续读取统一 `DashboardViewModel`，不修改 macOS 程序或 BLE 协议。复杂静态场景使用无字 RGB565 背景保留插画质感，所有业务数字、模式标题、进度、电量和状态均由 LVGL 透明实时绘制。

当前 Waveshare AMOLED 实屏会放大高亮黄、草绿和湖水青的视觉饱和度，因此运行时背景采用从原始无字图确定性生成的 70% 饱和度、94% 对比度版本，动态绿色、珊瑚色、木牌棕和状态金色同步校准。未经校色的原始图保存在 [`animal-crossing-theme-clean-original.png`](assets/animal-crossing-theme-clean-original.png)，正式运行时源图保存在 [`animal-crossing-theme-clean-display-70.png`](assets/animal-crossing-theme-clean-display-70.png)，方便后续针对其他屏幕型号重新生成而不累积色彩损失。

| 最终设计稿 | 480×480 真机截图 |
| --- | --- |
| ![Animal Crossing 最终设计稿](assets/animal-crossing-theme-final.png) | ![Animal Crossing 70% 饱和度真机主题](verification/animal-crossing-theme-display-70.png) |

视觉对比：[原图 / 70% 校色图](verification/animal-crossing-clean-original-vs-display-70.png) · [设计稿 / 真机并排图](verification/animal-crossing-design-vs-device-final.png) · [100% + 6 个任务](verification/animal-crossing-edge-100-6tasks.png) · [5h/7d 配额模式](verification/animal-crossing-quota-mode.png)

## 03 / 旷野之息·古代石板

- 核心隐喻：屏幕是一块被唤醒的古代终端，数据从石板雕刻中发光。
- 色彩：深青黑石板、发光青色符文、灰绿石刻边框和少量球珀橙定位点。
- 信息结构：今日 Token 进入中央符文圆盘；7 天总量是右侧竖向石板；剩余额度通过底部菱形符文轨表达。
- 空间细节：等高线纹理、断续圆轨、四向球珀橙定位针和切角石刻边框构成古代仪器感。
- 完成动效：四枚定位针依次点亮 → 符文圆盘旋转一周 → 青色光扫过石板 → 橙色节点锁定任务完成。

记忆点：它不是一张蓝色 HUD，而是一块有厚度、雕刻与发光机构的古代终端。

## 04 / 元祖高达·白色基地机体诊断

- 核心隐喻：整块屏幕是一台白色基地机库里的 RX-78-2 状态终端，而不是由装甲色块拼出的普通卡片页。
- 色彩：深海军蓝终端底、暖白遥测文字、RX-78-2 的白蓝红黄机体色，以及绿色眼部传感器和任务状态灯。
- 信息结构：今日和 7 天 Token 位于左侧遥测区；RX-78-2 半身占据右侧主视觉；7d 剩余额度、重置窗口、任务灯和同步状态进入底部三段式控制台。
- 双模式：Token 模式显示 `TODAY TOKEN` / `7 DAYS`；配额模式在相同遥测区切换为 `5H REMAINS` / `7D REMAINS`。
- 动态表达：剩余额度使用 10 段能源条；最多 7 个任务以机体状态灯显示，超出时保持 7 灯全亮并由文案显示真实计数。

记忆点：一眼先看到被监控的 RX-78-2，再读到左侧遥测和下方控制台，视觉关系明确，不依赖随机机甲装饰辨认主题。

实现状态：已注册为设备端主题 `gundam`，沿用统一 `DashboardViewModel`，不修改 macOS 程序或 BLE 协议。高达主体、白色基地身份和终端纹理由无字 RGB565 背景承载；模式标题、业务数字、额度条、电池、任务灯和同步状态全部由 LVGL 实时绘制。真机校准覆盖长短 Token 数值、`100%` 剩余额度、0–7 个任务、电池与底部状态栏，并统一左侧遥测模块的视觉基线。

| 最终设计稿 | 480×480 真机截图 |
| --- | --- |
| ![Gundam 主题设计稿](assets/gundam-theme-v2.png) | ![Gundam 真机主题](verification/gundam-theme-final.png) |

## 四套主题共同保留的信息

- 今日 Token：`96.2M`
- 近 7 天 Token：`347M`
- 7d 剩余额度：`83%`
- 重置时间：`06D 14H`
- 电量：`87%`
- 运行中任务：`2`

## 后续主题的建议确认顺序

1. 先确认旷野之息的世界观和信息布局。
2. 再确认数字字体、中英文标签和辅助图形。
3. 最后确认动效与真机屏幕上的亮度约束，确认完成后再进入代码实现。
