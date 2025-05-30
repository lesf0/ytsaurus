#include "service_table.h"
#include <contrib/ydb/core/grpc_services/base/base.h>
#include <contrib/ydb/core/grpc_services/grpc_integrity_trails.h>
#include "rpc_kqp_base.h"
#include "rpc_common/rpc_common.h"
#include "service_table.h"
#include "audit_dml_operations.h"

#include <contrib/ydb/core/grpc_services/base/base.h>
#include <contrib/ydb/public/api/protos/ydb_scheme.pb.h>
#include <contrib/ydb/core/base/counters.h>
#include <contrib/ydb/core/protos/console_config.pb.h>
#include <contrib/ydb/core/ydb_convert/ydb_convert.h>
#include <contrib/ydb/core/protos/query_stats.pb.h>
#include <contrib/ydb/public/sdk/cpp/include/ydb-cpp-sdk/library/operation_id/operation_id.h>

#include <contrib/ydb/core/kqp/executer_actor/kqp_executer.h>

#include <yql/essentials/public/issue/yql_issue.h>

namespace NKikimr {
namespace NGRpcService {

using namespace NActors;
using namespace NOperationId;
using namespace Ydb;
using namespace Ydb::Table;
using namespace NKqp;

bool NeedCollectDiagnostics(const Ydb::Table::ExecuteDataQueryRequest& req) {
    switch (req.collect_stats()) {
        case Ydb::Table::QueryStatsCollection::STATS_COLLECTION_FULL:
        case Ydb::Table::QueryStatsCollection::STATS_COLLECTION_PROFILE:
            return true;
        default:
            return false;
    }
}

using TEvExecuteDataQueryRequest = TGrpcRequestOperationCall<Ydb::Table::ExecuteDataQueryRequest,
    Ydb::Table::ExecuteDataQueryResponse>;

class TExecuteDataQueryRPC : public TRpcKqpRequestActor<TExecuteDataQueryRPC, TEvExecuteDataQueryRequest> {
    using TBase = TRpcKqpRequestActor<TExecuteDataQueryRPC, TEvExecuteDataQueryRequest>;

public:
    using TResult = Ydb::Table::ExecuteQueryResult;

    TExecuteDataQueryRPC(IRequestOpCtx* msg)
        : TBase(msg) {}

    void Bootstrap(const TActorContext &ctx) {
        TBase::Bootstrap(ctx);

        this->Become(&TExecuteDataQueryRPC::StateWork);
        Proceed(ctx);
    }

    void StateWork(TAutoPtr<IEventHandle>& ev) {
        switch (ev->GetTypeRewrite()) {
            HFunc(NKqp::TEvKqp::TEvQueryResponse, Handle);
            IgnoreFunc(NKqp::TEvKqpExecuter::TEvExecuterProgress);
            default: TBase::StateWork(ev);
        }
    }

    void Proceed(const TActorContext& ctx) {
        const auto req = GetProtoRequest();
        const auto traceId = Request_->GetTraceId();
        const auto requestType = Request_->GetRequestType();

        AuditContextAppend(Request_.get(), *req);
        NDataIntegrity::LogIntegrityTrails(traceId, *req, ctx);

        if (!CheckSession(req->session_id(), Request_.get())) {
            return Reply(Ydb::StatusIds::BAD_REQUEST, ctx);
        }

        if (!req->has_tx_control()) {
            NYql::TIssues issues;
            issues.AddIssue(MakeIssue(NKikimrIssues::TIssuesIds::DEFAULT_ERROR, "Empty tx_control."));
            return Reply(Ydb::StatusIds::BAD_REQUEST, issues, ctx);
        }

        if (req->tx_control().has_begin_tx() && !req->tx_control().commit_tx()) {
            switch (req->tx_control().begin_tx().tx_mode_case()) {
                case Table::TransactionSettings::kOnlineReadOnly:
                case Table::TransactionSettings::kStaleReadOnly: {
                    NYql::TIssues issues;
                    issues.AddIssue(MakeIssue(NKikimrIssues::TIssuesIds::DEFAULT_ERROR, TStringBuilder()
                        << "Failed to execute query: open transactions not supported for transaction mode: "
                        << GetTransactionModeName(req->tx_control().begin_tx())
                        << ", use commit_tx flag to explicitely commit transaction."));
                    return Reply(Ydb::StatusIds::BAD_REQUEST, issues, ctx);
                }
                default:
                    break;
            }
        }

        auto& query = req->query();
        TString yqlText;
        TString queryId;
        NKikimrKqp::EQueryAction queryAction;
        NKikimrKqp::EQueryType queryType;

        switch (query.query_case()) {
            case Ydb::Table::Query::kYqlText: {
                NYql::TIssues issues;
                if (!CheckQuery(query.yql_text(), issues)) {
                    return Reply(Ydb::StatusIds::BAD_REQUEST, issues, ctx);
                }
                queryAction = NKikimrKqp::QUERY_ACTION_EXECUTE;
                queryType = NKikimrKqp::QUERY_TYPE_SQL_DML;
                yqlText = query.yql_text();
                break;
            }

            case Ydb::Table::Query::kId: {
                if (query.id().empty()) {
                    NYql::TIssues issues;
                    issues.AddIssue(MakeIssue(NKikimrIssues::TIssuesIds::DEFAULT_ERROR, "Empty query id"));
                    return Reply(Ydb::StatusIds::BAD_REQUEST, issues, ctx);
                }

                try {
                    queryId = DecodePreparedQueryId(query.id());
                } catch (const std::exception& ex) {
                    NYql::TIssues issues;
                    issues.AddIssue(NYql::ExceptionToIssue(ex));
                    return Reply(Ydb::StatusIds::BAD_REQUEST, issues, ctx);
                }

                queryAction = NKikimrKqp::QUERY_ACTION_EXECUTE_PREPARED;
                queryType = NKikimrKqp::QUERY_TYPE_PREPARED_DML;
                break;
            }

            default: {
                NYql::TIssues issues;
                issues.AddIssue(MakeIssue(NKikimrIssues::TIssuesIds::DEFAULT_ERROR, "Unexpected query option"));
                return Reply(Ydb::StatusIds::BAD_REQUEST, issues, ctx);
            }
        }

        auto ev = MakeHolder<NKqp::TEvKqp::TEvQueryRequest>(
            queryAction,
            queryType,
            SelfId(),
            Request_,
            req->session_id(),
            std::move(yqlText),
            std::move(queryId),
            &req->tx_control(),
            &req->parameters(),
            req->collect_stats(),
            req->has_query_cache_policy() ? &req->query_cache_policy() : nullptr,
            req->has_operation_params() ? &req->operation_params() : nullptr);

        ev->Record.MutableRequest()->SetCollectDiagnostics(NeedCollectDiagnostics(*req));

        ReportCostInfo_ = req->operation_params().report_cost_info() == Ydb::FeatureFlag::ENABLED;

        ctx.Send(NKqp::MakeKqpProxyID(ctx.SelfID.NodeId()), ev.Release(), 0, 0, Span_.GetTraceId());
    }

