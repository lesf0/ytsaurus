#include "scan.h"

#include <contrib/ydb/core/kqp/compute_actor/kqp_compute_events.h>

#include <contrib/ydb/core/sys_view/auth/group_members.h>
#include <contrib/ydb/core/sys_view/auth/groups.h>
#include <contrib/ydb/core/sys_view/auth/owners.h>
#include <contrib/ydb/core/sys_view/auth/permissions.h>
#include <contrib/ydb/core/sys_view/auth/users.h>
#include <contrib/ydb/core/sys_view/common/schema.h>
#include <contrib/ydb/core/sys_view/nodes/nodes.h>
#include <contrib/ydb/core/sys_view/partition_stats/partition_stats.h>
#include <contrib/ydb/core/sys_view/partition_stats/top_partitions.h>
#include <contrib/ydb/core/sys_view/pg_tables/pg_tables.h>
#include <contrib/ydb/core/sys_view/query_stats/query_metrics.h>
#include <contrib/ydb/core/sys_view/query_stats/query_stats.h>
#include <contrib/ydb/core/sys_view/resource_pool_classifiers/resource_pool_classifiers.h>
#include <contrib/ydb/core/sys_view/resource_pools/resource_pools.h>
#include <contrib/ydb/core/sys_view/sessions/sessions.h>
#include <contrib/ydb/core/sys_view/show_create/show_create.h>
#include <contrib/ydb/core/sys_view/storage/groups.h>
#include <contrib/ydb/core/sys_view/storage/pdisks.h>
#include <contrib/ydb/core/sys_view/storage/storage_pools.h>
#include <contrib/ydb/core/sys_view/storage/storage_stats.h>
#include <contrib/ydb/core/sys_view/storage/vslots.h>
#include <contrib/ydb/core/sys_view/tablets/tablets.h>

#include <contrib/ydb/library/actors/core/actor_bootstrapped.h>
#include <contrib/ydb/library/actors/core/hfunc.h>
#include <contrib/ydb/library/actors/core/interconnect.h>
#include <contrib/ydb/library/actors/core/log.h>

namespace NKikimr {
namespace NSysView {

class TSysViewRangesReader : public TActor<TSysViewRangesReader> {
public:
    using TBase = TActor<TSysViewRangesReader>;

    TSysViewRangesReader(
        const NActors::TActorId& ownerId,
        ui32 scanId,
        const TTableId& tableId,
        const TString& tablePath,
        TVector<TSerializedTableRange> ranges,
        const TArrayRef<NMiniKQL::TKqpComputeContextBase::TColumn>& columns,
        TIntrusiveConstPtr<NACLib::TUserToken> userToken,
        const TString& database,
        bool reverse)
        : TBase(&TSysViewRangesReader::ScanState)
        , OwnerId(ownerId)
        , ScanId(scanId)
        , TableId(tableId)
        , TablePath(tablePath)
        , Ranges(std::move(ranges))
        , Columns(columns.begin(), columns.end())
        , UserToken(std::move(userToken))
        , Database(database)
        , Reverse(reverse)
    {
    }

    static constexpr auto ActorActivityType() {
        return NKikimrServices::TActivity::KQP_SYSTEM_VIEW_SCAN;
    }

    STFUNC(ScanState) {
        switch (ev->GetTypeRewrite()) {
            hFunc(NKqp::TEvKqpCompute::TEvScanDataAck, Handle);
            hFunc(NKikimr::NKqp::TEvKqpCompute::TEvScanData, Handle);
            hFunc(NKikimr::NKqp::TEvKqpCompute::TEvScanError, ResendToOwnerAndDie);
            hFunc(NKqp::TEvKqp::TEvAbortExecution, HandleAbortExecution);
            cFunc(TEvents::TEvPoison::EventType, PassAway);
            hFunc(NActors::TEvInterconnect::TEvNodeDisconnected, ResendToOwnerAndDie);
            hFunc(TEvents::TEvUndelivered, ResendToOwnerAndDie);
            hFunc(NKqp::TEvKqpCompute::TEvScanInitActor, Handle);
            default:
                LOG_CRIT(*TlsActivationContext, NKikimrServices::SYSTEM_VIEWS,
                    "NSysView: unexpected event 0x%08" PRIx32, ev->GetTypeRewrite());
        }
    }

    void Handle(NKqp::TEvKqpCompute::TEvScanInitActor::TPtr& msg) {
        ActorIdToProto(SelfId(), msg->Get()->Record.MutableScanActorId());
        Send(OwnerId, msg->Release().Release());
    }

