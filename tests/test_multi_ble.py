import asyncio
import time
from types import SimpleNamespace

from codexmeter.ble import BleIdentity, BleIdentityError, BleTransport
from codexmeter.device_registry import DeviceConfig, config_from_short_id
from codexmeter.multi_ble import (
    DeviceManager,
    DeviceSlot,
    DeviceWorker,
    DiscoveredDevice,
    DiscoveryService,
    scan_codexmeter_devices,
)
from codexmeter.payloads import Payload, build_activity_payload, build_screen_control_payload
from codexmeter.queueing import put_latest


def test_device_manager_broadcasts_to_each_device_queue():
    async def scenario():
        manager = DeviceManager(
            [config_from_short_id("A3F91C"), config_from_short_id("9B20D4")],
            scan_timeout_sec=0.1,
            scan_interval_sec=0.1,
            connect_timeout_sec=0.1,
            disconnect_timeout_sec=0.1,
            write_timeout_sec=0.1,
            ack_timeout_sec=0.1,
            notify_timeout_sec=0.1,
            healthcheck_interval_sec=1.0,
        )
        payload = build_activity_payload(2, now=1)

        await manager.broadcast(payload)

        assert [slot.queue.get_nowait() for slot in manager.slots] == [payload, payload]

    asyncio.run(scenario())


def test_device_manager_sets_connect_control_on_every_state():
    manager = DeviceManager(
        [config_from_short_id("A3F91C"), config_from_short_id("9B20D4")],
        scan_timeout_sec=0.1,
        scan_interval_sec=0.1,
        connect_timeout_sec=0.1,
        disconnect_timeout_sec=0.1,
        write_timeout_sec=0.1,
        ack_timeout_sec=0.1,
        notify_timeout_sec=0.1,
        healthcheck_interval_sec=1.0,
    )
    payload = build_screen_control_payload(True, "ble_restored", now=1)

    manager.set_connect_control(payload)

    assert [slot.state.connect_control for slot in manager.slots] == [payload, payload]


def test_put_latest_prefers_dropping_replaceable_payload_before_alert():
    async def scenario():
        queue: asyncio.Queue[Payload] = asyncio.Queue(maxsize=2)
        await queue.put(Payload("alert", {"k": "alert", "id": "a"}))
        await queue.put(Payload("usage", {"k": "usage", "h5": 1}))

        await put_latest(queue, Payload("alert", {"k": "alert", "id": "b"}))

        items = [queue.get_nowait(), queue.get_nowait()]
        assert [item.data["id"] for item in items] == ["a", "b"]

    asyncio.run(scenario())


def test_device_worker_rejects_identity_with_wrong_short_id():
    config = config_from_short_id("A3F91C")
    worker = DeviceWorker(
        DeviceSlot(config), DiscoveryService(), BleTransport()
    )

    try:
        worker._validate_identity(
            BleIdentity(
                "codexmeter-9b20d4858428",
                "9B20D4",
                "CodexMeter-9B20D4",
            )
        )
    except BleIdentityError:
        return
    raise AssertionError("expected mismatched device identity")


def test_device_worker_binds_short_config_to_full_identity():
    config = config_from_short_id("A3F91C", alias="Home")

    def bind(existing, identity):
        return DeviceConfig(
            device_id=identity.device_id,
            short_id=identity.short_id,
            alias=existing.alias,
            name=identity.name,
        )

    slot = DeviceSlot(config)
    worker = DeviceWorker(
        slot, DiscoveryService(), BleTransport(), identity_observer=bind
    )
    worker._validate_identity(
        BleIdentity(
            "codexmeter-a3f91c858428",
            "A3F91C",
            "CodexMeter-A3F91C",
        )
    )

    assert slot.config.device_id == "codexmeter-a3f91c858428"
    assert slot.config.alias == "Home"


def test_scan_merges_connected_device_that_is_not_advertising():
    async def scenario():
        advertised = SimpleNamespace(address="adv-address", name="CodexMeter-A3F91C")
        advertisement = SimpleNamespace(
            local_name="CodexMeter-A3F91C", service_uuids=[]
        )
        connected = SimpleNamespace(
            address="connected-address", name="CodexMeter-9B20D4"
        )
        unrelated = SimpleNamespace(address="other-address", name="Keyboard")

        async def discover(**_kwargs):
            return {advertised.address: (advertised, advertisement)}

        async def connected_lookup():
            return [connected, unrelated]

        found = await scan_codexmeter_devices(
            0.1, discover=discover, connected_lookup=connected_lookup
        )
        by_short_id = {device.short_id: device for device in found}

        assert set(by_short_id) == {"A3F91C", "9B20D4"}
        assert by_short_id["A3F91C"].source == "advertisement"
        assert by_short_id["9B20D4"].source == "connected"

    asyncio.run(scenario())


def test_scan_prefers_advertisement_when_connected_lookup_returns_duplicate():
    async def scenario():
        advertised = SimpleNamespace(address="same-address", name="CodexMeter-A3F91C")
        advertisement = SimpleNamespace(
            local_name="CodexMeter-A3F91C", service_uuids=[]
        )
        connected = SimpleNamespace(
            address="same-address", name="CodexMeter-A3F91C"
        )

        async def discover(**_kwargs):
            return {advertised.address: (advertised, advertisement)}

        async def connected_lookup():
            return [connected]

        found = await scan_codexmeter_devices(
            0.1, discover=discover, connected_lookup=connected_lookup
        )

        assert len(found) == 1
        assert found[0].source == "advertisement"
        assert found[0].device is advertised

    asyncio.run(scenario())


def test_discovery_status_reports_connected_recovery_results():
    async def scenario():
        async def scanner(_timeout):
            return [
                DiscoveredDevice(
                    address="connected-address",
                    name="CodexMeter-A3F91C",
                    short_id="A3F91C",
                    device=object(),
                    source="connected",
                )
            ]

        discovery = DiscoveryService(scanner=scanner)
        await discovery.scan_once()
        status = discovery.status(now=time.monotonic())

        assert status["last_scan_age_sec"] <= 0.1
        assert status["last_result_count"] == 1
        assert status["connected_result_count"] == 1
        assert status["cached_count"] == 1
        assert status["last_error"] is None
        assert status["connected_lookup_error"] is None

    asyncio.run(scenario())


def test_device_worker_records_clean_disconnect_while_waiting_for_recovery():
    async def scenario():
        config = config_from_short_id("A3F91C")
        slot = DeviceSlot(config)
        discovered = DiscoveredDevice(
            address="connected-address",
            name="CodexMeter-A3F91C",
            short_id="A3F91C",
            device=object(),
            source="connected",
        )

        class FakeDiscovery:
            async def wait_for(self, _config, _stop_event):
                return discovered

        class FakeTransport:
            async def connect_and_write(self, _target, _queue, state, _stop, **_kwargs):
                state.connected = False
                return True

        worker = DeviceWorker(slot, FakeDiscovery(), FakeTransport())
        stop_event = asyncio.Event()
        task = asyncio.create_task(worker.run(stop_event))
        for _ in range(20):
            if slot.state.last_error is not None:
                break
            await asyncio.sleep(0)

        assert slot.state.last_error == "BLE disconnected; waiting for rediscovery"
        assert slot.state.consecutive_failures == 1
        assert slot.state.last_discovery_source == "connected"
        assert slot.state.last_discovered_address == "connected-address"

        stop_event.set()
        await asyncio.wait_for(task, timeout=0.2)

    asyncio.run(scenario())
