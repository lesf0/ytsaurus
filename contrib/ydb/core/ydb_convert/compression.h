#pragma once

#include <contrib/ydb/core/protos/flat_scheme_op.pb.h>
#include <contrib/ydb/public/api/protos/ydb_status_codes.pb.h>

namespace NKikimr {

bool CheckCompression(const TString& in, Ydb::StatusIds::StatusCode& status, TString& error);
bool FillCompression(NKikimrSchemeOp::TBackupTask::TCompressionOptions& out, const TString& in);

} // NKikimr
