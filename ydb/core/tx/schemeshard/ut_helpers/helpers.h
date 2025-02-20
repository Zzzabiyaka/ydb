#pragma once

#include "ls_checks.h"
#include "test_env.h"

#include <library/cpp/testing/unittest/registar.h>

#include <ydb/core/engine/mkql_engine_flat.h>
#include <ydb/core/protos/tx_datashard.pb.h>
#include <ydb/core/testlib/minikql_compile.h>
#include <ydb/core/tx/datashard/datashard.h>
#include <ydb/core/tx/schemeshard/schemeshard_build_index.h>
#include <ydb/core/tx/schemeshard/schemeshard_export.h>
#include <ydb/core/tx/schemeshard/schemeshard_import.h>
#include <ydb/core/tx/schemeshard/schemeshard_types.h>
#include <ydb/library/yql/minikql/mkql_alloc.h>
#include <ydb/library/yql/minikql/mkql_node_serialization.h>

#include <util/stream/null.h>

#include <functional>

#undef Cdbg
#if 1
    #define Cdbg Cerr
#else
    #define Cdbg Cnull
#endif

namespace NSchemeShardUT_Private {
    using namespace NKikimr;

    using TEvTx = TEvSchemeShard::TEvModifySchemeTransaction;

    ////////// tablet
    NKikimrProto::EReplyStatus LocalMiniKQL(TTestActorRuntime& runtime, ui64 tabletId, const TString& query, NKikimrMiniKQL::TResult& result, TString& err);
    NKikimrMiniKQL::TResult LocalMiniKQL(TTestActorRuntime& runtime, ui64 tabletId, const TString& query);
    NKikimrProto::EReplyStatus LocalSchemeTx(TTestActorRuntime& runtime, ui64 tabletId, const TString& schemeChangesStr, bool dryRun, NTabletFlatScheme::TSchemeChanges& scheme, TString& err);

    bool CheckLocalRowExists(TTestActorRuntime& runtime, ui64 tabletId, const TString& tableName, const TString& keyColumn, ui64 keyValue);

    ////////// describe options
    struct TDescribeOptionsBuilder : public NKikimrSchemeOp::TDescribeOptions {
        TDescribeOptionsBuilder& SetReturnPartitioningInfo(bool value) { TDescribeOptions::SetReturnPartitioningInfo(value); return *this; }
        TDescribeOptionsBuilder& SetReturnPartitionConfig(bool value) { TDescribeOptions::SetReturnPartitionConfig(value); return *this; }
        TDescribeOptionsBuilder& SetBackupInfo(bool value) { TDescribeOptions::SetBackupInfo(value); return *this; }
        TDescribeOptionsBuilder& SetReturnBoundaries(bool value) { TDescribeOptions::SetReturnBoundaries(value); return *this; }
        TDescribeOptionsBuilder& SetShowPrivateTable(bool value) { TDescribeOptions::SetShowPrivateTable(value); return *this; }
    };

    ////////// describe
    NKikimrScheme::TEvDescribeSchemeResult DescribePath(TTestActorRuntime& runtime, ui64 schemeShard, const TString& path, const NKikimrSchemeOp::TDescribeOptions& opts);
    NKikimrScheme::TEvDescribeSchemeResult DescribePath(TTestActorRuntime& runtime, const TString& path, const NKikimrSchemeOp::TDescribeOptions& opts);
    NKikimrScheme::TEvDescribeSchemeResult DescribePrivatePath(TTestActorRuntime& runtime, ui64 schemeShard, const TString& path, bool returnPartitioning = false, bool returnBoundaries = false);
    NKikimrScheme::TEvDescribeSchemeResult DescribePrivatePath(TTestActorRuntime& runtime, const TString& path, bool returnPartitioning = false, bool returnBoundaries = false);
    NKikimrScheme::TEvDescribeSchemeResult DescribePath(TTestActorRuntime& runtime, ui64 schemeShard, const TString& path, bool returnPartitioning = false, bool returnBoundaries = false, bool showPrivate = false, bool returnBackups = false);
    NKikimrScheme::TEvDescribeSchemeResult DescribePath(TTestActorRuntime& runtime, const TString& path, bool returnPartitioning = false, bool returnBoundaries = false, bool showPrivate = false, bool returnBackups = false);
    TPathVersion ExtructPathVersion(const NKikimrScheme::TEvDescribeSchemeResult& describe);
    TPathVersion TestDescribeResult(const NKikimrScheme::TEvDescribeSchemeResult& describe, TVector<NLs::TCheckFunc> checks = {});

