#include "columnshard_ut_common.h"
#include <ydb/core/kqp/kqp_compute.h>
#include <ydb/core/base/blobstorage.h>
#include <util/string/printf.h>
#include <arrow/api.h>
#include <arrow/ipc/reader.h>

namespace NKikimr {

using namespace NColumnShard;
using namespace Tests;
using namespace NTxUT;

namespace
{

namespace NTypeIds = NScheme::NTypeIds;
using TTypeId = NScheme::TTypeId;

static const TVector<std::pair<TString, TTypeId>> testYdbSchema = TTestSchema::YdbSchema();
static const TVector<std::pair<TString, TTypeId>> testYdbPkSchema = TTestSchema::YdbPkSchema();

bool DataHas(const TVector<TString>& blobs, const TString& srtSchema, std::pair<ui64, ui64> range,
             bool requireUniq = false) {
    THashMap<ui64, ui32> keys;
    for (size_t i = range.first; i < range.second; ++i) {
        keys.emplace(i, 0);
    }

    auto schema = NArrow::DeserializeSchema(srtSchema);
    for (auto& blob : blobs) {
        auto batch = NArrow::DeserializeBatch(blob, schema);
        UNIT_ASSERT(batch);

        std::shared_ptr<arrow::Array> array = batch->GetColumnByName("timestamp");
        UNIT_ASSERT(array);
        auto& ts = dynamic_cast<const arrow::NumericArray<arrow::TimestampType>&>(*array);

        for (int i = 0; i < ts.length(); ++i) {
            ui64 value = ts.Value(i);
            ++keys[value];
        }
    }

    for (auto& [key, count] : keys) {
        if (!count) {
            Cerr << "No key: " << key << "\n";
            return false;
        }
        if (requireUniq && count > 1) {
            Cerr << "Not unique key: " << key << " (count: " << count << ")\n";
            return false;
        }
    }

    return true;
}

bool DataHasOnly(const TVector<TString>& blobs, const TString& srtSchema, std::pair<ui64, ui64> range) {
    THashSet<ui64> keys;
    for (size_t i = range.first; i < range.second; ++i) {
        keys.emplace(i);
    }

    auto schema = NArrow::DeserializeSchema(srtSchema);
    for (auto& blob : blobs) {
        auto batch = NArrow::DeserializeBatch(blob, schema);
        UNIT_ASSERT(batch);

        std::shared_ptr<arrow::Array> array = batch->GetColumnByName("timestamp");
        UNIT_ASSERT(array);
        auto& ts = dynamic_cast<const arrow::NumericArray<arrow::TimestampType>&>(*array);

        for (int i = 0; i < ts.length(); ++i) {
            ui64 value = ts.Value(i);
            if (!keys.count(value)) {
                Cerr << "Unexpected key: " << value << "\n";
                return false;
            }
        }
    }

    return true;
}

bool CheckOrdered(const TString& blob, const TString& srtSchema) {
    auto schema = NArrow::DeserializeSchema(srtSchema);
    auto batch = NArrow::DeserializeBatch(blob, schema);
    UNIT_ASSERT(batch);

    std::shared_ptr<arrow::Array> array = batch->GetColumnByName("timestamp");
    UNIT_ASSERT(array);
    auto& ts = dynamic_cast<const arrow::NumericArray<arrow::TimestampType>&>(*array);
    if (!ts.length()) {
        return true;
    }

    ui64 prev = ts.Value(0);
    for (int i = 1; i < ts.length(); ++i) {
        ui64 value = ts.Value(i);
        if (prev > value) {
            Cerr << "Unordered: " << prev << " " << value << "\n";
            return false;
        }
    }
    return true;
}

bool CheckColumns(const TString& blob, const NKikimrTxColumnShard::TMetadata& meta, const TVector<TString>& colNames,
                  size_t rowsCount = 100) {
    auto schema = NArrow::DeserializeSchema(meta.GetSchema());
    auto batch = NArrow::DeserializeBatch(blob, schema);
    UNIT_ASSERT(batch);

    for (auto& col : colNames) {
        if (!batch->GetColumnByName(col)) {
            return false;
        }
    }

    UNIT_ASSERT_VALUES_EQUAL((ui64)batch->num_columns(), colNames.size());
    UNIT_ASSERT_VALUES_EQUAL((ui64)batch->num_rows(), rowsCount);
    UNIT_ASSERT(batch->ValidateFull().ok());
    return true;
}

void SetupSchema(TTestBasicRuntime& runtime, TActorId& sender, ui64 pathId,
                 const TVector<std::pair<TString, TTypeId>>& schema = TTestSchema::YdbSchema(),
                 NOlap::TSnapshot snap = {10, 10}, TString codec = "") {
    bool ok = ProposeSchemaTx(runtime, sender,
                              TTestSchema::CreateTableTxBody(pathId, schema,
                                                             TTestSchema::TTableSpecials().WithCodec(codec)),
                              snap);
    UNIT_ASSERT(ok);
    PlanSchemaTx(runtime, sender, snap);
}

void TestWriteImpl(const TVector<std::pair<TString, TTypeId>>& ydbSchema) {
    TTestBasicRuntime runtime;
    TTester::Setup(runtime);

    TActorId sender = runtime.AllocateEdgeActor();
    CreateTestBootstrapper(runtime, CreateTestTabletInfo(TTestTxConfig::TxTablet0, TTabletTypes::COLUMNSHARD), &CreateColumnShard);

    TDispatchOptions options;
    options.FinalEvents.push_back(TDispatchOptions::TFinalEventCondition(TEvTablet::EvBoot));
    runtime.DispatchEvents(options);

    //

    ui64 metaShard = TTestTxConfig::TxTablet1;
    ui64 writeId = 0;
    ui64 tableId = 1;

    SetupSchema(runtime, sender, tableId, ydbSchema);
    bool ok = WriteData(runtime, sender, metaShard, writeId, tableId, MakeTestBlob({0, 100}, ydbSchema));
    UNIT_ASSERT(ok);

    TVector<std::pair<TString, TTypeId>> schema = ydbSchema;

    // no data

    ok = WriteData(runtime, sender, metaShard, writeId, tableId, TString());
    UNIT_ASSERT(!ok);

    // null column in PK

    ok = WriteData(runtime, sender, metaShard, writeId, tableId, MakeTestBlob({0, 100}, ydbSchema, {"timestamp"}));
    UNIT_ASSERT(!ok);

    // missing columns

    schema.resize(4);
    ok = WriteData(runtime, sender, metaShard, writeId, tableId, MakeTestBlob({0, 100}, schema));
    UNIT_ASSERT(!ok);

    // wrong type
    // TODO: better check (it probably does not work in general case)

    schema = ydbSchema;
    schema[1].second = NTypeIds::Int32;
    ok = WriteData(runtime, sender, metaShard, writeId, tableId, MakeTestBlob({0, 100}, schema));
    UNIT_ASSERT(!ok);

    schema = ydbSchema;
    schema[1].second = NTypeIds::Utf8;
    schema[5].second = NTypeIds::Int32;
    ok = WriteData(runtime, sender, metaShard, writeId, tableId, MakeTestBlob({0, 100}, schema));
    UNIT_ASSERT(!ok);

    // reordered columns

    schema.resize(0);
    schema.push_back({"level", NTypeIds::Int32 });
    schema.push_back({"timestamp", NTypeIds::Timestamp });
    schema.push_back({"uid", NTypeIds::Utf8 });
    schema.push_back({"resource_id", NTypeIds::Utf8 });
    schema.push_back({"resource_type", NTypeIds::Utf8 });
    schema.push_back({"message", NTypeIds::Utf8 });
    schema.push_back({"request_id", NTypeIds::Utf8 });
    schema.push_back({"saved_at", NTypeIds::Timestamp });
    schema.push_back({"ingested_at", NTypeIds::Timestamp });
    schema.push_back({"json_payload", NTypeIds::Json });

    ok = WriteData(runtime, sender, metaShard, writeId, tableId, MakeTestBlob({0, 100}, schema));
    UNIT_ASSERT(!ok);

    ok = WriteData(runtime, sender, metaShard, writeId, tableId, MakeTestBlob({0, 100}, schema),
                    NArrow::MakeArrowSchema(schema));
    UNIT_ASSERT(ok);

    // too much data

    TString bigData = MakeTestBlob({0, 150 * 1000}, ydbSchema);
    UNIT_ASSERT(bigData.size() > NColumnShard::TLimits::MAX_BLOB_SIZE);
    UNIT_ASSERT(bigData.size() < NColumnShard::TLimits::MAX_BLOB_SIZE + 2 * 1024 * 1024);
    ok = WriteData(runtime, sender, metaShard, writeId, tableId, bigData);
    UNIT_ASSERT(!ok);
}

void TestWriteReadImpl(bool reboots, const TVector<std::pair<TString, TTypeId>>& ydbSchema = TTestSchema::YdbSchema(),
                       TString codec = "") {
    TTestBasicRuntime runtime;
    TTester::Setup(runtime);

    runtime.SetLogPriority(NKikimrServices::BLOB_CACHE, NActors::NLog::PRI_DEBUG);

    TActorId sender = runtime.AllocateEdgeActor();
    CreateTestBootstrapper(runtime, CreateTestTabletInfo(TTestTxConfig::TxTablet0, TTabletTypes::COLUMNSHARD), &CreateColumnShard);

    TDispatchOptions options;
    options.FinalEvents.push_back(TDispatchOptions::TFinalEventCondition(TEvTablet::EvBoot));
    runtime.DispatchEvents(options);

    auto write = [&](TTestBasicRuntime& runtime, TActorId& sender, ui64 metaShard, ui64 writeId, ui64 tableId,
                     const TString& data) {
        bool ok = WriteData(runtime, sender, metaShard, writeId, tableId, data);
        if (reboots) {
            RebootTablet(runtime, TTestTxConfig::TxTablet0, sender);
        }
        return ok;
    };

    auto proposeCommit = [&](TTestBasicRuntime& runtime, TActorId& sender, ui64 metaShard, ui64 txId,
                             const TVector<ui64>& writeIds) {
        ProposeCommit(runtime, sender, metaShard, txId, writeIds);
        if (reboots) {
            RebootTablet(runtime, TTestTxConfig::TxTablet0, sender);
        }
    };

    auto planCommit = [&](TTestBasicRuntime& runtime, TActorId& sender, ui64 planStep, ui64 txId) {
        PlanCommit(runtime, sender, planStep, txId);
        if (reboots) {
            RebootTablet(runtime, TTestTxConfig::TxTablet0, sender);
        }
    };

    //

    ui64 metaShard = TTestTxConfig::TxTablet1;
    ui64 writeId = 0;
    ui64 tableId = 1;

    SetupSchema(runtime, sender, tableId, ydbSchema, {10, 10}, codec);

    // ----xx
    // -----xx..
    // xx----
    // -xxxxx
    TVector<std::pair<ui64, ui64>> portion = {
        {200, 300},
        {250, 250 + 80 * 1000}, // committed -> index
        {0, 100},
        {50, 300}
    };

    // write 1: ins:1, cmt:0, idx:0

    UNIT_ASSERT(write(runtime, sender, metaShard, writeId, tableId, MakeTestBlob(portion[0], ydbSchema)));

    // read

    ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender,
                new TEvColumnShard::TEvRead(sender, metaShard, 0, 0, tableId));
    TAutoPtr<IEventHandle> handle;
    auto event2 = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
    UNIT_ASSERT(event2);

