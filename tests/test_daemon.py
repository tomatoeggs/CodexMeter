import asyncio

from codexmeter.daemon import put_latest
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
