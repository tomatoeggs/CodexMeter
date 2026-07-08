import asyncio

from codexmeter.daemon import build_stale_usage_payload, put_latest
from codexmeter.payloads import Payload, build_screen_control_payload


def test_put_latest_keeps_only_latest_control_payload():
    async def scenario():
        queue: asyncio.Queue[Payload] = asyncio.Queue(maxsize=8)
        await put_latest(queue, Payload("usage", {"k": "usage", "v": 1}))
        await put_latest(queue, build_screen_control_payload(False, "mac_locked", now=1))
        await put_latest(queue, Payload("activity", {"k": "activity", "v": 1}))
        await put_latest(queue, build_screen_control_payload(True, "mac_unlocked", now=2))

        items = []
        while not queue.empty():
            items.append(queue.get_nowait())

        assert [item.kind for item in items] == ["usage", "activity", "control"]
        assert items[-1].data["on"] is True
        assert items[-1].data["why"] == "mac_unlocked"

    asyncio.run(scenario())


def test_put_latest_keeps_only_latest_usage_and_activity_payloads():
    async def scenario():
        queue: asyncio.Queue[Payload] = asyncio.Queue(maxsize=8)
        await put_latest(queue, Payload("usage", {"k": "usage", "h5": 1}))
        await put_latest(queue, Payload("alert", {"k": "alert", "id": "a"}))
        await put_latest(queue, Payload("usage", {"k": "usage", "h5": 2}))
        await put_latest(queue, Payload("activity", {"k": "activity", "run": 1}))
        await put_latest(queue, Payload("activity", {"k": "activity", "run": 2}))

        items = []
        while not queue.empty():
            items.append(queue.get_nowait())

        assert [item.kind for item in items] == ["alert", "usage", "activity"]
        assert items[1].data["h5"] == 2
        assert items[2].data["run"] == 2

    asyncio.run(scenario())


def test_build_stale_usage_payload_preserves_values_and_updates_status():
    payload = Payload(
        "usage",
        {
            "k": "usage",
            "h5": 88,
            "d7": 64,
            "st": "ok",
            "t": 1,
        },
    )

    stale = build_stale_usage_payload(payload, now=42)

    assert stale.kind == "usage"
    assert stale.data["h5"] == 88
    assert stale.data["d7"] == 64
    assert stale.data["st"] == "stale"
    assert stale.data["t"] == 42
    assert payload.data["st"] == "ok"