    void Handle(NKqp::TEvKqpCompute::TEvScanDataAck::TPtr& ack) {
        Y_DEBUG_ABORT_UNLESS(ack->Sender == OwnerId);
        if (!ScanActorId) {
            if (CurrentRange < Ranges.size()) {
                auto actor = CreateSystemViewScan(
                    SelfId(), ScanId, TableId, TablePath, Ranges[CurrentRange].ToTableRange(),
                    Columns, UserToken, Database, Reverse);
                ScanActorId = Register(actor.Release());
                CurrentRange += 1;
            } else {
                PassAway();
                return;
            }
        }

        Send(*ScanActorId, new NKqp::TEvKqpCompute::TEvScanDataAck(ack->Get()->FreeSpace, ack->Get()->Generation));
    }

    void Handle(NKqp::TEvKqpCompute::TEvScanData::TPtr& data) {
        bool& finished = (*data->Get()).Finished;
        if (finished) {
            if (CurrentRange != Ranges.size()) {
                finished = false;
                ScanActorId.Clear();
            } else {
                TBase::Send(OwnerId, THolder(data->Release().Release()));
                PassAway();
                return;
            }
        }

        TBase::Send(OwnerId, THolder(data->Release().Release()));
    }

    void HandleAbortExecution(NKqp::TEvKqp::TEvAbortExecution::TPtr& ev) {
        LOG_ERROR_S(TlsActivationContext->AsActorContext(), NKikimrServices::SYSTEM_VIEWS,
            "Got abort execution event, actor: " << TBase::SelfId()
                << ", owner: " << OwnerId
                << ", scan id: " << ScanId
                << ", table id: " << TableId
                << ", code: " << NYql::NDqProto::StatusIds::StatusCode_Name(ev->Get()->Record.GetStatusCode())
                << ", error: " << ev->Get()->GetIssues().ToOneLineString());

        if (ScanActorId) {
            Send(*ScanActorId, THolder(ev->Release().Release()));
        }

        PassAway();
    }

    void ResendToOwnerAndDie(auto& err) {
        TBase::Send(OwnerId, err->Release().Release());
        PassAway();
    }

    void PassAway() {
        if (ScanActorId) {
            Send(*ScanActorId, new TEvents::TEvPoison());
        }
        TBase::PassAway();
    }

private:
    TActorId OwnerId;
    ui32 ScanId;
    TTableId TableId;
    TString TablePath;
    TVector<TSerializedTableRange> Ranges;
    TVector<NMiniKQL::TKqpComputeContextBase::TColumn> Columns;
    const TIntrusiveConstPtr<NACLib::TUserToken> UserToken;
    const TString Database;
    const bool Reverse;

