from codexmeter.limits import (
    WINDOW_5H_MINS,
    WINDOW_7D_MINS,
    format_beijing_time,
    normalize_limits,
    usage_snapshot_from_rate_limits,
)


def sample_response():
    return {
        "rateLimitsByLimitId": {
            "codex": {
                "limitId": "codex",
                "planType": "pro",
                "primary": {
                    "usedPercent": 28,
                    "resetsAt": 1783093200,
                    "windowDurationMins": WINDOW_5H_MINS,
                },
                "secondary": {
                    "usedPercent": 16,
                    "resetsAt": 1783545600,
                    "windowDurationMins": WINDOW_7D_MINS,
                },
            },
            "other": {
                "limitId": "other",
                "primary": {"usedPercent": 99, "windowDurationMins": 60},
            },
        }
    }


def test_normalize_limits_prefers_codex_bucket():
    windows = normalize_limits(sample_response())
    assert [item.name for item in windows] == ["5h", "7d"]
    assert windows[0].remaining_percent == 72
    assert windows[1].remaining_percent == 84


def test_usage_snapshot_maps_expected_windows():
    snapshot = usage_snapshot_from_rate_limits(sample_response(), now=123)
    assert snapshot.source == "codex"
    assert snapshot.h5_remaining_percent == 72
    assert snapshot.h5_resets_at == 1783093200
    assert snapshot.d7_remaining_percent == 84
    assert snapshot.d7_resets_at == 1783545600
    assert snapshot.generated_at == 123


def test_usage_snapshot_falls_back_to_single_rate_limits():
    response = {
        "rateLimits": {
            "limitId": "codex",
            "primary": {"usedPercent": 1, "resetsAt": None},
            "secondary": {"usedPercent": 2, "resetsAt": 200},
        }
    }
    snapshot = usage_snapshot_from_rate_limits(response, now=10)
    assert snapshot.h5_remaining_percent == 99
    assert snapshot.h5_resets_at is None
    assert snapshot.d7_remaining_percent == 98
    assert snapshot.d7_resets_at == 200


def test_beijing_formatting():
    assert format_beijing_time(0, with_date=False) == "08:00"
    assert format_beijing_time(0, with_date=True) == "1月1日 08:00"