    auto& resRead = Proto(event2);
    UNIT_ASSERT_EQUAL(resRead.GetOrigin(), TTestTxConfig::TxTablet0);
    UNIT_ASSERT_EQUAL(resRead.GetTxInitiator(), metaShard);
    UNIT_ASSERT_EQUAL(resRead.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
    UNIT_ASSERT_EQUAL(resRead.GetBatch(), 0);
    UNIT_ASSERT_EQUAL(resRead.GetFinished(), true);
    UNIT_ASSERT_EQUAL(resRead.GetData(), "");

    // commit 1: ins:0, cmt:1, idx:0

    ui64 planStep = 21;
    ui64 txId = 100;
    proposeCommit(runtime, sender, metaShard, txId, {writeId});
    planCommit(runtime, sender, planStep, txId);

    // read 2 (committed, old snapshot)

    ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender,
                new TEvColumnShard::TEvRead(sender, metaShard, 0, 0, tableId));
    auto event5 = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
    UNIT_ASSERT(event5);

    auto& resRead2 = Proto(event5);
    UNIT_ASSERT_EQUAL(resRead2.GetOrigin(), TTestTxConfig::TxTablet0);
    UNIT_ASSERT_EQUAL(resRead2.GetTxInitiator(), metaShard);
    UNIT_ASSERT_EQUAL(resRead2.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
    UNIT_ASSERT_EQUAL(resRead2.GetBatch(), 0);
    UNIT_ASSERT_EQUAL(resRead2.GetFinished(), true);
    UNIT_ASSERT_EQUAL(resRead2.GetData(), "");

    // read 3 (committed)

    ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender,
                new TEvColumnShard::TEvRead(sender, metaShard, planStep, txId, tableId));
    auto event6 = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
    UNIT_ASSERT(event6);

    auto& resRead3 = Proto(event6);
    UNIT_ASSERT_EQUAL(resRead3.GetOrigin(), TTestTxConfig::TxTablet0);
    UNIT_ASSERT_EQUAL(resRead3.GetTxInitiator(), metaShard);
    UNIT_ASSERT_EQUAL(resRead3.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
    UNIT_ASSERT_EQUAL(resRead3.GetBatch(), 0);
    UNIT_ASSERT_EQUAL(resRead3.GetFinished(), true);
    //UNIT_ASSERT_EQUAL(resRead3.GetData(), data);
    UNIT_ASSERT(resRead3.GetData().size() > 0);
    UNIT_ASSERT(CheckColumns(resRead3.GetData(), resRead3.GetMeta(), TTestSchema::ExtractNames(ydbSchema)));
    {
        TVector<TString> readData;
        readData.push_back(resRead3.GetData());
        auto& schema = resRead3.GetMeta().GetSchema();
        UNIT_ASSERT(DataHas(readData, schema, portion[0]));
        UNIT_ASSERT(CheckOrdered(resRead3.GetData(), schema));
    }

    // read 4 (column by id)

    auto read_col1 = std::make_unique<TEvColumnShard::TEvRead>(sender, metaShard, planStep, txId, tableId);
    Proto(read_col1.get()).AddColumnIds(1);
    ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, read_col1.release());
    auto event7 = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
    UNIT_ASSERT(event7);

    auto& resRead4 = Proto(event7);
    UNIT_ASSERT_EQUAL(resRead4.GetOrigin(), TTestTxConfig::TxTablet0);
    UNIT_ASSERT_EQUAL(resRead4.GetTxInitiator(), metaShard);
    UNIT_ASSERT_EQUAL(resRead4.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
    UNIT_ASSERT_EQUAL(resRead4.GetBatch(), 0);
    UNIT_ASSERT_EQUAL(resRead4.GetFinished(), true);
    UNIT_ASSERT(CheckColumns(resRead4.GetData(), resRead4.GetMeta(), {"timestamp"}));
    {
        auto& schema = resRead4.GetMeta().GetSchema();
        UNIT_ASSERT(CheckOrdered(resRead4.GetData(), schema));
    }

    // read 5 (2 columns by name)

    auto read_col2 = std::make_unique<TEvColumnShard::TEvRead>(sender, metaShard, planStep, txId, tableId);
    Proto(read_col2.get()).AddColumnNames("timestamp");
    Proto(read_col2.get()).AddColumnNames("message");
    ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, read_col2.release());
    auto event8 = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
    UNIT_ASSERT(event8);

    auto& resRead5 = Proto(event8);
    UNIT_ASSERT_EQUAL(resRead5.GetOrigin(), TTestTxConfig::TxTablet0);
    UNIT_ASSERT_EQUAL(resRead5.GetTxInitiator(), metaShard);
    UNIT_ASSERT_EQUAL(resRead5.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
    UNIT_ASSERT_EQUAL(resRead5.GetBatch(), 0);
    UNIT_ASSERT_EQUAL(resRead5.GetFinished(), true);
    UNIT_ASSERT(CheckColumns(resRead5.GetData(), resRead5.GetMeta(), {"timestamp", "message"}));
    {
        auto& schema = resRead5.GetMeta().GetSchema();
        UNIT_ASSERT(CheckOrdered(resRead5.GetData(), schema));
    }

    // write 2 (big portion of data): ins:1, cmt:1, idx:0

    ++writeId;
    {
        TString triggerData = MakeTestBlob(portion[1], ydbSchema);
        UNIT_ASSERT(triggerData.size() > NColumnShard::TLimits::MIN_BYTES_TO_INSERT);
        UNIT_ASSERT(write(runtime, sender, metaShard, writeId, tableId, triggerData));
    }

    // commit 2 (init indexation): ins:0, cmt:0, idx:1

    planStep = 22;
    ++txId;
    proposeCommit(runtime, sender, metaShard, txId, {writeId});
    planCommit(runtime, sender, planStep, txId);

    // write 3: ins:1, cmt:0, idx:1

    ++writeId;
    UNIT_ASSERT(write(runtime, sender, metaShard, writeId, tableId, MakeTestBlob(portion[2], ydbSchema)));

    // read 6, planstep 0

    ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender,
                new TEvColumnShard::TEvRead(sender, metaShard, 0, 0, tableId));
    auto event9 = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
    UNIT_ASSERT(event9);

