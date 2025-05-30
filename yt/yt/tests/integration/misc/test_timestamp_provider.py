from yt_env_setup import YTEnvSetup

from yt_commands import authors, generate_timestamp, start_transaction, commit_transaction

import pytest

##################################################################


@pytest.mark.enabled_multidaemon
class TestTimestampProvider(YTEnvSetup):
    ENABLE_MULTIDAEMON = True
    NUM_MASTERS = 1
    NUM_NODES = 3
    NUM_TIMESTAMP_PROVIDERS = 1

    @authors("gritukan")
    def test_generate_timestamp(self):
        t1 = generate_timestamp()
        t2 = generate_timestamp()
        assert t2 > t1

    @authors("gritukan", "babenko")
    def test_tx(self):
        tx = start_transaction()
        commit_transaction(tx)


##################################################################


@pytest.mark.enabled_multidaemon
class TestTimestampProviderClocks(TestTimestampProvider):
    ENABLE_MULTIDAEMON = True
    NUM_CLOCKS = 1
