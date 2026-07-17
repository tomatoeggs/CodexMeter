import datetime as dt
import json
import os

from codexmeter.local_usage import LocalTokenUsageReader


TZ = dt.timezone(dt.timedelta(hours=8))
NOW = dt.datetime(2026, 7, 17, 12, 0, tzinfo=TZ)


def token_event(timestamp: str, total: int) -> str:
    return json.dumps(
        {
            "timestamp": timestamp,
            "type": "event_msg",
            "payload": {
                "type": "token_count",
                "info": {"total_token_usage": {"total_tokens": total}},
            },
        },
        separators=(",", ":"),
    )


def session_meta(timestamp: str, forked: bool = False) -> str:
    payload = {"id": "current"}
    if forked:
        payload["forked_from_id"] = "parent"
    return json.dumps(
        {"timestamp": timestamp, "type": "session_meta", "payload": payload},
        separators=(",", ":"),
    )


def test_local_usage_sums_cumulative_deltas_and_ignores_duplicates(tmp_path):
    sessions = tmp_path / "sessions" / "2026" / "07" / "17"
    sessions.mkdir(parents=True)
    rollout = sessions / "rollout.jsonl"
    rollout.write_text(
        "\n".join(
            [
                token_event("2026-07-16T15:59:00Z", 100),
                token_event("2026-07-16T16:00:00Z", 150),
                token_event("2026-07-16T16:01:00Z", 150),
                token_event("2026-07-16T16:02:00Z", 220),
            ]
        )
        + "\n",
        encoding="utf-8",
    )
    os.utime(rollout, (NOW.timestamp(), NOW.timestamp()))

    reader = LocalTokenUsageReader(tmp_path)

    assert reader.read_today_tokens(NOW) == 120

    with rollout.open("a", encoding="utf-8") as handle:
        handle.write(token_event("2026-07-17T02:00:00Z", 260) + "\n")
    os.utime(rollout, (NOW.timestamp(), NOW.timestamp()))

    assert reader.read_today_tokens(NOW) == 160


def test_local_usage_returns_none_without_compatible_events(tmp_path):
    (tmp_path / "sessions").mkdir()

    assert LocalTokenUsageReader(tmp_path).read_today_tokens(NOW) is None


def test_local_usage_treats_forked_history_replay_as_baseline(tmp_path):
    sessions = tmp_path / "sessions" / "2026" / "07" / "17"
    sessions.mkdir(parents=True)
    rollout = sessions / "forked.jsonl"
    rollout.write_text(
        "\n".join(
            [
                session_meta("2026-07-16T16:00:00Z", forked=True),
                token_event("2026-07-16T16:00:00.100Z", 100),
                token_event("2026-07-16T16:00:00.200Z", 200),
                token_event("2026-07-16T16:00:02Z", 230),
            ]
        )
        + "\n",
        encoding="utf-8",
    )
    os.utime(rollout, (NOW.timestamp(), NOW.timestamp()))

    assert LocalTokenUsageReader(tmp_path).read_today_tokens(NOW) == 30