    auto& resRead6 = Proto(event9);
    UNIT_ASSERT_EQUAL(resRead6.GetOrigin(), TTestTxConfig::TxTablet0);
    UNIT_ASSERT_EQUAL(resRead6.GetTxInitiator(), metaShard);
    UNIT_ASSERT_EQUAL(resRead6.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
    UNIT_ASSERT_EQUAL(resRead6.GetBatch(), 0);
    UNIT_ASSERT_EQUAL(resRead6.GetFinished(), true);
    UNIT_ASSERT_EQUAL(resRead6.GetData(), "");

    // read 7, planstep 21 (part of index)

    ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender,
                new TEvColumnShard::TEvRead(sender, metaShard, 21, txId, tableId));
    auto event10 = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
    UNIT_ASSERT(event10);

    auto& resRead7 = Proto(event10);
    UNIT_ASSERT_EQUAL(resRead7.GetOrigin(), TTestTxConfig::TxTablet0);
    UNIT_ASSERT_EQUAL(resRead7.GetTxInitiator(), metaShard);
    UNIT_ASSERT_EQUAL(resRead7.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
    UNIT_ASSERT_EQUAL(resRead7.GetBatch(), 0);
    UNIT_ASSERT_EQUAL(resRead7.GetFinished(), true);
    UNIT_ASSERT(resRead7.GetData().size() > 0);

    {
        TVector<TString> readData;
        readData.push_back(resRead7.GetData());
        auto& schema = resRead7.GetMeta().GetSchema();
        UNIT_ASSERT(DataHas(readData, schema, portion[0])); // checks no checks REPLACE (indexed vs indexed)
        UNIT_ASSERT(!DataHas(readData, schema, portion[1])); // checks snapshot filter in indexed data
        UNIT_ASSERT(!DataHas(readData, schema, portion[2]));
        UNIT_ASSERT(CheckOrdered(resRead7.GetData(), schema));
    }

    // read 8, planstep 22 (full index)

    ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender,
                new TEvColumnShard::TEvRead(sender, metaShard, 22, txId, tableId));
    auto event11 = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
    UNIT_ASSERT(event11);

