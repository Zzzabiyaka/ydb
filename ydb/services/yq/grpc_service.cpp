#include "grpc_service.h"

#include <ydb/core/grpc_services/grpc_helper.h>
#include <ydb/core/grpc_services/grpc_request_proxy.h>
#include <ydb/core/grpc_services/rpc_calls.h>
#include <ydb/core/grpc_services/service_yq.h>
#include <ydb/library/protobuf_printer/security_printer.h>

namespace NKikimr::NGRpcService {

TGRpcYandexQueryService::TGRpcYandexQueryService(NActors::TActorSystem *system,
    TIntrusivePtr<NMonitoring::TDynamicCounters> counters, NActors::TActorId id)
    : ActorSystem_(system)
    , Counters_(counters)
    , GRpcRequestProxyId_(id) {}

void TGRpcYandexQueryService::InitService(grpc::ServerCompletionQueue *cq, NGrpc::TLoggerPtr logger) {
    CQ_ = cq;
    SetupIncomingRequests(std::move(logger));
}

void TGRpcYandexQueryService::SetGlobalLimiterHandle(NGrpc::TGlobalLimiter* limiter) {
    Limiter_ = limiter;
}

bool TGRpcYandexQueryService::IncRequest() {
    return Limiter_->Inc();
}

void TGRpcYandexQueryService::DecRequest() {
    Limiter_->Dec();
    Y_ASSERT(Limiter_->GetCurrentInFlight() >= 0);
}

void TGRpcYandexQueryService::SetupIncomingRequests(NGrpc::TLoggerPtr logger) {
    auto getCounterBlock = CreateCounterCb(Counters_, ActorSystem_);

    using NPerms = NKikimr::TEvTicketParser::TEvAuthorizeTicket;

    static const std::function CreateQueryPermissions{[](const YandexQuery::CreateQueryRequest& request) {
        TVector<NPerms::TPermission> permissions{
            NPerms::Required("yq.queries.create"),
            NPerms::Optional("yq.connections.use"),
            NPerms::Optional("yq.bindings.use")
        };
        if (request.execute_mode() != YandexQuery::SAVE) {
            permissions.push_back(NPerms::Required("yq.queries.invoke"));
        }
        if (request.content().acl().visibility() == YandexQuery::Acl::SCOPE) {
            permissions.push_back(NPerms::Required("yq.resources.managePublic"));
        }
        return permissions;
    }};

    static const std::function ListQueriesPermissions{[](const YandexQuery::ListQueriesRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.queries.get"),
            NPerms::Optional("yq.resources.viewPublic"),
            NPerms::Optional("yq.resources.viewPrivate")
        };
    }};

