LIBRARY()

SRCS(
    result.cpp
    events.cpp
)

PEERDIR(
    contrib/ydb/core/kqp/query_data
    contrib/ydb/core/kqp/common/simple
    yql/essentials/public/issue
)

YQL_LAST_ABI_VERSION()

END()