    TString TestDescribe(TTestActorRuntime& runtime, const TString& path);
    TString TestLs(TTestActorRuntime& runtime, const TString& path, bool returnPartitioningInfo = false, NLs::TCheckFunc check = nullptr);
    TString TestLs(TTestActorRuntime& runtime, const TString& path, const NKikimrSchemeOp::TDescribeOptions& opts, NLs::TCheckFunc check = nullptr);
    TString TestLsPathId(TTestActorRuntime& runtime, ui64 pathId, NLs::TCheckFunc check = nullptr);

    ////////// modification results
    void CheckExpected(const TVector<TEvSchemeShard::EStatus>& expected, TEvSchemeShard::EStatus result, const TString& reason);
    void CheckExpected(const TVector<Ydb::StatusIds::StatusCode>& expected, Ydb::StatusIds::StatusCode result, const TString& reason);
    void TestModificationResult(TTestActorRuntime& runtime, ui64 txId,TEvSchemeShard::EStatus expectedResult = NKikimrScheme::StatusAccepted);
    ui64 TestModificationResults(TTestActorRuntime& runtime, ui64 txId, const TVector<TEvSchemeShard::EStatus>& expectedResults);
    void SkipModificationReply(TTestActorRuntime& runtime, ui32 num = 1);

    TEvTx* CombineSchemeTransactions(const TVector<TEvTx*>& transactions);
    void AsyncSendTransaction(TTestActorRuntime& runtime, ui64 schemeShard, TEvTx* evTx);

    ////////// generic

    #define UT_GENERIC_PARAMS \
        const TString& parentPath, const TString& scheme
    #define UT_PARAMS_BY_PATH_ID \
        ui64 pathId

