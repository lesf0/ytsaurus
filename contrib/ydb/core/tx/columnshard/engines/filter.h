#pragma once

#include "defs.h"

#include <contrib/ydb/core/formats/arrow/arrow_filter.h>
#include <contrib/ydb/core/tx/columnshard/common/snapshot.h>

#include <contrib/ydb/library/formats/arrow/replace_key.h>

namespace NKikimr::NOlap {

NArrow::TColumnFilter MakeSnapshotFilter(const std::shared_ptr<arrow::RecordBatch>& batch, const TSnapshot& snapshot);
NArrow::TColumnFilter MakeSnapshotFilter(const std::shared_ptr<arrow::Table>& batch, const TSnapshot& snapshot);

struct TReadMetadata;
}   // namespace NKikimr::NOlap
