#pragma once

#include <yql/essentials/public/issue/yql_issue.h>
#include <contrib/ydb/public/api/protos/ydb_status_codes.pb.h>
#include <contrib/ydb/core/kqp/common/simple/kqp_event_ids.h>

namespace NKikimr::NKqp {

namespace NShardResolver {
    struct TEvShardsResolveStatus : public TEventLocal<TEvShardsResolveStatus,
        TKqpExecuterEvents::EvShardsResolveStatus>
    {
        Ydb::StatusIds::StatusCode Status = Ydb::StatusIds::SUCCESS;
        NYql::TIssues Issues;

        TMap<ui64, ui64> ShardNodes;
        ui32 Unresolved = 0;
    };
};

}