    auto& resRead8 = Proto(event11);
    UNIT_ASSERT_EQUAL(resRead8.GetOrigin(), TTestTxConfig::TxTablet0);
    UNIT_ASSERT_EQUAL(resRead8.GetTxInitiator(), metaShard);
    UNIT_ASSERT_EQUAL(resRead8.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
    UNIT_ASSERT_EQUAL(resRead8.GetBatch(), 0);
    UNIT_ASSERT_EQUAL(resRead8.GetFinished(), true);
    UNIT_ASSERT(resRead8.GetData().size() > 0);

    {
        TVector<TString> readData;
        readData.push_back(resRead8.GetData());
        auto& schema = resRead8.GetMeta().GetSchema();
        UNIT_ASSERT(DataHas(readData, schema, portion[0], true)); // checks REPLACE (indexed vs indexed)
        UNIT_ASSERT(DataHas(readData, schema, portion[1]));
        UNIT_ASSERT(!DataHas(readData, schema, portion[2]));
        UNIT_ASSERT(CheckOrdered(resRead8.GetData(), schema));
    }

    // commit 3: ins:0, cmt:1, idx:1

    planStep = 23;
    ++txId;
    proposeCommit(runtime, sender, metaShard, txId, {writeId});
    planCommit(runtime, sender, planStep, txId);

    // write 4: ins:1, cmt:1, idx:1

    ++writeId;
    UNIT_ASSERT(write(runtime, sender, metaShard, writeId, tableId, MakeTestBlob(portion[3], ydbSchema)));

    // read 9 (committed, indexed)

    ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender,
                new TEvColumnShard::TEvRead(sender, metaShard, 23, txId, tableId));
    TVector<TString> readData;
    TString schema;
    ui32 expected = 1000;
    for (ui32 i = 0; i < expected; ++i) {
        auto event = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
        UNIT_ASSERT(event);

        auto& resRead = Proto(event);
        UNIT_ASSERT_EQUAL(resRead.GetOrigin(), TTestTxConfig::TxTablet0);
        UNIT_ASSERT_EQUAL(resRead.GetTxInitiator(), metaShard);
        UNIT_ASSERT_EQUAL(resRead.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
        UNIT_ASSERT(resRead.GetData().size() > 0);
        //UNIT_ASSERT_EQUAL(resRead.GetBatch(), 0);
        //UNIT_ASSERT_EQUAL(resRead.GetFinished(), true);
        if (resRead.GetFinished()) {
            expected = resRead.GetBatch() + 1;
        }
        readData.push_back(resRead.GetData());
        if (schema.empty()) {
            schema = resRead.GetMeta().GetSchema();
        }
        UNIT_ASSERT(CheckOrdered(resRead.GetData(), schema));
    }
    UNIT_ASSERT(DataHas(readData, schema, portion[0]));
    UNIT_ASSERT(DataHas(readData, schema, portion[1]));
    UNIT_ASSERT(DataHas(readData, schema, portion[2]));
    UNIT_ASSERT(!DataHas(readData, schema, portion[3]));

    // commit 4: ins:0, cmt:2, idx:1 (with duplicates in PK)

    planStep = 24;
    ++txId;
    proposeCommit(runtime, sender, metaShard, txId, {writeId});
    planCommit(runtime, sender, planStep, txId);

    // read 10

    ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender,
                new TEvColumnShard::TEvRead(sender, metaShard, 24, txId, tableId));
    readData.clear();
    schema.clear();
    expected = 1000;
    for (ui32 i = 0; i < expected; ++i) {
        auto event = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
        UNIT_ASSERT(event);

        auto& resRead = Proto(event);
        UNIT_ASSERT_EQUAL(resRead.GetOrigin(), TTestTxConfig::TxTablet0);
        UNIT_ASSERT_EQUAL(resRead.GetTxInitiator(), metaShard);
        UNIT_ASSERT_EQUAL(resRead.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
        UNIT_ASSERT(resRead.GetData().size() > 0);
        //UNIT_ASSERT_EQUAL(resRead.GetBatch(), 0);
        //UNIT_ASSERT_EQUAL(resRead.GetFinished(), true);
        if (resRead.GetFinished()) {
            expected = resRead.GetBatch() + 1;
        }
        readData.push_back(resRead.GetData());

        auto meta = resRead.GetMeta();
        if (schema.empty()) {
            schema = meta.GetSchema();
        }
        UNIT_ASSERT(CheckOrdered(resRead.GetData(), schema));

        UNIT_ASSERT(meta.HasReadStats());
        auto& readStats = meta.GetReadStats();

        if (ydbSchema == TTestSchema::YdbSchema()) {
            if (codec == "" || codec == "lz4") {
                UNIT_ASSERT_VALUES_EQUAL(readStats.GetDataBytes(), 5054176);
            } else if (codec == "none") {
                UNIT_ASSERT_VALUES_EQUAL(readStats.GetDataBytes(), 7557872);
            } else if (codec == "zstd") {
                UNIT_ASSERT_VALUES_EQUAL(readStats.GetDataBytes(), 2636440);
            } else {
                UNIT_ASSERT(false);
            }
        }
    }
    UNIT_ASSERT(DataHas(readData, schema, portion[0]));
    UNIT_ASSERT(DataHas(readData, schema, portion[1]));
    UNIT_ASSERT(DataHas(readData, schema, portion[2]));
    UNIT_ASSERT(DataHas(readData, schema, portion[3]));
    UNIT_ASSERT(DataHas(readData, schema, {0, 500}, true)); // checks REPLACE (committed vs indexed)

    // read 11 (range predicate: closed interval)
    {
        TSerializedTableRange range = MakeTestRange({10, 42}, true, true, testYdbPkSchema);
        NOlap::TPredicate prGreater, prLess;
        std::tie(prGreater, prLess) = RangePredicates(range, testYdbPkSchema);

        auto evRead = std::make_unique<TEvColumnShard::TEvRead>(sender, metaShard, 24, txId, tableId);
        auto* greater = Proto(evRead.get()).MutableGreaterPredicate();
        auto* less = Proto(evRead.get()).MutableLessPredicate();
        for (auto& name : prGreater.ColumnNames()) {
            greater->AddColumnNames(name);
        }
        for (auto& name : prLess.ColumnNames()) {
            less->AddColumnNames(name);
        }
        greater->SetRow(NArrow::SerializeBatchNoCompression(prGreater.Batch));
        less->SetRow(NArrow::SerializeBatchNoCompression(prLess.Batch));
        greater->SetInclusive(prGreater.Inclusive);
        less->SetInclusive(prLess.Inclusive);

        ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, evRead.release());
    }
    readData.clear();
    schema.clear();
    {
        auto event = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
        UNIT_ASSERT(event);

        auto& resRead = Proto(event);
        UNIT_ASSERT_EQUAL(resRead.GetOrigin(), TTestTxConfig::TxTablet0);
        UNIT_ASSERT_EQUAL(resRead.GetTxInitiator(), metaShard);
        UNIT_ASSERT_EQUAL(resRead.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
        UNIT_ASSERT(resRead.GetData().size() > 0);
        UNIT_ASSERT_EQUAL(resRead.GetBatch(), 0);
        UNIT_ASSERT_EQUAL(resRead.GetFinished(), true);
        readData.push_back(resRead.GetData());
        if (schema.empty()) {
            schema = resRead.GetMeta().GetSchema();
        }
        UNIT_ASSERT(CheckOrdered(resRead.GetData(), schema));
    }
    UNIT_ASSERT(DataHas(readData, schema, {10, 42 + 1}));
    UNIT_ASSERT(DataHasOnly(readData, schema, {10, 42 + 1}));

    // read 12 (range predicate: open interval)
    {
        TSerializedTableRange range = MakeTestRange({10, 42}, false, false, testYdbPkSchema);
        NOlap::TPredicate prGreater, prLess;
        std::tie(prGreater, prLess) = RangePredicates(range, testYdbPkSchema);

        auto evRead = std::make_unique<TEvColumnShard::TEvRead>(sender, metaShard, 24, txId, tableId);
        auto* greater = Proto(evRead.get()).MutableGreaterPredicate();
        auto* less = Proto(evRead.get()).MutableLessPredicate();
        for (auto& name : prGreater.ColumnNames()) {
            greater->AddColumnNames(name);
        }
        for (auto& name : prLess.ColumnNames()) {
            less->AddColumnNames(name);
        }

        greater->SetRow(NArrow::SerializeBatchNoCompression(prGreater.Batch));
        less->SetRow(NArrow::SerializeBatchNoCompression(prLess.Batch));
        greater->SetInclusive(prGreater.Inclusive);
        less->SetInclusive(prLess.Inclusive);

        ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, evRead.release());
    }
    readData.clear();
    schema.clear();
    {
        auto event = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
        UNIT_ASSERT(event);

        auto& resRead = Proto(event);
        UNIT_ASSERT_EQUAL(resRead.GetOrigin(), TTestTxConfig::TxTablet0);
        UNIT_ASSERT_EQUAL(resRead.GetTxInitiator(), metaShard);
        UNIT_ASSERT_EQUAL(resRead.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
        UNIT_ASSERT(resRead.GetData().size() > 0);
        UNIT_ASSERT_EQUAL(resRead.GetBatch(), 0);
        UNIT_ASSERT_EQUAL(resRead.GetFinished(), true);
        readData.push_back(resRead.GetData());
        if (schema.empty()) {
            schema = resRead.GetMeta().GetSchema();
        }
        UNIT_ASSERT(CheckOrdered(resRead.GetData(), schema));
    }
    UNIT_ASSERT(DataHas(readData, schema, {11, 41 + 1}));
    UNIT_ASSERT(DataHasOnly(readData, schema, {11, 41 + 1}));
}

void TestCompactionInGranuleImpl(bool reboots) {
    TTestBasicRuntime runtime;
    TTester::Setup(runtime);

    TActorId sender = runtime.AllocateEdgeActor();
    CreateTestBootstrapper(runtime, CreateTestTabletInfo(TTestTxConfig::TxTablet0, TTabletTypes::COLUMNSHARD), &CreateColumnShard);

    TDispatchOptions options;
    options.FinalEvents.push_back(TDispatchOptions::TFinalEventCondition(TEvTablet::EvBoot));
    runtime.DispatchEvents(options);

    auto write = [&](TTestBasicRuntime& runtime, TActorId& sender, ui64 metaShard, ui64 writeId, ui64 tableId,
                     const TString& data) {
        bool ok = WriteData(runtime, sender, metaShard, writeId, tableId, data);
        if (reboots) {
            RebootTablet(runtime, TTestTxConfig::TxTablet0, sender);
        }
        return ok;
    };

    auto proposeCommit = [&](TTestBasicRuntime& runtime, TActorId& sender, ui64 metaShard, ui64 txId,
                             const TVector<ui64>& writeIds) {
        ProposeCommit(runtime, sender, metaShard, txId, writeIds);
        if (reboots) {
            RebootTablet(runtime, TTestTxConfig::TxTablet0, sender);
        }
    };

    auto planCommit = [&](TTestBasicRuntime& runtime, TActorId& sender, ui64 planStep, ui64 txId) {
        PlanCommit(runtime, sender, planStep, txId);
        if (reboots) {
            RebootTablet(runtime, TTestTxConfig::TxTablet0, sender);
        }
    };

    //

    ui64 metaShard = TTestTxConfig::TxTablet1;
    ui64 writeId = 0;
    ui64 tableId = 1;
    ui64 planStep = 100;
    ui64 txId = 100;

    SetupSchema(runtime, sender, tableId);
    TAutoPtr<IEventHandle> handle;

    // Write same keys: merge on compaction

    static const ui32 triggerPortionSize = 75 * 1000;
    std::pair<ui64, ui64> triggerPortion = {0, triggerPortionSize};
    TString triggerData = MakeTestBlob(triggerPortion, testYdbSchema);
    UNIT_ASSERT(triggerData.size() > NColumnShard::TLimits::MIN_BYTES_TO_INSERT);
    UNIT_ASSERT(triggerData.size() < NColumnShard::TLimits::MAX_BLOB_SIZE);

    static const ui32 portionSize = 1;

    ui32 numWrites = NColumnShard::TLimits::MIN_SMALL_BLOBS_TO_INSERT; // trigger InsertTable -> Index

    // inserts triggered by count
    ui32 pos = triggerPortionSize;
    for (ui32 i = 0; i < 1; ++i, ++planStep, ++txId) {
        TVector<ui64> ids;
        ids.reserve(numWrites);
        for (ui32 w = 0; w < numWrites; ++w, ++writeId, pos += portionSize) {
            std::pair<ui64, ui64> portion = {pos, pos + portionSize};
            TString data = MakeTestBlob(portion, testYdbSchema);

            ids.push_back(writeId);
            UNIT_ASSERT(WriteData(runtime, sender, metaShard, writeId, tableId, data));
        }

        if (reboots) {
            RebootTablet(runtime, TTestTxConfig::TxTablet0, sender);
        }

        proposeCommit(runtime, sender, metaShard, txId, ids);
        planCommit(runtime, sender, planStep, txId);
    }
    std::pair<ui64, ui64> smallWrites = {triggerPortionSize, pos};

    // inserts triggered by size
    NOlap::TCompactionLimits engineLimits;
    ui32 numTxs = engineLimits.GranuleExpectedSize / triggerData.size() + 1;

    for (ui32 i = 0; i < numTxs; ++i, ++writeId, ++planStep, ++txId) {
        UNIT_ASSERT(write(runtime, sender, metaShard, writeId, tableId, triggerData));

        proposeCommit(runtime, sender, metaShard, txId, {writeId});
        planCommit(runtime, sender, planStep, txId);
    }

    // TODO: Move tablet's time to the future with mediator timecast instead
    --planStep;
    --txId;

    for (ui32 i = 0; i < 2; ++i) {
        auto read = std::make_unique<TEvColumnShard::TEvRead>(sender, metaShard, planStep, txId, tableId);
        Proto(read.get()).AddColumnNames("timestamp");
        Proto(read.get()).AddColumnNames("message");

        ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, read.release());
        auto event = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
        UNIT_ASSERT(event);

        auto& resRead = Proto(event);
        UNIT_ASSERT_EQUAL(resRead.GetOrigin(), TTestTxConfig::TxTablet0);
        UNIT_ASSERT_EQUAL(resRead.GetTxInitiator(), metaShard);
        UNIT_ASSERT_EQUAL(resRead.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
        UNIT_ASSERT_EQUAL(resRead.GetBatch(), 0);
        UNIT_ASSERT_EQUAL(resRead.GetFinished(), true);
        UNIT_ASSERT(resRead.GetData().size() > 0);
        UNIT_ASSERT(resRead.HasMeta());

        auto& meta = resRead.GetMeta();
        auto& schema = meta.GetSchema();

        TVector<TString> readData;
        readData.push_back(resRead.GetData());
        UNIT_ASSERT(DataHas(readData, schema, triggerPortion, true));
        UNIT_ASSERT(DataHas(readData, schema, smallWrites, true));

        UNIT_ASSERT(meta.HasReadStats());
        auto& readStats = meta.GetReadStats();

        UNIT_ASSERT(readStats.GetBeginTimestamp() > 0);
        UNIT_ASSERT(readStats.GetDurationUsec() > 0);
        UNIT_ASSERT_VALUES_EQUAL(readStats.GetSelectedIndex(), 0);
        UNIT_ASSERT_VALUES_EQUAL(readStats.GetIndexGranules(), 1);
        UNIT_ASSERT(readStats.GetIndexBatches() > 0);
        UNIT_ASSERT_VALUES_EQUAL(readStats.GetNotIndexedBatches(), 0);
        UNIT_ASSERT_VALUES_EQUAL(readStats.GetUsedColumns(), 7); // planStep, txId + 4 PK columns + "message"
        UNIT_ASSERT(readStats.GetIndexPortions() <= 2); // got compaction

        RebootTablet(runtime, TTestTxConfig::TxTablet0, sender);
    }
}

void TestReadWithProgramImpl()
{
    TTestBasicRuntime runtime;
    TTester::Setup(runtime);

    TActorId sender = runtime.AllocateEdgeActor();
    CreateTestBootstrapper(runtime, CreateTestTabletInfo(TTestTxConfig::TxTablet0, TTabletTypes::COLUMNSHARD), &CreateColumnShard);

    TDispatchOptions options;
    options.FinalEvents.push_back(TDispatchOptions::TFinalEventCondition(TEvTablet::EvBoot));
    runtime.DispatchEvents(options);

    ui64 metaShard = TTestTxConfig::TxTablet1;
    ui64 tableId = 1;

    SetupSchema(runtime, sender, tableId);
    {
        auto* readEvent = new TEvColumnShard::TEvRead(sender, metaShard, 0, 0, tableId);
        auto& readProto = Proto(readEvent);

        readProto.SetOlapProgramType(::NKikimrSchemeOp::EOlapProgramType::OLAP_PROGRAM_SSA_PROGRAM);
        readProto.SetOlapProgram("XXXYYYZZZ");

        ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, readEvent);

        TAutoPtr<IEventHandle> handle;
        auto result = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
        UNIT_ASSERT(result);

        auto& resRead = Proto(result);

        UNIT_ASSERT_EQUAL(resRead.GetOrigin(), TTestTxConfig::TxTablet0);
        UNIT_ASSERT_EQUAL(resRead.GetTxInitiator(), metaShard);
        UNIT_ASSERT_EQUAL(resRead.GetStatus(), NKikimrTxColumnShard::EResultStatus::ERROR);
        UNIT_ASSERT_EQUAL(resRead.GetBatch(), 0);
        UNIT_ASSERT_EQUAL(resRead.GetFinished(), true);
        UNIT_ASSERT_EQUAL(resRead.GetData(), "");
    }