    static const std::function DescribeQueryPermissions{[](const YandexQuery::DescribeQueryRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.queries.get"),
            NPerms::Optional("yq.queries.viewAst"),
            NPerms::Optional("yq.resources.viewPublic"),
            NPerms::Optional("yq.resources.viewPrivate")
        };
    }};

    static const std::function GetQueryStatusPermissions{[](const YandexQuery::GetQueryStatusRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.queries.getStatus"),
            NPerms::Optional("yq.resources.viewPublic"),
            NPerms::Optional("yq.resources.viewPrivate")
        };
    }};

    static const std::function ModifyQueryPermissions{[](const YandexQuery::ModifyQueryRequest& request) {
        TVector<NPerms::TPermission> permissions{
            NPerms::Required("yq.queries.update"),
            NPerms::Optional("yq.connections.use"),
            NPerms::Optional("yq.bindings.use"),
            NPerms::Optional("yq.resources.managePrivate")
        };
        if (request.execute_mode() != YandexQuery::SAVE) {
            permissions.push_back(NPerms::Required("yq.queries.invoke"));
        }
        if (request.content().acl().visibility() == YandexQuery::Acl::SCOPE) {
            permissions.push_back(NPerms::Required("yq.resources.managePublic"));
        }
        return permissions;
    }};

    static const std::function DeleteQueryPermissions{[](const YandexQuery::DeleteQueryRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.queries.delete"),
            NPerms::Optional("yq.resources.managePublic"),
            NPerms::Optional("yq.resources.managePrivate")
        };
    }};

    static const std::function ControlQueryPermissions{[](const YandexQuery::ControlQueryRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.queries.control"),
            NPerms::Optional("yq.resources.managePublic"),
            NPerms::Optional("yq.resources.managePrivate")
        };
    }};

    static const std::function GetResultDataPermissions{[](const YandexQuery::GetResultDataRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.queries.getData"),
            NPerms::Optional("yq.resources.viewPublic"),
            NPerms::Optional("yq.resources.viewPrivate")
        };
    }};

    static const std::function ListJobsPermissions{[](const YandexQuery::ListJobsRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.jobs.get"),
            NPerms::Optional("yq.resources.viewPublic"),
            NPerms::Optional("yq.resources.viewPrivate")
        };
    }};

    static const std::function DescribeJobPermissions{[](const YandexQuery::DescribeJobRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.jobs.get"),
            NPerms::Optional("yq.resources.viewPublic"),
            NPerms::Optional("yq.resources.viewPrivate")
        };
    }};

    static const std::function CreateConnectionPermissions{[](const YandexQuery::CreateConnectionRequest& request) {
        TVector<NPerms::TPermission> permissions{
            NPerms::Required("yq.connections.create"),
        };
        if (request.content().acl().visibility() == YandexQuery::Acl::SCOPE) {
            permissions.push_back(NPerms::Required("yq.resources.managePublic"));
        }
        return permissions;
    }};

    static const std::function ListConnectionsPermissions{[](const YandexQuery::ListConnectionsRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.connections.get"),
            NPerms::Optional("yq.resources.viewPublic"),
            NPerms::Optional("yq.resources.viewPrivate")
        };
    }};

    static const std::function DescribeConnectionPermissions{[](const YandexQuery::DescribeConnectionRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.connections.get"),
            NPerms::Optional("yq.resources.viewPublic"),
            NPerms::Optional("yq.resources.viewPrivate")
        };
    }};

    static const std::function ModifyConnectionPermissions{[](const YandexQuery::ModifyConnectionRequest& request) {
        TVector<NPerms::TPermission> permissions{
            NPerms::Required("yq.connections.update"),
            NPerms::Optional("yq.resources.managePrivate")
        };
        if (request.content().acl().visibility() == YandexQuery::Acl::SCOPE) {
            permissions.push_back(NPerms::Required("yq.resources.managePublic"));
        }
        return permissions;
    }};

    static const std::function DeleteConnectionPermissions{[](const YandexQuery::DeleteConnectionRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.connections.delete"),
            NPerms::Optional("yq.resources.managePublic"),
            NPerms::Optional("yq.resources.managePrivate")
        };
    }};

    static const std::function TestConnectionPermissions{[](const YandexQuery::TestConnectionRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.connections.create")
        };
    }};

    static const std::function CreateBindingPermissions{[](const YandexQuery::CreateBindingRequest& request) {
        TVector<NPerms::TPermission> permissions{
            NPerms::Required("yq.bindings.create"),
        };
        if (request.content().acl().visibility() == YandexQuery::Acl::SCOPE) {
            permissions.push_back(NPerms::Required("yq.resources.managePublic"));
        }
        return permissions;
    }};

    static const std::function ListBindingsPermissions{[](const YandexQuery::ListBindingsRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.bindings.get"),
            NPerms::Optional("yq.resources.viewPublic"),
            NPerms::Optional("yq.resources.viewPrivate")
        };
    }};

    static const std::function DescribeBindingPermissions{[](const YandexQuery::DescribeBindingRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.bindings.get"),
            NPerms::Optional("yq.resources.viewPublic"),
            NPerms::Optional("yq.resources.viewPrivate")
        };
    }};

    static const std::function ModifyBindingPermissions{[](const YandexQuery::ModifyBindingRequest& request) {
        TVector<NPerms::TPermission> permissions{
            NPerms::Required("yq.bindings.update"),
            NPerms::Optional("yq.resources.managePrivate")
        };
        if (request.content().acl().visibility() == YandexQuery::Acl::SCOPE) {
            permissions.push_back(NPerms::Required("yq.resources.managePublic"));
        }
        return permissions;
    }};

    static const std::function DeleteBindingPermissions{[](const YandexQuery::DeleteBindingRequest&) -> TVector<NPerms::TPermission> {
        return {
            NPerms::Required("yq.bindings.delete"),
            NPerms::Optional("yq.resources.managePublic"),
            NPerms::Optional("yq.resources.managePrivate")
        };
    }};