    ui64 CurrentRange = 0;
    TMaybe<TActorId> ScanActorId;
};

THolder<NActors::IActor> CreateSystemViewScan(
    const NActors::TActorId& ownerId,
    ui32 scanId,
    const TTableId& tableId,
    const TString& tablePath,
    TVector<TSerializedTableRange> ranges,
    const TArrayRef<NMiniKQL::TKqpComputeContextBase::TColumn>& columns,
    TIntrusiveConstPtr<NACLib::TUserToken> userToken,
    const TString& database,
    bool reverse
) {
    if (ranges.size() == 1) {
        return CreateSystemViewScan(ownerId, scanId, tableId, tablePath, ranges[0].ToTableRange(), columns, std::move(userToken), database, reverse);
    } else {
        return MakeHolder<TSysViewRangesReader>(ownerId, scanId, tableId, tablePath, ranges, columns, std::move(userToken), database, reverse);
    }
}

THolder<NActors::IActor> CreateSystemViewScan(
    const NActors::TActorId& ownerId,
    ui32 scanId,
    const TTableId& tableId,
    const TString& tablePath,
    const TTableRange& tableRange,
    const TArrayRef<NMiniKQL::TKqpComputeContextBase::TColumn>& columns,
    TIntrusiveConstPtr<NACLib::TUserToken> userToken,
    const TString& database,
    bool reverse
) {
    if (tableId.SysViewInfo == PartitionStatsName) {
        return CreatePartitionStatsScan(ownerId, scanId, tableId, tableRange, columns);
    }

    if (tableId.SysViewInfo == NodesName) {
        return CreateNodesScan(ownerId, scanId, tableId, tableRange, columns);
    }

    if (tableId.SysViewInfo == QuerySessions) {
        return CreateSessionsScan(ownerId, scanId, tableId, tableRange, columns);
    }

    if (tableId.SysViewInfo == TopQueriesByDuration1MinuteName ||
        tableId.SysViewInfo == TopQueriesByDuration1HourName ||
        tableId.SysViewInfo == TopQueriesByReadBytes1MinuteName ||
        tableId.SysViewInfo == TopQueriesByReadBytes1HourName ||
        tableId.SysViewInfo == TopQueriesByCpuTime1MinuteName ||
        tableId.SysViewInfo == TopQueriesByCpuTime1HourName ||
        tableId.SysViewInfo == TopQueriesByRequestUnits1MinuteName ||
        tableId.SysViewInfo == TopQueriesByRequestUnits1HourName)
    {
        return CreateQueryStatsScan(ownerId, scanId, tableId, tableRange, columns);
    }

    if (tableId.SysViewInfo == PDisksName) {
        return CreatePDisksScan(ownerId, scanId, tableId, tableRange, columns);
    }

    if (tableId.SysViewInfo == VSlotsName) {
        return CreateVSlotsScan(ownerId, scanId, tableId, tableRange, columns);
    }

    if (tableId.SysViewInfo == GroupsName) {
        return CreateGroupsScan(ownerId, scanId, tableId, tableRange, columns);
    }

    if (tableId.SysViewInfo == StoragePoolsName) {
        return CreateStoragePoolsScan(ownerId, scanId, tableId, tableRange, columns);
    }

    if (tableId.SysViewInfo == StorageStatsName) {
        return CreateStorageStatsScan(ownerId, scanId, tableId, tableRange, columns);
    }

    if (tableId.SysViewInfo == TabletsName) {
        return CreateTabletsScan(ownerId, scanId, tableId, tableRange, columns);
    }

    if (tableId.SysViewInfo == QueryMetricsName) {
        return CreateQueryMetricsScan(ownerId, scanId, tableId, tableRange, columns);
    }

    if (tableId.SysViewInfo == TopPartitionsByCpu1MinuteName ||
        tableId.SysViewInfo == TopPartitionsByCpu1HourName)
    {
        return CreateTopPartitionsByCpuScan(ownerId, scanId, tableId, tableRange, columns);
    }

    if (tableId.SysViewInfo == TopPartitionsByTli1MinuteName ||
        tableId.SysViewInfo == TopPartitionsByTli1HourName)
    {
        return CreateTopPartitionsByTliScan(ownerId, scanId, tableId, tableRange, columns);
    }    
    
    if (tableId.SysViewInfo == PgTablesName) {
        return CreatePgTablesScan(ownerId, scanId, tableId, tablePath, tableRange, columns);
    }

    if (tableId.SysViewInfo == InformationSchemaTablesName) {
        return CreateInformationSchemaTablesScan(ownerId, scanId, tableId, tablePath, tableRange, columns);
    }

    if (tableId.SysViewInfo == PgClassName) {
        return CreatePgClassScan(ownerId, scanId, tableId, tablePath, tableRange, columns);
    }

    if (tableId.SysViewInfo == ResourcePoolClassifiersName) {
        return CreateResourcePoolClassifiersScan(ownerId, scanId, tableId, tableRange, columns, std::move(userToken), database, reverse);
    }

    if (tableId.SysViewInfo == ResourcePoolsName) {
        return CreateResourcePoolsScan(ownerId, scanId, tableId, tableRange, columns, std::move(userToken), database, reverse);
    }

    {
        using namespace NAuth;
        if (tableId.SysViewInfo == UsersName) {
            return CreateUsersScan(ownerId, scanId, tableId, tableRange, columns, std::move(userToken));
        }
        if (tableId.SysViewInfo == NAuth::GroupsName) {
            return NAuth::CreateGroupsScan(ownerId, scanId, tableId, tableRange, columns, std::move(userToken));
        }
        if (tableId.SysViewInfo == GroupMembersName) {
            return NAuth::CreateGroupMembersScan(ownerId, scanId, tableId, tableRange, columns, std::move(userToken));
        }
        if (tableId.SysViewInfo == OwnersName) {
            return NAuth::CreateOwnersScan(ownerId, scanId, tableId, tableRange, columns, std::move(userToken));
        }
        if (tableId.SysViewInfo == PermissionsName || tableId.SysViewInfo == EffectivePermissionsName) {
            return NAuth::CreatePermissionsScan(tableId.SysViewInfo == EffectivePermissionsName,
                ownerId, scanId, tableId, tableRange, columns, std::move(userToken));
        }
    }

    if (tableId.SysViewInfo == ShowCreateName) {
        return CreateShowCreate(ownerId, scanId, tableId, tableRange, columns, database, std::move(userToken));
    }

    return {};
}

} // NSysView
} // NKikimr