    {
        auto* readEvent = new TEvColumnShard::TEvRead(sender, metaShard, 0, 0, tableId);
        auto& readProto = Proto(readEvent);

        readProto.SetOlapProgramType(::NKikimrSchemeOp::EOlapProgramType::OLAP_PROGRAM_SSA_PROGRAM_WITH_PARAMETERS);
        readProto.SetOlapProgram("XXXYYYZZZ");

        ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, readEvent);

        TAutoPtr<IEventHandle> handle;
        auto result = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
        UNIT_ASSERT(result);

        auto& resRead = Proto(result);
        UNIT_ASSERT_EQUAL(resRead.GetOrigin(), TTestTxConfig::TxTablet0);
        UNIT_ASSERT_EQUAL(resRead.GetTxInitiator(), metaShard);
        UNIT_ASSERT_EQUAL(resRead.GetStatus(), NKikimrTxColumnShard::EResultStatus::ERROR);
        UNIT_ASSERT_EQUAL(resRead.GetBatch(), 0);
        UNIT_ASSERT_EQUAL(resRead.GetFinished(), true);
        UNIT_ASSERT_EQUAL(resRead.GetData(), "");
    }
}

}

Y_UNIT_TEST_SUITE(TColumnShardTestReadWrite) {
    Y_UNIT_TEST(Write) {
        TestWriteImpl(TTestSchema::YdbSchema());
    }

    Y_UNIT_TEST(WriteExoticTypes) {
        TestWriteImpl(TTestSchema::YdbExoticSchema());
    }

    Y_UNIT_TEST(WriteRead) {
        TestWriteReadImpl(false);
    }

    Y_UNIT_TEST(WriteReadExoticTypes) {
        TestWriteReadImpl(false, TTestSchema::YdbExoticSchema());
    }

    Y_UNIT_TEST(RebootWriteRead) {
        TestWriteReadImpl(true);
    }

    Y_UNIT_TEST(WriteReadNoCompression) {
        TestWriteReadImpl(true, TTestSchema::YdbSchema(), "none");
    }

    Y_UNIT_TEST(WriteReadZSTD) {
        TestWriteReadImpl(true, TTestSchema::YdbSchema(), "zstd");
    }

    Y_UNIT_TEST(CompactionInGranule) {
        TestCompactionInGranuleImpl(false);
    }

    Y_UNIT_TEST(RebootCompactionInGranule) {
        TestCompactionInGranuleImpl(true);
    }

    Y_UNIT_TEST(TestReadWithProgram) {
        TestReadWithProgramImpl();
    }

    Y_UNIT_TEST(CompactionSplitGranule) {
        TTestBasicRuntime runtime;
        TTester::Setup(runtime);

        TActorId sender = runtime.AllocateEdgeActor();
        CreateTestBootstrapper(runtime, CreateTestTabletInfo(TTestTxConfig::TxTablet0, TTabletTypes::COLUMNSHARD), &CreateColumnShard);

        TDispatchOptions options;
        options.FinalEvents.push_back(TDispatchOptions::TFinalEventCondition(TEvTablet::EvBoot));
        runtime.DispatchEvents(options);

        ui64 metaShard = TTestTxConfig::TxTablet1;
        ui64 writeId = 0;
        ui64 tableId = 1;
        ui64 planStep = 100;
        ui64 txId = 100;

        SetupSchema(runtime, sender, tableId);
        TAutoPtr<IEventHandle> handle;

        // Write different keys: grow on compatcion

        static const ui32 triggerPortionSize = 85 * 1000;
        static const ui32 overlapSize = 5 * 1000;

        ui32 numWrites = 23;
        for (ui32 i = 0; i < numWrites; ++i, ++writeId, ++planStep, ++txId) {
            ui64 start = i * (triggerPortionSize - overlapSize);
            std::pair<ui64, ui64> triggerPortion = {start, start + triggerPortionSize};
            TString triggerData = MakeTestBlob(triggerPortion, testYdbSchema);
            UNIT_ASSERT(triggerData.size() > NColumnShard::TLimits::MIN_BYTES_TO_INSERT);
            UNIT_ASSERT(triggerData.size() < NColumnShard::TLimits::MAX_BLOB_SIZE);

            UNIT_ASSERT(WriteData(runtime, sender, metaShard, writeId, tableId, triggerData));

            ProposeCommit(runtime, sender, metaShard, txId, {writeId});
            PlanCommit(runtime, sender, planStep, txId);
        }

        // TODO: Move tablet's time to the future with mediator timecast instead
        --planStep;
        --txId;

        ui32 numRows = numWrites * (triggerPortionSize - overlapSize) + overlapSize;
        TString schema;

        for (ui32 i = 0; i < 2; ++i) {
            {
                auto read = std::make_unique<TEvColumnShard::TEvRead>(sender, metaShard, planStep, txId, tableId);
                Proto(read.get()).AddColumnNames("timestamp");
                Proto(read.get()).AddColumnNames("message");

                ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, read.release());
            }

            ui32 expected = 0;
            ui32 num = 0;
            TVector<TString> readData;
            while (!expected || num < expected) {
                auto event = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
                UNIT_ASSERT(event);

                auto& resRead = Proto(event);
                UNIT_ASSERT_EQUAL(resRead.GetOrigin(), TTestTxConfig::TxTablet0);
                UNIT_ASSERT_EQUAL(resRead.GetTxInitiator(), metaShard);
                UNIT_ASSERT_EQUAL(resRead.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
                UNIT_ASSERT(resRead.GetData().size() > 0);
                //UNIT_ASSERT_EQUAL(resRead.GetBatch(), 0);

                readData.push_back(resRead.GetData());

                if (resRead.GetFinished()) {
                    expected = resRead.GetBatch() + 1;

                    UNIT_ASSERT(resRead.HasMeta());

                    auto& meta = resRead.GetMeta();
                    schema = meta.GetSchema();

                    UNIT_ASSERT(meta.HasReadStats());
                    auto& readStats = meta.GetReadStats();

                    UNIT_ASSERT(readStats.GetBeginTimestamp() > 0);
                    UNIT_ASSERT(readStats.GetDurationUsec() > 0);
                    UNIT_ASSERT_VALUES_EQUAL(readStats.GetSelectedIndex(), 0);
                    UNIT_ASSERT(readStats.GetIndexBatches() > 0);
                    //UNIT_ASSERT_VALUES_EQUAL(readStats.GetNotIndexedBatches(), 0); // TODO
                    UNIT_ASSERT_VALUES_EQUAL(readStats.GetUsedColumns(), 7); // planStep, txId + 4 PK columns + "message"
                    UNIT_ASSERT_VALUES_EQUAL(readStats.GetIndexGranules(), 3); // got 2 split compactions
                    //UNIT_ASSERT_VALUES_EQUAL(readStats.GetIndexPortions(), x);
                }

                ++num;
                UNIT_ASSERT(num < 100);
            }

            UNIT_ASSERT(DataHas(readData, schema, {0, numRows}, true));
            readData.clear();

            { // read with predicate (TO)
                auto read = std::make_unique<TEvColumnShard::TEvRead>(sender, metaShard, planStep, txId, tableId);
                Proto(read.get()).AddColumnNames("timestamp");
                Proto(read.get()).AddColumnNames("message");

                TSerializedTableRange range = MakeTestRange({0, 1000}, false, false, testYdbPkSchema);
                NOlap::TPredicate prGreater, prLess;
                std::tie(prGreater, prLess) = RangePredicates(range, testYdbPkSchema);

                auto* less = Proto(read.get()).MutableLessPredicate();
                for (auto& name : prLess.ColumnNames()) {
                    less->AddColumnNames(name);
                }

                less->SetRow(NArrow::SerializeBatchNoCompression(prLess.Batch));
                //less->SetInclusive(prLess.Inclusive); TODO

                ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, read.release());
            }

            { // one result expected
                auto event = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
                UNIT_ASSERT(event);

                auto& resRead = Proto(event);
                UNIT_ASSERT_EQUAL(resRead.GetOrigin(), TTestTxConfig::TxTablet0);
                UNIT_ASSERT_EQUAL(resRead.GetTxInitiator(), metaShard);
                UNIT_ASSERT_EQUAL(resRead.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
                UNIT_ASSERT(resRead.GetData().size() > 0);
                UNIT_ASSERT_EQUAL(resRead.GetBatch(), 0);
                UNIT_ASSERT(resRead.GetFinished());

                {
                    UNIT_ASSERT(resRead.HasMeta());
                    auto& meta = resRead.GetMeta();
                    //auto& schema = meta.GetSchema();

                    UNIT_ASSERT(meta.HasReadStats());
                    auto& readStats = meta.GetReadStats();

                    UNIT_ASSERT(readStats.GetBeginTimestamp() > 0);
                    UNIT_ASSERT(readStats.GetDurationUsec() > 0);
                    UNIT_ASSERT_VALUES_EQUAL(readStats.GetSelectedIndex(), 0);
                    UNIT_ASSERT(readStats.GetIndexBatches() > 0);
                    //UNIT_ASSERT_VALUES_EQUAL(readStats.GetNotIndexedBatches(), 0); // TODO
                    UNIT_ASSERT_VALUES_EQUAL(readStats.GetUsedColumns(), 7); // planStep, txId + 4 PK columns + "message"
                    UNIT_ASSERT_VALUES_EQUAL(readStats.GetIndexGranules(), 1);
                }

                // TODO: check data
            }

            { // read with predicate (FROM)
                auto read = std::make_unique<TEvColumnShard::TEvRead>(sender, metaShard, planStep, txId, tableId);
                Proto(read.get()).AddColumnNames("timestamp");
                Proto(read.get()).AddColumnNames("message");

                TSerializedTableRange range = MakeTestRange({2000 * 1000, 1000 * 1000 * 1000}, false, false, testYdbPkSchema);
                NOlap::TPredicate prGreater, prLess;
                std::tie(prGreater, prLess) = RangePredicates(range, testYdbPkSchema);

                auto* greater = Proto(read.get()).MutableGreaterPredicate();
                for (auto& name : prGreater.ColumnNames()) {
                    greater->AddColumnNames(name);
                }

                greater->SetRow(NArrow::SerializeBatchNoCompression(prGreater.Batch));
                //greater->SetInclusive(prGreater.Inclusive); TODO

                ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, read.release());
            }

            { // one result expected
                auto event = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
                UNIT_ASSERT(event);

                auto& resRead = Proto(event);
                UNIT_ASSERT_EQUAL(resRead.GetOrigin(), TTestTxConfig::TxTablet0);
                UNIT_ASSERT_EQUAL(resRead.GetTxInitiator(), metaShard);
                UNIT_ASSERT_EQUAL(resRead.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);
                UNIT_ASSERT(resRead.GetData().size() > 0);
                UNIT_ASSERT_EQUAL(resRead.GetBatch(), 0);
                UNIT_ASSERT(resRead.GetFinished());

                {
                    UNIT_ASSERT(resRead.HasMeta());
                    auto& meta = resRead.GetMeta();
                    //auto& schema = meta.GetSchema();

                    UNIT_ASSERT(meta.HasReadStats());
                    auto& readStats = meta.GetReadStats();

                    UNIT_ASSERT(readStats.GetBeginTimestamp() > 0);
                    UNIT_ASSERT(readStats.GetDurationUsec() > 0);
                    UNIT_ASSERT_VALUES_EQUAL(readStats.GetSelectedIndex(), 0);
                    UNIT_ASSERT(readStats.GetIndexBatches() > 0);
                    //UNIT_ASSERT_VALUES_EQUAL(readStats.GetNotIndexedBatches(), 0); // TODO
                    UNIT_ASSERT_VALUES_EQUAL(readStats.GetUsedColumns(), 7); // planStep, txId + 4 PK columns + "message"
                    UNIT_ASSERT_VALUES_EQUAL(readStats.GetIndexGranules(), 1);
                }

                // TODO: check data
            }

            RebootTablet(runtime, TTestTxConfig::TxTablet0, sender);
        }

        { // Get index stats
            ScanIndexStats(runtime, sender, {tableId, 42}, {planStep, txId}, 0);
            auto scanInited = runtime.GrabEdgeEvent<NKqp::TEvKqpCompute::TEvScanInitActor>(handle);
            auto& msg = scanInited->Record;
            auto scanActorId = ActorIdFromProto(msg.GetScanActorId());

            ui32 resultLimit = 1024 * 1024;
            runtime.Send(new IEventHandle(scanActorId, sender, new NKqp::TEvKqpCompute::TEvScanDataAck(resultLimit)));
            auto scan = runtime.GrabEdgeEvent<NKqp::TEvKqpCompute::TEvScanData>(handle);
            auto batchStats = scan->ArrowBatch;
            UNIT_ASSERT(batchStats);
            // Cerr << batchStats->ToString() << Endl;
            UNIT_ASSERT_VALUES_EQUAL(batchStats->num_rows(), 5);

            for (ui32 i = 0; i < batchStats->num_rows(); ++i) {
                auto paths = batchStats->GetColumnByName("PathId");
                auto kinds = batchStats->GetColumnByName("Kind");
                auto rows = batchStats->GetColumnByName("Rows");
                auto bytes = batchStats->GetColumnByName("Bytes");
                auto rawBytes = batchStats->GetColumnByName("RawBytes");

                ui64 pathId = static_cast<arrow::UInt64Array&>(*paths).Value(i);
                ui32 kind = static_cast<arrow::UInt32Array&>(*kinds).Value(i);
                ui64 numRows = static_cast<arrow::UInt64Array&>(*rows).Value(i);
                ui64 numBytes = static_cast<arrow::UInt64Array&>(*bytes).Value(i);
                ui64 numRawBytes = static_cast<arrow::UInt64Array&>(*rawBytes).Value(i);

                if (pathId == tableId) {
                    if (kind == 2) {
                        UNIT_ASSERT_VALUES_EQUAL(numRows, (triggerPortionSize - overlapSize) * numWrites + overlapSize);
                        UNIT_ASSERT(numBytes > numRows);
                        UNIT_ASSERT(numRawBytes > numBytes);
                    }
                } else {
                    UNIT_ASSERT_VALUES_EQUAL(numRows, 0);
                    UNIT_ASSERT_VALUES_EQUAL(numBytes, 0);
                    UNIT_ASSERT_VALUES_EQUAL(numRawBytes, 0);
                }

                Cerr << pathId << " " << kind << " " << numRows << " " << numBytes << " " << numRawBytes << "\n";
            }
        }
    }

    Y_UNIT_TEST(ReadStale) {
        TTestBasicRuntime runtime;
        TTester::Setup(runtime);

        TActorId sender = runtime.AllocateEdgeActor();
        CreateTestBootstrapper(runtime, CreateTestTabletInfo(TTestTxConfig::TxTablet0, TTabletTypes::COLUMNSHARD), &CreateColumnShard);

        TDispatchOptions options;
        options.FinalEvents.push_back(TDispatchOptions::TFinalEventCondition(TEvTablet::EvBoot));
        runtime.DispatchEvents(options);

        ui64 metaShard = TTestTxConfig::TxTablet1;
        ui64 writeId = 0;
        ui64 tableId = 1;
        ui64 planStep = 1000000;
        ui64 txId = 100;

        SetupSchema(runtime, sender, tableId);
        TAutoPtr<IEventHandle> handle;

        // Write some test data to adavnce the time
        {
            std::pair<ui64, ui64> triggerPortion = {1, 1000};
            TString triggerData = MakeTestBlob(triggerPortion, testYdbSchema);

            UNIT_ASSERT(WriteData(runtime, sender, metaShard, writeId, tableId, triggerData));

            ProposeCommit(runtime, sender, metaShard, txId, {writeId});
            PlanCommit(runtime, sender, planStep, txId);
        }

        TDuration staleness = TDuration::Minutes(6);

        // Try to read snapshot that is too old
        {
            {
                auto request = std::make_unique<TEvColumnShard::TEvRead>(sender, metaShard, planStep - staleness.MilliSeconds(), Max<ui64>(), tableId);
                request->Record.AddColumnNames("timestamp");
                request->Record.AddColumnNames("message");

                ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, request.release());
            }

            auto event = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
            UNIT_ASSERT(event);

            auto& response = event->Record;
            UNIT_ASSERT_VALUES_EQUAL(response.GetOrigin(), TTestTxConfig::TxTablet0);
            UNIT_ASSERT_VALUES_EQUAL(response.GetTxInitiator(), metaShard);
            UNIT_ASSERT_VALUES_EQUAL(response.GetStatus(), (ui32)NKikimrTxColumnShard::EResultStatus::ERROR);
        }

        // Try to scan snapshot that is too old
        {
            {
                auto request = std::make_unique<TEvColumnShard::TEvScan>();
                request->Record.SetTxId(1000);
                request->Record.SetScanId(1);
                request->Record.SetLocalPathId(tableId);
                request->Record.SetTablePath("test_olap_table");
                request->Record.MutableSnapshot()->SetStep(planStep - staleness.MilliSeconds());
                request->Record.MutableSnapshot()->SetTxId(Max<ui64>());

                ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, request.release());
            }

            auto event = runtime.GrabEdgeEvent<NKqp::TEvKqpCompute::TEvScanError>(handle);
            UNIT_ASSERT(event);

            auto& response = event->Record;
            // Cerr << response << Endl;
            UNIT_ASSERT_VALUES_EQUAL(response.GetStatus(), Ydb::StatusIds::BAD_REQUEST);
            UNIT_ASSERT_VALUES_EQUAL(response.IssuesSize(), 1);
            UNIT_ASSERT_STRING_CONTAINS(response.GetIssues(0).message(), "Snapshot 640000:18446744073709551615 too old");
        }
    }

    // Private events of different actors reuse the same ES_PRIVATE range
    // So in order to capture the right private event we need to check its type via dynamic_cast
    template <class TPrivateEvent>
    TPrivateEvent* TryGetPrivateEvent(TAutoPtr<IEventHandle> &ev) {
        if (ev->GetTypeRewrite() != TPrivateEvent::EventType) {
            return nullptr;
        }
        return dynamic_cast<TPrivateEvent*>(ev->GetBase());
    }

    void TestCompactionGC(bool enableSmallBlobs) {
        TTestBasicRuntime runtime;
        TTester::Setup(runtime);

        runtime.SetLogPriority(NKikimrServices::BLOB_CACHE, NActors::NLog::PRI_DEBUG);

        TActorId sender = runtime.AllocateEdgeActor();
        CreateTestBootstrapper(runtime, CreateTestTabletInfo(TTestTxConfig::TxTablet0, TTabletTypes::COLUMNSHARD), &CreateColumnShard);

        TDispatchOptions options;
        options.FinalEvents.push_back(TDispatchOptions::TFinalEventCondition(TEvTablet::EvBoot));
        runtime.DispatchEvents(options);

        ui64 metaShard = TTestTxConfig::TxTablet1;
        ui64 writeId = 0;
        ui64 tableId = 1;

        SetupSchema(runtime, sender, tableId);
        TAutoPtr<IEventHandle> handle;

        bool blockReadFinished = true;
        THashSet<ui64> inFlightReads;
        ui64 addedPortions = 0;
        THashSet<ui64> oldPortions;
        THashSet<ui64> deletedPortions;
        THashSet<TString> deletedBlobs;
        THashSet<TString> delayedBlobs;
        ui64 compactionsHappened = 0;
        ui64 cleanupsHappened = 0;

        auto captureEvents = [&](TTestActorRuntimeBase&, TAutoPtr<IEventHandle> &ev) {
            if (auto* msg = TryGetPrivateEvent<NColumnShard::TEvPrivate::TEvReadFinished>(ev)) {
                Cerr <<  "EvReadFinished " << msg->RequestCookie << Endl;
                inFlightReads.insert(msg->RequestCookie);
                if (blockReadFinished) {
                    return true;
                }
            } else if (auto* msg = TryGetPrivateEvent<NColumnShard::TEvPrivate::TEvWriteIndex>(ev)) {
                // Cerr <<  "EvWriteIndex" << Endl << *msg->IndexChanges << Endl;

                if (!msg->IndexChanges->AppendedPortions.empty()) {
                    Cerr << "Added portions:";
                    for (const auto& portion : msg->IndexChanges->AppendedPortions) {
                        ++addedPortions;
                        ui64 portionId = addedPortions;
                        Cerr << " " << portionId << "(" << portion.Records[0].Portion << ")";
                    }
                    Cerr << Endl;
                }
                if (msg->IndexChanges->CompactionInfo) {
                    ++compactionsHappened;
                    Cerr << "Compaction at snaphsot "<< msg->IndexChanges->ApplySnapshot
                        << " old portions:";
                    ui64 srcGranule{0};
                    for (const auto& portionInfo : msg->IndexChanges->SwitchedPortions) {
                        ui64 granule = portionInfo.Granule();
                        Y_VERIFY(!srcGranule || srcGranule == granule);
                        srcGranule = granule;
                        ui64 portionId = portionInfo.Portion();
                        Cerr << " " << portionId;
                        oldPortions.insert(portionId);
                    }
                    Cerr << Endl;
                }
                if (!msg->IndexChanges->PortionsToDrop.empty()) {
                    ++cleanupsHappened;
                    Cerr << "Cleanup older than snaphsot "<< msg->IndexChanges->InitSnapshot
                        << " old portions:";
                    for (const auto& portion : msg->IndexChanges->PortionsToDrop) {
                        ui64 portionId = portion.Records[0].Portion;
                        Cerr << " " << portionId;
                        deletedPortions.insert(portionId);
                    }
                    Cerr << Endl;
                }
            } else if (auto* msg = TryGetPrivateEvent<NActors::NLog::TEvLog>(ev)) {
                bool matchedEvent = false;
                {
                    TString prefixes[2] = {"Delay Delete Blob ", "Delay Delete Small Blob "};
                    for (TString prefix : prefixes) {
                        size_t pos = msg->Line.find(prefix);
                        if (pos != TString::npos) {
                            TString blobId = msg->Line.substr(pos + prefix.size());
                            Cerr << "Delayed delete: " << blobId << Endl;
                            delayedBlobs.insert(blobId);
                            matchedEvent = true;
                            break;
                        }
                    }
                }
                if (!matchedEvent){
                    TString prefix = "Delete Small Blob ";
                    size_t pos = msg->Line.find(prefix);
                    if (pos != TString::npos) {
                        TString blobId = msg->Line.substr(pos + prefix.size());
                        Cerr << "Delete small blob: " << blobId << Endl;
                        deletedBlobs.insert(blobId);
                        delayedBlobs.erase(blobId);
                        matchedEvent = true;
                    }
                }
            } else if (auto* msg = TryGetPrivateEvent<NKikimr::TEvBlobStorage::TEvCollectGarbage>(ev)) {
                // Extract and save all DoNotKeep blobIds
                Cerr << "GC for channel " << msg->Channel;
                if (msg->DoNotKeep) {
                    Cerr << " deletes blobs: " << JoinStrings(msg->DoNotKeep->begin(), msg->DoNotKeep->end(), " ");
                    for (const auto& blobId : *msg->DoNotKeep) {
                        deletedBlobs.insert(blobId.ToString());
                        delayedBlobs.erase(TUnifiedBlobId(0, blobId).ToStringNew());
                    }
                }
                Cerr << Endl;
            }
            return false;
        };
        runtime.SetEventFilter(captureEvents);

        // Enable/Disable small blobs
        {
            TAtomic unused;
            TAtomic maxSmallBlobSize = enableSmallBlobs ? 1000000 : 0;
            runtime.GetAppData().Icb->SetValue("ColumnShardControls.MaxSmallBlobSize",maxSmallBlobSize, unused);
        }

        // Disable GC batching so that deleted blobs get collected without a delay
        {
            TAtomic unusedPrev;
            runtime.GetAppData().Icb->SetValue("ColumnShardControls.BlobCountToTriggerGC", 1, unusedPrev);
        }

        // Write different keys: grow on compaction

        static const ui32 triggerPortionSize = 75 * 1000;
        std::pair<ui64, ui64> triggerPortion = {0, triggerPortionSize};
        TString triggerData = MakeTestBlob(triggerPortion, testYdbSchema);
        UNIT_ASSERT(triggerData.size() > NColumnShard::TLimits::MIN_BYTES_TO_INSERT);
        UNIT_ASSERT(triggerData.size() < NColumnShard::TLimits::MAX_BLOB_SIZE);

        ui64 planStep = 5000000;
        ui64 txId = 1000;

        // Ovewrite the same data multiple times to produce multiple portions at different timestamps
        ui32 numWrites = 14; // trigger split granule compaction by GranuleBlobSplitSize
        for (ui32 i = 0; i < numWrites; ++i, ++writeId, ++planStep, ++txId) {
            UNIT_ASSERT(WriteData(runtime, sender, metaShard, writeId, tableId, triggerData));

            ProposeCommit(runtime, sender, metaShard, txId, {writeId});
            PlanCommit(runtime, sender, planStep, txId);
        }

        // Do a small write that is not indexed so that we will get a committed blob in read request
        {
            TString smallData = MakeTestBlob({0, 2}, testYdbSchema);
            UNIT_ASSERT(smallData.size() < 100 * 1024);
            UNIT_ASSERT(WriteData(runtime, sender, metaShard, writeId, tableId, smallData));

            ProposeCommit(runtime, sender, metaShard, txId, {writeId});
            PlanCommit(runtime, sender, planStep, txId);
            ++writeId;
            ++planStep;
            ++txId;
        }

        --planStep;
        --txId;

        UNIT_ASSERT_VALUES_EQUAL(compactionsHappened, 2); // we catch it twice per action

        ui64 previousCompactionsHappened = compactionsHappened;
        ui64 previousCleanupsHappened = cleanupsHappened;

        // Send a request that reads the latest version
        // This request is expected to read at least 1 committed blob and several index portions
        // These committed blob and portions must not be deleted by the BlobManager until the read request finishes
        auto read = std::make_unique<TEvColumnShard::TEvRead>(sender, metaShard, planStep, txId, tableId);
        Proto(read.get()).AddColumnNames("timestamp");
        Proto(read.get()).AddColumnNames("message");

        ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, read.release());

        ui32 expected = 0;
        ui32 num = 0;
        while (!expected || num < expected) {
            auto event = runtime.GrabEdgeEvent<TEvColumnShard::TEvReadResult>(handle);
            UNIT_ASSERT(event);

            auto& resRead = Proto(event);
            UNIT_ASSERT_EQUAL(resRead.GetOrigin(), TTestTxConfig::TxTablet0);
            UNIT_ASSERT_EQUAL(resRead.GetTxInitiator(), metaShard);
            UNIT_ASSERT_EQUAL(resRead.GetStatus(), NKikimrTxColumnShard::EResultStatus::SUCCESS);

            if (resRead.GetFinished()) {
                expected = resRead.GetBatch() + 1;
                UNIT_ASSERT(resRead.HasMeta());
            }
            UNIT_ASSERT(resRead.GetData().size() > 0);

            ++num;
            UNIT_ASSERT(num < 10);
        }

        // We captured EvReadFinished event and dropped is so the columnshard still thinks that
        // read request is in progress and keeps the portions

        // Advance the time in order to trigger GC
        TDuration delay = TDuration::Minutes(6);
        planStep += delay.MilliSeconds();
        numWrites = 10; // trigger in granule compaction by size
        for (ui32 i = 0; i < numWrites; ++i, ++writeId, ++planStep, ++txId) {
            UNIT_ASSERT(WriteData(runtime, sender, metaShard, writeId, tableId, triggerData));

            ProposeCommit(runtime, sender, metaShard, txId, {writeId});
            PlanCommit(runtime, sender, planStep, txId);
        }

        Cerr << "Compactions happened: " << compactionsHappened << Endl;
        Cerr << "Cleanups happened: " << cleanupsHappened << Endl;
        Cerr << "Old portions: " << JoinStrings(oldPortions.begin(), oldPortions.end(), " ") << Endl;
        Cerr << "Cleaned up portions: " << JoinStrings(deletedPortions.begin(), deletedPortions.end(), " ") << Endl;

        // Check that GC happened but it didn't collect some old poritons
        UNIT_ASSERT_GT(compactionsHappened, previousCompactionsHappened);
        UNIT_ASSERT_GT(cleanupsHappened, previousCleanupsHappened);
        UNIT_ASSERT_GT_C(oldPortions.size(), deletedPortions.size(), "Some old portions must not be deleted because the are in use by read");
        UNIT_ASSERT_GT_C(delayedBlobs.size(), 0, "Read request is expected to have at least one committed blob, which deletion must be delayed");
        previousCompactionsHappened = compactionsHappened;
        previousCleanupsHappened = cleanupsHappened;

        // Send EvReadFinished to release kept portions
        blockReadFinished = false;
        UNIT_ASSERT_VALUES_EQUAL(inFlightReads.size(), 1);
        {
            auto read = std::make_unique<NColumnShard::TEvPrivate::TEvReadFinished>(*inFlightReads.begin());
            ForwardToTablet(runtime, TTestTxConfig::TxTablet0, sender, read.release());
        }

        // Advance the time and trigger some more compactions and cleanups
        planStep += 2*delay.MilliSeconds();
        numWrites = 10;
        for (ui32 i = 0; i < numWrites; ++i, ++writeId, ++planStep, ++txId) {
            UNIT_ASSERT(WriteData(runtime, sender, metaShard, writeId, tableId, triggerData));

            ProposeCommit(runtime, sender, metaShard, txId, {writeId});
            PlanCommit(runtime, sender, planStep, txId);
        }

        Cerr << "Compactions happened: " << compactionsHappened << Endl;
        Cerr << "Cleanups happened: " << cleanupsHappened << Endl;
        Cerr << "Old portions: " << JoinStrings(oldPortions.begin(), oldPortions.end(), " ") << Endl;
        Cerr << "Cleaned up portions: " << JoinStrings(deletedPortions.begin(), deletedPortions.end(), " ") << Endl;

        // Check that previously kept portions are collected
        UNIT_ASSERT_GE(compactionsHappened, previousCompactionsHappened);
        UNIT_ASSERT_GT(cleanupsHappened, previousCleanupsHappened);
        UNIT_ASSERT_VALUES_EQUAL_C(oldPortions.size(), deletedPortions.size(), "All old portions must be deleted after read has finished");
        UNIT_ASSERT_VALUES_EQUAL_C(delayedBlobs.size(), 0, "All previously delayed deletions must now happen");
    }

    Y_UNIT_TEST(CompactionGC) {
        TestCompactionGC(false);
    }

    Y_UNIT_TEST(CompactionGCWithSmallBlobs) {
        TestCompactionGC(true);
    }
}

}
