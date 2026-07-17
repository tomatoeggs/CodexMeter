import asyncio
from unittest.mock import patch

from codexmeter.app_server import AppServerError
from codexmeter.limits import WINDOW_7D_MINS
from codexmeter.provider import CodexUsageProvider


RATE_LIMITS = {
    "rateLimitsByLimitId": {
        "codex": {
            "limitId": "codex",
            "primary": {
                "usedPercent": 3,
                "resetsAt": 1784234099,
                "windowDurationMins": WINDOW_7D_MINS,
            },
        }
    }
}


class FakeClient:
    fail_usage = False

    def __init__(self, *_args, **_kwargs):
        pass

    async def __aenter__(self):
        return self

    async def __aexit__(self, *_exc):
        return None

    async def initialize(self):
        return {}

    async def request(self, method, _params=None):
        if method == "account/read":
            return {"account": {"type": "chatgpt"}}
        if method == "account/rateLimits/read":
            return RATE_LIMITS
        if method == "account/usage/read":
            if self.fail_usage:
                raise AppServerError("token activity unavailable")
            return {
                "dailyUsageBuckets": [],
                "summary": {"lifetimeTokens": 123},
            }
        raise AssertionError(f"unexpected method: {method}")


class FakeLocalUsageReader:
    def read_today_tokens(self):
        return 123


def test_provider_fetches_optional_token_activity_without_inventing_h5():
    FakeClient.fail_usage = False
    with patch("codexmeter.provider.JsonRpcClient", FakeClient):
        snapshot = asyncio.run(
            CodexUsageProvider(local_usage_reader=FakeLocalUsageReader()).fetch()
        )

    assert snapshot.h5_remaining_percent is None
    assert snapshot.d7_remaining_percent == 97
    assert snapshot.today_tokens == 123
    assert snapshot.last_7d_tokens == 123


def test_provider_keeps_quota_when_token_activity_request_fails():
    FakeClient.fail_usage = True
    try:
        with patch("codexmeter.provider.JsonRpcClient", FakeClient):
            snapshot = asyncio.run(
                CodexUsageProvider(local_usage_reader=FakeLocalUsageReader()).fetch()
            )
    finally:
        FakeClient.fail_usage = False

    assert snapshot.h5_remaining_percent is None
    assert snapshot.d7_remaining_percent == 97
    assert snapshot.today_tokens == 123
    assert snapshot.last_7d_tokens is None
