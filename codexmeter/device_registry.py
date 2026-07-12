"""Persistent CodexMeter device registry."""

from __future__ import annotations

import json
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from .settings import DEVICE_NAME, DEVICES_FILE

DEVICE_NAME_PREFIX = f"{DEVICE_NAME}-"
SHORT_ID_RE = re.compile(rf"^{re.escape(DEVICE_NAME_PREFIX)}([0-9A-Fa-f]{{4,16}})$")


@dataclass(frozen=True)
class DeviceConfig:
    device_id: str
    short_id: str | None = None
    alias: str | None = None
    enabled: bool = True
    name: str | None = None
    macos_uuid: str | None = None
    legacy: bool = False

    @property
    def label(self) -> str:
        return self.alias or self.short_id or self.name or self.device_id

    def matches(
        self,
        *,
        device_id: str | None = None,
        short_id: str | None = None,
        name: str | None = None,
        address: str | None = None,
    ) -> bool:
        if device_id and normalize_device_id(device_id) == normalize_device_id(self.device_id):
            return True
        if short_id and self.short_id and normalize_short_id(short_id) == self.short_id:
            return True
        if address and self.macos_uuid and address == self.macos_uuid:
            return True
        if name and self.name and name == self.name:
            return True
        if self.legacy and name == (self.name or DEVICE_NAME):
            return True
        return False

    def to_json(self) -> dict[str, Any]:
        data: dict[str, Any] = {
            "device_id": self.device_id,
            "enabled": self.enabled,
        }
        if self.short_id:
            data["short_id"] = self.short_id
        if self.alias:
            data["alias"] = self.alias
        if self.name:
            data["name"] = self.name
        if self.macos_uuid:
            data["macos_uuid"] = self.macos_uuid
        if self.legacy:
            data["legacy"] = True
        return data

    @classmethod
    def from_json(cls, data: dict[str, Any]) -> "DeviceConfig":
        device_id = str(data.get("device_id") or "").strip()
        if not device_id:
            raise ValueError("device config requires device_id")
        short_id_value = data.get("short_id")
        short_id = (
            normalize_short_id(str(short_id_value))
            if isinstance(short_id_value, str) and short_id_value.strip()
            else None
        )
        return cls(
            device_id=normalize_device_id(device_id),
            short_id=short_id,
            alias=_optional_str(data.get("alias")),
            enabled=bool(data.get("enabled", True)),
            name=_optional_str(data.get("name")),
            macos_uuid=_optional_str(data.get("macos_uuid")),
            legacy=bool(data.get("legacy", False)),
        )


class DeviceRegistry:
    def __init__(self, path: Path = DEVICES_FILE) -> None:
        self.path = path
        self.devices: list[DeviceConfig] = []

    @classmethod
    def load(cls, path: Path = DEVICES_FILE) -> "DeviceRegistry":
        registry = cls(path)
        try:
            with path.open(encoding="utf-8") as handle:
                raw = json.load(handle)
        except FileNotFoundError:
            return registry
        except (OSError, json.JSONDecodeError) as exc:
            raise ValueError(f"failed to load device registry {path}: {exc}") from exc
        if not isinstance(raw, dict):
            raise ValueError("device registry must be a JSON object")
        devices = raw.get("devices", [])
        if not isinstance(devices, list):
            raise ValueError("device registry devices must be a list")
        registry.devices = [
            DeviceConfig.from_json(item) for item in devices if isinstance(item, dict)
        ]
        return registry

    def save(self) -> None:
        self.path.parent.mkdir(parents=True, exist_ok=True)
        data = {
            "version": 1,
            "devices": [device.to_json() for device in self.devices],
        }
        with self.path.open("w", encoding="utf-8") as handle:
            json.dump(data, handle, ensure_ascii=False, indent=2)
            handle.write("\n")

    def enabled_devices(self) -> list[DeviceConfig]:
        return [device for device in self.devices if device.enabled]

    def find(self, value: str) -> DeviceConfig | None:
        norm_device = normalize_device_id(value)
        norm_short = normalize_short_id(value)
        for device in self.devices:
            if normalize_device_id(device.device_id) == norm_device:
                return device
            if device.short_id and device.short_id == norm_short:
                return device
            if device.alias and device.alias == value:
                return device
            if device.name and device.name == value:
                return device
        return None

    def upsert(self, device: DeviceConfig) -> None:
        for index, existing in enumerate(self.devices):
            if existing.matches(device_id=device.device_id, short_id=device.short_id):
                self.devices[index] = device
                return
        self.devices.append(device)


def default_legacy_device(name: str = DEVICE_NAME) -> DeviceConfig:
    return DeviceConfig(
        device_id="legacy-codexmeter",
        alias="Legacy" if name == DEVICE_NAME else name,
        name=name,
        legacy=True,
    )


def parse_short_id_from_name(name: str | None) -> str | None:
    if not name:
        return None
    match = SHORT_ID_RE.match(name)
    if match:
        return normalize_short_id(match.group(1))
    return None


def name_from_short_id(short_id: str) -> str:
    return f"{DEVICE_NAME_PREFIX}{normalize_short_id(short_id)}"


def normalize_short_id(value: str) -> str:
    text = value.strip()
    if text.startswith(DEVICE_NAME_PREFIX):
        text = text[len(DEVICE_NAME_PREFIX) :]
    return re.sub(r"[^0-9A-Fa-f]", "", text).upper()


def normalize_device_id(value: str) -> str:
    return value.strip().lower()


def config_from_short_id(short_id: str, *, alias: str | None = None) -> DeviceConfig:
    normalized = normalize_short_id(short_id)
    if not normalized:
        raise ValueError("short id cannot be empty")
    return DeviceConfig(
        device_id=f"codexmeter-{normalized.lower()}",
        short_id=normalized,
        alias=alias,
        name=name_from_short_id(normalized),
    )


def _optional_str(value: object) -> str | None:
    if isinstance(value, str) and value.strip():
        return value.strip()
    return None