    #define DEFINE_HELPERS(name, params, ...) \
        TEvTx* name##Request(ui64 schemeShardId, ui64 txId, params, __VA_ARGS__); \
        TEvTx* name##Request(ui64 txId, params, __VA_ARGS__); \
        void Async##name(TTestActorRuntime& runtime, ui64 schemeShardId, ui64 txId, params, __VA_ARGS__); \
        void Async##name(TTestActorRuntime& runtime, ui64 txId, params, __VA_ARGS__); \
        ui64 Test##name(TTestActorRuntime& runtime, ui64 schemeShardId, ui64 txId, params, \
            const TVector<TEvSchemeShard::EStatus>& expectedResults = {NKikimrScheme::StatusAccepted}, __VA_ARGS__); \
        ui64 Test##name(TTestActorRuntime& runtime, ui64 txId, params, \
            const TVector<TEvSchemeShard::EStatus>& expectedResults = {NKikimrScheme::StatusAccepted}, __VA_ARGS__)

    #define GENERIC_HELPERS(name) DEFINE_HELPERS(name, UT_GENERIC_PARAMS, const TApplyIf& applyIf = {})
    #define GENERIC_WITH_ATTRS_HELPERS(name) DEFINE_HELPERS(name, UT_GENERIC_PARAMS, const NKikimrSchemeOp::TAlterUserAttributes& userAttrs = {}, const TApplyIf& applyIf = {})
    #define DROP_BY_PATH_ID_HELPERS(name) DEFINE_HELPERS(name, UT_PARAMS_BY_PATH_ID, const TApplyIf& applyIf = {})

    // subdomain
    GENERIC_WITH_ATTRS_HELPERS(CreateSubDomain);
    GENERIC_HELPERS(AlterSubDomain);
    GENERIC_HELPERS(DropSubDomain);
    GENERIC_HELPERS(ForceDropSubDomain);
    ui64 TestCreateSubDomain(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& scheme,
        const NKikimrSchemeOp::TAlterUserAttributes& userAttrs);

    // ext subdomain
    GENERIC_WITH_ATTRS_HELPERS(CreateExtSubDomain);
    GENERIC_WITH_ATTRS_HELPERS(AlterExtSubDomain);
    GENERIC_HELPERS(ForceDropExtSubDomain);
    ui64 TestCreateExtSubDomain(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& scheme,
        const NKikimrSchemeOp::TAlterUserAttributes& userAttrs);

    // dir
    GENERIC_WITH_ATTRS_HELPERS(MkDir);
    GENERIC_HELPERS(RmDir);
    DROP_BY_PATH_ID_HELPERS(ForceDropUnsafe);

    // user attrs
    GENERIC_WITH_ATTRS_HELPERS(UserAttrs);
    NKikimrSchemeOp::TAlterUserAttributes AlterUserAttrs(const TVector<std::pair<TString, TString>>& add, const TVector<TString>& drop = {});
    ui64 TestUserAttrs(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& name,
        const NKikimrSchemeOp::TAlterUserAttributes& userAttrs);

    // table
    GENERIC_WITH_ATTRS_HELPERS(CreateTable);
    GENERIC_HELPERS(CreateIndexedTable);
    GENERIC_HELPERS(ConsistentCopyTables);
    GENERIC_HELPERS(AlterTable);
    GENERIC_HELPERS(SplitTable);
    GENERIC_HELPERS(DropTable);
    DROP_BY_PATH_ID_HELPERS(DropTable);
    GENERIC_HELPERS(DropTableIndex);

    // backup & restore
    GENERIC_HELPERS(BackupTable);
    GENERIC_HELPERS(Restore);

    // cdc stream
    GENERIC_HELPERS(CreateCdcStream);
    GENERIC_HELPERS(AlterCdcStream);
    GENERIC_HELPERS(DropCdcStream);

    // olap store
    GENERIC_HELPERS(CreateOlapStore);
    GENERIC_HELPERS(AlterOlapStore);
    GENERIC_HELPERS(DropOlapStore);
    DROP_BY_PATH_ID_HELPERS(DropOlapStore);

    // olap table
    GENERIC_HELPERS(CreateOlapTable);
    GENERIC_HELPERS(AlterOlapTable);
    GENERIC_HELPERS(DropOlapTable);
    DROP_BY_PATH_ID_HELPERS(DropOlapTable);

    // sequence
    GENERIC_HELPERS(CreateSequence);
    GENERIC_HELPERS(DropSequence);
    DROP_BY_PATH_ID_HELPERS(DropSequence);

    // replication
    GENERIC_HELPERS(CreateReplication);
    GENERIC_HELPERS(DropReplication);
    DROP_BY_PATH_ID_HELPERS(DropReplication);

    // pq
    GENERIC_HELPERS(CreatePQGroup);
    GENERIC_HELPERS(AlterPQGroup);
    GENERIC_HELPERS(DropPQGroup);
    DROP_BY_PATH_ID_HELPERS(DropPQGroup);

    // rtmr
    GENERIC_HELPERS(CreateRtmrVolume);

    // solomon
    GENERIC_HELPERS(CreateSolomon);
    GENERIC_HELPERS(AlterSolomon);
    GENERIC_HELPERS(DropSolomon);
    DROP_BY_PATH_ID_HELPERS(DropSolomon);
    NKikimrSchemeOp::TCreateSolomonVolume TakeTabletsFromAnotherSolomonVol(TString name, TString ls, ui32 count = 0);

    // kesus
    GENERIC_HELPERS(CreateKesus);
    GENERIC_HELPERS(AlterKesus);
    GENERIC_HELPERS(DropKesus);
    DROP_BY_PATH_ID_HELPERS(DropKesus);

    // filestore
    GENERIC_HELPERS(CreateFileStore);
    GENERIC_HELPERS(AlterFileStore);
    GENERIC_HELPERS(DropFileStore);
    DROP_BY_PATH_ID_HELPERS(DropFileStore);

    // nbs
    GENERIC_HELPERS(CreateBlockStoreVolume);
    GENERIC_HELPERS(AlterBlockStoreVolume);
    GENERIC_HELPERS(DropBlockStoreVolume);
    DROP_BY_PATH_ID_HELPERS(DropBlockStoreVolume);
    void AsyncAssignBlockStoreVolume(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& name, const TString& mountToken, ui64 tokenVersion = 0);
    void TestAssignBlockStoreVolume(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& name, const TString& mountToken, ui64 tokenVersion = 0, const TVector<TEvSchemeShard::EStatus>& expectedResults = {NKikimrScheme::StatusSuccess});

    #undef DROP_BY_PATH_ID_HELPERS
    #undef GENERIC_WITH_ATTRS_HELPERS
    #undef GENERIC_HELPERS
    #undef DEFINE_HELPERS
    #undef UT_PARAMS_BY_PATH_ID
    #undef UT_GENERIC_PARAMS

    ////////// non-generic

    // cancel tx
    TEvSchemeShard::TEvCancelTx* CancelTxRequest(ui64 txId, ui64 targetTxId);
    void AsyncCancelTxTable(TTestActorRuntime& runtime, ui64 txId, ui64 targetTxId);
    void TestCancelTxTable(TTestActorRuntime& runtime, ui64 txId, ui64 targetTxId,
                               const TVector<TEvSchemeShard::EStatus>& expectedResults = {NKikimrScheme::StatusAccepted});

    // modify acl
    TEvTx* CreateModifyACLRequest(ui64 txId, TString parentPath, TString name, const TString& diffAcl, const TString& newOwner);
    void AsyncModifyACL(TTestActorRuntime& runtime, ui64 txId, TString parentPath, TString name, const TString& diffAcl, const TString& newOwner);
    void AsyncModifyACL(TTestActorRuntime& runtime, ui64 schemeShard, ui64 txId, TString parentPath, TString name, const TString& diffAcl, const TString& newOwner);
    void TestModifyACL(TTestActorRuntime& runtime, ui64 txId, TString parentPath, TString name, const TString& diffAcl, const TString& newOwner, TEvSchemeShard::EStatus expectedResult = NKikimrScheme::StatusSuccess);
    void TestModifyACL(TTestActorRuntime& runtime, ui64 schemeShard, ui64 txId, TString parentPath, TString name, const TString& diffAcl, const TString& newOwner, TEvSchemeShard::EStatus expectedResult = NKikimrScheme::StatusSuccess);

    // upgrade subdomain
    TEvTx* UpgradeSubDomainRequest(ui64 txId, const TString& parentPath, const TString& name);
    void AsyncUpgradeSubDomain(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& name);
    void TestUpgradeSubDomain(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& name, const TVector<TEvSchemeShard::EStatus>& expectedResults);
    void TestUpgradeSubDomain(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& name);

    TEvTx* UpgradeSubDomainDecisionRequest(ui64 txId, const TString& parentPath, const TString& name, NKikimrSchemeOp::TUpgradeSubDomain::EDecision taskType);
    void AsyncUpgradeSubDomainDecision(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& name, NKikimrSchemeOp::TUpgradeSubDomain::EDecision taskType);
    void TestUpgradeSubDomainDecision(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& name, const TVector<TEvSchemeShard::EStatus>& expectedResults, NKikimrSchemeOp::TUpgradeSubDomain::EDecision taskType);
    void TestUpgradeSubDomainDecision(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& name, NKikimrSchemeOp::TUpgradeSubDomain::EDecision taskType);

    // copy table
    TEvTx* CopyTableRequest(ui64 txId, const TString& dstPath, const TString& dstName, const TString& srcFullName, TApplyIf applyIf = {});
    void AsyncCopyTable(TTestActorRuntime& runtime, ui64 schemeShard, ui64 txId, const TString& dstPath, const TString& dstName, const TString& srcFullName);
    void AsyncCopyTable(TTestActorRuntime& runtime, ui64 txId, const TString& dstPath, const TString& dstName, const TString& srcFullName);
    void TestCopyTable(TTestActorRuntime& runtime, ui64 schemeShard, ui64 txId, const TString& dstPath, const TString& dstName, const TString& srcFullName, TEvSchemeShard::EStatus expectedResult = NKikimrScheme::StatusAccepted);
    void TestCopyTable(TTestActorRuntime& runtime, ui64 txId, const TString& dstPath, const TString& dstName, const TString& srcFullName, TEvSchemeShard::EStatus expectedResult = NKikimrScheme::StatusAccepted);

    // move table
    TEvTx* MoveTableRequest(ui64 txId, const TString& srcPath, const TString& dstPath, ui64 schemeShard = TTestTxConfig::SchemeShard, const TApplyIf& applyIf = {});
    void AsyncMoveTable(TTestActorRuntime& runtime, ui64 txId, const TString& srcPath, const TString& dstPath, ui64 schemeShard = TTestTxConfig::SchemeShard);
    void TestMoveTable(TTestActorRuntime& runtime, ui64 txId, const TString& srcMove, const TString& dstMove, const TVector<TEvSchemeShard::EStatus>& expectedResults = {NKikimrScheme::StatusAccepted});
    void TestMoveTable(TTestActorRuntime& runtime, ui64 schemeShard, ui64 txId, const TString& srcMove, const TString& dstMove, const TVector<TEvSchemeShard::EStatus>& expectedResults = {NKikimrScheme::StatusAccepted});

    // locks
    TEvTx* LockRequest(ui64 txId, const TString &parentPath, const TString& name);
    void AsyncLock(TTestActorRuntime& runtime, ui64 schemeShard, ui64 txId, const TString& parentPath, const TString& name);
    void AsyncLock(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& name);
    void TestLock(TTestActorRuntime& runtime, ui64 schemeShard, ui64 txId, const TString& parentPath, const TString& name,
                  const TVector<TEvSchemeShard::EStatus> expectedResults = {NKikimrScheme::StatusAccepted});
    void TestLock(TTestActorRuntime& runtime, ui64 txId, const TString& parentPath, const TString& name,
                  const TVector<TEvSchemeShard::EStatus> expectedResults = {NKikimrScheme::StatusAccepted});
    void AsyncUnkock(TTestActorRuntime& runtime, ui64 schemeShard, ui64 txId, ui64 lockId, const TString& parentPath, const TString& name);
    void AsyncUnkock(TTestActorRuntime& runtime, ui64 txId, ui64 lockId, const TString& parentPath, const TString& name);
    void TestUnlock(TTestActorRuntime& runtime, ui64 schemeShard, ui64 txId, ui64 lockId, const TString& parentPath, const TString& name,
                  const TVector<TEvSchemeShard::EStatus> expectedResults = {NKikimrScheme::StatusAccepted});
    void TestUnlock(TTestActorRuntime& runtime, ui64 txId, ui64 lockId, const TString& parentPath, const TString& name,
                  const TVector<TEvSchemeShard::EStatus> expectedResults = {NKikimrScheme::StatusAccepted});

    // index build
    struct TBuildIndexConfig {
        TString IndexName;
        NKikimrSchemeOp::EIndexType IndexType = NKikimrSchemeOp::EIndexTypeGlobal;
        TVector<TString> IndexColumns;
        TVector<TString> DataColumns;
    };

    TEvIndexBuilder::TEvCreateRequest* CreateBuildIndexRequest(ui64 id, const TString& dbName, const TString& src, const TBuildIndexConfig& cfg);
    void AsyncBuilIndex(TTestActorRuntime& runtime, ui64 id, ui64 schemeShard, const TString &dbName, const TString &src, const TBuildIndexConfig &cfg);
    void AsyncBuilIndex(TTestActorRuntime& runtime, ui64 id, ui64 schemeShard, const TString &dbName, const TString &src, const TString &name, TVector<TString> columns, TVector<TString> dataColumns = {});
    void TestBuilIndex(TTestActorRuntime& runtime, ui64 id, ui64 schemeShard, const TString &dbName, const TString &src, const TBuildIndexConfig &cfg, Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    void TestBuilIndex(TTestActorRuntime& runtime, ui64 id, ui64 schemeShard, const TString &dbName, const TString &src, const TString &name, TVector<TString> columns, Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    TEvIndexBuilder::TEvCancelRequest* CreateCancelBuildIndexRequest(const ui64 id, const TString& dbName, const ui64 buildIndexId);
    NKikimrIndexBuilder::TEvCancelResponse TestCancelBuildIndex(TTestActorRuntime& runtime, const ui64 id, const ui64 schemeShard, const TString &dbName, const ui64 buildIndexId, const TVector<Ydb::StatusIds::StatusCode>& expectedStatuses = {Ydb::StatusIds::SUCCESS});
    TEvIndexBuilder::TEvListRequest* ListBuildIndexRequest(const TString& dbName);
    NKikimrIndexBuilder::TEvListResponse TestListBuilIndex(TTestActorRuntime& runtime, ui64 schemeShard, const TString &dbName);
    TEvIndexBuilder::TEvGetRequest* GetBuildIndexRequest(const TString& dbName, ui64 id);
    NKikimrIndexBuilder::TEvGetResponse TestGetBuilIndex(TTestActorRuntime& runtime, ui64 schemeShard, const TString &dbName, ui64 id);
    TEvIndexBuilder::TEvForgetRequest* ForgetBuildIndexRequest(const ui64 id, const TString &dbName, const ui64 buildIndexId);
    NKikimrIndexBuilder::TEvForgetResponse TestForgetBuilIndex(TTestActorRuntime& runtime, const ui64 id, const ui64 schemeShard, const TString &dbName, const ui64 buildIndexId, Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);

    ////////// export
    void AsyncExport(TTestActorRuntime& runtime, ui64 schemeshardId, ui64 id, const TString& dbName, const TString& requestStr, const TString& userSID = "");
    void AsyncExport(TTestActorRuntime& runtime, ui64 id, const TString& dbName, const TString& requestStr, const TString& userSID = "");
    void TestExport(TTestActorRuntime& runtime, ui64 schemeshardId, ui64 id, const TString& dbName, const TString& requestStr, const TString& userSID = "",
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    void TestExport(TTestActorRuntime& runtime, ui64 id, const TString& dbName, const TString& requestStr, const TString& userSID = "",
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    NKikimrExport::TEvGetExportResponse TestGetExport(TTestActorRuntime& runtime, ui64 schemeshardId, ui64 id, const TString& dbName,
            const TVector<Ydb::StatusIds::StatusCode>& expectedStatuses);
    NKikimrExport::TEvGetExportResponse TestGetExport(TTestActorRuntime& runtime, ui64 id, const TString& dbName,
            const TVector<Ydb::StatusIds::StatusCode>& expectedStatuses);
    NKikimrExport::TEvGetExportResponse TestGetExport(TTestActorRuntime& runtime, ui64 schemeshardId, ui64 id, const TString& dbName,
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    NKikimrExport::TEvGetExportResponse TestGetExport(TTestActorRuntime& runtime, ui64 id, const TString& dbName,
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    TEvExport::TEvCancelExportRequest* CancelExportRequest(ui64 txId, const TString& dbName, ui64 exportId);
    NKikimrExport::TEvCancelExportResponse TestCancelExport(TTestActorRuntime& runtime, ui64 schemeshardId, ui64 txId, const TString& dbName, ui64 exportId,
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    NKikimrExport::TEvCancelExportResponse TestCancelExport(TTestActorRuntime& runtime, ui64 txId, const TString& dbName, ui64 exportId,
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    TEvExport::TEvForgetExportRequest* ForgetExportRequest(ui64 txId, const TString& dbName, ui64 exportId);
    void AsyncForgetExport(TTestActorRuntime& runtime, ui64 schemeshardId, ui64 txId, const TString& dbName, ui64 exportId);
    void AsyncForgetExport(TTestActorRuntime& runtime, ui64 txId, const TString& dbName, ui64 exportId);
    NKikimrExport::TEvForgetExportResponse TestForgetExport(TTestActorRuntime& runtime, ui64 schemeshardId, ui64 txId, const TString& dbName, ui64 exportId,
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    NKikimrExport::TEvForgetExportResponse TestForgetExport(TTestActorRuntime& runtime, ui64 txId, const TString& dbName, ui64 exportId,
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);

    ////////// import
    void AsyncImport(TTestActorRuntime& runtime, ui64 schemeshardId, ui64 id, const TString& dbName, const TString& requestStr);
    void AsyncImport(TTestActorRuntime& runtime, ui64 id, const TString& dbName, const TString& requestStr);
    void TestImport(TTestActorRuntime& runtime, ui64 schemeshardId, ui64 id, const TString& dbName, const TString& requestStr,
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    void TestImport(TTestActorRuntime& runtime, ui64 id, const TString& dbName, const TString& requestStr,
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    NKikimrImport::TEvGetImportResponse TestGetImport(TTestActorRuntime& runtime, ui64 schemeshardId, ui64 id, const TString& dbName,
            const TVector<Ydb::StatusIds::StatusCode>& expectedStatuses);
    NKikimrImport::TEvGetImportResponse TestGetImport(TTestActorRuntime& runtime, ui64 id, const TString& dbName,
            const TVector<Ydb::StatusIds::StatusCode>& expectedStatuses);
    NKikimrImport::TEvGetImportResponse TestGetImport(TTestActorRuntime& runtime, ui64 schemeshardId, ui64 id, const TString& dbName,
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    NKikimrImport::TEvGetImportResponse TestGetImport(TTestActorRuntime& runtime, ui64 id, const TString& dbName,
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    TEvImport::TEvCancelImportRequest* CancelImportRequest(ui64 txId, const TString& dbName, ui64 importId);
    NKikimrImport::TEvCancelImportResponse TestCancelImport(TTestActorRuntime& runtime, ui64 schemeshardId, ui64 txId, const TString& dbName, ui64 importId,
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);
    NKikimrImport::TEvCancelImportResponse TestCancelImport(TTestActorRuntime& runtime, ui64 txId, const TString& dbName, ui64 importId,
            Ydb::StatusIds::StatusCode expectedStatus = Ydb::StatusIds::SUCCESS);

    ////////// datashard
    ui64 GetDatashardState(TTestActorRuntime& runtime, ui64 tabletId);
    TString SetAllowLogBatching(TTestActorRuntime& runtime, ui64 tabletId, bool v);

    ui64 GetDatashardSysTableValue(TTestActorRuntime& runtime, ui64 tabletId, ui64 sysKey);
    ui64 GetTxReadSizeLimit(TTestActorRuntime& runtime, ui64 tabletId);
    ui64 GetStatDisabled(TTestActorRuntime& runtime, ui64 tabletId);

    ui64 GetExecutorCacheSize(TTestActorRuntime& runtime, ui64 tabletId);
    bool GetFastLogPolicy(TTestActorRuntime& runtime, ui64 tabletId);
    bool GetByKeyFilterEnabled(TTestActorRuntime& runtime, ui64 tabletId, ui32 table);
    bool GetEraseCacheEnabled(TTestActorRuntime& runtime, ui64 tabletId, ui32 table);
    NKikimr::NLocalDb::TCompactionPolicyPtr GetCompactionPolicy(TTestActorRuntime& runtime, ui64 tabletId, ui32 localTableId);
    void SetSchemeshardReadOnlyMode(TTestActorRuntime& runtime, bool isReadOnly);
    void SetSchemeshardSchemaLimits(TTestActorRuntime& runtime, NSchemeShard::TSchemeLimits limits);
    void SetSchemeshardSchemaLimits(TTestActorRuntime& runtime, NSchemeShard::TSchemeLimits limits, ui64 schemeShard);
    void SetSchemeshardDatabaseQuotas(TTestActorRuntime& runtime, Ydb::Cms::DatabaseQuotas databaseQuotas, ui64 domainId);
    void SetSchemeshardDatabaseQuotas(TTestActorRuntime& runtime, Ydb::Cms::DatabaseQuotas databaseQuotas, ui64 dimainId, ui64 schemeShard);

    NKikimrSchemeOp::TTableDescription GetDatashardSchema(TTestActorRuntime& runtime, ui64 tabletId, ui64 tid);

    NLs::TCheckFunc ShardsIsReady(TTestActorRuntime& runtime);

    template <typename TCreateFunc>
    void CreateWithIntermediateDirs(TCreateFunc func) {
        TTestWithReboots t;
        t.Run([&](TTestActorRuntime& runtime, bool& activeZone) {
            func(runtime, ++t.TxId, "/MyRoot", false); // invalid
            func(runtime, ++t.TxId, "/MyRoot", true); // valid
            t.TestEnv->TestWaitNotification(runtime, t.TxId);

            {
                TInactiveZone inactive(activeZone);
                TestDescribeResult(DescribePath(runtime, "/MyRoot/Valid/x/y/z"),
                                   {NLs::Finished});
                TestDescribeResult(DescribePath(runtime, "/MyRoot/Invalid"),
                                   {NLs::PathNotExist});
            }
        });
    }

    template <typename TCreateFunc>
    void CreateWithIntermediateDirsForceDrop(TCreateFunc func) {
        TTestWithReboots t;
        t.Run([&](TTestActorRuntime& runtime, bool& activeZone) {
            func(runtime, ++t.TxId, "/MyRoot");
            AsyncForceDropUnsafe(runtime, ++t.TxId, 3);
            t.TestEnv->TestWaitNotification(runtime, {t.TxId - 1, t.TxId});

            {
                TInactiveZone inactive(activeZone);
                TestDescribeResult(DescribePath(runtime, "/MyRoot/x"),
                                   {NLs::PathNotExist});
            }
        });
    }

    TRowVersion CreateVolatileSnapshot(
        TTestActorRuntime& runtime,
        const TVector<TString>& tables,
        TDuration timeout);

    TPathId TestFindTabletSubDomainPathId(
        TTestActorRuntime& runtime, ui64 tabletId,
        NKikimrScheme::TEvFindTabletSubDomainPathIdResult::EStatus expected = NKikimrScheme::TEvFindTabletSubDomainPathIdResult::SUCCESS);
    TPathId TestFindTabletSubDomainPathId(
        TTestActorRuntime& runtime, ui64 schemeShard, ui64 tabletId,
        NKikimrScheme::TEvFindTabletSubDomainPathIdResult::EStatus expected = NKikimrScheme::TEvFindTabletSubDomainPathIdResult::SUCCESS);

    // Login
    TEvTx* CreateAlterLoginCreateUser(ui64 txId, const TString& user, const TString& password);
    NKikimrScheme::TEvLoginResult Login(TTestActorRuntime& runtime, const TString& user, const TString& password);

    // Mimics data query to a single table with multiple partitions
    class TFakeDataReq {
    public:
        TFakeDataReq(TTestActorRuntime& runtime, ui64 txId, const TString& table, const TString& query);
        ~TFakeDataReq();

        // returns Unknown if plan is required, Error/Complete/Abort otherwise
        NMiniKQL::IEngineFlat::EStatus Propose(bool immediate, bool& activeZone, ui32 txFlags = NDataShard::TTxFlags::Default);

        // Propose to coordinator
        void Plan(ui64 coordinatorId);

        TMap<ui64, TVector<NKikimrTxDataShard::TError>> GetErrors() const {
            return Errors;
        }

    private:
        NMiniKQL::TRuntimeNode ProgramText2Bin(const TString& query);

        void FillTableInfo(TMockDbSchemeResolver& dbSchemeResolver) const;

        struct TTablePartitioningInfo {
            struct TBorder {
                TSerializedCellVec KeyTuple;
                bool Inclusive = false;
                bool Point = false;
                bool Defined = false;
                ui64 Datashard = 0;
            };

            TVector<NScheme::TTypeId> KeyColumnTypes;
            TVector<TBorder> Partitioning;

            void ResolveKey(const TTableRange& range, TVector<TKeyDesc::TPartitionInfo>& partitions) const;
        };

        void FillTablePartitioningInfo();

        void ResolveKey(TKeyDesc& dbKey) {
            if (TablePartitioningInfo.Partitioning.empty()) {
                FillTablePartitioningInfo();
            }

            TablePartitioningInfo.ResolveKey(dbKey.Range, dbKey.Partitions);
            dbKey.Status = TKeyDesc::EStatus::Ok;
        }

    private:
        TTestActorRuntime& Runtime;
        NMiniKQL::TScopedAlloc Alloc;
        NMiniKQL::TTypeEnvironment Env;
        ui64 TxId;
        const TString Table;
        const TString Query;
        TAutoPtr<NMiniKQL::IEngineFlat> Engine;
        TVector<ui64> AffectedShards;
        TMap<ui64, TVector<NKikimrTxDataShard::TError>> Errors;
        TTablePartitioningInfo TablePartitioningInfo;
    };

    TTestActorRuntimeBase::TEventObserver SetSuppressObserver(TTestActorRuntime& runtime, TVector<THolder<IEventHandle>>& suppressed, ui32 type);
    void WaitForSuppressed(TTestActorRuntime& runtime, TVector<THolder<IEventHandle>>& suppressed, ui32 count, TTestActorRuntime::TEventObserver prevObserver);


    NKikimrTxDataShard::TEvCompactTableResult CompactTable(
        TTestActorRuntime& runtime, ui64 shardId, const TTableId& tableId, bool compactBorrowed = false);

} //NSchemeShardUT_Private
