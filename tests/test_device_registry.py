from codexmeter.device_registry import (
    DeviceRegistry,
    config_from_short_id,
    name_from_short_id,
    parse_short_id_from_name,
)


def test_parse_short_id_from_ble_name():
    assert parse_short_id_from_name("CodexMeter-A3F91C") == "A3F91C"
    assert parse_short_id_from_name("CodexMeter") is None
    assert parse_short_id_from_name("Other-A3F91C") is None


def test_config_from_short_id_uses_stable_name():
    config = config_from_short_id("a3:f9-1c", alias="Home")

    assert config.short_id == "A3F91C"
    assert config.name == "CodexMeter-A3F91C"
    assert config.alias == "Home"
    assert config.matches(short_id="A3F91C")
    assert config.matches(name=name_from_short_id("A3F91C"))


def test_registry_round_trips_devices(tmp_path):
    path = tmp_path / "devices.json"
    registry = DeviceRegistry(path)
    registry.upsert(config_from_short_id("A3F91C", alias="Home"))
    registry.save()

    loaded = DeviceRegistry.load(path)

    assert len(loaded.devices) == 1
    assert loaded.devices[0].short_id == "A3F91C"
    assert loaded.devices[0].alias == "Home"
