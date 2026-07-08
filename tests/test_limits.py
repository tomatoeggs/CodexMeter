from codexmeter.limits import (
    UsageSnapshot,
    UsageSnapshotStabilizer,
    WINDOW_5H_MINS,
    WINDOW_7D_MINS,
    format_beijing_time,
    is_suspicious_initial_snapshot,
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


def test_usage_stabilizer_rejects_transient_d7_empty_window():
    stabilizer = UsageSnapshotStabilizer()
    trusted = usage_snapshot(
        h5=91,
        h5r=1783499127,
        d7=36,
        d7r=1783526383,
        generated_at=1783483100,
    )
    transient = usage_snapshot(
        h5=98,
        h5r=1783499154,
        d7=100,
        d7r=1784085954,
        generated_at=1783483132,
    )

    assert stabilizer.stabilize(trusted).accepted is True
    decision = stabilizer.stabilize(transient)

    assert decision.accepted is False
    assert decision.reason == "d7_early_reset_jump"
    assert decision.snapshot.d7_remaining_percent == 36
    assert decision.snapshot.d7_resets_at == 1783526383
    assert decision.snapshot.h5_remaining_percent == 91
    assert decision.snapshot.generated_at == transient.generated_at


def test_suspicious_initial_snapshot_detects_empty_d7_window():
    suspicious = usage_snapshot(
        h5=97,
        h5r=1783499154,
        d7=100,
        d7r=1784085954,
        generated_at=1783483796,
    )
    normal = usage_snapshot(
        h5=100,
        h5r=1783499154,
        d7=100,
        d7r=1784085954,
        generated_at=1783483796,
    )

    assert is_suspicious_initial_snapshot(suspicious) is True
    assert is_suspicious_initial_snapshot(normal) is False


def test_usage_stabilizer_does_not_confirm_early_reset_jump():
    stabilizer = UsageSnapshotStabilizer()
    trusted = usage_snapshot(
        h5=91,
        h5r=1783499127,
        d7=36,
        d7r=1783526383,
        generated_at=1783483100,
    )
    transient = usage_snapshot(
        h5=98,
        h5r=1783499154,
        d7=100,
        d7r=1784085954,
        generated_at=1783483132,
    )

    stabilizer.stabilize(trusted)
    first = stabilizer.stabilize(transient)
    second = stabilizer.stabilize(transient)

    assert first.accepted is False
    assert second.accepted is False
    assert second.reason == "d7_early_reset_jump"
    assert second.snapshot.d7_remaining_percent == 36


def test_usage_stabilizer_accepts_new_window_after_reset_time():
    stabilizer = UsageSnapshotStabilizer()
    trusted = usage_snapshot(
        h5=91,
        h5r=1783499127,
        d7=36,
        d7r=1783526383,
        generated_at=1783483100,
    )
    reset = usage_snapshot(
        h5=100,
        h5r=1783545000,
        d7=100,
        d7r=1784131183,
        generated_at=1783526500,
    )

    stabilizer.stabilize(trusted)
    decision = stabilizer.stabilize(reset)

    assert decision.accepted is True
    assert decision.snapshot.d7_remaining_percent == 100
    assert decision.snapshot.d7_resets_at == 1784131183


def test_usage_stabilizer_requires_confirmation_for_large_same_window_recovery():
    stabilizer = UsageSnapshotStabilizer()
    trusted = usage_snapshot(
        h5=91,
        h5r=1783499127,
        d7=36,
        d7r=1783526383,
        generated_at=1783483100,
    )
    recovery = usage_snapshot(
        h5=91,
        h5r=1783499127,
        d7=70,
        d7r=1783526400,
        generated_at=1783483160,
    )

    stabilizer.stabilize(trusted)
    first = stabilizer.stabilize(recovery)
    second = stabilizer.stabilize(recovery)

    assert first.accepted is False
    assert first.reason == "d7_remaining_recovery"
    assert first.snapshot.d7_remaining_percent == 36
    assert second.accepted is True
    assert second.reason == "confirmed:d7_remaining_recovery"
    assert second.snapshot.d7_remaining_percent == 70


def test_beijing_formatting():
    assert format_beijing_time(0, with_date=False) == "08:00"
    assert format_beijing_time(0, with_date=True) == "1月1日 08:00"


def usage_snapshot(
    h5: int,
    h5r: int,
    d7: int,
    d7r: int,
    generated_at: int,
) -> UsageSnapshot:
    return UsageSnapshot(
        source="codex",
        h5_remaining_percent=h5,
        h5_resets_at=h5r,
        d7_remaining_percent=d7,
        d7_resets_at=d7r,
        status="ok",
        generated_at=generated_at,
    )