#ifdef ADD_REQUEST
#error ADD_REQUEST macro already defined
#endif
#define ADD_REQUEST(NAME, CB, PERMISSIONS)                                                                                  \
MakeIntrusive<TGRpcRequest<YandexQuery::NAME##Request, YandexQuery::NAME##Response, TGRpcYandexQueryService, TSecurityTextFormatPrinter<YandexQuery::NAME##Request>, TSecurityTextFormatPrinter<YandexQuery::NAME##Response>>>( \
    this, &Service_, CQ_,                                                                                      \
    [this](NGrpc::IRequestContextBase *ctx) {                                                                  \
        NGRpcService::ReportGrpcReqToMon(*ActorSystem_, ctx->GetPeer());                                       \
        ActorSystem_->Send(GRpcRequestProxyId_,                                                                \
            new TGrpcYqRequestOperationCall<YandexQuery::NAME##Request, YandexQuery::NAME##Response>                 \
                (ctx, &CB, PERMISSIONS));                                                                                   \
    },                                                                                                         \
    &YandexQuery::V1::YandexQueryService::AsyncService::Request##NAME,                                  \
    #NAME, logger, getCounterBlock("yq", #NAME))                                                     \
    ->Run();                                                                                                   \

    ADD_REQUEST(CreateQuery, DoYandexQueryCreateQueryRequest, CreateQueryPermissions)
    ADD_REQUEST(ListQueries, DoYandexQueryListQueriesRequest, ListQueriesPermissions)
    ADD_REQUEST(DescribeQuery, DoYandexQueryDescribeQueryRequest, DescribeQueryPermissions)
    ADD_REQUEST(GetQueryStatus, DoYandexQueryGetQueryStatusRequest, GetQueryStatusPermissions)
    ADD_REQUEST(ModifyQuery, DoYandexQueryModifyQueryRequest, ModifyQueryPermissions)
    ADD_REQUEST(DeleteQuery, DoYandexQueryDeleteQueryRequest, DeleteQueryPermissions)
    ADD_REQUEST(ControlQuery, DoYandexQueryControlQueryRequest, ControlQueryPermissions)
    ADD_REQUEST(GetResultData, DoGetResultDataRequest, GetResultDataPermissions)
    ADD_REQUEST(ListJobs, DoListJobsRequest, ListJobsPermissions)
    ADD_REQUEST(DescribeJob, DoDescribeJobRequest, DescribeJobPermissions)
    ADD_REQUEST(CreateConnection, DoCreateConnectionRequest, CreateConnectionPermissions)
    ADD_REQUEST(ListConnections, DoListConnectionsRequest, ListConnectionsPermissions)
    ADD_REQUEST(DescribeConnection, DoDescribeConnectionRequest, DescribeConnectionPermissions)
    ADD_REQUEST(ModifyConnection, DoModifyConnectionRequest, ModifyConnectionPermissions)
    ADD_REQUEST(DeleteConnection, DoDeleteConnectionRequest, DeleteConnectionPermissions)
    ADD_REQUEST(TestConnection, DoTestConnectionRequest, TestConnectionPermissions)
    ADD_REQUEST(CreateBinding, DoCreateBindingRequest, CreateBindingPermissions)
    ADD_REQUEST(ListBindings, DoListBindingsRequest, ListBindingsPermissions)
    ADD_REQUEST(DescribeBinding, DoDescribeBindingRequest, DescribeBindingPermissions)
    ADD_REQUEST(ModifyBinding, DoModifyBindingRequest, ModifyBindingPermissions)
    ADD_REQUEST(DeleteBinding, DoDeleteBindingRequest, DeleteBindingPermissions)

#undef ADD_REQUEST

}

} // namespace NKikimr::NGRpcService
