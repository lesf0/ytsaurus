#pragma once
#include "counters.h"

#include <contrib/ydb/core/tx/columnshard/engines/scheme/tiering/tier_info.h>
#include <contrib/ydb/core/tx/columnshard/engines/scheme/versions/abstract_scheme.h>
#include <contrib/ydb/core/tx/columnshard/engines/storage/actualizer/abstract/abstract.h>
#include <contrib/ydb/core/tx/columnshard/engines/storage/actualizer/common/address.h>
#include <contrib/ydb/core/tx/tiering/manager.h>

namespace NKikimr::NOlap {
class TTiering;
class TCSMetadataRequest;
}

namespace NKikimr::NOlap::NActualizer {

class TTieringActualizer: public IActualizer {
private:
    TTieringCounters Counters;
    class TFullActualizationInfo {
    private:
        TRWAddress Address;
        YDB_ACCESSOR_DEF(TString, TargetTierName);
        YDB_ACCESSOR_DEF(ISnapshotSchema::TPtr, TargetScheme);
        i64 WaitDurationValue;
    public:
        TString DebugString() const {
            return TStringBuilder() << "{address=" << Address.DebugString() << ";target_tier=" << TargetTierName << ";wait_duration=" << TDuration::FromValue(WaitDurationValue) << "}";
        }

        const TRWAddress& GetAddress() const {
            return Address;
        }

        TFullActualizationInfo(TRWAddress&& address, const TString& targetTierName, const i64 waitDurationValue, const ISnapshotSchema::TPtr& targetScheme)
            : Address(std::move(address))
            , TargetTierName(targetTierName)
            , TargetScheme(targetScheme)
            , WaitDurationValue(waitDurationValue)
        {

        }

        TInstant GetWaitInstant(const TInstant now) const {
            if (WaitDurationValue >= 0) {
                return now + TDuration::FromValue(WaitDurationValue);
            } else {
                return now;
            }
        }

        TDuration GetLateness() const {
            if (WaitDurationValue >= 0) {
                return TDuration::Zero();
            } else {
                return TDuration::FromValue(-WaitDurationValue);
            }
        }
    };

    class TFindActualizationInfo {
    private:
        TRWAddress RWAddress;
        YDB_READONLY_DEF(TInstant, WaitInstant);
    public:
        const TRWAddress& GetRWAddress() const {
            return RWAddress;
        }

        TFindActualizationInfo(TRWAddress&& rwAddress, const TInstant waitInstant)
            : RWAddress(std::move(rwAddress))
            , WaitInstant(waitInstant) {

        }
    };

    class TRWAddressPortionsInfo {
    private:
        std::map<TInstant, THashSet<ui64>> Portions;
    public:
        const std::map<TInstant, THashSet<ui64>>& GetPortions() const {
            return Portions;
        }

        void CorrectSignals(ui64& queueSize, ui64& waitSeconds, const TInstant now) const {
            if (Portions.empty()) {
                return;
            }
            for (auto&& i : Portions) {
                if (i.first > now) {
                    break;
                }
                queueSize += i.second.size();
            }
            if (Portions.begin()->first < now) {
                waitSeconds = std::max(waitSeconds, (now - Portions.begin()->first).Seconds());
            }
        }

        [[nodiscard]] bool AddPortion(const TFullActualizationInfo& info, const ui64 portionId, const TInstant now) {
            return Portions[info.GetWaitInstant(now)].emplace(portionId).second;
        }

        bool RemovePortion(const TFindActualizationInfo& info, const ui64 portionId) {
            auto itInstant = Portions.find(info.GetWaitInstant());
            AFL_VERIFY(itInstant != Portions.end());
            AFL_VERIFY(itInstant->second.erase(portionId));
            if (itInstant->second.empty()) {
                Portions.erase(itInstant);
            }
            return Portions.empty();
        }
    };

    std::optional<TTiering> Tiering;
    std::optional<ui32> TieringColumnId;

    std::shared_ptr<ISnapshotSchema> TargetCriticalSchema;
    const TInternalPathId PathId;
    const TVersionedIndex& VersionedIndex;
    const std::shared_ptr<IStoragesManager>& StoragesManager;

    THashMap<TRWAddress, TRWAddressPortionsInfo> PortionIdByWaitDuration;
    THashMap<ui64, TFindActualizationInfo> PortionsInfo;
    THashSet<ui64> NewPortionIds;
    THashMap<ui64, std::shared_ptr<arrow::Scalar>> MaxByPortionId;

    std::shared_ptr<ISnapshotSchema> GetTargetSchema(const std::shared_ptr<ISnapshotSchema>& portionSchema) const;

    std::optional<TFullActualizationInfo> BuildActualizationInfo(const TPortionInfo& portion, const TInstant now) const;

    void AddPortionImpl(const TPortionInfo& portion, const TInstant now);

    virtual void DoAddPortion(const TPortionInfo& portion, const TAddExternalContext& addContext) override;
    virtual void DoRemovePortion(const ui64 portionId) override;
    virtual void DoExtractTasks(TTieringProcessContext& tasksContext, const TExternalTasksContext& externalContext, TInternalTasksContext& internalContext) override;
public:
    void ActualizePortionInfo(const TPortionDataAccessor& accessor, const TActualizationContext& context);
    std::vector<TCSMetadataRequest> BuildMetadataRequests(
        const TInternalPathId pathId, const THashMap<ui64, TPortionInfo::TPtr>& portions, const std::shared_ptr<TTieringActualizer>& index);

    void Refresh(const std::optional<TTiering>& info, const TAddExternalContext& externalContext);

    TTieringActualizer(const TInternalPathId pathId, const TVersionedIndex& versionedIndex, const std::shared_ptr<IStoragesManager>& storagesManager)
        : PathId(pathId)
        , VersionedIndex(versionedIndex)
        , StoragesManager(storagesManager)
    {
        Y_UNUSED(PathId);
        AFL_VERIFY(StoragesManager);
    }
};

}