    static void ConvertReadStats(const NKikimrQueryStats::TReadOpStats& from, Ydb::TableStats::OperationStats* to) {
        to->set_rows(to->rows() + from.GetRows());
        to->set_bytes(to->bytes() + from.GetBytes());
    }

    static void ConvertWriteStats(const NKikimrQueryStats::TWriteOpStats& from, Ydb::TableStats::OperationStats* to) {
        to->set_rows(from.GetCount());
        to->set_bytes(from.GetBytes());
    }

    static void ConvertQueryStats(const NKikimrKqp::TQueryResponse& from, Ydb::Table::ExecuteQueryResult* to) {
        if (from.HasQueryStats()) {
            FillQueryStats(*to->mutable_query_stats(), from);
            to->mutable_query_stats()->set_query_ast(from.GetQueryAst());
            if (from.HasQueryDiagnostics()) {
                to->mutable_query_stats()->set_query_meta(from.GetQueryDiagnostics());
            }
            return;
        }
    }

    void Handle(NKqp::TEvKqp::TEvQueryResponse::TPtr& ev, const TActorContext& ctx) {
        NDataIntegrity::LogIntegrityTrails(Request_->GetTraceId(), *GetProtoRequest(), ev, ctx);

        auto& record = ev->Get()->Record;
        SetCost(record.GetConsumedRu());
        AddServerHintsIfAny(record);

        if (record.GetYdbStatus() == Ydb::StatusIds::SUCCESS) {
            const auto& kqpResponse = record.GetResponse();
            const auto& issueMessage = kqpResponse.GetQueryIssues();
            auto queryResult = ev->Get()->Arena->Allocate<Ydb::Table::ExecuteQueryResult>();

            try {
                if (kqpResponse.GetYdbResults().size()) {
                    queryResult->mutable_result_sets()->Swap(record.MutableResponse()->MutableYdbResults());
                }

                ConvertQueryStats(kqpResponse, queryResult);
                if (kqpResponse.HasTxMeta()) {
                    queryResult->mutable_tx_meta()->CopyFrom(kqpResponse.GetTxMeta());
                }
                if (!kqpResponse.GetPreparedQuery().empty()) {
                    auto& queryMeta = *queryResult->mutable_query_meta();

                    queryMeta.set_id(FormatPreparedQueryIdCompat(kqpResponse.GetPreparedQuery()));

                    const auto& queryParameters = kqpResponse.GetQueryParameters();
                    for (const auto& queryParameter: queryParameters) {
                        Ydb::Type parameterType;
                        ConvertMiniKQLTypeToYdbType(queryParameter.GetType(), parameterType);
                        queryMeta.mutable_parameters_types()->insert({queryParameter.GetName(), parameterType});
                    }
                }
            } catch (const std::exception& ex) {
                NYql::TIssues issues;
                issues.AddIssue(NYql::ExceptionToIssue(ex));
                return Reply(Ydb::StatusIds::INTERNAL_ERROR, issues, ctx);
            }

            AuditContextAppend(Request_.get(), *GetProtoRequest(), *queryResult);

            ReplyWithResult(Ydb::StatusIds::SUCCESS, issueMessage, *queryResult, ctx);
        } else {
            return OnQueryResponseErrorWithTxMeta(record, ctx);
        }
    }
};

void DoExecuteDataQueryRequest(std::unique_ptr<IRequestOpCtx> p, const IFacilityProvider& provider) {
    provider.RegisterActor(new TExecuteDataQueryRPC(p.release()));
}

template<>
IActor* TEvExecuteDataQueryRequest::CreateRpcActor(NKikimr::NGRpcService::IRequestOpCtx* msg) {
    return new TExecuteDataQueryRPC(msg);
}

} // namespace NGRpcService
} // namespace NKikimr
