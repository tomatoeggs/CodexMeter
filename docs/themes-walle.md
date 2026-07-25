# CodexMeter WALL-E 主题

![WALL-E V9 最终设计稿](assets/walle-theme-v9.png)

`WALL-E` 是 CodexMeter 的第六套设备端仪表盘主题。整个 480×480
界面被组织为一台黄黑工程机器人仪表，而不是在通用卡片布局上替换颜色：
双目镜头和电池位于顶部，警示带划分信息层级，Token 数据进入左右遥测区，
额度由九片幼苗叶表达，RESET 使用磁带计时器，底部履带区域承载任务、
BLE 与同步状态。

## 实现结构

- `firmware/src/walle_theme.*` 实现独立 LVGL 对象树与主题生命周期。
- `firmware/data/themes/walle_bg.rgb565` 保存无动态文字的机械背景。
- `walle_leaves_active.rgb565` 与 `walle_leaves_mask.bin` 保留设计稿中的
  九片叶轮廓，并按实时 7d 剩余额度动态点亮。
- Teko SemiBold 负责数值与单位，D-DIN Condensed Bold 负责仪表标签和
  状态文字，与 Gundam 主题共享已有字体资源。
- 所有业务数据继续来自统一 `DashboardViewModel`；macOS 程序和 BLE
  协议没有为主题增加专用字段。

运行时背景可由最终设计稿确定性重新生成：

```bash
python3 tools/generate_walle_theme_assets.py
```

生成过程只清理动态数字、模式标题、任务数与状态灯，保留眼睛、渐变、
警示带、额度拱形、磁带、履带、幼苗和静态标签的原始像素。电池内部采用
逐行渐变重建，避免文字修补污染边框。

## 动态与边界

- Token 模式显示 `TODAY TOKEN` / `7 DAYS`；配额模式切换为
  `5H REMAINS` / `7D REMAINS`。
- 7d 剩余额度支持 `0%`–`100%`，九片叶按百分比点亮。
- 近 7 天 Token 达到 B/T 量级后固定保留两位小数，例如 `1.14B`。
- RESET 将天数与小时拆为独立数值和单位，保持共同底线。
- 底部任务数、BLE、SYNC 与两颗状态灯经过真机像素中心线校准。

| 最终设计稿 | 480×480 真机截图 |
| --- | --- |
| ![WALL-E 最终设计稿](assets/walle-theme-v9.png) | ![WALL-E 真机主题](verification/walle-theme-final.png) |

边界截图：[近 7 天用量 1.14B](verification/walle-theme-1.14b.png)
