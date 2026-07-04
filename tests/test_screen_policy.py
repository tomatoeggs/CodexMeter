from codexmeter.screen_policy import LockScreenPolicy, parse_ioreg_lock_state


def test_lock_policy_turns_off_after_timeout_once():
    policy = LockScreenPolicy(timeout_sec=300)

    first = policy.observe(True, now=1000.0, epoch_now=1)
    assert first.payloads == ()
    assert first.connect_control is None

    early = policy.observe(True, now=1299.0, epoch_now=2)
    assert early.payloads == ()

    due = policy.observe(True, now=1300.0, epoch_now=3)
    assert len(due.payloads) == 1
    assert due.payloads[0].data["on"] is False
    assert due.payloads[0].data["why"] == "mac_locked"

    repeat = policy.observe(True, now=1600.0, epoch_now=4)
    assert repeat.payloads == ()


def test_lock_policy_turns_on_when_unlocked_and_allows_connect_wake():
    policy = LockScreenPolicy(timeout_sec=300)

    policy.observe(True, now=1000.0, epoch_now=1)
    result = policy.observe(False, now=1100.0, epoch_now=2)

    assert len(result.payloads) == 1
    assert result.payloads[0].data["on"] is True
    assert result.payloads[0].data["why"] == "mac_unlocked"
    assert result.connect_control is not None
    assert result.connect_control.data["on"] is True
    assert result.connect_control.data["why"] == "ble_restored"


def test_lock_policy_initial_unlocked_turns_on_and_sets_connect_wake():
    policy = LockScreenPolicy(timeout_sec=300)
    result = policy.observe(False, now=1000.0, epoch_now=1)

    assert len(result.payloads) == 1
    assert result.payloads[0].data["on"] is True
    assert result.payloads[0].data["why"] == "mac_unlocked"
    assert result.connect_control is not None
    assert result.connect_control.data["on"] is True


def test_parse_ioreg_lock_state():
    assert parse_ioreg_lock_state('"IOConsoleLocked" = Yes') is True
    assert parse_ioreg_lock_state('"IOConsoleLocked" = No') is False
    assert parse_ioreg_lock_state('"CGSSessionScreenIsLocked"=Yes') is True
    assert parse_ioreg_lock_state("no lock key") is None
