#pragma once
#include "defs.h"
#include "keyvalue_simple_db.h"
#include <contrib/ydb/library/actors/core/actor.h>

namespace NKikimr {

namespace NTable {
    class TDatabase;
}

namespace NKeyValue {

class TSimpleDbFlat : public ISimpleDb {
protected:
    NTable::TDatabase &Db;
    TVector<TLogoBlobID>& TrashBeingCommitted;

public:
    TSimpleDbFlat(NTable::TDatabase &db, TVector<TLogoBlobID>& trashBeingCommitted);
    void Erase(const TString &key) override;
    void Update(const TString &key, const TString &value) override;
    void AddTrash(const TLogoBlobID& id) override;
};

} // NKeyValue
} // NKikimr
