"""BLE payload builders."""

from __future__ import annotations

import json
import re
import time
import uuid
from dataclasses import dataclass
from typing import Any

from .limits import UsageSnapshot


MAX_BLE_PAYLOAD_BYTES = 512
MAX_ALERT_BODY_BYTES = 210
DEVICE_TEXT_CHARS = (
    "剩余用量重置任务完成测试提醒等待已一个月日摘要内容构建运行编译烧录失败成功结束"
    "项目修复检查通过更新刷新连接断开错误正常通知兜底链路乱码信息详情调整固件显示"
    "界面功能生效请看查看文件目录代码函数模块配置脚本依赖安装服务后台蓝牙设备屏幕"
    "字体样式布局页面组件按钮日志单元异常优化新增删除重启启动停止读取写入解析发送"
    "接收队列事件状态结果验证问题原因方案实现继续准备可以无法需要注意默认支持保持"
    "返回订阅限额百分比电量电池时间日期北京时间小时分钟秒红黄绿颜色闪动全屏动画"
    "提示当前本地在线离线收到恢复告警标题正文窗口面板顶部底部左侧右侧居中对齐间距"
    "边距宽度高度大小蓝色绿色黄色红色白色黑色灰色靠左靠右百分数没有部分整体后先"
    "第一行第二行中文去掉增加放到只存在偏小方框重叠不漂亮很多细节换成图标根据进行"
    "填充上下过大总结修改保存生成计划调试定位处理好发现检测部署应用守护进程未找到"
    "扫描配对断线重连账户窗口空值主要次要源码参考架构职责未来适配模型软件简洁优雅"
    "单一职责让我们一起硬件软件的了一是在和有为到与把对中后前上下面里这里那里这个"
    "那个这些那些目前现在刚才会将能不能未被从按等或及如果因为所以但是并且然后直接"
    "关于使用选择确认我你他她它们Codex，。！？：；、“”‘’（）【】《》…·"
)
_DEVICE_ALLOWED_CHARS = set(map(chr, range(0x20, 0x7F))) | set(DEVICE_TEXT_CHARS)


@dataclass(frozen=True)
class Payload:
    kind: str
    data: dict[str, Any]

    def to_json_bytes(self) -> bytes:
        return encode_payload(self.data)


def build_usage_payload(snapshot: UsageSnapshot) -> Payload:
    data = {
        "v": 1,
        "k": "usage",
        "src": snapshot.source,
        "h5": snapshot.h5_remaining_percent,
        "h5r": snapshot.h5_resets_at,
        "d7": snapshot.d7_remaining_percent,
        "d7r": snapshot.d7_resets_at,
        "st": snapshot.status,
        "t": snapshot.generated_at,
    }
    return Payload("usage", data)


def build_alert_payload(
    body: str,
    title: str = "任务完成！",
    event_id: str | None = None,
    now: int | None = None,
) -> Payload:
    title_text = sanitize_device_text(clean_summary(title), fallback="任务完成！")
    body_text = truncate_utf8(
        sanitize_device_text(clean_summary(body)),
        MAX_ALERT_BODY_BYTES,
    )
    data = {
        "v": 1,
        "k": "alert",
        "id": event_id or uuid.uuid4().hex[:12],
        "title": title_text,
        "body": body_text,
        "t": int(now if now is not None else time.time()),
    }
    return Payload("alert", data)


def encode_payload(data: dict[str, Any]) -> bytes:
    payload = json.dumps(data, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(payload) <= MAX_BLE_PAYLOAD_BYTES:
        return payload
    if data.get("k") != "alert":
        raise ValueError(f"BLE payload too large: {len(payload)} bytes")

    trimmed = dict(data)
    body = str(trimmed.get("body") or "")
    while body and len(payload) > MAX_BLE_PAYLOAD_BYTES:
        body = truncate_utf8(body, max(0, len(body.encode("utf-8")) - 24))
        trimmed["body"] = body
        payload = json.dumps(trimmed, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(payload) > MAX_BLE_PAYLOAD_BYTES:
        raise ValueError(f"BLE alert payload too large: {len(payload)} bytes")
    return payload


def clean_summary(text: str) -> str:
    compact = " ".join(line.strip() for line in text.splitlines() if line.strip())
    return compact or "Codex 任务已完成"


def sanitize_device_text(text: str, fallback: str = "Codex 任务已完成") -> str:
    """Keep text within the glyph set flashed to the ESP32 display."""

    cleaned = "".join(ch if ch in _DEVICE_ALLOWED_CHARS else " " for ch in text)
    compact = re.sub(r"\s+", " ", cleaned).strip()
    return compact or fallback


def truncate_utf8(text: str, max_bytes: int) -> str:
    raw = text.encode("utf-8")
    if len(raw) <= max_bytes:
        return text
    if max_bytes <= 1:
        return ""
    suffix = "..."
    keep = max_bytes - len(suffix)
    trimmed = raw[:keep]
    while trimmed:
        try:
            return trimmed.decode("utf-8") + suffix
        except UnicodeDecodeError:
            trimmed = trimmed[:-1]
    return suffix[:max_bytes]
