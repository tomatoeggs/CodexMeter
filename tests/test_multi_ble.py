import asyncio

from codexmeter.device_registry import config_from_short_id
from codexmeter.multi_ble import DeviceManager
from codexmeter.payloads import Payload, build_activity_payload, build_screen_control_payload
from codexmeter.queueing import put_latest


def test_device_manager_broadcasts_to_each_device_queue():
    async def scenario():
        manager = DeviceManager(
            [config_from_short_id("A3F91C"), config_from_short_id("9B20D4")],
            scan_timeout_sec=0.1,
            scan_interval_sec=0.1,
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
